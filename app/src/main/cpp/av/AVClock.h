//
// Created by odm on 2023/6/1.
//

#ifndef TESTPLAYER_AVCLOCK_H
#define TESTPLAYER_AVCLOCK_H

#include <cstdint>

struct Clock {
    int64_t pts;           /* clock base */
    int64_t lastUpdated;
};

class AVClock {
public:
    Clock videoClk = {0};
    Clock audioClk = {0};

public:

    AVClock();

    int64_t GetVideoPtsClock();

    void SetVideoClock(int64_t pts);

    int64_t GetAudioPtsClock();

    void SetAudioClock(int64_t pts);

    void Reset();

};

#endif //TESTPLAYER_AVCLOCK_H
