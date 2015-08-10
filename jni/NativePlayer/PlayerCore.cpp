#include "PlayerCore.h"
#include "Alog.h"
#include <sys/time.h>
#include <unistd.h>

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

void PlayerCore::wait() {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 30;
	select(0, NULL, NULL, NULL, &tv);
}
