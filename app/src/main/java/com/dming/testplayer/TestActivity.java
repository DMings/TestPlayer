package com.dming.testplayer;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
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
import com.dming.testplayer.FPlayer;
import com.dming.testplayer.OnProgressListener;
import com.dming.testplayer.R;

import java.io.File;


public class TestActivity extends AppCompatActivity {

    private Surface mSurface;
    private SurfaceView mSurfaceView;
    private SeekBar mSeekBar;
    TextView curTimeTv;
    TextView totalTimeTv;
    private boolean mSeeking = false;

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
                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/video.mp4").getPath();
//                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/animation.mp4").getPath();
                        FPlayer.play(srcPath, mSurface, new OnProgressListener() {
                            @Override
                            public void onProgress(final int curTime, final int totalTime) {
                                if (!mSeeking) {
                                    runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            curTimeTv.setText(DUtils.secToTime(curTime));
                                            totalTimeTv.setText(DUtils.secToTime(totalTime));
//                                            Log.i("DMFF", "curTime: " + curSecTime + " totalTime: " + totalSecTime);
                                            mSeekBar.setProgress((int) (100.0f * curTime / totalTime));
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
                FPlayer.pause();
            }
        });

        findViewById(R.id.btn_test_3).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                FPlayer.resume();
            }
        });
        findViewById(R.id.btn_test_4).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                FPlayer.release();
            }
        });

        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                curTimeTv.setText(DUtils.secToTime((long) (1.0f * seekBar.getProgress() / seekBar.getMax() * FPlayer.get_duration_time())));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                mSeeking = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                FPlayer.seek(1.0f * seekBar.getProgress() / seekBar.getMax());
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
                FPlayer.update_surface(mSurface);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.i("DMFF", "surfaceDestroyed");
            }
        });
    }

    @Override
    protected void onPause() {
        FPlayer.pause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        FPlayer.resume();
        super.onResume();
    }

    @Override
    protected void onDestroy() {
        FPlayer.release();
        super.onDestroy();
    }
}
