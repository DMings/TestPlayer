package com.dming.testplayer;

import android.content.pm.ActivityInfo;
import android.os.*;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Created by DMing at 2017/12/21 0021
 */
public class MainActivity extends MonitorActivity {

    SurfaceView surfaceView;
    TextView tvSrc;
    ImageView playBtn;
    TextView curTimeTv;
    TextView totalTimeTv;
    SeekBar playSeekBar;
    private long totalTime;
    private long curTime;
    LinearLayout controlLL;
    FrameLayout titleLayout;
    private StaticHandler handler;
    private AtomicBoolean search = new AtomicBoolean();
    private ListView dataLv;
    private ArrayList<String> fileList = new ArrayList<>();
    private String pathName;
    private boolean fileListShow;
    private final static int updateUI = 1;
    private ProgressBar searchPb;
    private FrameLayout fileLayout;
    private FrameLayout baseLayout;
    private long system_time;

    private FPlayer mFPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS); //透明状态栏
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION); //透明导航栏
        }
        dataLv = findViewById(R.id.dataLv);
        fileLayout = findViewById(R.id.fileLayout);
        searchPb = findViewById(R.id.searchPb);
        tvSrc = findViewById(R.id.tvSrc);
        surfaceView = findViewById(R.id.surface_view);
        titleLayout = findViewById(R.id.titleLayout);
        playBtn = findViewById(R.id.playBtn);
        curTimeTv = findViewById(R.id.curTimeTv);
        totalTimeTv = findViewById(R.id.totalTimeTv);
        playSeekBar = findViewById(R.id.playSeekBar);
        controlLL = findViewById(R.id.controlLL);
        handler = new StaticHandler(this);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.item_file_list, fileList);
        dataLv.setAdapter(adapter);
        dataLv.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                pathName = fileList.get(position);
                playOrPause(true);
            }
        });
        playBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playOrPause(false);
            }
        });
        mFPlayer = new FPlayer(this, surfaceView);
        mFPlayer.setOnProgressListener(new OnProgressListener() {
            @Override
            public void onProgress(long curTime, long totalTime) {
                MainActivity.this.totalTime = totalTime;
                MainActivity.this.curTime = curTime;
                handler.removeCallbacks(textRunnable);
                handler.post(textRunnable);
            }
        });
        playSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                curTimeTv.setText(DUtils.secToTime((long) (1.0f * seekBar.getProgress() / seekBar.getMax() * mFPlayer.getDurationTime())));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                int progress = seekBar.getProgress();
                mFPlayer.seekTime(progress);
            }
        });
        final ImageView fullBtn = findViewById(R.id.fullBtn);
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
                removeAndPost();
            }
        });
        baseLayout = findViewById(R.id.baseLayout);
        baseLayout.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playBtn.setVisibility(View.VISIBLE);
                controlLL.setVisibility(View.VISIBLE);
                tvSrc.setVisibility(View.VISIBLE);
                titleLayout.setVisibility(View.VISIBLE);
                fileLayout.setVisibility(fileListShow ? View.VISIBLE : View.GONE);
                if (!fileListShow) {
                    if (handler.hasMessages(updateUI)) {
                        handler.removeMessages(updateUI);
                        handler.sendEmptyMessage(updateUI);
                    } else {
                        removeAndPost();
                    }
                } else {
                    fileListShow = false;
                    fileLayout.setVisibility(View.GONE);
                    playBtn.setVisibility(View.VISIBLE);
                    removeAndPost();
                }
            }
        });

        findViewById(R.id.fileListIv).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                handler.removeMessages(updateUI);
                fileLayout.setVisibility(View.VISIBLE);
                playBtn.setVisibility(View.GONE);
                fileListShow = true;
                if (fileList.size() > 0) {
                    return;
                }
                searchPb.setVisibility(View.VISIBLE);
                requiresSDPermission(new Runnable() {
                    @Override
                    public void run() {
                        if (!search.get()) {
                            search.set(true);
                            new Thread(new Runnable() {
                                @Override
                                public void run() {
                                    mFPlayer.searchFile(Environment.getExternalStorageDirectory().toString(), new FileAction() {
                                        @Override
                                        public void update(final String fileName) {
                                            handler.post(new Runnable() {
                                                @Override
                                                public void run() {
                                                    fileList.add(fileName);
                                                    BaseAdapter baseAdapter = (BaseAdapter) dataLv.getAdapter();
                                                    baseAdapter.notifyDataSetChanged();
                                                }
                                            });
                                        }
                                    });
                                    handler.post(new Runnable() {
                                        @Override
                                        public void run() {
                                            searchPb.setVisibility(View.GONE);
                                        }
                                    });
                                    search.set(false);
                                }
                            }).start();
                        }
                    }
                });
            }
        });
        removeAndPost();
    }


    private void playOrPause(final boolean changeFile) {
        requiresSDPermission(new Runnable() {
            @Override
            public void run() {
                if (pathName == null) {
                    Toast.makeText(MainActivity.this, "无视频文件播放", Toast.LENGTH_SHORT).show();
                    return;
                }
                File srcFile = new File(pathName);
                tvSrc.setText(srcFile.getName());
                if (changeFile) {
                    mFPlayer.play(srcFile.toString());
                    playBtn.setImageResource(R.drawable.ic_button_pause);
                } else {
                    if (mFPlayer.pauseOrResume()) {
                        playBtn.setImageResource(R.drawable.ic_button_pause);
                    } else {
                        playBtn.setImageResource(R.drawable.ic_button_play);
                    }
                }
                removeAndPost();
            }
        });
    }

    private void removeAndPost() {
        handler.removeMessages(updateUI);
        handler.sendEmptyMessageDelayed(updateUI, 3000);
    }


    private Runnable textRunnable = new Runnable() {
        @Override
        public void run() {
            if (totalTime > 0 && curTime == totalTime) {
                playBtn.setImageResource(R.drawable.ic_button_play);
            }
            playSeekBar.setVisibility(View.VISIBLE);
            curTimeTv.setText(DUtils.secToTime(curTime));
            totalTimeTv.setText(DUtils.secToTime(totalTime));
            int p = (int) ((curTime * 1.0f / totalTime) * 100);
            playSeekBar.setProgress(p);
        }
    };

    private static class StaticHandler extends Handler {

        private WeakReference<MainActivity> activityWeak;

        StaticHandler(MainActivity activity) {
            this.activityWeak = new WeakReference<>(activity);
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (msg.what == updateUI) {
                if (activityWeak != null) {
                    activityWeak.get().playBtn.setVisibility(View.GONE);
                    activityWeak.get().controlLL.setVisibility(View.GONE);
                    activityWeak.get().tvSrc.setVisibility(View.GONE);
                    activityWeak.get().titleLayout.setVisibility(View.GONE);
                    activityWeak.get().fileLayout.setVisibility(View.GONE);
                }
            }
        }
    }

    @Override
    protected void onPause() {
        mFPlayer.onPause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        mFPlayer.onResume();
        super.onResume();
        handler.post(new Runnable() {
            @Override
            public void run() {
                surfaceView.setVisibility(View.VISIBLE);
            }
        });
    }


    @Override
    protected void onDestroy() {
        mFPlayer.onDestroy();
        handler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
//        if (System.currentTimeMillis() - system_time > 2000) {
//            Toast.makeText(this, "再按一次退出", Toast.LENGTH_SHORT).show();
//            system_time = System.currentTimeMillis();
//        } else {
            finish();
//        }
    }

}
