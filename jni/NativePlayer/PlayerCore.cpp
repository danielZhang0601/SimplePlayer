#include "PlayerCore.h"
#include <unistd.h>
#include <assert.h>

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

static const GLfloat vertexVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, 1.0f, };
static const GLfloat textureVertices[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, };

static const SLEnvironmentalReverbSettings reverbSettings =
		SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

void staticPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
	((PlayerCore*) context)->bqPlayerCallback(bq, NULL);
}

void *data_thread_fun(void* arg) {
	return ((PlayerCore*) arg)->dataThreadFun();
}

void *decode_thread_fun(void* arg) {
	return ((PlayerCore*) arg)->decodeThreadFun();
}

void ffmpeg_log(void* ptr, int level, const char* fmt, va_list vl) {
	if (level > 16) {
//		LOGI("ffmpeg log:%s", fmt);
	} else {
		LOGE("ffmpeg error:%s", fmt);
	}
}

PlayerCore::PlayerCore() {
	//ffmpeg
	pFormatCtx = NULL;
	pVideoCodecCtx = NULL;
	pAudioCodecCtx = NULL;
	pVideoCodec = NULL;
	pAudioCodec = NULL;
	videoindex = -1;
	audioindex = -1;
	preDecodeListMax = 10;
	decodedListMax = 3;

	//opensl
	engineObject = NULL;
	outputMixObject = NULL;
	outputMixEnvironmentalReverb = NULL;
	bqPlayerObject = NULL;
	//thread
	pthread_mutex_init(&preDecodeListMutex, NULL);
	pthread_mutex_init(&decodedListMutex, NULL);
	dataThread = (pthread_t) 0;
	decodeThread = (pthread_t) 0;

	//control
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
	avcodec_register_all();
	avformat_network_init();
	av_log_set_callback(ffmpeg_log);
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
		} else if (pFormatCtx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
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

	pVideoCodecCtx = pFormatCtx->streams[videoindex]->codec;
	if (pVideoCodecCtx == NULL) {
		LOGE("VideoCodec context not found.");
		return false;
	}
	LOGI("Find VideoCodec context:%d", pVideoCodecCtx->codec_id);

	pVideoCodec = avcodec_find_decoder(pVideoCodecCtx->codec_id);
	if (pVideoCodec == NULL) {
		LOGE("VideoCodec not found.");
		return false;
	}
	LOGI("Find VideoCodec:%d.", pVideoCodec->id);

	if (avcodec_open2(pVideoCodecCtx, pVideoCodec, NULL) < 0) {
		LOGE("Could not open VideoCodec.");
		return false;
	}
	LOGI("Open VideoCodec success.");

	pAudioCodecCtx = pFormatCtx->streams[audioindex]->codec;
	if (pAudioCodecCtx) {
		LOGI("Find AudioCodec context:%d", pAudioCodecCtx->codec_id);

		pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);
		if (pAudioCodec) {
			LOGI("Find AudioCodec:%d.", pAudioCodec->id);

			if (avcodec_open2(pAudioCodecCtx, pAudioCodec, NULL) < 0) {
				LOGE("Could not open AudioCodec.");
			} else {
				LOGI("Open AudioCodec success.");
			}
		} else {
			LOGE("AudioCodec not found.");
		}
	} else {
		LOGE("AudioCodec context not found.");
	}

	//opensl
	createEngine();
	createBufferQueueAudioPlayer(pAudioCodecCtx->sample_rate,
			pAudioCodecCtx->channels, SL_PCMSAMPLEFORMAT_FIXED_16);
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
	int ret, gotPicture = 0, gotAudio = 0;

	int count = 0;
	uint8_t *out_buffer;
	FILE *pFile = fopen("/sdcard/save.PCM", "wb+");

	if (pFile) {
		LOGI("open save file.");
	} else {
		LOGE("fail save file.");
	}

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pAudioCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels,
			out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	//FIX:Some Codec's Context Information is missing
	int in_channel_layout = av_get_default_channel_layout(pAudioCodecCtx->channels);
	//Swr
	struct SwrContext *au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout,
			out_sample_fmt, out_sample_rate, in_channel_layout,
			pAudioCodecCtx->sample_fmt, pAudioCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

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
			ret = avcodec_decode_video2(pVideoCodecCtx, pFrame, &gotPicture,
					packet);

			if (ret < 0) {
				LOGE("Decode Video Error.\n");
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
		} else if (packet->stream_index == audioindex) {
			AVFrame *pAudioFrame = av_frame_alloc();
			ret = avcodec_decode_audio4(pAudioCodecCtx, pAudioFrame, &gotAudio,
					packet);

			if (ret < 0) {
				LOGE("Decode Audio Error.\n");
				av_frame_free(&pAudioFrame);
				av_free_packet(packet);
				return 0;
			}
			if (gotAudio) {
//				audioWrite(pAudioFrame);
				swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pAudioFrame->data , pAudioFrame->nb_samples);
				(*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, out_buffer,
						out_buffer_size);
//				if (count++ < 1000) {
//					if (pFile) {
//						LOGI("index:%5d\t pts:%lld\t packet size:%d\n",count,packet->pts,packet->size);
//						fwrite(out_buffer, 1, out_buffer_size, pFile);
//					}
//				} else {
//					if (pFile) {
//						fclose(pFile);
//						delete pFile;
//						pFile = NULL;
//					}
//				}
				av_frame_free(&pAudioFrame);
			} else {
				av_frame_free(&pAudioFrame);
			}
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
	getFrameByTime();

	if (currentFrame == NULL) {
		return;
	}

	glClearColor(0.0, 0.0, 0.0, 1.0);
	checkGlError("glClearColor");
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
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
}

