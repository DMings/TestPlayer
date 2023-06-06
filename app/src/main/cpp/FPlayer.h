//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "audio/Audio.h"
#include "video/Video.h"

enum PlayStatus {
    IDLE, PREPARE, PLAYING, STOPPING,
};

class FPlayer {
private:
    AVFormatContext *fmt_ctx = nullptr;
    Video *video = nullptr;
    Audio *audio = nullptr;
    GLThread *glThread;
    AVClock *avClock;
    AVPacket *pkt;
    std::atomic_bool running = false;
    AVIOInterruptCB avIOInterruptCB_;
    std::atomic_int64_t timeoutMS_;
    pthread_mutex_t lock_mutex;
    int64_t cacheTimeMs = 220;

    bool findKeyFrame = false;
    std::list<AVPacket *> pktList;
    bool reachCacheTime = false;
    int64_t vPts = -1;
    int64_t aPts = -1;
public:
    static constexpr int SEND_TIMEOUT_MS = 3000;
    static constexpr int SEND_BUFFER_TIME_MS = 3000;

    static int TimeoutInterruptCb(void *ctx);

public:

    FPlayer();

    GLThread *GetGLThread();

    int Open(const char *url);

    int Handle();

    void Pause();

    void Resume();

    int64_t GetCurTimeMs();

    int GetAudioCacheTimeMs();

    void Stop();

    void Close();

    ~FPlayer();
};

#endif //TESTPLAYER_FFUTILS_H
