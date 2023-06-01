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
private:
    AVFormatContext *fmt_ctx = NULL;
    Video *video = nullptr;
    Audio *audio = nullptr;
    GLThread *glThread;
    AVClock *avClock;
    AVPacket *pkt;

public:

    FPlayer();

    GLThread *GetGLThread();

    int Start(const char *url);

    int Loop();

    void Pause();

    void Resume();

    int64_t GetCurTimeMs();

    void Stop();

    ~FPlayer();
};

#endif //TESTPLAYER_FFUTILS_H