bool PlayerCore::saveFrame(const char *filePath) {
	AVFormatContext* pIMGFormatCtx;
	AVOutputFormat* fmt;
	AVStream* video_st;
	AVCodecContext* pIMGCodecCtx;
	AVCodec* pIMGCodec;

	uint8_t* picture_buf;
	AVFrame* picture;
	int size;

	AVFrame *tmpFram = getFromDecodedList(true);
	if (tmpFram == NULL) {
		LOGE("not get frame.");
		return false;
	}

	picture = av_frame_alloc();
	memcpy(picture, tmpFram, sizeof(AVFrame));

	int in_w = picture->width, in_h = picture->height;					//锟斤拷锟�

	LOGI("in_w:%d,in_h:%d", in_w, in_h);

	av_register_all();
	avcodec_register_all();

	avformat_alloc_output_context2(&pIMGFormatCtx, NULL, NULL, filePath);
	fmt = pIMGFormatCtx->oformat;

	video_st = avformat_new_stream(pIMGFormatCtx, 0);
	if (video_st == NULL) {
		return false;
	}
	pIMGCodecCtx = video_st->codec;
	pIMGCodecCtx->codec_id = fmt->video_codec;
	pIMGCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pIMGCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;

	pIMGCodecCtx->width = in_w;
	pIMGCodecCtx->height = in_h;

	pIMGCodecCtx->time_base.num = 1;
	pIMGCodecCtx->time_base.den = 25;
	//锟斤拷锟斤拷锟绞斤拷锟较�
	av_dump_format(pIMGFormatCtx, 0, filePath, 1);

	pIMGCodec = avcodec_find_encoder(pIMGCodecCtx->codec_id);
	if (!pIMGCodec) {
		LOGE("avcodec_find_encoder error");
		return false;
	}
	if (avcodec_open2(pIMGCodecCtx, pIMGCodec, NULL) < 0) {
		LOGE("avcodec_open2 error");
		return false;
	}

	//写锟侥硷拷头
	avformat_write_header(pIMGFormatCtx, NULL);

	AVPacket pkt;
	int y_size = pIMGCodecCtx->width * pIMGCodecCtx->height;
	av_new_packet(&pkt, y_size * 3);

	int got_picture = 0;
	//锟斤拷锟斤拷
	int ret = avcodec_encode_video2(pIMGCodecCtx, &pkt, picture, &got_picture);
	if (ret < 0) {
		LOGE("encode error锟斤拷\n");
		return false;
	}
	if (got_picture == 1) {
		pkt.stream_index = video_st->index;
		ret = av_write_frame(pIMGFormatCtx, &pkt);
	}

	av_free_packet(&pkt);
	//写锟侥硷拷尾
	av_write_trailer(pIMGFormatCtx);

	LOGI("save success.");

	if (video_st) {
		avcodec_close(video_st->codec);
		av_free(picture);
		av_free(picture_buf);
	}
	avio_close(pIMGFormatCtx->pb);
	avformat_free_context(pIMGFormatCtx);
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

void PlayerCore::createEngine() {
	SLresult result;

	// create engine
	result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// realize the engine
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// get the engine interface, which is needed in order to create other objects
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE,
			&engineEngine);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// create output mix, with environmental reverb specified as a non-required interface
	const SLInterfaceID ids[1] = { SL_IID_ENVIRONMENTALREVERB };
	const SLboolean req[1] = { SL_BOOLEAN_FALSE };
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1,
			ids, req);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// realize the output mix
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// get the environmental reverb interface
	// this could fail if the environmental reverb effect is not available,
	// either because the feature is not present, excessive CPU load, or
	// the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
	result = (*outputMixObject)->GetInterface(outputMixObject,
			SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	if (SL_RESULT_SUCCESS == result) {
		result =
				(*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
						outputMixEnvironmentalReverb, &reverbSettings);
		LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
				SL_RESULT_SUCCESS == result);
		(void) result;
	}
	// ignore unsuccessful result codes for environmental reverb, as it is optional for this example
}

