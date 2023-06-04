//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "./render/GLThread.h"
#include "../audio/Audio.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Video : BaseAV {
public:

    Video(GLThread* glThread,AVClock* avClock);

    ~Video();

    int open_stream(AVFormatContext *fmt_ctx);

    void putAvPacket(FPacket* pkt);

    uint64_t getAvPacketSize();

    void pause();

    void resume();

    void release();

    int stream_id = -1;

    AVClock* avClock;

    AVCodecContext *av_dec_ctx = NULL;

    AVStream *av_stream = NULL;
private:
    SwsContext *sws_context = NULL;
    GLThread* glThread;
    uint8_t *dst_data[4];
    int dst_line_size[4];
    pthread_t p_video_tid = 0;
    std::atomic_bool thread_finish = false;
    std::atomic_bool is_pause = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;

    static PthreadSleep pthread_sleep;

    uint synchronize_video(double pkt_duration);

    static void *videoProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
