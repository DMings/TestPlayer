package com.dming.testplayer;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class FPlayer implements SurfaceHolder.Callback {

    private OnProgressListener mOnProgressListener;
    private Surface mSurface;
    private boolean mIsPause = false;
    private String mSrcPath;
    private boolean isDelayToPlay = false;
    private Thread mCurThread;

    public FPlayer(SurfaceView surfaceView) {
        surfaceView.getHolder().addCallback(this);
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        mOnProgressListener = onProgressListener;
    }

    public void play(final String srcPath) {
        mSrcPath = srcPath;
        if (mSurface != null) {
            changeToPlay();
        } else {
            isDelayToPlay = true;
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
    }

    public long getDurationTime() {
        return get_current_time();
    }


    private void changeToPlay() {
        try {
            release();
            mCurThread.join();
            startPlay();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void startPlay() {
        mCurThread = new Thread(playRunnable);
        mCurThread.start();
    }

    private Runnable playRunnable = new Runnable() {
        @Override
        public void run() {
            FPlayer.play(mSrcPath, mSurface, new OnProgressListener() {
                @Override
                public void onProgress(long curTime, long totalTime) {
                    if (mOnProgressListener != null) {
                        mOnProgressListener.onProgress(curTime, totalTime);
                    }
                }
            });
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
            startPlay();
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
