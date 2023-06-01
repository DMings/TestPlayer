//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "Audio.h"
#include "Video.h"

enum PlayStatus {
    IDLE, PREPARE, PLAYING, STOPPING,
};

class FPlayer {
public:
    PlayStatus play_status = IDLE;
    pthread_mutex_t play_mutex = PTHREAD_MUTEX_INITIALIZER;

    AVFormatContext *fmt_ctx = NULL;
    Video *video = nullptr;
    Audio *audio = nullptr;
    GLThread glThread;
    AVClock avClock;

public:
    int start_player(const char *src_filename);

    void pause();

    void resume();

    int64_t get_current_time();

    int get_play_state();

    void release();
};

#endif //TESTPLAYER_FFUTILS_H
