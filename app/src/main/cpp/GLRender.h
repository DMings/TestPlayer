//
// Created by DMing on 2019/9/10.
//

#ifndef TESTPLAYER_GLSHAPE_H
#define TESTPLAYER_GLSHAPE_H

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "log.h"
#include <jni.h>
#include "GLUtils.h"

class GLRender {
public:
    void onDraw(GLuint texture);
    void init(float ratioY);
    void release();

private:
    GLuint mProgram;
    GLuint mPosition;
    GLuint mTextureCoordinate;
    GLuint mImageTexture;
    GLuint mMatrix;
};


#endif //TESTPLAYER_GLSHAPE_H
