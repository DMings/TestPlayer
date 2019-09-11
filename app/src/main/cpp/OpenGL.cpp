//
// Created by Administrator on 2019/9/10.
//

#include "OpenGL.h"

OpenGL::OpenGL() {

}

OpenGL::~OpenGL() {

}

int OpenGL::init(ANativeWindow *surface,EGLContext eglContext, int width, int height) {
    if (!surface) {
        return -1;
    }
    mTexWidth = width;
    mTexHeight = height;
    LOGI("income width %d, surface height %d", width, height);
    mWindow = surface;
    ANativeWindow_acquire(mWindow);
    GLint majorVersion;
    GLint minorVersion;
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (EGL_NO_DISPLAY == mEglDisplay) {
        return -1;
    }
    if (!eglInitialize(mEglDisplay, &majorVersion, &minorVersion)) {
        return -1;
    }
    EGLint config_attr[] = {
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
    };
    int num_configs = 0;
    EGLConfig eglConfig;
    if (!eglChooseConfig(mEglDisplay, config_attr, &eglConfig, 1, &num_configs)) {
        return -1;
    }
    mEglSurface = eglCreateWindowSurface(mEglDisplay, eglConfig, mWindow, NULL);
    if (EGL_NO_SURFACE == mEglSurface) {
        return -1;
    }
    if (!eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &width) ||
        !eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &height)) {
        return -1;
    }
    EGLint context_attr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };
    mEglContext = eglCreateContext(mEglDisplay, eglConfig,
                                   eglContext != NULL ? eglContext : EGL_NO_CONTEXT, context_attr);
    if (EGL_NO_CONTEXT == mEglContext) {
        return -1;
    }
    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        return -1;
    }
    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    createTexture();
    float tr = mTexWidth * 1.0f / mTexHeight;
    float ratioY = (width / tr) / height;
    LOGI("OpenGL init success: surface width %d, surface height %d ratioY:%f", width, height,ratioY);
    glShape.init(ratioY);
    return mTexture;
}

void OpenGL::release() {
    if (mEglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (mEglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(mEglDisplay, mEglContext);
        }
        if (mEglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(mEglDisplay, mEglSurface);
        }
        if (mEglDisplay != NULL) {
            eglTerminate(mEglDisplay);
        }
    }
    mEglDisplay = EGL_NO_DISPLAY;
    mEglContext = EGL_NO_CONTEXT;
    mEglSurface = EGL_NO_SURFACE;
    ANativeWindow_release(mWindow);
    mWindow = NULL;
}

void OpenGL::createTexture() {
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexParameterf(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLUtils::checkErr("createTexture");
}

void OpenGL::draw(void *pixels) {
    glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTexWidth, mTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    glShape.onDraw(mTexture);
    eglSwapBuffers(mEglDisplay, mEglSurface);
    GLUtils::checkErr("draw");
}

