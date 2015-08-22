#include "PlayerCore.h"
#include <unistd.h>
#include <assert.h>

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
	audioindex = -1;
	preDecodeListMax = 10;
	decodedListMax = 3;
	pthread_mutex_init(&preDecodeListMutex, NULL);
	pthread_mutex_init(&decodedListMutex, NULL);
	dataThread = (pthread_t) 0;
	decodeThread = (pthread_t) 0;
	isRun = false;
	isPause = false;
	currentFrame = NULL;
	lastPTS = -1;
	speed = 40000;
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
		}else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioindex = i;
		}
		if (videoindex != -1 && audioindex != -1)
			break;
	}
	if (videoindex == -1) {
		LOGE("Didn't find a video stream.");
		return false;
	}

	if (audioindex == -1) {
		LOGE("Didn't find a audio stream.");
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
	isRun = true;

	pthread_create(&dataThread, NULL, data_thread_fun, (void*) this);
	pthread_create(&decodeThread, NULL, decode_thread_fun, (void*) this);
}

int PlayerCore::getPreDecodeListSize() {
	int count = 0;
	pthread_mutex_lock(&preDecodeListMutex);
	count = preDecodeList.size();
	pthread_mutex_unlock(&preDecodeListMutex);
	return count;
}

int PlayerCore::getDecodedListSize() {
	int count = 0;
	pthread_mutex_lock(&decodedListMutex);
	count = decodedList.size();
	pthread_mutex_unlock(&decodedListMutex);
	return count;
}

void PlayerCore::addToPreDecodeList(AVPacket* packet) {
	pthread_mutex_lock(&preDecodeListMutex);
	preDecodeList.push_back(packet);
	pthread_mutex_unlock(&preDecodeListMutex);
}

AVPacket* PlayerCore::getFromPreDecodeList() {
	AVPacket* packet = NULL;
	pthread_mutex_lock(&preDecodeListMutex);
	if (preDecodeList.size() > 0) {
		packet = preDecodeList.front();
		preDecodeList.pop_front();
	}
	pthread_mutex_unlock(&preDecodeListMutex);
	return packet;
}

void PlayerCore::addToDecodedList(AVFrame* frame) {
	AVFrame *sFrame = new AVFrame;
	memcpy(sFrame, frame, sizeof(AVFrame));

	sFrame->data[0] = new uint8_t[frame->width * frame->height];
	sFrame->data[1] = new uint8_t[frame->width * frame->height / 4];
	sFrame->data[2] = new uint8_t[frame->width * frame->height / 4];

	memcpy(sFrame->data[0], frame->data[0], frame->width * frame->height);
	memcpy(sFrame->data[1], frame->data[1], frame->width * frame->height / 4);
	memcpy(sFrame->data[2], frame->data[2], frame->width * frame->height / 4);

	pthread_mutex_lock(&decodedListMutex);
	decodedList.push_back(sFrame);
	pthread_mutex_unlock(&decodedListMutex);
}

AVFrame* PlayerCore::getFromDecodedList(bool isHold) {
	AVFrame* frame = NULL;
	pthread_mutex_lock(&decodedListMutex);
	if (decodedList.size() > 0) {
		frame = decodedList.front();
		if (!isHold)
			decodedList.pop_front();
	}
	pthread_mutex_unlock(&decodedListMutex);
	return frame;
}

void *PlayerCore::dataThreadFun() {

	while (isRun) {
		if (isPause) {
			wait();
			continue;
		}

		if (getPreDecodeListSize() >= preDecodeListMax) {
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

	while (isRun) {
		if (getPreDecodeListSize() < 1) {
			wait();
			continue;
		}

		if (getDecodedListSize() >= decodedListMax) {
			wait();
			continue;
		}

		AVPacket *packet = getFromPreDecodeList();
		if (packet->stream_index == videoindex) {

			AVFrame *pFrame = av_frame_alloc();
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &gotPicture, packet);
			if ((ret == 0) || (0 == gotPicture)) {
				LOGE("ret:%d, gotPicture:%d", ret, gotPicture);
			}

			if (ret < 0) {
				LOGE("Decode Error.\n");
				av_frame_free(&pFrame);
				av_free_packet(packet);
				return 0;
			}
			if (gotPicture) {
				addToDecodedList(pFrame);
				av_frame_free(&pFrame);
			} else {
				av_frame_free(&pFrame);
			}
		}else if (packet->stream_index = audioindex) {

		}
		av_free_packet(packet);
		packet = NULL;
	}
	return 0;
}

bool PlayerCore::getIsPause() {
	return isPause;
}

void PlayerCore::setIsPause(bool pause) {
	isPause = pause;
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

	//Init Texture Y
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

	//U
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

	//V
	glGenTextures(1, &g_texVId);
	checkGlError("glGenTextures g_texVId");
	glBindTexture(GL_TEXTURE_2D, g_texVId);
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
	/*if (!getFrameByTime()) {
	 }*/
	getFrameByTime();
	//currentFrame = getFromDecodedList(false);

	if (currentFrame == NULL) {
		return;
	}

	glClearColor(0.0, 0.0, 0.0, 1.0);
	checkGlError("glClearColor");
//	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glClear (GL_COLOR_BUFFER_BIT);
	checkGlError("glClear");
	glActiveTexture (GL_TEXTURE0);
	checkGlError("glActiveTexture GL_TEXTURE0");
	glBindTexture(GL_TEXTURE_2D, g_texYId);
	checkGlError("glBindTexture GL_TEXTURE0");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, currentFrame->width,
			currentFrame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
			currentFrame->data[0]);
	checkGlError("glTexImage2D GL_TEXTURE0");
	glUniform1i(textureUniformY, 0);
	checkGlError("glUniform1i GL_TEXTURE0");

	//U
	glActiveTexture (GL_TEXTURE1);
	checkGlError("glActiveTexture GL_TEXTURE1");
	glBindTexture(GL_TEXTURE_2D, g_texUId);
	checkGlError("glBindTexture GL_TEXTURE1");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, currentFrame->width / 2,
			currentFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
			currentFrame->data[1]);

	checkGlError("glTexImage2D GL_TEXTURE1");
	glUniform1i(textureUniformU, 1);
	checkGlError("glUniform1i GL_TEXTURE1");
	//V
	glActiveTexture (GL_TEXTURE2);
	checkGlError("glActiveTexture GL_TEXTURE2");
	glBindTexture(GL_TEXTURE_2D, g_texVId);
	checkGlError("glBindTexture GL_TEXTURE2");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, currentFrame->width / 2,
			currentFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
			currentFrame->data[2]);
	checkGlError("glTexImage2D GL_TEXTURE2");
	glUniform1i(textureUniformV, 2);
	checkGlError("glUniform1i GL_TEXTURE2");

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	checkGlError("glDrawArrays");
	glFlush();
	checkGlError("glFlush");

	/*delete[] currentFrame->data[0];
	 delete[] currentFrame->data[1];
	 delete[] currentFrame->data[2];
	 delete currentFrame;
	 currentFrame = NULL;*/
}

