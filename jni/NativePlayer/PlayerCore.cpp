#include "PlayerCore.h"
#include <sys/time.h>
#include <unistd.h>

static const GLfloat vertexVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, 1.0f, };
static const GLfloat textureVertices[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, };

void *data_thread_fun(void* arg) {
	return ((PlayerCore*) arg)->dataThreadFun();
}

void *decode_thread_fun(void* arg) {
	return ((PlayerCore*) arg)->decodeThreadFun();
}

PlayerCore::PlayerCore() {
	pFormatCtx = NULL;
	pCodecCtx = NULL;
	pCodec = NULL;
	videoindex = -1;
	preDecodeListMax = 10;
	decodedListMax = 3;
	pthread_mutex_init(&preDecodeListMutex, NULL);
	pthread_mutex_init(&decodedListMutex, NULL);
	dataThread = (pthread_t) 0;
	decodeThread = (pthread_t) 0;
	isRunning = false;
	startTime = -1;
}

PlayerCore::~PlayerCore() {
	pthread_mutex_destroy(&preDecodeListMutex);
	pthread_mutex_destroy(&decodedListMutex);
}

bool PlayerCore::init(const char* path) {
	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	if (avformat_open_input(&pFormatCtx, path, NULL, NULL) != 0) {
		LOGE("Couldn't open input stream:%s.", path);
		return false;
	}
	LOGI("Open file success:%s.", path);
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		LOGE("Couldn't find stream information.");
		return false;
	}
	LOGI("File stream information.");
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	}
	if (videoindex == -1) {
		LOGE("Didn't find a video stream.");
		return false;
	}
	LOGI("Find video stream:%d.", videoindex);
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	if (pCodecCtx == NULL) {
		LOGE("Codec context not found.");
		return false;
	}
	LOGI("Find codec context:%d", pCodecCtx->codec_id);
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		LOGE("Codec not found.");
		return false;
	}
	LOGI("Find codec:%d.", pCodec->id);
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		LOGE("Could not open codec.");
		return false;
	}
	LOGI("Open codec success.");
}

void PlayerCore::start() {
	isRunning = true;
	pthread_create(&dataThread, NULL, data_thread_fun, (void*) this);
	pthread_create(&decodeThread, NULL, decode_thread_fun, (void*) this);
}

void PlayerCore::addToPreDecodeList(AVPacket* packet) {
	pthread_mutex_lock(&preDecodeListMutex);
	preDecodeList.push_back(packet);
	pthread_mutex_unlock(&preDecodeListMutex);
}

AVPacket* PlayerCore::getFromPreDecodeList() {
	pthread_mutex_lock(&preDecodeListMutex);
	AVPacket* packet = preDecodeList.front();
	preDecodeList.pop_front();
	pthread_mutex_unlock(&preDecodeListMutex);
	return packet;
}

void PlayerCore::addToDecodedList(AVFrame* frame) {
	pthread_mutex_lock(&decodedListMutex);
	decodedList.push_back(frame);
	pthread_mutex_unlock(&decodedListMutex);
}

void *PlayerCore::dataThreadFun() {

	while (isRunning) {
		if (preDecodeList.size() >= preDecodeListMax) {
			wait();
			continue;
		}
		AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
		if (av_read_frame(pFormatCtx, packet) >= 0) {
			addToPreDecodeList(packet);
		}
	}
	return 0;
}

void *PlayerCore::decodeThreadFun() {
	int ret, gotPicture;
	while (true) {
		LOGE("preDecodeList size: %d",preDecodeList.size());
		LOGE("decodedList size: %d",decodedList.size());
		if (preDecodeList.size() < 1) {
			wait();
			continue;
		}
		if (decodedList.size() >= decodedListMax) {
			wait();
			continue;
		}
		AVPacket *packet = getFromPreDecodeList();
		if (packet->stream_index == videoindex) {
			AVFrame *pFrame = av_frame_alloc();
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &gotPicture, packet);
			if (ret < 0) {
				LOGE("Decode Error.\n");
				return 0;
			}
			if (gotPicture) {
				LOGE("gotPicture:%d",gotPicture);
				addToDecodedList(pFrame);
			}
		}
		av_free_packet(packet);
	}
	return 0;
}

AVFrame* PlayerCore::getFromDecodedList() {
	pthread_mutex_lock(&decodedListMutex);
	AVFrame* frame = decodedList.front();
	decodedList.pop_front();
	pthread_mutex_unlock(&decodedListMutex);
	return frame;
}

bool PlayerCore::getRunning() {
	return isRunning;
}

void PlayerCore::setRunning(bool isRun) {
	isRunning = isRun;
}

