//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "GLThread.h"
#include "Audio.h"
#include "AV.h"
#include "PthreadSleep.h"

class Video : AV {
public:

    Video(GLThread* glThread,UpdateTimeFun *fun);

    ~Video();

    int open_stream(bool hasAudio);

    void pause();

    void resume();

    void release();

    int stream_id = -1;

    int view_width = 0;
    int view_height = 0;

    AVCodecContext *av_dec_ctx = NULL;
private:
    AVStream *av_stream = NULL;
    SwsContext *sws_context = NULL;
    GLThread* glThread;
    uint8_t *dst_data[4];
    int dst_line_size[4];
    pthread_t p_video_tid = 0;
    bool thread_finish = false;
    bool is_pause = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;
    UpdateTimeFun* updateTimeFun = NULL;
    bool has_audio = false;

    static PthreadSleep pthread_sleep;

    uint synchronize_video(double pkt_duration);

    static void *videoProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
