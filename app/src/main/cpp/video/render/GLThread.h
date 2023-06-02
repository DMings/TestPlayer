//
// Created by DMing on 2019/10/16.
//

#ifndef TESTPLAYER_GLTHREAD_H
#define TESTPLAYER_GLTHREAD_H

#include "OpenGL.h"
#include <pthread.h>
#include <list>

class GLThread {

public:
    GLThread();
    ~GLThread();
    void surfaceCreated(ANativeWindow *surface);
    void surfaceChanged(ANativeWindow *surface,int view_width, int view_height);
    void surfaceDestroyed();

    void setParams(uint8_t *data,int tex_width, int tex_height);
    void draw();
    void drawBackground();
    void lockDraw();
    void unlockDraw();
    std::list<int> command_list;
    //
    const static int CREATE_OR_UPDATE;
    const static int CHANGE_SIZE;
    const static int DESTROY;
    const static int DRAW;
    const static int CLEAN_DRAW;
private:
    OpenGL openGL;
    uint8_t *data = NULL;
    pthread_t p_gl_tid = NULL;
    bool gl_finish = false;
    static void *glProcess(void *arg);
    //
    pthread_mutex_t gl_mutex;
    pthread_cond_t gl_cond;

    ANativeWindow *surface;
    int view_width = 1;
    int view_height= 1;
    int tex_width= 1;
    int tex_height= 1;
};


#endif //TESTPLAYER_GLTHREAD_H
