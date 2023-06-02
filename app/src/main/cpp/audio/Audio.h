//
// Created by Administrator on 2019/9/12.
//
//类的静态成员变量需要在类外分配内存空间

#ifndef TESTPLAYER_AUDIO_H
#define TESTPLAYER_AUDIO_H

#include "render/OpenSL.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Audio : BaseAV {
public:
    Audio(AVClock* avClock);

    void putAvPacket(FPacket* pkt);

    int synchronize_audio(int nb_samples);

    int open_stream(AVFormatContext *fmt_ctx);

    void pause();

    void resume();

    void release();

    AVClock* avClock;

    int stream_id = -1;
    AVStream *av_stream = NULL;
    AVCodecContext *av_dec_ctx = NULL;
private:
    static void *audioProcess(void *arg);

    uint8_t **getDstData();

    bool chooseDstData = false;
    OpenSL openSL;
    SwrContext *swr_context = NULL;
    uint8_t *dst_data_1 = NULL;
    uint8_t *dst_data_2 = NULL;
    pthread_t p_audio_tid = 0;
    bool thread_finish = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;
    bool is_pause = false;
};


#endif //TESTPLAYER_AUDIO_H