void PlayerCore::createBufferQueueAudioPlayer(int rate, int channel,
		int bitsPerSample) {
	SLresult result;

	// configure audio source
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
			SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
//	SLDataFormat_PCM format_pcm = { SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
//			SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
//			SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN };

	SLDataFormat_PCM format_pcm;
	format_pcm.formatType = SL_DATAFORMAT_PCM;
	format_pcm.numChannels = channel;
	format_pcm.samplesPerSec = rate * 1000;
	format_pcm.bitsPerSample = bitsPerSample;
	format_pcm.containerSize = bitsPerSample;
	if (channel == 2)
		format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	else
		format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
	format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

	SLDataSource audioSrc = { &loc_bufq, &format_pcm };

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX,
			outputMixObject };
	SLDataSink audioSnk = { &loc_outmix, NULL };

	// create audio player
	const SLInterfaceID ids[3] = { SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND,
	/*SL_IID_MUTESOLO,*/SL_IID_VOLUME };
	const SLboolean req[3] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
	/*SL_BOOLEAN_TRUE,*/SL_BOOLEAN_TRUE };
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject,
			&audioSrc, &audioSnk, 3, ids, req);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// realize the player
	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// get the play interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY,
			&bqPlayerPlay);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// get the buffer queue interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
			&bqPlayerBufferQueue);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// register callback on the buffer queue
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue,
			staticPlayerCallback, this);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// get the effect send interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
			&bqPlayerEffectSend);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
	// get the mute/solo interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
	assert(SL_RESULT_SUCCESS == result);
	(void)result;
#endif

	// get the volume interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME,
			&bqPlayerVolume);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;

	// set the player's state to playing
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	assert(SL_RESULT_SUCCESS == result);
	LOGI("reach line : %d,result = SL_RESULT_SUCCESS?%d", __LINE__,
			SL_RESULT_SUCCESS == result);
	(void) result;
}

void PlayerCore::bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq,
		void* context) {
	assert(bq == bqPlayerBufferQueue);
	assert(NULL == context);
	// for streaming playback, replace this test by logic to find and fill the next buffer
	if (--nextCount > 0 && NULL != nextBuffer && 0 != nextSize) {
		SLresult result;
		// enqueue another buffer
		result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue,
				nextBuffer, nextSize);
		// the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
		// which for this code example would indicate a programming error
		assert(SL_RESULT_SUCCESS == result);
		(void) result;
	}
}

void PlayerCore::audioWrite(AVFrame* pFrame) {
	int data_size = av_samples_get_buffer_size(pFrame->linesize,
			pAudioCodecCtx->channels, pFrame->nb_samples,
			pAudioCodecCtx->sample_fmt, 1);
	(*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, pFrame->data[0],
			data_size);
}
