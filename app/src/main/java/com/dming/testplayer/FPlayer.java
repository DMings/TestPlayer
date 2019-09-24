package com.dming.testplayer;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.atomic.AtomicInteger;

public class FPlayer implements SurfaceHolder.Callback {
    public static class PlayStatus {
        public static int IDLE = 0;
        public static int PREPARE = 1;
        public static int PLAYING = 2;
        public static int STOPPING = 3;
    }

    private OnProgressListener mOnProgressListener;
    private Surface mSurface;
    private String mSrcPath;
    private boolean isDelayToPlay = false;
    private HandlerThread mPlayThread;
    private Handler mPlayHandler;
    //    private Lock mLock = new ReentrantLock();
    private Runnable mPrepareRunnable;
    private AtomicInteger mControlStatus = new AtomicInteger(PlayStatus.IDLE); // 0 idle 1 prepare 2 playing
    private Handler mMainHandle = new Handler(Looper.getMainLooper());
    private int mSurfaceWidth;
    private int mSurfaceHeight;
    private OnSurfaceChange mOnSurfaceChange;
//    private EglHelper mEglHelper;

    public FPlayer(SurfaceView surfaceView) {
        surfaceView.getHolder().addCallback(this);
//        mEglHelper = new EglHelper();
        mPlayThread = new HandlerThread("FPlayer");
        mPlayThread.start();
        mPlayHandler = new Handler(mPlayThread.getLooper());
    }

    public void setOnProgressListener(OnProgressListener onProgressListener, OnSurfaceChange onSurfaceChange) {
        mOnProgressListener = onProgressListener;
        mOnSurfaceChange = onSurfaceChange;
    }

    public int play(final String srcPath, Runnable prepareRunnable) {
        mSrcPath = srcPath;
        mPrepareRunnable = prepareRunnable;
        int ret = mControlStatus.get();
        if (ret == PlayStatus.IDLE || ret == PlayStatus.PLAYING) {
            if (mSurface != null) {
                changeToPlay();
            } else {
                isDelayToPlay = true;
            }
            return ret;
        } else { // 处于正在准备
            return ret;
        }
    }

    public void searchFile(String path, FileAction fileAction) {
        scan_file(path, fileAction);
    }

    public void seekTime(float percent) {
        resume();
        seek(percent);
    }

    public void onResume() {
        resume();
    }

    public void onPause() {
        pause();
    }

    public void onDestroy() {
        release();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mPlayThread.quitSafely();
        } else {
            mPlayThread.quit();
        }
        try {
            mPlayThread.join();
        } catch (InterruptedException e) {
            DLog.e("Join encountered an error!");
        }
        mSurface = null;
    }

    public long getDurationTime() {
        return get_current_time();
    }

    public int getPlayState() {
        int s = get_play_state();
        Log.i("DMFF", "getPlayState:     " + s);
        return s;
    }

    private void changeToPlay() {
        release();
        startPlay();
    }

    private void startPlay() {
        mPlayHandler.post(new Runnable() {
            @Override
            public void run() {
                mControlStatus.set(PlayStatus.PREPARE);
                mMainHandle.post(mPrepareRunnable);
            }
        });
        mPlayHandler.post(playRunnable);
    }


    private Runnable playRunnable = new Runnable() {
        @Override
        public void run() {
            mControlStatus.set(PlayStatus.PLAYING);
            FPlayer.play(mSrcPath, mSurface, mSurfaceWidth, mSurfaceHeight, new OnProgressListener() {
                @Override
                public void onProgress(int curTime, int totalTime) {
                    if (mOnProgressListener != null) {
                        mOnProgressListener.onProgress(curTime, totalTime);
                    }
                }
            });
            mControlStatus.set(PlayStatus.IDLE);
        }
    };

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
//        mEglHelper.initEgl(null, holder.getSurface());
//        mEglHelper.glBindThread();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
//        GLES20.glClearColor(1, 0, 0, 1);
//        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
//        mEglHelper.swapBuffers();
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        mSurface = holder.getSurface();
        Log.i("OpenGL", "OpenGL: width: " + width + " height: " + height);
        if (isDelayToPlay) {
            isDelayToPlay = false;
            startPlay();
        } else {
            FPlayer.update_surface(mSurface, width, height);
            mOnSurfaceChange.change();
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
//        mEglHelper.destroyEgl();
    }

    public interface OnSurfaceChange {
        void change();
    }

    static {
        System.loadLibrary("fplayer");
    }

    public static native void play(String path, Surface surface, int width, int height, OnProgressListener onProgressListener);

    public static native void seek(float percent);

    public static native long get_current_time();

    public static native long get_duration_time();

    public static native void pause();

    public static native void resume();

    public static native int get_play_state();

    public static native void update_surface(Surface surface, int width, int height);

    public static native void release();

    public static native void scan_file(String path, FileAction fileAction);

}
