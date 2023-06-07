//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "audio/Audio.h"
#include "video/Video.h"

class FPlayer {

private:
    AVFormatContext *fmtCtx_ = nullptr;
    Video *video_ = nullptr;
    Audio *audio_ = nullptr;
    GLThread *glThread_ = nullptr;
    AVClock *avClock_ = nullptr;
    AVPacket *pkt_ = nullptr;
    std::atomic_bool running_ = false;
    AVIOInterruptCB avIOInterruptCB_;
    std::atomic_int64_t timeoutMS_;
    pthread_mutex_t lockMutex_;
    int64_t cacheTimeMs_ = 220;
    bool findKeyFrame_ = false;
    std::list<AVPacket *> pktList_;
    bool reachCacheTime_ = false;
    int64_t vPts_ = -1;
    int64_t aPts_ = -1;
public:
    static constexpr int READ_TIMEOUT_MS = 3000;

    static int TimeoutInterruptCb(void *ctx);

public:

    FPlayer();

    GLThread *GetGLThread();

    int Open(const char *url);

    int Handle();

    int64_t GetCurTimeMs();

    int GetAudioMaxCacheTimeMs();

    int GetAudioCacheTimeMs();

    void Release();

    void Close();

    ~FPlayer();
};

#endif //TESTPLAYER_FFUTILS_H
