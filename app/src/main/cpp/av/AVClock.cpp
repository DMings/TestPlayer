//
// Created by odm on 2023/6/1.
//

#include "AVClock.h"

extern "C" {
#include <libavutil/time.h>
}

AVClock::AVClock() {
    master_clk = {0};
    video_clk = {0};
    audio_clk = {0};
    set_master_clock(0);
}

double AVClock::get_master_clock() {
    double time = av_gettime_relative() / 1000.0;
    if (master_clk.last_updated == 0) {
        master_clk.last_updated = time;
    }
    return time - master_clk.last_updated; // ms
}

void AVClock::set_master_clock(double time) {
    if (time < 0) {
        master_clk.last_updated = 0;
    }
    master_clk.last_updated = time;
}

double AVClock::get_video_pts_clock() {
    return video_clk.pts; // ms
}

void AVClock::set_video_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    video_clk.pts = pts; // ms
    video_clk.last_updated = time;
}

double AVClock::get_video_clock() {
    return video_clk.pts; // ms
}

double AVClock::get_audio_clock() {
    if (audio_clk.pts == 0) {
        return 0;
    }
    double time = av_gettime_relative() / 1000.0;
    if (audio_clk.last_updated == 0) {
        audio_clk.last_updated = time;
    }
    return audio_clk.pts + (time - audio_clk.last_updated); // ms
}

double AVClock::get_audio_pts_clock() {
    return audio_clk.pts; // ms
}

void AVClock::set_audio_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    audio_clk.pts = pts; // ms
    audio_clk.last_updated = time;
}
