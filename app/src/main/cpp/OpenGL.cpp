//
// Created by Administrator on 2019/9/10.
//

#include "OpenGL.h"

OpenGL::OpenGL() {

}

OpenGL::~OpenGL() {

}

int OpenGL::createEgl(ANativeWindow *surface, EGLContext eglContext) {
    LOGI("...createEgl: %p", eglContext);
    if (!surface) {
        return -1;
    }
    mWindow = surface;
//    ANativeWindow_acquire(mWindow);
    GLint majorVersion;
    GLint minorVersion;
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (EGL_NO_DISPLAY == mEglDisplay) {
        return -2;
    }
    if (!eglInitialize(mEglDisplay, &majorVersion, &minorVersion)) {
        return -3;
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
        return -4;
    }

//    if (view_width == 0 && view_height == 0) {
//        if (!eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &view_width) ||
//            !eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &view_height)) {
//            return -1;
//        }
//    }

    EGLint context_attr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };
    mEglContext = eglCreateContext(mEglDisplay, eglConfig,
                                   eglContext != NULL || eglConfig != EGL_NO_CONTEXT ? eglContext : EGL_NO_CONTEXT,
                                   context_attr);
    if (EGL_NO_CONTEXT == mEglContext) {
        return -6;
    }

//    if(eglSurface == NULL){
        mEglSurface = eglCreateWindowSurface(mEglDisplay, eglConfig, mWindow, NULL);
        if (EGL_NO_SURFACE == mEglSurface) {
            return -5;
        }
//    }else {
//        LOGI("eglSurface != NULL");
//    }

    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        return -7;
    }
    glClearColor(0.0, 0.0, 0.0, 1.0);
//    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    createTexture();
    glRender.init();
    isCreateEgl = true;
    return mTexture;
}

void OpenGL::surfaceChange(int view_width, int view_height,
                           int tex_width, int tex_height) {
    glViewport(0, 0, view_width, view_height);
    mTexWidth = tex_width;
    mTexHeight = tex_height;
    LOGI("OpenGL income width %d, surface height %d", view_width, view_height);
    float ratioY = 1;
    float ratioX = 1;
    float rTex = 1.0F * mTexWidth / mTexHeight;
    float rView = 1.0F * view_width / view_height;
    if (view_width < view_height) {
        if (rTex > rView) {
            ratioY = (float) (1.0 * mTexHeight * view_width / mTexWidth / view_height);
        } else {
            ratioX = (float) (1 / (1.0 * mTexWidth * view_height / (mTexHeight * view_width)));
        }
    } else {
        if (rTex > rView) {
            ratioY = 1.0F / (1.0F * mTexHeight * view_width / mTexWidth / view_height);
        } else {
            ratioX = 1.0F * (1.0F * mTexWidth * view_height / (mTexHeight * view_width));
        }
    }
    glRender.onSizeChange(ratioX, ratioY);
    LOGI("OpenGL init success: surface width %d, surface height %d texture width %d, texture height %d ratioX:%f  ratioY:%f",
         view_width, view_height, mTexWidth, mTexHeight, ratioX, ratioY);
}

int OpenGL::updateEgl(ANativeWindow *surface) {
    if (isCreateEgl) {
        release(false);
        int ret = createEgl(surface, NULL);
        return ret;
    }
    return -1;
}

void OpenGL::release(bool reset_view) {
    if (reset_view) {
        drawBackground();
    }
    isCreateEgl = false;
    deleteTexture();
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
//    if (mWindow != NULL) {
//        ANativeWindow_release(mWindow);
//    }
    mEglDisplay = EGL_NO_DISPLAY;
    mEglContext = EGL_NO_CONTEXT;
    mEglSurface = EGL_NO_SURFACE;
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

void OpenGL::deleteTexture() {
    glDeleteTextures(1, &mTexture);
    glRender.release();
    GLUtils::checkErr("createTexture");
}

void OpenGL::draw(void *pixels) {
    glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTexWidth, mTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    glRender.onDraw(mTexture);
    eglSwapBuffers(mEglDisplay, mEglSurface);
    GLUtils::checkErr("draw");
}

void OpenGL::draw(int texture) {
    glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glRender.onDraw(texture);
    eglSwapBuffers(mEglDisplay, mEglSurface);
    GLUtils::checkErr("draw");
}

void OpenGL::drawBackground() {
    LOGI("drawBackground");
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(mEglDisplay, mEglSurface);
    GLUtils::checkErr("draw");
}

