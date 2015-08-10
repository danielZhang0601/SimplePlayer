/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_zxd_simpleplayer_NativePlayer */

#ifndef _Included_com_zxd_simpleplayer_NativePlayer
#define _Included_com_zxd_simpleplayer_NativePlayer
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeInit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeInit
  (JNIEnv *, jobject);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeSetDataSource
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSetDataSource
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeStart
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeStart
  (JNIEnv *, jobject);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativePause
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativePause
  (JNIEnv *, jobject);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeStop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeStop
  (JNIEnv *, jobject);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeSetPlaySpeed
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSetPlaySpeed
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeSurfaceCreated
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSurfaceCreated
  (JNIEnv *, jobject);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeSurfaceChanged
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeSurfaceChanged
  (JNIEnv *, jobject, jint, jint);

/*
 * Class:     com_zxd_simpleplayer_NativePlayer
 * Method:    nativeDrawFrame
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_zxd_simpleplayer_NativePlayer_nativeDrawFrame
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif