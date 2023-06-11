package com.dming.testplayer;

import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.text.SimpleDateFormat;
import java.util.Locale;

/**
 * Created by DMing at 2017/12/21 0021
 */
public class MainActivity extends AppCompatActivity {

    private SurfaceView mPlaySv;
    private TextView mTvSrc;
    private ProgressBar mWaitPb;
    private TextView mCurTimeTv;
    private LinearLayout mControlLL;
    private FrameLayout mTitleLayout;
    private Handler mHandler;
    private String mUrlPath;
    private FPlayer mFPlayer;
    private boolean mIsShowPlayUI = true;
    private TextView mMsgInfoTv;
    private final SimpleDateFormat mSdf = new SimpleDateFormat("HH:mm:ss.SSS", Locale.CHINA);
    private final SimpleDateFormat mTimeSdf = new SimpleDateFormat("HH:mm:ss", Locale.CHINA);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mTvSrc = findViewById(R.id.iiv_src_name);
        mMsgInfoTv = findViewById(R.id.iiv_msg);
        mPlaySv = findViewById(R.id.sv_play);
        mTitleLayout = findViewById(R.id.fl_title);
        mWaitPb = findViewById(R.id.pb_wait);
        mCurTimeTv = findViewById(R.id.tv_cur_time);
        mControlLL = findViewById(R.id.ll_control);
        mHandler = new Handler(Looper.getMainLooper());
        mUrlPath = "rtmp://43.138.249.153:1935/live/livestream";
        mFPlayer = new FPlayer(mPlaySv);
        mFPlayer.setOnPlayListener(new FPlayer.OnPlayListener() {
            @Override
            public void onStart() {
                mHandler.post(() -> {
                    mIsShowPlayUI = false;
                    mTvSrc.setText("正在连接...");
                    mWaitPb.setVisibility(View.VISIBLE);
                });
            }

            @Override
            public void onConnect() {
                mHandler.post(() -> {
                    mIsShowPlayUI = false;
                    mTvSrc.setText(mUrlPath);
                    mCurTimeTv.setText("--:--");
                    mWaitPb.setVisibility(View.GONE);
                });
            }

            @SuppressLint("SetTextI18n")
            @Override
            public void onMsgInfoRequest() {
                mHandler.post(() -> {
                    String msg = "Audio Cache: " + mFPlayer.getAudioCacheTime() + "Ms Max:" + mFPlayer.getAudioMaxCacheTime() + "Ms" +
                            "\nVideo Cache: " + mFPlayer.getVideoCacheTime() + " Ms";
                    if (mFPlayer.hasNTP()) {
                        long t = (System.currentTimeMillis() + mFPlayer.getNTPDelta());
                        msg += "\nNTP: " + mSdf.format(t);
                        long videoNTPDelta = mFPlayer.getVideoNTPDelta();
                        if (videoNTPDelta != -999999) {
                            msg += "\nNTP SendToRec Delta: " + videoNTPDelta;
                        }
                    }
                    mMsgInfoTv.setText(msg);
                    mCurTimeTv.setText(mTimeSdf.format(1685548800_000L + mFPlayer.getCurrentTime()));
                });
            }

            @Override
            public void onEnd() {
                mHandler.post(() -> {
                    mCurTimeTv.setText("--:--");
                    mMsgInfoTv.setText("");
                    mTvSrc.setText("已断开");
                    mWaitPb.setVisibility(View.GONE);
                });
            }
        });
        final ImageView fullBtn = findViewById(R.id.iv_full);
        fullBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (getRequestedOrientation() == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
                    fullBtn.setImageResource(R.drawable.ic_button_zoom);
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                } else {
                    fullBtn.setImageResource(R.drawable.ic_button_full);
                    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
                }
            }
        });
        FrameLayout baseLayout = findViewById(R.id.baseLayout);
        baseLayout.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                setVisible();
            }
        });
        if (mUrlPath == null) {
            Toast.makeText(MainActivity.this, "当前无视频可播放", Toast.LENGTH_SHORT).show();
            return;
        }
        mFPlayer.start(mUrlPath);
    }

    private void setVisible() {
        if (mIsShowPlayUI) {
            mIsShowPlayUI = false;
            mControlLL.setVisibility(View.GONE);
            mTvSrc.setVisibility(View.GONE);
            mTitleLayout.setVisibility(View.GONE);
        } else {
            mIsShowPlayUI = true;
            mControlLL.setVisibility(View.VISIBLE);
            mTvSrc.setVisibility(View.VISIBLE);
            mTitleLayout.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mFPlayer.syncNTP();
//        FLog.i("SyncNTP2 MS: " + System.currentTimeMillis());
    }

    @Override
    protected void onDestroy() {
        mFPlayer.onDestroy();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        finish();
    }

}