bool PlayerCore::setupGraphics(int width, int height) {
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	LOGI("setupGraphics(%d, %d)", width, height);
	gProgram = createProgram(gVertexShader, gFragmentShader);
	if (!gProgram) {
		LOGE("Could not create program.");
		return false;
	}
	LOGI("create program:%d", gProgram);
	glUseProgram(gProgram);

	textureUniformY = glGetUniformLocation(gProgram, "tex_y");
	checkGlError("glGetUniformLocation tex_y");
	textureUniformU = glGetUniformLocation(gProgram, "tex_u");
	checkGlError("glGetUniformLocation tex_u");
	textureUniformV = glGetUniformLocation(gProgram, "tex_v");
	checkGlError("glGetUniformLocation tex_v");

	glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexVertices);
	checkGlError("glVertexAttribPointer vertexVertices");
	glEnableVertexAttribArray(ATTRIB_VERTEX);
	checkGlError("glEnableVertexAttribArray ATTRIB_VERTEX");
	glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
	checkGlError("glVertexAttribPointer textureVertices");
	glEnableVertexAttribArray(ATTRIB_TEXTURE);
	checkGlError("glEnableVertexAttribArray ATTRIB_TEXTURE");

	glGenTextures(1, &g_texYId);
	checkGlError("glGenTextures g_texYId");
	glBindTexture(GL_TEXTURE_2D, g_texYId);
	checkGlError("glBindTexture g_texYId");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texYId GL_TEXTURE_MAG_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texYId GL_TEXTURE_MIN_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texYId GL_TEXTURE_WRAP_S");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texYId GL_TEXTURE_WRAP_T");

	glGenTextures(1, &g_texUId);
	checkGlError("glGenTextures g_texUId");
	glBindTexture(GL_TEXTURE_2D, g_texUId);
	checkGlError("glBindTexture g_texUId");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texUId GL_TEXTURE_MAG_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texUId GL_TEXTURE_MIN_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texUId GL_TEXTURE_WRAP_S");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texUId GL_TEXTURE_WRAP_T");

	glGenTextures(1, &g_texVId);
	checkGlError("glGenTextures g_texVId");
	glBindTexture(GL_TEXTURE_2D, g_texUId);
	checkGlError("glBindTexture g_texVId");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texVId GL_TEXTURE_MAG_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	checkGlError("glTexParameteri g_texVId GL_TEXTURE_MIN_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texVId GL_TEXTURE_WRAP_S");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	checkGlError("glTexParameteri g_texVId GL_TEXTURE_WRAP_T");

	glViewport(0, 0, width, height);
	checkGlError("glViewport");
	return true;
}

void PlayerCore::renderFrame() {

}

void PlayerCore::wait() {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 30;
	select(0, NULL, NULL, NULL, &tv);
}

void PlayerCore::printGLString(const char* name, GLenum s) {
	const char *v = (const char *) glGetString(s);
	LOGI("GL %s = %s\n", name, v);
}

void PlayerCore::checkGlError(const char* op) {
	LOGI("checkGlError %s", op);
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGE("after %s() glError (0x%x)\n", op, error);
	}
}

GLuint PlayerCore::loadShader(GLenum shaderType, const char* pSource) {
	GLuint shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &pSource, NULL);
		glCompileShader(shader);
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen) {
				char* buf = (char*) malloc(infoLen);
				if (buf) {
					glGetShaderInfoLog(shader, infoLen, NULL, buf);
					LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
					free(buf);
				}
				glDeleteShader(shader);
				shader = 0;
			}
		}
	}
	return shader;
}

GLuint PlayerCore::createProgram(const char* pVertexSource,
		const char* pFragmentSource) {
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
	if (!vertexShader) {
		return 0;
	}

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
	if (!pixelShader) {
		return 0;
	}

	GLuint program = glCreateProgram();
	if (program) {
		glAttachShader(program, vertexShader);
		checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		checkGlError("glAttachShader");
		glBindAttribLocation(program, ATTRIB_VERTEX, "vertexIn");
		checkGlError("glBindAttribLocation vertexIn");
		glBindAttribLocation(program, ATTRIB_TEXTURE, "textureIn");
		checkGlError("glBindAttribLocation textureIn");
		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE) {
			GLint bufLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
			if (bufLength) {
				char* buf = (char*) malloc(bufLength);
				if (buf) {
					glGetProgramInfoLog(program, bufLength, NULL, buf);
					LOGE("Could not link program:\n%s\n", buf);
					free(buf);
				}
			}
			glDeleteProgram(program);
			program = 0;
		}
	}
	return program;
}
