package com.dming.testplayer;

import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

public class FPlayer implements SurfaceHolder.Callback {
    public static class PlayStatus {
        public static int IDLE = 0;
        public static int PREPARING = 1;
        public static int PLAY = 2;
        public static int STOPPING = 4;
    }

    private String mUrl;
    private final HandlerThread mPlayThread;
    private final Handler mPlayHandler;
    private final AtomicInteger mControlStatus = new AtomicInteger(PlayStatus.IDLE); // 0 idle 1 prepare 2 playing
    private long mPtr;
    private OnPlayListener mOnPlayListener;
    private long mTime;
    private final AtomicBoolean mFinish = new AtomicBoolean(false);

    public FPlayer(SurfaceView surfaceView) {
        mPtr = newInstance();
        surfaceView.getHolder().addCallback(this);
        mPlayThread = new HandlerThread("FPlayer");
        mPlayThread.start();
        mPlayHandler = new Handler(mPlayThread.getLooper());
    }

    public interface OnPlayListener {
        void onStart();

        void onConnect();

        void onAudioCacheTime(long timeMs, long maxTimeMs);

        void onEnd();

    }

    public void setOnPlayListener(@NonNull OnPlayListener onPlayListener) {
        mOnPlayListener = onPlayListener;
    }

    public void start(final String url) {
        mUrl = url;
        if (mControlStatus.get() == PlayStatus.IDLE) {
            if (mOnPlayListener != null) {
                mOnPlayListener.onStart();
            }
            mControlStatus.set(PlayStatus.PREPARING);
            mPlayHandler.post(mStartRunnable);
        }
    }

    public void onDestroy() {
        if (mFinish.get()) {
            return;
        }
        mFinish.set(true);
        release(mPtr);
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

    public long getAudioMaxCacheTime() {
        return getAudioMaxCacheTime(mPtr);
    }


    public void syncNTP() {
        syncNTP(mPtr);
    }

    public long getNTPDelta() {
        return getNTPDelta(mPtr);
    }

    public boolean hasNTP() {
        return hasNTP(mPtr);
    }

    public int getPlayState() {
        return mControlStatus.get();
    }

    private final Runnable mStartRunnable = new Runnable() {
        @Override
        public void run() {
            if (mFinish.get()) {
                return;
            }
            int ret = FPlayer.open(mPtr, mUrl);
            FLog.i("PlayStatus.PREPARE: " + ret);
            if (ret >= 0) {
                if (mOnPlayListener != null) {
                    mOnPlayListener.onConnect();
                }
                mControlStatus.set(PlayStatus.PLAY);
                while (FPlayer.handle(mPtr) >= 0 && !mFinish.get()) {
                    if (System.currentTimeMillis() - mTime > 95) {
                        if (mOnPlayListener != null) {
                            mOnPlayListener.onAudioCacheTime(getAudioCacheTime(), getAudioMaxCacheTime());
                        }
                        mTime = System.currentTimeMillis();
                    }
                }
            }
            mControlStatus.set(PlayStatus.STOPPING);
            FPlayer.close(mPtr);
            FLog.i("PlayStatus.IDLE");
            mControlStatus.set(PlayStatus.IDLE);
            if (mOnPlayListener != null) {
                mOnPlayListener.onEnd();
            }
            if (!mFinish.get()) {
                if (mOnPlayListener != null) {
                    mOnPlayListener.onStart();
                }
                mControlStatus.set(PlayStatus.PREPARING);
                mPlayHandler.postDelayed(mStartRunnable, 1000);
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

    private static native void release(long ptr);

    private static native long getCurrentTime(long ptr);

    private static native long getAudioCacheTime(long ptr);

    private static native long getAudioMaxCacheTime(long ptr);

    private static native void syncNTP(long ptr);

    private static native long getNTPDelta(long ptr);

    private static native boolean hasNTP(long ptr);

    private static native void surfaceCreated(long ptr, Surface surface);

    private static native void surfaceChanged(long ptr, Surface surface, int width, int height);

    private static native void surfaceDestroyed(long ptr);

    private static native void deleteInstance(long ptr);
}
