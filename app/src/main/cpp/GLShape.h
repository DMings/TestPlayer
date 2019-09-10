//
// Created by DMing on 2019/9/10.
//

#ifndef TESTPLAYER_GLSHAPE_H
#define TESTPLAYER_GLSHAPE_H

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "log.h"
#include <malloc.h>
#include <jni.h>

class GLShape {
public:
    void onDraw(GLuint texture);
    void init();

private:
    GLuint mProgram;
    GLuint mPosition;
    GLuint mTextureCoordinate;
    GLuint mImageTexture;
    GLuint mMatrix;
    float* mModelMatrix = new float[4 * 4];
};


#endif //TESTPLAYER_GLSHAPE_H
