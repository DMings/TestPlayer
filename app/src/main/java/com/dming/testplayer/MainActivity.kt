package com.dming.testplayer

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.support.v7.app.AppCompatActivity
import android.util.Log
import kotlinx.android.synthetic.main.activity_main.*
import java.io.File

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
//        val srcPath = File(Environment.getExternalStorageDirectory(), "1/video.mp4").path
        val srcPath = File(Environment.getExternalStorageDirectory(), "1/animation.mp4").path
        Log.i("DMFF", "srcPath: $srcPath")
        btn_test.setOnClickListener {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) ==
                    PackageManager.PERMISSION_GRANTED
                ) {
                    if (File(srcPath).exists()) {
                        testPlay(srcPath)
                    }
                } else {
                    requestPermissions(arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE), 666)
                }
            } else {
                if (File(srcPath).exists()) {
                    testPlay(srcPath)
                }
            }
        }
    }

    private fun testPlay(srcPath: String){
        Thread(Runnable {
            testFF(srcPath)
        }).start()
    }

    private external fun testFF(path: String)

    companion object {

        init {
            System.loadLibrary("native-lib")
        }
    }
}
