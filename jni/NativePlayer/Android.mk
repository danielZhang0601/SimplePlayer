LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := native_player
LOCAL_SRC_FILES := PlayerCore.cpp \
					NativePlayer_jni.cpp
LOCAL_LDLIBS := -llog -landroid -lGLESv2
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../FFmpeg/include $(LOCAL_PATH)/../
LOCAL_SHARED_LIBRARIES := ffmpeg
include $(BUILD_SHARED_LIBRARY)