package com.dming.testplayer.gl;

public class GLBindRunnable implements Runnable {

    private Runnable mRunnable;

    public GLBindRunnable(Runnable runnable) {
        mRunnable = runnable;
    }

    @Override
    public void run() {
        if (mRunnable != null) {
            mRunnable.run();
        }
    }
}
