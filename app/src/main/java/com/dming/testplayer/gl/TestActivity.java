package com.dming.testplayer.gl;

import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.Surface;
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

    private native void testFF(String path, Surface surface);

    private native void seek(float percent);

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.act_test);
        mSeekBar = findViewById(R.id.test_sb);
        mSurfaceView = findViewById(R.id.sv_test);
        findViewById(R.id.btn_test_1).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

//                FragmentManager fragmentManager = getSupportFragmentManager();
//                FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
//                fragmentTransaction.replace(R.id.ll_test,new TestFragment());
//                fragmentTransaction.commit();

                new Thread(new Runnable() {
                    @Override
                    public void run() {
//                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/video2.mp4").getPath();
                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/animation.mp4").getPath();
                        mSurface = mSurfaceView.getHolder().getSurface();
                        testFF(srcPath, mSurface);
                    }
                }).start();
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
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
