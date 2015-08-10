/*
 * NativePlayer_jni.cpp
 *
 *  Created on: 2015年8月10日
 *      Author: danielzhang
 */

#include "com_zxd_simpleplayer_NativePlayer.h"

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeInit(
		JNIEnv *env, jobject clazz) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSetDataSource(
		JNIEnv *env, jobject clazz, jstring path) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeStart(
		JNIEnv *env, jobject clazz) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativePause(
		JNIEnv *env, jobject clazz) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeStop(
		JNIEnv *env, jobject clazz) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSetPlaySpeed(
		JNIEnv *env, jobject clazz, jint speed) {

}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSurfaceCreated(
		JNIEnv *env, jobject clazz) {

}

JNIEXPORT jboolean JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSurfaceChanged(
		JNIEnv *env, jobject clazz, jint width, jint height) {
	return true;
}

JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeDrawFrame(
		JNIEnv *env, jobject clazz) {
}
