package com.dming.testplayer;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.app.AppCompatActivity;
import android.view.SurfaceView;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;

/**
 * Created by DMing at 2017/12/21 0021
 */
public class MainActivity extends AppCompatActivity {

    private SurfaceView mPlaySv;
    private TextView mTvSrc;
    private ImageView mPlayBtn;
    private TextView mCurTimeTv;
    private TextView mTotalTimeTv;
    private SeekBar mPlaySeekBar;
    private long mTotalTime;
    private long mCurTime;
    private LinearLayout mControlLL;
    private FrameLayout mTitleLayout;
    private Handler mHandler;
    private boolean mIsSearch = false;
    private ListView mDataLv;
    private ArrayList<String> mFileList = new ArrayList<>();
    private String mFilePath;
    private boolean mFileListShow;
    private ProgressBar mSearchPb;
    private FrameLayout mFileLayout;
    private long mSystemTime;
    private FPlayer mFPlayer;
    private boolean mIsSeeking = false;
    private boolean mIsPlaying = false;
    private boolean mIsShowPlayUI = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mDataLv = findViewById(R.id.lv_data);
        mFileLayout = findViewById(R.id.fl_file);
        mSearchPb = findViewById(R.id.pb_search);
        mTvSrc = findViewById(R.id.iiv_src_name);
        mPlaySv = findViewById(R.id.sv_play);
        mTitleLayout = findViewById(R.id.fl_title);
        mPlayBtn = findViewById(R.id.iv_play);
        mCurTimeTv = findViewById(R.id.tv_cur_time);
        mTotalTimeTv = findViewById(R.id.tv_total_time);
        mPlaySeekBar = findViewById(R.id.sb_play);
        mControlLL = findViewById(R.id.ll_control);
        mHandler = new Handler(Looper.getMainLooper());
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.item_file_list, mFileList);
        mDataLv.setAdapter(adapter);
        mDataLv.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (mFilePath != null && mFilePath.equals(mFileList.get(position))) {
                    return;
                }
                mFilePath = mFileList.get(position);
                playOrPause(true);
            }
        });
        mPlayBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mFilePath == null) {
                    Toast.makeText(MainActivity.this, "当前无视频文件可播放", Toast.LENGTH_SHORT).show();
                    return;
                }
                playOrPause(false);
            }
        });
        mFPlayer = new FPlayer(mPlaySv);
        mFPlayer.setOnProgressListener(new OnProgressListener() {
            @Override
            public void onProgress(int curTime, final int totalTime) { // curTime == -1 视频结束
                if (curTime == -1) {
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            mIsPlaying = false;
                            mCurTimeTv.setText("00:00");
                            mPlayBtn.setImageResource(R.drawable.ic_button_play);
                            mPlayBtn.setVisibility(View.GONE);
                            mControlLL.setVisibility(View.VISIBLE);
                            mTvSrc.setVisibility(View.VISIBLE);
                            mTitleLayout.setVisibility(View.VISIBLE);
                            mFileLayout.setVisibility(View.VISIBLE);
                        }
                    });
                } else {
                    if (!mIsSeeking) {
                        mCurTime = curTime;
                        mTotalTime = totalTime;
                        mHandler.removeCallbacks(textRunnable);
                        mHandler.post(textRunnable);
                    }
                }
            }
        });
        mPlaySeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mCurTimeTv.setText(DUtils.secToTime((long) (1.0f * seekBar.getProgress() / seekBar.getMax() * mFPlayer.getDurationTime())));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                mIsSeeking = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                mFPlayer.seekTime(1.0f * seekBar.getProgress() / seekBar.getMax());
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mIsSeeking = false;
                    }
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

        findViewById(R.id.iv_file_list).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mFileLayout.setVisibility(View.VISIBLE);
                mPlayBtn.setVisibility(View.GONE);
                mFileListShow = true;
                if (mFileList.size() > 0) {
                    return;
                }
                mSearchPb.setVisibility(View.VISIBLE);
                PermissionFragment.checkPermission(MainActivity.this, new Runnable() {
                    @Override
                    public void run() {
                        if (!mIsSearch) {
                            mIsSearch = true;
                            new Thread(new Runnable() {
                                @Override
                                public void run() {
                                    mFPlayer.searchFile(Environment.getExternalStorageDirectory().toString(), new FileAction() {
                                        @Override
                                        public void update(final String fileName) {
                                            DLog.i("update fileName: " + fileName);
                                            mHandler.post(new Runnable() {
                                                @Override
                                                public void run() {
                                                    mFileList.add(fileName);
                                                    BaseAdapter baseAdapter = (BaseAdapter) mDataLv.getAdapter();
                                                    baseAdapter.notifyDataSetChanged();
                                                }
                                            });
                                        }
                                    });
                                    mHandler.post(new Runnable() {
                                        @Override
                                        public void run() {
                                            mSearchPb.setVisibility(View.GONE);
                                        }
                                    });
                                }
                            }).start();
                        }
                    }
                });
            }
        });
    }

    private void setVisible() {
        mFileLayout.setVisibility(mFileListShow ? View.VISIBLE : View.GONE);
        if (!mFileListShow) {
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
        } else {
            mFileListShow = false;
            mFileLayout.setVisibility(View.GONE);
            mPlayBtn.setVisibility(View.VISIBLE);
//                    removeAndPost();
        }
    }

    private void playOrPause(final boolean isChangeFile) {
        PermissionFragment.checkPermission(this, new Runnable() {
            @Override
            public void run() {
                if (mFPlayer.getPlayState() == FPlayer.PlayStatus.PLAYING && !isChangeFile) {
                    if (!mIsPlaying) {
                        mIsPlaying = true;
                        mFPlayer.onResume();
                        mPlayBtn.setImageResource(R.drawable.ic_button_pause);
                    } else {
                        mIsPlaying = false;
                        mFPlayer.onPause();
                        mPlayBtn.setImageResource(R.drawable.ic_button_play);
                    }
                } else {
                    final File srcFile = new File(mFilePath);
                    mPlaySv.setVisibility(View.VISIBLE);
                    int ret = mFPlayer.play(srcFile.toString(), new Runnable() {
                        @Override
                        public void run() {
                            mIsShowPlayUI = false;
                            setVisible();
                        }
                    }, new Runnable() {
                        @Override
                        public void run() {
                            mIsPlaying = true;
                            mTvSrc.setText(srcFile.getName());
                            mPlayBtn.setImageResource(R.drawable.ic_button_pause);
                        }
                    }, new Runnable() {
                        @Override
                        public void run() {
                            Toast.makeText(MainActivity.this, "视频播放失败", Toast.LENGTH_SHORT).show();
                        }
                    });
                    DLog.i("ret=" + ret);
                    if (ret != FPlayer.PlayStatus.IDLE) {
//                        if (ret == FPlayer.PlayStatus.PLAYING) {
//                            Toast.makeText(MainActivity.this, "视频切换中", Toast.LENGTH_SHORT).show();
//                        } else if (ret == FPlayer.PlayStatus.PREPARE) {
//                            Toast.makeText(MainActivity.this, "视频准备中", Toast.LENGTH_SHORT).show();
//                        }
                    }
                }
            }
        });
    }

    private Runnable textRunnable = new Runnable() {
        @Override
        public void run() {
            if (mTotalTime > 0 && mCurTime == mTotalTime) {
                mPlayBtn.setImageResource(R.drawable.ic_button_play);
            }
            mPlaySeekBar.setVisibility(View.VISIBLE);
            mCurTimeTv.setText(DUtils.secToTime(mCurTime));
            mTotalTimeTv.setText(DUtils.secToTime(mTotalTime));
            int p = (int) ((mCurTime * 1.0f / mTotalTime) * 100);
            mPlaySeekBar.setProgress(p);
        }
    };

    @Override
    protected void onPause() {
        mFPlayer.onPause();
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
        if (System.currentTimeMillis() - mSystemTime > 2000) {
            Toast.makeText(this, "再按一次退出", Toast.LENGTH_SHORT).show();
            mSystemTime = System.currentTimeMillis();
        } else {
            finish();
        }
    }

}
