//
// Created by Administrator on 2019/9/12.
//
//类的静态成员变量需要在类外分配内存空间

#ifndef TESTPLAYER_AUDIO_H
#define TESTPLAYER_AUDIO_H

#include "render/AudioPlay.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Audio : BaseAV {
public:
    Audio(AVClock* avClock);

    void putAvPacket(FPacket* pkt);

    uint64_t getAvPacketSize();

    int synchronize_audio(int nb_samples);

    float getPktListTime();

    float getSpeed(float listTime);

    int open_stream(AVFormatContext *fmt_ctx);

    void pause();

    void resume();

    void release();

    AVClock* avClock;

    int stream_id = -1;
    AVStream *av_stream = NULL;
    AVCodecContext *av_dec_ctx = NULL;
private:
    bool isSampleRateValid(int sampleRate);

    static void *audioProcess(void *arg);

    AudioPlay* playAudio;
    pthread_t p_audio_tid = 0;
    std::atomic_bool thread_finish = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;
    std::atomic_bool is_pause = false;

    float pktDuration = 48000.0f;
};


#endif //TESTPLAYER_AUDIO_H
