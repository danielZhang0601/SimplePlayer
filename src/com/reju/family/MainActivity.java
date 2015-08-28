package com.reju.family;

import java.io.File;

import com.zxd.simpleplayer.R;

import android.app.Activity;
import android.graphics.Bitmap;
import android.media.audiofx.EnvironmentalReverb;
import android.os.Bundle;
import android.os.Environment;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.AnimationUtils;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.RelativeLayout;

public class MainActivity extends Activity implements OnClickListener {

	private ImageView snapShotView;
	private PlayerGLSurfaceView glSurfaceView;
	private Button play, pause, stop, slow, normal, fast;

	private NativePlayer player;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		DisplayMetrics dm = getResources().getDisplayMetrics();
		LinearLayout layout = new LinearLayout(this);
		layout.setOrientation(LinearLayout.VERTICAL);
		RelativeLayout videoLayout = new RelativeLayout(this);
		snapShotView = new ImageView(this);
		glSurfaceView = new PlayerGLSurfaceView(this);
		videoLayout.addView(glSurfaceView,
				new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
		videoLayout.addView(snapShotView,
				new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
		snapShotView.setVisibility(View.GONE);
		LinearLayout playButtonLayout = new LinearLayout(this);
		playButtonLayout.setOrientation(LinearLayout.HORIZONTAL);
		play = new Button(this);
		play.setText(R.string.play);
		pause = new Button(this);
		pause.setText(R.string.pause);
		stop = new Button(this);
		stop.setText(R.string.stop);
		playButtonLayout.addView(play, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		playButtonLayout.addView(pause, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		playButtonLayout.addView(stop, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		LinearLayout speedButtonLayout = new LinearLayout(this);
		speedButtonLayout.setOrientation(LinearLayout.HORIZONTAL);
		slow = new Button(this);
		slow.setText(R.string.slow);
		normal = new Button(this);
		normal.setText(R.string.normal);
		fast = new Button(this);
		fast.setText(R.string.fast);
		speedButtonLayout.addView(slow, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		speedButtonLayout.addView(normal, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		speedButtonLayout.addView(fast, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
		layout.addView(videoLayout, dm.widthPixels, dm.widthPixels * 9 / 16);
		layout.addView(playButtonLayout);
		layout.addView(speedButtonLayout);
		setContentView(layout);

		player = new NativePlayer();
		glSurfaceView.setRenderer(player);
		play.setOnClickListener(this);
		pause.setOnClickListener(this);
		player.setDataSource(Environment.getExternalStorageDirectory().toString() + File.separator + "720.mp4");
		slow.setOnClickListener(this);
		normal.setOnClickListener(this);
		fast.setOnClickListener(this);
		stop.setOnClickListener(this);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		if (id == R.id.action_settings) {
			return true;
		}
		return super.onOptionsItemSelected(item);
	}

	@Override
	public void onClick(View v) {
		if (v == play) {
			player.start();
		} else if (v == pause) {
			player.pause();
		} else if (v == stop) {
			String imagePath = Environment.getExternalStorageDirectory().toString() + File.separator + "IMG"
					+ System.currentTimeMillis() + ".jpg";
			boolean saveSuccess = player.saveFrame(imagePath);
			if (saveSuccess) {
				Bitmap bitmap = BitmapUtil.getLoacalBitmap(imagePath);
				if (bitmap != null) {
					snapShotView.setImageBitmap(bitmap);
					snapShotView.setVisibility(View.VISIBLE);
					snapShotView.startAnimation(AnimationUtils.loadAnimation(v.getContext(), R.anim.snapshot_out));
				}
			}
		} else if (v == slow) {
			player.setPlaySpeed(NativePlayer.PlaySpeed.SLOW);
		} else if (v == normal) {
			player.setPlaySpeed(NativePlayer.PlaySpeed.NORMAL);
		} else if (v == fast) {
			player.setPlaySpeed(NativePlayer.PlaySpeed.FAST);
		} else {

		}
	}
}
