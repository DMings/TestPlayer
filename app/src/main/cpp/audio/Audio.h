//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_AUDIO_H
#define TESTPLAYER_AUDIO_H

#include "render/AudioPlay.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Audio : BaseAV {
public:
    Audio(AVClock *avClock, uint cacheTime);

    void putAvPacket(FPacket *pkt);

    uint64_t getAvPacketSize();

    float getPktListTime();

    float getSpeed(float listTime);

    int open_stream(AVFormatContext *fmt_ctx);

    void pause();

    void resume();

    void release();

    uint getCacheTime();

    AVStream *av_stream = nullptr;
    AVCodecContext *av_dec_ctx = nullptr;
    int stream_id = -1;
private:
    bool isSampleRateValid(int sampleRate);

    static void *audioProcess(void *arg);

    AVClock *avClock = nullptr;
    AudioPlay *playAudio;
    pthread_t p_audio_tid = 0;
    std::atomic_bool thread_finish = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;
    std::atomic_bool is_pause = false;
    std::list<float> speedList;
    float pktDuration = 1024000.f / 48000.0f;
    float cacheSetTime = 200;
    float cacheCurTime = 200;
    uint reachMinTimeCount = 0;
    uint reachNormalTimeCount = 0;
};


#endif //TESTPLAYER_AUDIO_H
