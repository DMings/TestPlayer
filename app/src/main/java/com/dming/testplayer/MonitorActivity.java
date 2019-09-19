package com.dming.testplayer;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 客户端
 * Created by DMing at 2017/12/14 0014
 */

public class MonitorActivity extends AppCompatActivity {

    private TextView infoTv;
    double temMaxValue;
    private Thread thread;
    private volatile boolean flag = true;
    private StringBuilder stringBuilder = new StringBuilder();
    private Debug.MemoryInfo memoryInfo = new Debug.MemoryInfo();
    private AtomicBoolean atomicBoolean = new AtomicBoolean(false);

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initMonitorTextViewInfo();
    }

    private void initMonitorTextViewInfo() {
        ViewGroup decorView = (ViewGroup) getWindow().getDecorView();
//        if (decorView.getChildCount() == 0){
//            throw new NullPointerException("Main Layout is Null!");
//        }
        infoTv = new TextView(this);
        int sizeDp = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 130, getResources().getDisplayMetrics());
        infoTv.setLayoutParams(new ViewGroup.LayoutParams(sizeDp, ViewGroup.LayoutParams.WRAP_CONTENT));
//        infoTv.setBackgroundColor(0x55000000);
        int top = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 30, this.getResources().getDisplayMetrics());
        infoTv.setPadding(0, top, 0, 0);
        infoTv.setTextColor(0x99ffffff);
        infoTv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        decorView.addView(infoTv);
        thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (flag) {
                    getMsg();
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            atomicBoolean.set(true);
                            displayBriefMemory();
                            atomicBoolean.set(false);
                        }
                    });
                    if (!atomicBoolean.get()) {
                        try {
                            Thread.sleep(30);
                        } catch (InterruptedException e) {
//                            e.printStackTrace();
                        }
                    }
                }
            }
        });
        thread.start();
    }


    private void displayBriefMemory() {
//        final ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
//        ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
//        activityManager.getMemoryInfo(info);
//        synchronized (MonitorActivity.this) {
        infoTv.setText(stringBuilder);
//        }
    }

    private double twoPointFloat(double a, int b) {
        return (double) (Math.round(a * 100 / b)) / 100;
    }

    private void getMsg() {
//        synchronized (MonitorActivity.this) {
        stringBuilder.delete(0, stringBuilder.length());
//            stringBuilder.append("Native堆中剩余:");
//            stringBuilder.append(twoPointFloat(Debug.getNativeHeapFreeSize() * 1.0f, 1048576));
//            stringBuilder.append("M\n");
//            stringBuilder.append("Native堆中已使用:");
//            stringBuilder.append(twoPointFloat(Debug.getNativeHeapSize() * 1.0f, 1048576));
//            stringBuilder.append("M\n");
//        stringBuilder.append("native分配:");
//        stringBuilder.append(twoPointFloat(Debug.getNativeHeapAllocatedSize() * 1.0f, 1048576));
//        stringBuilder.append("M\n");
        Debug.getMemoryInfo(memoryInfo);
        stringBuilder.append("dalvik分配:");
        stringBuilder.append(twoPointFloat(memoryInfo.dalvikPss * 1.0f, 1024));
        stringBuilder.append("M\n");
        stringBuilder.append("native分配:");
        stringBuilder.append(twoPointFloat(memoryInfo.nativePss * 1.0f, 1024));
        stringBuilder.append("M\n");
        stringBuilder.append("other分配:");
        stringBuilder.append(twoPointFloat((memoryInfo.getTotalPss() - (memoryInfo.dalvikPss + memoryInfo.nativePss)) * 1.0f, 1024));
        stringBuilder.append("M\n");
//            stringBuilder.append("otherPss:");
//            stringBuilder.append(twoPointFloat(memoryInfo.otherPss * 1.0f, 1024));
//            stringBuilder.append("M\n");
        stringBuilder.append("应用总分配:");
        temMaxValue = twoPointFloat(memoryInfo.getTotalPss() * 1.0f, 1024);
        stringBuilder.append(temMaxValue);
        stringBuilder.append("M");
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        thread.interrupt();
        flag = false;
    }

}
