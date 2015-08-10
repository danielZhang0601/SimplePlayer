package com.zxd.simpleplayer;

import android.app.Activity;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

public class MainActivity extends Activity
{

	private PlayerGLSurfaceView glSurfaceView;
	private Button play,pause,stop,slow,normal,fast;
	
	private NativePlayer player;
	
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		DisplayMetrics dm = getResources().getDisplayMetrics();
		LinearLayout layout = new LinearLayout(this);
		layout.setOrientation(LinearLayout.VERTICAL);
		glSurfaceView = new PlayerGLSurfaceView(this);
		LinearLayout playButtonLayout = new LinearLayout(this);
		playButtonLayout.setOrientation(LinearLayout.HORIZONTAL);
		play = new Button(this);
		play.setText(R.string.play);
		pause = new Button(this);
		pause.setText(R.string.pause);
		stop = new Button(this);
		stop.setText(R.string.stop);
		playButtonLayout.addView(play,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		playButtonLayout.addView(pause,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		playButtonLayout.addView(stop,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		LinearLayout speedButtonLayout = new LinearLayout(this);
		speedButtonLayout.setOrientation(LinearLayout.HORIZONTAL);
		slow = new Button(this);
		slow.setText(R.string.slow);
		normal = new Button(this);
		normal.setText(R.string.normal);
		fast = new Button(this);
		fast.setText(R.string.fast);
		speedButtonLayout.addView(slow,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		speedButtonLayout.addView(normal,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		speedButtonLayout.addView(fast,new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		layout.addView(glSurfaceView, dm.widthPixels, dm.widthPixels * 9 / 16);
		layout.addView(playButtonLayout);
		layout.addView(speedButtonLayout);
		setContentView(layout);
		
		player = new NativePlayer();
		glSurfaceView.setRenderer(player);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		if (id == R.id.action_settings)
		{
			return true;
		}
		return super.onOptionsItemSelected(item);
	}
}
