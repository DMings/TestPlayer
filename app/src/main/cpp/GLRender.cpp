//
// Created by DMing on 2019/9/10.
//

#include "GLRender.h"

static const char VERTEX_SHADER[] =
        "precision mediump float;\n"
        "attribute vec4 inputPosition;\n"
        "attribute vec2 inputTextureCoordinate;\n"
        "varying vec2 textureCoordinate;\n"
        "uniform mat4 inputMatrix;\n"
        "void main() {\n"
        "gl_Position  = inputPosition * inputMatrix;\n"
        "textureCoordinate = inputTextureCoordinate;\n"
        "}\n";

static const char FRAGMENT_SHADER[] =
        "precision mediump float;\n"
        "varying vec2 textureCoordinate;\n"
        "uniform sampler2D inputImageTexture;\n"
        "void main() {\n"
        "gl_FragColor = texture2D(inputImageTexture, textureCoordinate);\n"
        "}\n";

static short VERTEX_INDEX[] = {
        0, 1, 3,
        2, 3, 1
};
static float VERTEX_POS[] = {
        -1, 1.0, 0,
        -1, -1, 0,
        1, -1, 0,
        1, 1.0, 0,
};

static float TEX_VERTEX[] = {
        0, 0,
        0, 1,
        1, 1,
        1, 0,
};

static float model_matrix[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
};

void GLShape::init(float ratioY) {
    mProgram = GLUtils::createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    mPosition = (GLuint) glGetAttribLocation(mProgram, "inputPosition");
    mTextureCoordinate = (GLuint) glGetAttribLocation(mProgram, "inputTextureCoordinate");
    mImageTexture = (GLuint) glGetUniformLocation(mProgram, "inputImageTexture");
    mMatrix = (GLuint) glGetUniformLocation(mProgram, "inputMatrix");
    model_matrix[5] = ratioY;
}


void GLShape::onDraw(GLuint texture) {
    glUseProgram(mProgram);
    glEnableVertexAttribArray(mPosition);
    glVertexAttribPointer(mPosition, 3, GL_FLOAT, 0, 0, VERTEX_POS);
    glEnableVertexAttribArray(mTextureCoordinate);
    glVertexAttribPointer(mTextureCoordinate, 2, GL_FLOAT, 0, 0, TEX_VERTEX);
    glUniformMatrix4fv(mMatrix, 1, 0, model_matrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(mImageTexture, 0);
    glDrawElements(GL_TRIANGLES, sizeof(VERTEX_INDEX) / sizeof(VERTEX_INDEX[0]),
                   GL_UNSIGNED_SHORT, VERTEX_INDEX);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(mPosition);
    glDisableVertexAttribArray(mTextureCoordinate);
    glUseProgram(0);
}


