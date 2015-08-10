/*
 * PlayerCore.h
 *
 *  Created on: 2015年8月10日
 *      Author: danielzhang
 */

#ifndef NATIVEPLAYER_PLAYERCORE_H_
#define NATIVEPLAYER_PLAYERCORE_H_

//FFmpeg
#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <list>
#include <pthread.h>

class PlayerCore {
public:
	PlayerCore();
	~PlayerCore();

	bool init(const char *path);
	void start();
	void pause();
	void resume();
	void stop();
	bool getRunning();
	void setRunning(bool isRun);

	void *dataThreadFun();
	void *decodeThreadFun();

private:
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	int videoindex;
	bool isRunning;
	std::list<AVPacket *> preDecodeList;
	std::list<AVFrame *> decodedList;
	int preDecodeListMax, decodedListMax;
	pthread_mutex_t preDecodeListMutex, decodedListMutex;
	pthread_t dataThread, decodeThread;

	void addToPreDecodeList(AVPacket *packet);
	AVPacket *getFromPreDecodeList();
	void addToDecodedList(AVFrame *frame);
	AVFrame *getFromDecodedList();
	void wait();
};

#endif /* NATIVEPLAYER_PLAYERCORE_H_ */
