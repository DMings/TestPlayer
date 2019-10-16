//
// Created by DMing on 2019/10/16.
//

#ifndef TESTPLAYER_GLTHREAD_H
#define TESTPLAYER_GLTHREAD_H

#include "OpenGL.h"
#include <cstdint>
#include <pthread.h>
#include <list>

class GLThread {

public:
    void GLThread();
    void ~GLThread();
    void surfaceCreated(ANativeWindow *surface);
    void surfaceChanged(int view_width, int view_height,
                        int tex_width, int tex_height);
    void surfaceDestroyed();

    void setDataSource(uint8_t *data);
    void draw();

    std::list<int> command_list;
private:
    OpenGL openGL;
    uint8_t *data = NULL;
    pthread_t p_gl_tid = 0;
    bool gl_finish = false;
    static void *glProcess(void *arg);
    //
    pthread_mutex_t gl_mutex;
    pthread_cond_t gl_cond;

    ANativeWindow *surface;
    int view_width;
    int view_height;
    int tex_width;
    int tex_height;
};


#endif //TESTPLAYER_GLTHREAD_H
