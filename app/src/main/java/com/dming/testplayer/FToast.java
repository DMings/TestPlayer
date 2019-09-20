package com.dming.testplayer;

import android.annotation.SuppressLint;
import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.support.annotation.UiThread;
import android.text.TextUtils;
import android.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Toast工具类
 */
public class FToast {
    /**
     *
     */
    @IntDef({LENGTH_SHORT, LENGTH_LONG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Duration {
    }

    /**
     * Show the view or text notification for a short period of time.  This time
     * could be user-definable.  This is the default.
     */
    public static final int LENGTH_SHORT = 0;

    /**
     * Show the view or text notification for a long period of time.  This time
     * could be user-definable.
     */
    public static final int LENGTH_LONG = 1;
    private static Toast toast;

    /**
     * 显示Toast
     *
     * @param message 字符串
     */
    public static void show(Context context, String message) {
        show(context, message, LENGTH_SHORT);
    }

    /**
     * 显示Toast
     *
     * @param messageRes 资源
     */
    public static void show(Context context, @StringRes int messageRes) {
        show(context, context.getString(messageRes), LENGTH_SHORT);
    }

    /**
     * 显示Toast
     *
     * @param message
     */
    @SuppressLint("ShowToast")
    @UiThread
    public static void show(Context context, String message, @Duration int duration) {
        if (TextUtils.isEmpty(message)) {
            return;
        }
        if (toast == null) {
            toast = Toast.makeText(context, message, duration);
        } else {
            toast.setText(message);
        }
        toast.show();
    }
}
