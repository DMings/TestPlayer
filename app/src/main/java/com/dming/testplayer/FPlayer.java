package com.dming.testplayer;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

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
    private HandlerThread mControlThread;
    private Handler mControlHandler;
    private Lock mLock = new ReentrantLock();
    private Runnable mPrepareRunnable;
    private AtomicBoolean mPlayFinish = new AtomicBoolean(true);
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
        mControlThread = new HandlerThread("FControl");
        mControlThread.start();
        mControlHandler = new Handler(mControlThread.getLooper());
    }

    public void setOnProgressListener(OnProgressListener onProgressListener, OnSurfaceChange onSurfaceChange) {
        mOnProgressListener = onProgressListener;
        mOnSurfaceChange = onSurfaceChange;
    }

    public boolean play(final String srcPath, Runnable prepareRunnable) {
        mSrcPath = srcPath;
        mPrepareRunnable = prepareRunnable;
        if (mPlayFinish.get()) {
            mPlayFinish.set(false);
            if (mSurface != null) {
                changeToPlay();
            } else {
                isDelayToPlay = true;
            }
            return true;
        } else {
            return false;
        }
    }

    public void searchFile(String path, FileAction fileAction) {
        scan_file(path, fileAction);
    }

    public void seekTime(final float percent) {
        mControlHandler.post(new Runnable() {
            @Override
            public void run() {
//                resume();
                seek(percent);
            }
        });
    }

    public void onResume() {
        mControlHandler.post(new Runnable() {
            @Override
            public void run() {
                resume();
            }
        });
    }

    public void onPause() {
        mControlHandler.post(new Runnable() {
            @Override
            public void run() {
                pause();
            }
        });
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
            DLog.e("mPlayThread Join encountered an error!");
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mControlThread.quitSafely();
        } else {
            mControlThread.quit();
        }
        try {
            mControlThread.join();
        } catch (InterruptedException e) {
            DLog.e("mControlThread Join encountered an error!");
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
        mControlHandler.post(new Runnable() {
            @Override
            public void run() {
                release();
                mLock.lock();
                mLock.unlock();
                startPlay();
            }
        });
    }

    private void startPlay() {
        mPlayHandler.post(new Runnable() {
            @Override
            public void run() {
                mPlayFinish.set(true);
                mMainHandle.post(mPrepareRunnable);
            }
        });
        mPlayHandler.post(playRunnable);
    }


    private Runnable playRunnable = new Runnable() {
        @Override
        public void run() {
            mLock.lock();
            FPlayer.play(mSrcPath, mSurface, mSurfaceWidth, mSurfaceHeight, new OnProgressListener() {
                @Override
                public void onProgress(int curTime, int totalTime) {
                    if (mOnProgressListener != null) {
                        mOnProgressListener.onProgress(curTime, totalTime);
                    }
                }
            });
            mLock.unlock();
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
        mControlHandler.post(new Runnable() {
            @Override
            public void run() {
                Log.i("OpenGL", "OpenGL: width: " + mSurfaceWidth + " height: " + mSurfaceHeight);
                if (isDelayToPlay) {
                    isDelayToPlay = false;
                    startPlay();
                } else {
                    FPlayer.update_surface(mSurface, mSurfaceWidth, mSurfaceHeight);
                    mOnSurfaceChange.change();
                }
            }
        });
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

    private static native void play(String path, Surface surface, int width, int height, OnProgressListener onProgressListener);

    private static native void seek(float percent);

    private static native long get_current_time();

    private static native long get_duration_time();

    private static native void pause();

    private static native void resume();

    private static native int get_play_state();

    private static native void update_surface(Surface surface, int width, int height);

    private static native void release();

    private static native void scan_file(String path, FileAction fileAction);

}
