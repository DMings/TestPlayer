package com.dming.testplayer;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Created by DMing at 2017/12/21 0021
 */
public class MainActivity extends AppCompatActivity {

    private SurfaceView mPlaySv;
    private TextView mTvSrc;
    private ImageView mPlayBtn;
    private TextView mCurTimeTv;
    private long mCurTime;
    private LinearLayout mControlLL;
    private FrameLayout mTitleLayout;
    private Handler mHandler;
    private String mUrlPath;
    private FPlayer mFPlayer;
    private boolean mIsPlaying = false;
    private boolean mIsShowPlayUI = true;
    private TextView mAudioCacheTimeTv;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mTvSrc = findViewById(R.id.iiv_src_name);
        mAudioCacheTimeTv = findViewById(R.id.iiv_msg);
        mPlaySv = findViewById(R.id.sv_play);
        mTitleLayout = findViewById(R.id.fl_title);
        mPlayBtn = findViewById(R.id.iv_play);
        mCurTimeTv = findViewById(R.id.tv_cur_time);
        mControlLL = findViewById(R.id.ll_control);
        mHandler = new Handler(Looper.getMainLooper());
        mUrlPath = "rtmp://43.138.249.153:1935/live/livestream";
        mPlayBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mUrlPath == null) {
                    Toast.makeText(MainActivity.this, "当前无视频可播放", Toast.LENGTH_SHORT).show();
                    return;
                }
                playOrPause();
            }
        });
        mFPlayer = new FPlayer(mPlaySv);
        mFPlayer.setOnPlayListener(new FPlayer.OnPlayListener() {
            @Override
            public void onStart() {
                mHandler.post(() -> {
                    mIsPlaying = true;
                    mIsShowPlayUI = false;
                    mTvSrc.setText(mUrlPath);
                    mCurTimeTv.setText(DUtils.secToTime(mCurTime));
                    mPlayBtn.setImageResource(R.drawable.ic_button_pause);
                });
            }

            @Override
            public void onAudioCacheTime(long timeMs) {
                mHandler.post(() -> {
                    mAudioCacheTimeTv.setText("AC: " + timeMs + "Ms");
                });
            }

            @Override
            public void onEnd(boolean success) {
                mHandler.post(() -> {
                    mIsPlaying = false;
                    mCurTimeTv.setText("--:--");
                    mTvSrc.setText("无播放");
                    mPlayBtn.setImageResource(R.drawable.ic_button_play);
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
        playOrPause();
    }

    private void setVisible() {
        if (mIsShowPlayUI) {
            mIsShowPlayUI = false;
            mPlayBtn.setVisibility(View.GONE);
            mControlLL.setVisibility(View.GONE);
            mTvSrc.setVisibility(View.GONE);
            mTitleLayout.setVisibility(View.GONE);
        } else {
            mIsShowPlayUI = true;
            mPlayBtn.setVisibility(View.VISIBLE);
            mControlLL.setVisibility(View.VISIBLE);
            mTvSrc.setVisibility(View.VISIBLE);
            mTitleLayout.setVisibility(View.VISIBLE);
        }
    }

    private void playOrPause() {
        PermissionFragment.checkPermission(this, new Runnable() {
            @Override
            public void run() {
                if (mFPlayer.getPlayState() == FPlayer.PlayStatus.PLAY) {
                    if (!mIsPlaying) {
                        mIsPlaying = true;
                        mFPlayer.onResume();
                        mPlayBtn.setImageResource(R.drawable.ic_button_pause);
                    } else {
                        mIsPlaying = false;
                        mFPlayer.onPause();
                        mPlayBtn.setImageResource(R.drawable.ic_button_play);
                    }
                } else if (mFPlayer.getPlayState() == FPlayer.PlayStatus.IDLE) {
                    mFPlayer.start(mUrlPath);
                }
            }
        });
    }

    @Override
    protected void onPause() {
        if (mIsPlaying) {
            mFPlayer.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mIsPlaying) {
            mFPlayer.onResume();
        }
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
