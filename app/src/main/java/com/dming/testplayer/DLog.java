package com.dming.testplayer;

import android.util.Log;

/**
 *
 * Created by DMing on 2017/9/19.
 */

public class DLog {

    private static String TAG = "DMUI";

    public static void i(String msg){
        if (BuildConfig.DEBUG) Log.i(TAG,msg);
    }

    public static void e(String msg){
        if (BuildConfig.DEBUG) Log.e(TAG,msg);
    }
}
