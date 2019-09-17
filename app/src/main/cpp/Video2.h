//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "OpenGL.h"
#include "Audio2.h"

class Video {
public:
    int open_stream(ANativeWindow *window);

    void release();

private:
    static Clock video_clk;
    SwsContext *sws_context;
    ANativeWindow *mWindow;
    OpenGL openGL;
    uint8_t *dst_data[4];
    int dst_line_size[4];
    pthread_t p_video_tid;
    bool thread_flag = true;
    //test
    double test_video_time = 0;

    int synchronize_video(double pkt_duration);

    static void *videoProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
