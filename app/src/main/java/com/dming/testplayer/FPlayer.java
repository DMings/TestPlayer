package com.dming.testplayer;

import android.view.Surface;

public class FPlayer {

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

}
