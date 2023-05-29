//
// Created by DMing on 2019/10/16.
//

#include "GLThread.h"

const int GLThread::CREATE_OR_UPDATE = 1;
const int GLThread::CHANGE_SIZE = 2;
const int GLThread::DESTROY = 3;
const int GLThread::DRAW = 4;
const int GLThread::CLEAN_DRAW = 5;

void *GLThread::glProcess(void *arg) {
    GLThread *glThread = (GLThread *) (arg);
    while (!glThread->gl_finish) {
        pthread_mutex_lock(&glThread->gl_mutex);
        while (!glThread->command_list.empty() && !glThread->gl_finish) {
            int command = glThread->command_list.front();
            glThread->command_list.pop_front();
            if (command == GLThread::CHANGE_SIZE) {
                glThread->openGL.surfaceChange(glThread->view_width, glThread->view_height,
                                               glThread->tex_width, glThread->tex_height);
//                if (glThread->data != NULL) {
//                    glThread->openGL.draw(glThread->data);
//                }
            } else if (command == GLThread::CREATE_OR_UPDATE) {
                glThread->openGL.updateEgl(glThread->surface);
                glThread->openGL.surfaceChange(glThread->view_width, glThread->view_height,
                                               glThread->tex_width, glThread->tex_height);
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                }
            } else if (command == GLThread::DESTROY) {
                LOGI("GLThread::DESTROY");
                glThread->gl_finish = true;
                glThread->openGL.release();
                pthread_mutex_unlock(&glThread->gl_mutex);
                return NULL;
            } else if (command == GLThread::DRAW) {
                if (glThread->data != NULL) {
                    glThread->openGL.draw(glThread->data);
                } else {
                    LOGI("GLThread::DRAW: %p", glThread->data);
                }
            } else if (command == GLThread::CLEAN_DRAW) {
                glThread->openGL.drawBackground();
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
    LOGI("GLThread::surfaceCreated");
    pthread_mutex_lock(&gl_mutex);
    this->surface = surface;
    this->gl_finish = false;
    pthread_mutex_unlock(&gl_mutex);
    pthread_create(&p_gl_tid, NULL, GLThread::glProcess, this);
}

void GLThread::surfaceChanged(ANativeWindow *surface, int view_width, int view_height) {
    LOGI("GLThread::surfaceChanged");
    pthread_mutex_lock(&gl_mutex);
    this->surface = surface;
    this->view_width = view_width;
    this->view_height = view_height;
    if (!this->gl_finish) {
        for (std::list<int>::iterator it = command_list.begin(); it != command_list.end();) {
            if (*it == GLThread::DRAW) {
                command_list.erase(it++);
            } else {
                ++it;
            }
        }
        this->command_list.push_back(GLThread::CREATE_OR_UPDATE); // surface发生改变要更新
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);

}

void GLThread::surfaceDestroyed() {
    LOGI("GLThread::surfaceDestroyed");
    pthread_mutex_lock(&gl_mutex);
    this->command_list.clear();
    this->command_list.push_back(GLThread::DESTROY);
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
    pthread_join(p_gl_tid, NULL);
    p_gl_tid = NULL;
    LOGI("GLThread::surfaceDestroyed done");
}

void GLThread::setParams(uint8_t *data, int tex_width, int tex_height) {
    pthread_mutex_lock(&gl_mutex);
    this->data = data;
    this->tex_width = tex_width;
    this->tex_height = tex_height;
    LOGI("setParams: %p", this->data);
    if (!this->gl_finish && this->data != NULL) {
        this->command_list.push_back(GLThread::CHANGE_SIZE);
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::draw() {
    pthread_mutex_lock(&gl_mutex);
    if (!this->gl_finish) {
        this->command_list.push_back(GLThread::DRAW);
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::drawBackground() {
    pthread_mutex_lock(&gl_mutex);
    if (!this->gl_finish) {
        this->command_list.push_back(GLThread::CLEAN_DRAW);
    }
    pthread_cond_signal(&gl_cond);
    pthread_mutex_unlock(&gl_mutex);
}

void GLThread::lockDraw() {
    pthread_mutex_lock(&gl_mutex);
}

void GLThread::unlockDraw() {
    pthread_mutex_unlock(&gl_mutex);
}