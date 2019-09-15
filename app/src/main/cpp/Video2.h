//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "OpenGL.h"
#include "Audio2.h"

class Video {
public:
    static double get_video_pts_clock();

    static void set_video_clock(double pts);

    static double get_video_clock();

    int open_stream(ANativeWindow *window);

    void release();

private:
    static Clock video_clk;
    AVCodecContext *video_dec_ctx = NULL;
    SwsContext *sws_context;
    AVStream *video_stream = NULL;
    ANativeWindow *mWindow;
    OpenGL openGL;
    uint8_t *dst_data[4];
    int dst_line_size[4];
    pthread_t p_video_tid;
    bool thread_flag = true;
    //test
    double test_video_time;

    int synchronize_video(double pkt_duration);

    static void *videoProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
