/*
 * Alog.h
 *
 *  Created on: 2015年8月10日
 *      Author: danielzhang
 */

#ifndef ALOG_H_
#define ALOG_H_

//log
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"NDK_INFO",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"NDK_INFO",__VA_ARGS__)

#endif /* ALOG_H_ */