void PlayerCore::setSpeed(int sp) {
	speed = sp;
}

void PlayerCore::stop() {
	isRun = false;
	if (getPreDecodeListSize() > 0) {
		clearPreDecodeList();
	}
	if (getDecodedListSize() > 0) {
		clearDecodeList();
	}
}

void PlayerCore::clearPreDecodeList() {
	pthread_mutex_lock(&preDecodeListMutex);
	preDecodeList.clear();
	pthread_mutex_unlock(&preDecodeListMutex);
}

void PlayerCore::clearDecodeList() {
	pthread_mutex_lock(&decodedListMutex);
	decodedList.clear();
	pthread_mutex_unlock(&decodedListMutex);
}

bool PlayerCore::getFrameByTime() {
	AVFrame * tmpFrame = getFromDecodedList(true);
	if (tmpFrame == NULL) {
		return false;
	}
	if (lastPTS < 0) {	//first
		lastPTS = tmpFrame->pkt_dts;
		gettimeofday(&lastShow, 0);
		currentFrame = getFromDecodedList(false);
		return true;
	} else {
		currentPTS = tmpFrame->pkt_dts;
		gettimeofday(&currentShow, 0);
//		LOGE("time :%ld,speed:%d",
//				(currentShow.tv_sec - lastShow.tv_sec) * 1000000
//						+ (currentShow.tv_usec - lastShow.tv_usec), speed);
		if ((currentShow.tv_sec - lastShow.tv_sec) * 1000000
				+ (currentShow.tv_usec - lastShow.tv_usec) > speed) {	//show
			lastPTS = currentPTS;
			lastShow = currentShow;

			delete[] currentFrame->data[0];
			delete[] currentFrame->data[1];
			delete[] currentFrame->data[2];
			delete currentFrame;
			currentFrame = getFromDecodedList(false);
			return true;
		} else {
			return false;
		}
	}

	/*if (currentFrame == NULL) {
	 currentFrame = getFromDecodedList(false);
	 }
	 if (currentFrame == NULL) {
	 return false;
	 }
	 if (lastPTS < 0) {	//first
	 lastPTS = currentFrame->pkt_dts;
	 gettimeofday(&lastShow, 0);
	 return true;
	 } else {
	 currentPTS = currentFrame->pkt_dts;
	 gettimeofday(&currentShow, 0);
	 LOGE("time :%d",
	 (currentShow.tv_sec - lastShow.tv_sec) * 1000000
	 + (currentShow.tv_usec - lastShow.tv_usec));
	 if ((currentShow.tv_sec - lastShow.tv_sec) * 1000000
	 + (currentShow.tv_usec - lastShow.tv_usec) > speed) {	//show
	 lastPTS = currentPTS;
	 lastShow = currentShow;
	 return true;
	 } else {
	 return false;
	 }
	 }*/
}

bool PlayerCore::saveFrame(const char *filePath) {
	FILE *fp_save = fopen(filePath, "wb+");
	LOGE("save file:%s", filePath);
	if (NULL == fp_save) {
		LOGE("open error.");
		return false;
	}
	AVFrame *frame = getFromDecodedList(true);
	if (NULL != frame) {
		fwrite(frame->data[0], 1, frame->width * frame->height, fp_save);
		fwrite(frame->data[1], 1, frame->width * frame->height / 4, fp_save);
		fwrite(frame->data[2], 1, frame->width * frame->height / 4, fp_save);
	}
	fclose(fp_save);
	LOGE("save success.");
	return true;
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
//	LOGI("checkGlError %s", op);
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
