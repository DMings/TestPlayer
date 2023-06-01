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
    private String mSrcPath;
    private HandlerThread mPlayThread;
    private Handler mPlayHandler;
    //    private Lock mLock = new ReentrantLock();
    private Runnable mPrepareRunnable;
    private Runnable mErrorRunnable;
    private Runnable mEndRunnable;
    private AtomicInteger mControlStatus = new AtomicInteger(PlayStatus.IDLE); // 0 idle 1 prepare 2 playing
    private Handler mMainHandle = new Handler(Looper.getMainLooper());
    private int mSurfaceWidth;
    private int mSurfaceHeight;
    private long mPtr;

    public FPlayer(SurfaceView surfaceView) {
        mPtr = newInstance();
        surfaceView.getHolder().addCallback(this);
//        mEglHelper = new EglHelper();
        mPlayThread = new HandlerThread("FPlayer");
        mPlayThread.start();
        mPlayHandler = new Handler(mPlayThread.getLooper());
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        mOnProgressListener = onProgressListener;
    }

    public int play(final String srcPath, Runnable prepareRunnable, Runnable endRunnable, Runnable errorRunnable) {
        mSrcPath = srcPath;
        mPrepareRunnable = prepareRunnable;
        mEndRunnable = endRunnable;
        mErrorRunnable = errorRunnable;
        int ret = mControlStatus.get();
        if (ret == PlayStatus.IDLE || ret == PlayStatus.PLAYING) {
            changeToPlay();
            return mControlStatus.get();
        } else { // 处于正在准备
            return mControlStatus.get();
        }
    }

    public void onResume() {
        resume(mPtr);
    }

    public void onPause() {
        pause(mPtr);
    }

    public void onDestroy() {
        release(mPtr);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mPlayThread.quitSafely();
        } else {
            mPlayThread.quit();
        }
        try {
            mPlayThread.join();
        } catch (InterruptedException e) {
            FLog.e("Join encountered an error!");
        }
        deleteInstance(mPtr);
    }

    public long getDurationTime() {
        return get_current_time(mPtr);
    }

    public int getPlayState() {
        int s = get_play_state(mPtr);
        Log.i("DMFF", "getPlayState:     " + s);
        return s;
    }

    private void changeToPlay() {
        release(mPtr);
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
            int ret = FPlayer.play(mPtr, mSrcPath);
            FLog.i("PlayStatus.IDLE");
            mControlStatus.set(PlayStatus.IDLE);
            if (ret < 0) {
                if (mErrorRunnable != null) {
                    mMainHandle.post(mErrorRunnable);
                }
            } else {
                if (mEndRunnable != null) {
                    mMainHandle.post(mEndRunnable);
                }
            }
        }
    };

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surface_created(mPtr, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        Log.i("OpenGL", "OpenGL: width: " + width + " height: " + height);
        surface_changed(mPtr, holder.getSurface(), width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surface_destroyed(mPtr);
    }

    static {
        System.loadLibrary("fplayer");
    }

    private static native long newInstance();

    private static native int play(long ptr, String path);

    private static native long get_current_time(long ptr);

    private static native void pause(long ptr);

    private static native void resume(long ptr);

    private static native int get_play_state(long ptr);

    private static native void surface_created(long ptr, Surface surface);

    private static native void surface_changed(long ptr, Surface surface, int width, int height);

    private static native void surface_destroyed(long ptr);

    private static native void release(long ptr);

    private static native void deleteInstance(long ptr);
}
