//
// Created by Administrator on 2019/9/10.
//

#ifndef TESTPLAYER_EGLENGINE_H
#define TESTPLAYER_EGLENGINE_H

#include "../../utils/FLog.h"
#include <android/native_window_jni.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "GLRender.h"
#include "GLUtils.h"

class OpenGL {
public:
    OpenGL();

    ~OpenGL();

    int createEgl(ANativeWindow *surface, EGLContext eglContext);

    void surfaceChange(int view_width, int view_height,
                       int tex_width, int tex_height);

    int updateEgl(ANativeWindow *surface);

    void draw(void *pixels);

    void draw(int texture);

    void drawBackground();

    void release();

    EGLContext mEglContext = EGL_NO_CONTEXT;

    GLuint mTexture = 0;

    EGLSurface mEglSurface;
private:
    GLRender glRender;
    ANativeWindow *mWindow;
    EGLDisplay mEglDisplay;
    int mTexWidth;
    int mTexHeight;
    bool isCreateEgl = false;

    void createTexture();

    void deleteTexture();
};


#endif //TESTPLAYER_EGLENGINE_H
