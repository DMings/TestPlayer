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
import android.widget.TextView;
import com.dming.testplayer.DUtils;
import com.dming.testplayer.R;

import java.io.File;


public class TestActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private Surface mSurface;
    private SurfaceView mSurfaceView;
    private SeekBar mSeekBar;
    TextView curTimeTv;
    TextView totalTimeTv;
    private boolean mSeeking = false;

    private native void play(String path, Surface surface, OnProgressListener onProgressListener);

    private native void seek(float percent);

    private native long get_current_time();

    private native long get_duration_time();

    private native void pause();

    private native void resume();

    private native void update_surface(Surface surface);

    private native void release();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.act_test);
        mSeekBar = findViewById(R.id.play_sb);
        mSurfaceView = findViewById(R.id.sv_test);
        curTimeTv = findViewById(R.id.curTimeTv);
        totalTimeTv = findViewById(R.id.totalTimeTv);
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
                                if (!mSeeking) {
                                    runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            long curSecTime = curTime / 1000;
                                            long totalSecTime = totalTime / 1000;
                                            curTimeTv.setText(DUtils.secToTime(curSecTime));
                                            totalTimeTv.setText(DUtils.secToTime(totalSecTime));
                                            Log.i("DMFF", "curTime: " + curSecTime + " totalTime: " + totalSecTime);
                                            mSeekBar.setProgress((int) (100.0f * curSecTime / totalSecTime));
                                        }
                                    });
                                }
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
                mSeeking = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                seek(1.0f * seekBar.getProgress() / seekBar.getMax());
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mSeeking = false;
                    }
                });

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
