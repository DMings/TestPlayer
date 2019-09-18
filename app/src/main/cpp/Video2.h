//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "OpenGL.h"
#include "Audio2.h"
#include "AV.h"

class Video : AV {
public:

    Video(UpdateTimeFun* updateTimeFun);

    int open_stream(ANativeWindow *window);

    void pause();

    void resume();

    void release();

    void update_surface(ANativeWindow *window);

private:
    SwsContext *sws_context;
    ANativeWindow *mWindow;
    OpenGL openGL;
    uint8_t *dst_data[4];
    int dst_line_size[4];
    pthread_t p_video_tid;
    bool thread_finish = false;
    bool is_pause = false;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_mutex;
    bool will_update_surface = false;
    UpdateTimeFun* updateTimeFun = NULL;
    //test
    double test_video_time = 0;

    int synchronize_video(double pkt_duration);

    static void *videoProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
