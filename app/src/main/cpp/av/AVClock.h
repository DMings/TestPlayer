//
// Created by odm on 2023/6/1.
//

#ifndef TESTPLAYER_AVCLOCK_H
#define TESTPLAYER_AVCLOCK_H

#include <cstdint>

struct Clock {
    double pts;           /* clock base */
    double last_updated;
};

class AVClock {
public:
    Clock master_clk = {0};
    Clock video_clk = {0};
    Clock audio_clk = {0};

public:
    int32_t ff_sec_time = 0; // 当前时间

    AVClock();

    double get_master_clock();

    double get_video_pts_clock();

    void set_video_clock(double pts);

    double get_video_clock();

    double get_audio_clock();

    double get_audio_pts_clock();

    void set_audio_clock(double pts);

    void set_master_clock(double time);
};

#endif //TESTPLAYER_AVCLOCK_H
