package com.reju.family;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLSurfaceView.Renderer;
import android.os.Handler;

public class NativePlayer implements Renderer {

	enum PlaySpeed {
		SLOW, NORMAL, FAST
	}

	public static final int MSG_PLAY = 1;
	public static final int MSG_CACHE = 2;
	public static final int MSG_STOP = 3;

	private Handler nativeHandler;

	public void setNativeHandler(Handler nativeHandler) {
		this.nativeHandler = nativeHandler;
	}

	public void init() {

	}

	private native void nativeInit();

	public void setDataSource(String path) {
		nativeSetDataSource(path);
	}

	private native void nativeSetDataSource(String path);

	public void start() {
		nativeStart();
	}

	private native void nativeStart();

	public void pause() {
		nativePause();
	}

	private native void nativePause();

	public void stop() {
		nativeStop();
	}

	private native void nativeStop();

	public boolean saveFrame(String path) {
		return nativeSaveFrame(path);
	}

	private native boolean nativeSaveFrame(String path);

	public void setPlaySpeed(PlaySpeed speed) {
		nativeSetPlaySpeed(speed.ordinal());
	}

	private native void nativeSetPlaySpeed(int speed);

	private native void nativeSurfaceCreated();

	private native boolean nativeSurfaceChanged(int width, int height);

	private native void nativeDrawFrame();

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {
		nativeSurfaceCreated();
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height) {
		nativeSurfaceChanged(width, height);
	}

	@Override
	public void onDrawFrame(GL10 gl) {
		nativeDrawFrame();
	}

	static {
		System.loadLibrary("ffmpeg");
		System.loadLibrary("native_player");
	}
}
