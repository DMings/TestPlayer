package com.dming.testplayer;

import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.concurrent.atomic.AtomicInteger;

public class FPlayer implements SurfaceHolder.Callback {
    public static class PlayStatus {
        public static int IDLE = 0;
        public static int PREPARING = 1;
        public static int PLAY = 2;
        public static int PAUSE = 3;
        public static int STOPING = 4;
    }

    private String mUrl;
    private final HandlerThread mPlayThread;
    private final Handler mPlayHandler;
    private final AtomicInteger mControlStatus = new AtomicInteger(PlayStatus.IDLE); // 0 idle 1 prepare 2 playing
    private long mPtr;
    private OnPlayListener mOnPlayListener;
    private long mTime;

    public FPlayer(SurfaceView surfaceView) {
        mPtr = newInstance();
        surfaceView.getHolder().addCallback(this);
        mPlayThread = new HandlerThread("FPlayer");
        mPlayThread.start();
        mPlayHandler = new Handler(mPlayThread.getLooper());
    }

    public interface OnPlayListener {
        void onStart();

        void onAudioCacheTime(long timeMs);

        void onEnd(boolean success);

    }

    public void setOnPlayListener(OnPlayListener onPlayListener) {
        mOnPlayListener = onPlayListener;
    }

    public void start(final String url) {
        mUrl = url;
        if (mControlStatus.get() == PlayStatus.IDLE) {
            mControlStatus.set(PlayStatus.PREPARING);
            mPlayHandler.post(mStartRunnable);
        }
    }

    public void onResume() {
        resume(mPtr);
    }

    public void onPause() {
        pause(mPtr);
    }

    public void onDestroy() {
        stop(mPtr);
        mPlayHandler.post(() -> {
            FLog.e("deleteInstance success");
            if (mPtr != 0) {
                deleteInstance(mPtr);
                mPtr = 0;
            }
        });
        mPlayThread.quitSafely();
    }

    public long getDurationTime() {
        return getCurrentTime(mPtr);
    }

    public long getAudioCacheTime() {
        return getAudioCacheTime(mPtr);
    }

    public int getPlayState() {
        return mControlStatus.get();
    }

    private final Runnable mStartRunnable = new Runnable() {
        @Override
        public void run() {
            int ret = FPlayer.open(mPtr, mUrl);
            if (mOnPlayListener != null) {
                mOnPlayListener.onStart();
            }
            FLog.i("PlayStatus.PREPARE: " + ret);
            if (ret >= 0) {
                mControlStatus.set(PlayStatus.PLAY);
                while (FPlayer.handle(mPtr) >= 0) {
                    if (System.currentTimeMillis() - mTime > 200) {
                        if (mOnPlayListener != null) {
                            mOnPlayListener.onAudioCacheTime(getAudioCacheTime());
                        }
                        mTime = System.currentTimeMillis();
                    }
                }
                FPlayer.close(mPtr);
                FLog.i("PlayStatus.STOP");
            }
            mControlStatus.set(PlayStatus.STOPING);
            FPlayer.stop(mPtr);
            FLog.i("PlayStatus.IDLE");
            mControlStatus.set(PlayStatus.IDLE);
            int finalRet = ret;
            if (mOnPlayListener != null) {
                mOnPlayListener.onEnd(finalRet >= 0);
            }
        }
    };

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceCreated(mPtr, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (width > 0 && height > 0) {
            surfaceChanged(mPtr, holder.getSurface(), width, height);
        } else {
            Log.e("OpenGL", "OpenGL: width: " + width + " height: " + height);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceDestroyed(mPtr);
    }

    @Override
    protected void finalize() throws Throwable {
        if (mPtr != 0) {
            deleteInstance(mPtr);
            mPtr = 0;
        }
        super.finalize();
    }

    static {
        System.loadLibrary("fplayer");
    }

    private static native long newInstance();

    private static native int open(long ptr, String path);

    private static native int handle(long ptr);

    private static native void close(long ptr);

    private static native void pause(long ptr);

    private static native void resume(long ptr);

    private static native void stop(long ptr);

    private static native long getCurrentTime(long ptr);

    private static native long getAudioCacheTime(long ptr);

    private static native void surfaceCreated(long ptr, Surface surface);

    private static native void surfaceChanged(long ptr, Surface surface, int width, int height);

    private static native void surfaceDestroyed(long ptr);

    private static native void deleteInstance(long ptr);
}
