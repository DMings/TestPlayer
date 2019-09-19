package com.dming.testplayer;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.atomic.AtomicInteger;

public class FPlayer implements SurfaceHolder.Callback {

    private OnProgressListener mOnProgressListener;
    private Surface mSurface;
    private boolean mIsPause = false;
    private String mSrcPath;
    private boolean isDelayToPlay = false;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private AtomicInteger mPlayCount = new AtomicInteger(0);

    public FPlayer(SurfaceView surfaceView) {
        surfaceView.getHolder().addCallback(this);
        mHandlerThread = new HandlerThread("FPlayer");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        mOnProgressListener = onProgressListener;
    }

    public boolean play(final String srcPath) {
        mSrcPath = srcPath;
        if (mSurface != null) {
            return changeToPlay();
        } else {
            isDelayToPlay = true;
            return true;
        }
    }

    public void searchFile(String path, FileAction fileAction) {
        scan_file(path, fileAction);
    }

    public boolean pauseOrResume() {
        if (mIsPause) {
            mIsPause = false;
            resume();
        } else {
            mIsPause = true;
            pause();
        }
        return mIsPause;
    }

    public void seekTime(float percent) {
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
            mHandlerThread.quitSafely();
        } else {
            mHandlerThread.quit();
        }
        try {
            mHandlerThread.join(1000);
        } catch (InterruptedException e) {
            DLog.e("Join encountered an error!");
        }
        mSurface = null;
    }

    public long getDurationTime() {
        return get_current_time();
    }


    private boolean changeToPlay() {
        DLog.e("changeToPlay: "+Thread.currentThread());
        if (mPlayCount.get() <= 1) {
            release();
            mHandler.post(playRunnable);
            return true;
        } else {
            return false;
        }
    }


    private Runnable playRunnable = new Runnable() {
        @Override
        public void run() {
            mPlayCount.getAndAdd(1);
            DLog.e("-mPlayCount: "+mPlayCount.get());
            FPlayer.play(mSrcPath, mSurface, new OnProgressListener() {
                @Override
                public void onProgress(long curTime, long totalTime) {
                    if (mOnProgressListener != null) {
                        mOnProgressListener.onProgress(curTime, totalTime);
                    }
                }
            });
            mPlayCount.getAndAdd(-1);
            DLog.e("+mPlayCount: "+mPlayCount.get());
        }
    };

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurface = holder.getSurface();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mSurface = holder.getSurface();
        if (isDelayToPlay) {
            isDelayToPlay = false;
            mHandler.post(playRunnable);
        } else {
            FPlayer.update_surface(mSurface);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    static {
        System.loadLibrary("fplayer");
    }

    public static native void play(String path, Surface surface, OnProgressListener onProgressListener);

    public static native void seek(float percent);

    public static native long get_current_time();

    public static native long get_duration_time();

    public static native void pause();

    public static native void resume();

    public static native void update_surface(Surface surface);

    public static native void release();

    public static native void scan_file(String path, FileAction fileAction);

}
