/*
 * PlayerCore.h
 *
 *  Created on: 2015骞�8鏈�10鏃�
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

//GL
#include <GLES/gl.h>
#include <GLES2/gl2ext.h>

// for native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "VertexShaderSL.h"
#include "FragmentShaderSL.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

#include "Alog.h"

#include <list>
#include <pthread.h>

#include <sys/time.h>

class PlayerCore {
public:
	PlayerCore();
	~PlayerCore();

	bool init(const char *path);
	void start();
	void stop();
	bool getIsPause();
	void setIsPause(bool pause);

	void *dataThreadFun();
	void *decodeThreadFun();
	void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

	bool setupGraphics(int width, int height);
	void renderFrame();
	bool saveFrame(const char *filePath);
	void setSpeed(int sp);
private:
	AVFormatContext *pFormatCtx;
	AVCodecContext *pVideoCodecCtx, *pAudioCodecCtx;
	AVCodec *pVideoCodec, *pAudioCodec;
	AVFrame *currentFrame;

	int videoindex,audioindex;
	bool isPause,isRun;
	std::list<AVPacket *> preDecodeList;
	std::list<AVFrame *> decodedList;
	int preDecodeListMax, decodedListMax;
	pthread_mutex_t preDecodeListMutex, decodedListMutex;
	pthread_t dataThread, decodeThread;

	int64_t lastPTS,currentPTS;
	struct timeval lastShow, currentShow;
	int speed;

	GLuint gProgram;
	GLuint g_texYId, g_texUId, g_texVId;
	GLuint textureUniformY, textureUniformU, textureUniformV;

	//opensl es
	SLObjectItf engineObject;
	SLEngineItf engineEngine;

	SLObjectItf outputMixObject;
	SLEnvironmentalReverbItf outputMixEnvironmentalReverb;

	SLObjectItf bqPlayerObject;
	SLPlayItf bqPlayerPlay;
	SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
	SLEffectSendItf bqPlayerEffectSend;
	SLMuteSoloItf bqPlayerMuteSolo;
	SLVolumeItf bqPlayerVolume;

	short *nextBuffer;
	unsigned nextSize;
	int nextCount;

	int getPreDecodeListSize();
	int getDecodedListSize();
	void clearPreDecodeList();
	void clearDecodeList();
	void addToPreDecodeList(AVPacket *packet);
	AVPacket *getFromPreDecodeList();
	void addToDecodedList(AVFrame *frame);
	AVFrame *getFromDecodedList(bool isHold);
	void wait();

	void printGLString(const char *name, GLenum s);
	void checkGlError(const char* op);
	GLuint loadShader(GLenum shaderType, const char* pSource);
	GLuint createProgram(const char* pVertexSource,
			const char* pFragmentSource);
	bool getFrameByTime();
	//opensl es
	void createEngine();
	void createBufferQueueAudioPlayer(int rate, int channel, int bitsPerSample);
	void audioWrite(AVFrame *pFrame);
};

#endif /* NATIVEPLAYER_PLAYERCORE_H_ */
