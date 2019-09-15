package com.dming.testplayer.gl;

import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AppCompatActivity;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.widget.LinearLayout;
import com.dming.testplayer.R;

import java.io.File;


public class TestActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private Surface mSurface;
    private SurfaceView mSurfaceView;

    private native void testFF(String path, Surface surface);

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.act_test);
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
                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/video2.mp4").getPath();
//                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/animation.mp4").getPath();
                        mSurface = mSurfaceView.getHolder().getSurface();
                        testFF(srcPath, mSurface);
                    }
                }).start();
            }
        });

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
