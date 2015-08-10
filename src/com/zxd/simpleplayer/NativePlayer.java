package com.zxd.simpleplayer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLSurfaceView.Renderer;

public class NativePlayer implements Renderer
{
	
	enum PlaySpeed{
		SLOW,
		NORMAL,
		FAST
	}
	
	public void init(){
		
	}
	
	private native void nativeInit();
	
	public void setDataSource(String path){
		
	}
	
	private native void nativeSetDataSource(String path);
	
	public void start(){
		
	}
	
	private native void nativeStart();
	
	public void pause(){
		
	}
	
	private native void nativePause();
	
	public void stop(){
		
	}
	
	private native void nativeStop();
	
	public void setPlaySpeed(PlaySpeed speed){
		
	}
	
	private native void nativeSetPlaySpeed(int speed);
	
	private native void nativeSurfaceCreated();
	
	private native boolean nativeSurfaceChanged(int width,int height);
	
	private native void nativeDrawFrame();

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config)
	{
//		nativeSurfaceCreated();
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height)
	{
//		nativeSurfaceChanged(width,height);
	}

	@Override
	public void onDrawFrame(GL10 gl)
	{
		//nativeDrawFrame();
	}

	
}
