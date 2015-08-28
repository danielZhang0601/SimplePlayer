/*
 * NativePlayer_jni.cpp
 *
 *  Created on: 2015骞�8鏈�10鏃�
 *      Author: danielzhang
 */

//#include "com_reju_family_NativePlayer.h"
#include <jni.h>
#include "PlayerCore.h"

const char * class_name = "com/reju/family/NativePlayer";
PlayerCore player;

/*
 * methods list
 */
void native_init(JNIEnv *env, jobject clazz) {

}

void native_set_datasource(JNIEnv *env, jobject clazz, jstring path) {
	player.init(env->GetStringUTFChars(path, JNI_FALSE));
}

void native_start(JNIEnv *env, jobject clazz) {
	player.start();
}

void native_pause(JNIEnv *env, jobject clazz) {
	player.setIsPause(!player.getIsPause());
}

void native_stop(JNIEnv *env, jobject clazz) {
	player.stop();
}

jboolean native_save_frame(JNIEnv *env, jobject clazz, jstring path) {
	return player.saveFrame(env->GetStringUTFChars(path, NULL));
}

void native_set_speed(JNIEnv *env, jobject clazz, jint speed) {
	LOGE("speed:%d",speed);
	switch (speed) {
	case 0:
		player.setSpeed(80000);
		break;
	case 1:
		player.setSpeed(40000);
		break;
	case 2:
		player.setSpeed(10000);
		break;
	}
}

void native_surface_create(JNIEnv *env, jobject clazz) {

}

jboolean native_surface_changed(JNIEnv *env, jobject clazz, jint width,
		jint height) {
	return player.setupGraphics(width, height);
}

void native_draw_frame(JNIEnv *env, jobject clazz) {
	player.renderFrame();
}

/**
 * Methods
 */
static JNINativeMethod gMethods[] ={
		{ "nativeInit", "()V", (void *) native_init },
		{ "nativeSetDataSource", "(Ljava/lang/String;)V", (void *) native_set_datasource },
		{ "nativeStart", "()V",	(void *) native_start },
		{ "nativePause", "()V",	(void *) native_pause },
		{ "nativeStop", "()V", (void *) native_stop },
		{ "nativeSaveFrame", "(Ljava/lang/String;)Z", (void *) native_save_frame },
		{ "nativeSetPlaySpeed", "(I)V", (void *) native_set_speed },
		{ "nativeSurfaceCreated", "()V", (void *) native_surface_create },
		{ "nativeSurfaceChanged", "(II)Z", (void *) native_surface_changed },
		{ "nativeDrawFrame", "()V", (void *) native_draw_frame }, };

int register_native_methods(JNIEnv *env) {
	jclass clazz;

	clazz = env->FindClass(class_name);
	if (clazz == NULL) {
		LOGE("Can't find class %s\n", class_name);
		return -1;
	}
	if (env->RegisterNatives(clazz, gMethods,
			sizeof(gMethods) / sizeof(gMethods[0])) != JNI_OK) {
		LOGE("Failed registering methods for %s\n", class_name);
		return -1;
	}
	return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	JNIEnv* env = NULL;

	if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		LOGE("ERROR: GetEnv failed\n");
		return JNI_FALSE;
	}
	if (register_native_methods(env) < 0) {
		LOGE("ERROR: Native registration failed");
		return JNI_FALSE;
	}
	return JNI_VERSION_1_4;
}
