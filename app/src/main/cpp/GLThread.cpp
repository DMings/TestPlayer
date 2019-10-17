//
// Created by DMing on 2019/10/16.
//

#include "GLThread.h"

const int GLThread::CREATE_OR_UPDATE = 1;
const int GLThread::CHANGE_SIZE = 2;
const int GLThread::DESTROY = 3;
const int GLThread::DRAW = 4;

void *GLThread::glProcess(void *arg) {
    GLThread *glThread = (GLThread *) (arg);
    while (!glThread->gl_finish) {
        pthread_mutex_lock(&glThread->gl_mutex);
        while (!glThread->command_list.empty() && !glThread->gl_finish) {
            int command = glThread->command_list.front();
            glThread->command_list.pop_front();
            if (command == GLThread::CHANGE_SIZE) {
                LOGI("GLThread::CHANGE view_width %d, view_height %d tex_width %d, tex_height %d",
                     glThread->view_width, glThread->view_height,
                     glThread->tex_width, glThread->tex_height);
                glThread->openGL.surfaceChange(glThread->view_width, glThread->view_height,
                                               glThread->tex_width, glThread->tex_height);
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                }
            } else if (command == GLThread::CREATE_OR_UPDATE) {
                LOGI("GLThread::UPDATE view_width %d, view_height %d tex_width %d, tex_height %d",
                     glThread->view_width, glThread->view_height,
                     glThread->tex_width, glThread->tex_height);
                glThread->openGL.updateEgl(glThread->surface);
                glThread->openGL.surfaceChange(glThread->view_width, glThread->view_height,
                                               glThread->tex_width, glThread->tex_height);
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                }
            } else if (command == GLThread::DESTROY) {
                LOGE("GLThread::DESTROY");
                glThread->gl_finish = true;
                glThread->openGL.release();
                glThread->data = NULL;
            } else if (command == 4) {
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                }
            }
        }
        pthread_cond_wait(&glThread->gl_cond, &glThread->gl_mutex);
        pthread_mutex_unlock(&glThread->gl_mutex);
    }
    return NULL;
}

GLThread::GLThread() {
    pthread_mutex_init(&gl_mutex, NULL);
    pthread_cond_init(&gl_cond, NULL);
}

GLThread::~GLThread() {
    pthread_mutex_destroy(&gl_mutex);
    pthread_cond_destroy(&gl_cond);
}

void GLThread::surfaceCreated(ANativeWindow *surface) {
    LOGE("GLThread::surfaceCreated");
    this->surface = surface;
    pthread_mutex_lock(&gl_mutex);
    this->gl_finish = false;
    pthread_mutex_unlock(&gl_mutex);
    pthread_create(&p_gl_tid, 0, GLThread::glProcess, this);
}

void GLThread::surfaceChanged(ANativeWindow *surface, int view_width, int view_height) {
    LOGE("GLThread::surfaceChanged");
    this->view_width = view_width;
    this->view_height = view_height;
    pthread_mutex_lock(&gl_mutex);
    if (!this->gl_finish) {
        this->command_list.push_back(GLThread::CREATE_OR_UPDATE); // surface发生改变要更新
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);

}

void GLThread::surfaceDestroyed() {
    LOGE("GLThread::surfaceDestroyed");
    pthread_mutex_lock(&gl_mutex);
    this->command_list.push_back(GLThread::DESTROY);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
    pthread_join(p_gl_tid, 0);
    p_gl_tid = 0;
    LOGE("GLThread::surfaceDestroyed done");
}

void GLThread::setParams(uint8_t *data, int tex_width, int tex_height) {
    this->tex_width = tex_width;
    this->tex_height = tex_height;
    this->data = data;
    pthread_mutex_lock(&gl_mutex);
    if (!this->gl_finish) {
        this->command_list.push_back(GLThread::CHANGE_SIZE);
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::draw() {
    pthread_mutex_lock(&gl_mutex);
    this->command_list.clear();
    this->command_list.push_back(GLThread::DRAW);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::lockDraw() {
    pthread_mutex_lock(&gl_mutex);
}

void GLThread::unlockDraw() {
    pthread_mutex_unlock(&gl_mutex);
}