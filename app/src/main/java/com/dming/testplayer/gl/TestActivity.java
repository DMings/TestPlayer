package com.dming.testplayer.gl;

import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.SeekBar;
import com.dming.testplayer.R;

import java.io.File;


public class TestActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private Surface mSurface;
    private SurfaceView mSurfaceView;
    private SeekBar mSeekBar;

    private native void play(String path, Surface surface,OnProgressListener onProgressListener);

    private native void seek(float percent);

    private native void pause();

    private native void resume();

    private native void update_surface(Surface surface);

    private native void release();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.act_test);
        mSeekBar = findViewById(R.id.test_sb);
        mSurfaceView = findViewById(R.id.sv_test);
        findViewById(R.id.btn_test_1).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/video2.mp4").getPath();
//                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/animation.mp4").getPath();
                        play(srcPath, mSurface, new OnProgressListener() {
                            @Override
                            public void onProgress(final long curTime, final long totalTime) {
                                runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        Log.i("DMFF","curTime: "+curTime+ " totalTime: "+totalTime);
                                        mSeekBar.setProgress((int) (100.0f * curTime / totalTime));
                                    }
                                });
                            }
                        });
                    }
                }).start();
            }
        });

        findViewById(R.id.btn_test_2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pause();
            }
        });

        findViewById(R.id.btn_test_3).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                resume();
            }
        });
        findViewById(R.id.btn_test_4).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                release();
            }
        });

        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                seek(1.0f * seekBar.getProgress() / seekBar.getMax());
            }
        });

        mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.i("DMFF", "surfaceCreated");
                mSurface = holder.getSurface();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.i("DMFF", "surfaceChanged");
                mSurface = holder.getSurface();
                update_surface(mSurface);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.i("DMFF", "surfaceDestroyed");
            }
        });
    }

    @Override
    protected void onPause() {
        pause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        resume();
        super.onResume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
