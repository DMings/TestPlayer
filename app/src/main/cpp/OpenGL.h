//
// Created by Administrator on 2019/9/10.
//

#ifndef TESTPLAYER_EGLENGINE_H
#define TESTPLAYER_EGLENGINE_H

#include "log.h"
#include <android/native_window_jni.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "GLRender.h"
#include "GLUtils.h"

class OpenGL {
public:
    OpenGL();

    ~OpenGL();

    int createEgl(ANativeWindow *surface, EGLContext eglContext,
                  int view_width, int view_height,
                  int tex_width, int tex_height);

    int updateEgl(ANativeWindow *surface, int view_width, int view_height);

    void draw(void *pixels);

    void drawBackground();

    void release(bool reset_view);

private:
    GLRender glRender;
    ANativeWindow *mWindow;
    EGLDisplay mEglDisplay;
    EGLContext mEglContext;
    EGLSurface mEglSurface;
    GLuint mTexture = 0;
    int mTexWidth;
    int mTexHeight;
    bool isCreateEgl = false;

    void createTexture();

    void deleteTexture();
};


#endif //TESTPLAYER_EGLENGINE_H
