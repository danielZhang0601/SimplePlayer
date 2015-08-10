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
#include "VertexShaderSL.h"
#include "FragmentShaderSL.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

#include "Alog.h"

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

	bool setupGraphics(int width, int height);
	void renderFrame();

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

	long startTime;

	GLuint gProgram;
	GLuint g_texYId, g_texUId, g_texVId;
	GLuint textureUniformY, textureUniformU, textureUniformV;

	void addToPreDecodeList(AVPacket *packet);
	AVPacket *getFromPreDecodeList();
	void addToDecodedList(AVFrame *frame);
	AVFrame *getFromDecodedList();
	void wait();

	void printGLString(const char *name, GLenum s);
	void checkGlError(const char* op);
	GLuint loadShader(GLenum shaderType, const char* pSource);
	GLuint createProgram(const char* pVertexSource,
			const char* pFragmentSource);
};

#endif /* NATIVEPLAYER_PLAYERCORE_H_ */
