//
// Created by odm on 2023/6/1.
//

#include "AVClock.h"

extern "C" {
#include <libavutil/time.h>
}

AVClock::AVClock() {
    videoClk = {0};
    audioClk = {0};
}

int64_t AVClock::GetVideoPtsClock() {
    return videoClk.pts;
}

void AVClock::SetVideoClock(int64_t pts) {
    int64_t time = av_gettime_relative();
    videoClk.pts = pts; // ms
    videoClk.lastUpdated = time;
}

int64_t AVClock::GetAudioPtsClock() {
    if (audioClk.lastUpdated == 0) {
        return INT64_MIN;
    }
    int64_t time = av_gettime_relative();
    if (audioClk.lastUpdated == 0) {
        audioClk.lastUpdated = time;
    }
    return audioClk.pts + (time - audioClk.lastUpdated); // ms
}

void AVClock::SetAudioClock(int64_t pts) {
    int64_t time = av_gettime_relative();
    audioClk.pts = pts; // ms
    audioClk.lastUpdated = time;
}

void AVClock::Reset() {
    videoClk = {0};
    audioClk = {0};
}
