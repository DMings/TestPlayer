//
// Created by DMing on 2019/10/16.
//

#include "GLThread.h"

void *GLThread::glProcess(void *arg) {
    GLThread *glThread = (GLThread *) (arg);
    while (!glThread->gl_finish) {
        pthread_mutex_lock(&gl_mutex);
        while (!glThread->command_list.empty() && !glThread->gl_finish) {
            int command = glThread->command_list.front();
            glThread->command_list.pop_front();
            if (command == 1) {
                glThread->openGL.createEgl(glThread->surface, NULL);
            } else if (command == 2) {
                glThread->openGL.surfaceChange(glThread->view_width, glThread->view_height,
                                               glThread->tex_width, glThread->tex_height);
            } else if (command == 3) {
                glThread->gl_finish = true;
                glThread->openGL.release(false);
                glThread->data = NULL;
            } else if (command == 4) {
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                }
            }
        }
        pthread_cond_wait(&gl_cond, &gl_mutex);
        pthread_mutex_unlock(&gl_mutex);
    }
}

void GLThread::GLThread() {
    pthread_mutex_init(&gl_mutex, NULL);
    pthread_cond_init(&gl_cond, NULL);
    pthread_create(&p_gl_tid, 0, GLThread::glProcess, this);
}

void GLThread::~GLThread() {
    pthread_mutex_destroy(&gl_mutex);
    pthread_cond_destroy(&gl_cond);
}

void GLThread::surfaceCreated(ANativeWindow *surface) {
    this->surface = surface;
    pthread_mutex_lock(&gl_mutex);
    this->gl_finish = false;
    this->command_list.push_back(1);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::surfaceChanged(int view_width, int view_height,
                              int tex_width, int tex_height) {

    this->view_width = view_width;
    this->view_height = view_height;
    this->tex_width = tex_width;
    this->tex_height = tex_height;
    pthread_mutex_lock(&gl_mutex);
    if (!this->gl_finish) {
        this->command_list.push_back(2);
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);

}

void GLThread::surfaceDestroyed() {
    pthread_mutex_lock(&gl_mutex);
    this->command_list.push_back(3);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
    pthread_join(p_gl_tid, 0);
}

void GLThread::setDataSource(uint8_t *data) {
    this->data = data;
}

void GLThread::draw() {
    pthread_mutex_lock(&gl_mutex);
    this->command_list.push_back(666);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}