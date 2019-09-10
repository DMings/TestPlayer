//
// Created by Administrator on 2019/9/10.
//

#ifndef TESTPLAYER_EGLENGINE_H
#define TESTPLAYER_EGLENGINE_H

#include "log.h"
#include <android/native_window_jni.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "GLShape.h"

class OpenGL {
public:
    OpenGL();
    ~OpenGL();
    int init(ANativeWindow * surface,int width,int height);
    void draw(void *pixels);
    void release();
private:
    GLShape glShape;
    ANativeWindow * mWindow;
    EGLDisplay mEglDisplay;
    EGLContext mEglContext;
    EGLSurface mEglSurface;
    GLuint mTexture = 0;
    int mWidth;
    int mHeight;
    void checkErr();
    void createTexture();
};


#endif //TESTPLAYER_EGLENGINE_H
