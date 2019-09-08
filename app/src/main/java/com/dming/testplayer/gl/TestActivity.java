package com.dming.testplayer.gl;

import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.Surface;
import android.view.View;
import com.dming.testplayer.R;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;


public class TestActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;
    private EglHelper mEglHelper2 = new EglHelper();
    private NormalFilter mNoFilter;
    private int mTexture = -1;
    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private Handler mMainHandler = new Handler();
    private FSurfaceView mFSurfaceView;
    private ByteBuffer mByteBuffer;
    private int count = 0;
    private LineGraph mLineGraph;
    int w = 856;
    int h = 480;
    int fmt = 4;
    private int vH, vW;

    private native void startPlay(String path, Runnable runnable);

    private native void testFF(String path, Runnable init, Runnable update);

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.act_test);
        mFSurfaceView = findViewById(R.id.sv_test);
        mHandlerThread = new HandlerThread("gl2");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());

        findViewById(R.id.btn_test_1).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        String srcPath = new File(Environment.getExternalStorageDirectory(), "1/animation.mp4").getPath();
                        testFF(srcPath, new Runnable() {
                            @Override
                            public void run() {
//                        856 x 480
                                DLog.i("ndk call>>>gl init");
                                mEglHelper2.glBindThread();
//                                mTexture = FGLUtils.createTexture();
                                mTexture = 1;
                                GLES20.glViewport(0, 0, w, h);
                                GLES20.glClearColor(0, 0, 0, 1);
                                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                                FGLUtils.glCheckErr("gl init");
//                                mByteBuffer = ByteBuffer.allocateDirect(w * h * fmt);
//                                mByteBuffer.order(ByteOrder.nativeOrder());
//                                mByteBuffer.position(0);
                            }
                        }, new Runnable() {
                            @Override
                            public void run() {
                                long time = System.currentTimeMillis();
                                DLog.i("count: " + count);
                                mByteBuffer.position(0);
                                count++;
                                if (count > 255) {
                                    count = 0;
                                }
                                for (int i = 0; i < w * h * fmt; i += fmt) {
                                    mByteBuffer.put((byte) count);
                                    mByteBuffer.put((byte) count);
                                    mByteBuffer.put((byte) count);
                                    mByteBuffer.put((byte) 0xFF);
                                }
                                mByteBuffer.position(0);
                                GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTexture);
                                GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, w, h, 0,
                                        GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, mByteBuffer);
                                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
                                FGLUtils.glCheckErr("mEglHelper2");
                                DLog.i("time: " + (System.currentTimeMillis() - time));
                            }
                        });
                    }
                }).start();
            }
        });

        mFSurfaceView.setGLRenderer(new FSurfaceView.GLRenderer() {
            @Override
            public void surfaceCreated() {
                mLineGraph = new LineGraph(TestActivity.this);
                mNoFilter = new NormalFilter(TestActivity.this);

                mSurfaceTexture = new SurfaceTexture(0);
                mSurface = new Surface(mSurfaceTexture);
                mSurfaceTexture.setDefaultBufferSize(w, h);
                mEglHelper2.initEgl(mFSurfaceView.getEglContext(), mSurface);
                FGLUtils.glCheckErr("surfaceCreated");
//                testTwoThread();
            }

            @Override
            public void surfaceChanged(int width, int height) {
                DLog.i("EglHelper surfaceChanged: " + width + " > " + height);
                vH = height;
                vW = width;
                mNoFilter.onChange(width, height, 0);
                mLineGraph.onChange(width, height, 0);
                GLES20.glClearColor(1, 1, 1, 1);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                FGLUtils.glCheckErr(11);

//                ByteBuffer readByte = ByteBuffer.allocateDirect(w * h * fmt);
//                GLES20.glReadPixels(0,0,w,h,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,readByte);
//                DLog.i("readByte 0: "+readByte.get(0) + " 1: " + readByte.get(1)+ " 2: " + readByte.get(2)+ " 3: " + readByte.get(3));
//                Bitmap bitmap = Bitmap.createBitmap(w,h, Bitmap.Config.ARGB_8888);
//                bitmap.copyPixelsFromBuffer(readByte);
//                mTestIv.setImageBitmap(bitmap);
                mLineGraph.onDraw(0, 0, 0, vW, vH);
                mFSurfaceView.swapBuffers();
            }

            @Override
            public void surfaceDestroyed() {
                mNoFilter.onDestroy();
                mLineGraph.onDestroy();
            }
        });
        mMainHandler.postDelayed(mRunnable, 16);
    }

    private Runnable mRunnable = new Runnable() {
        @Override
        public void run() {
            mFSurfaceView.postDraw(new Runnable() {
                @Override
                public void run() {
                    if (mTexture != -1) {
                        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTexture);
                        mLineGraph.onDraw(0, 0, 0, vW, vH);
                        mNoFilter.onDraw(mTexture, 0, 0, w, h);
                        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
                        FGLUtils.glCheckErr("Runnable");
                    }
                }
            });
            mMainHandler.postDelayed(mRunnable, 16);
        }
    };

    public void testTwoThread() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                int w = 200;
                int h = 200;
                int fmt = 4;
//                mSurfaceTexture = new SurfaceTexture(0);
//                mSurface = new Surface(mSurfaceTexture);
//                mSurfaceTexture.setDefaultBufferSize(w, h);
//                mEglHelper2.initEgl(mFSurfaceView.getEglContext(), mSurface);
                mEglHelper2.glBindThread();
                mTexture = FGLUtils.createTexture();
                GLES20.glViewport(0, 0, w, h);
                GLES20.glClearColor(0, 0, 1, 1);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                ByteBuffer byteBuffer = ByteBuffer.allocateDirect(w * h * fmt);
                byteBuffer.order(ByteOrder.nativeOrder());

                int count = 0;
                for (int j = 0; j < 3000; j++) {
//                    DLog.i("j>" + j);
                    byteBuffer.position(0);
                    count++;
                    if (count > 255) {
                        count = 0;
                    }
                    for (int i = 0; i < w * h * fmt; i += fmt) {
                        byteBuffer.put((byte) count);
                        byteBuffer.put((byte) count);
                        byteBuffer.put((byte) count);
                        byteBuffer.put((byte) 0xFF);
                    }
                    byteBuffer.position(0);
                    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
                    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTexture);
                    GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, w, h, 0,
                            GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, byteBuffer);
                    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
                }

                FGLUtils.glCheckErr("mEglHelper2");
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mFSurfaceView.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mFSurfaceView.onPause();
    }

    @Override
    protected void onDestroy() {
        if (mHandlerThread != null) {
            mHandlerThread.quit();
        }
        if (mSurfaceTexture != null) {
            mSurfaceTexture.release();
        }
        if (mSurface != null) {
            mSurface.release();
        }
        super.onDestroy();
    }
}
