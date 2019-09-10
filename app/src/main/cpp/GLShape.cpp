//
// Created by DMing on 2019/9/10.
//


#include "GLShape.h"


static const char VERTEX_SHADER[] =
        "precision mediump float;\n"
        "attribute vec4 inputPosition;\n"
        "attribute vec2 inputTextureCoordinate;\n"
        "varying vec2 textureCoordinate;\n"
//        "uniform mat4 inputMatrix;\n"
        "void main() {\n"
//        "gl_Position  = inputPosition * inputMatrix;\n"
        "gl_Position  = inputPosition;\n"
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
        -1, -1.0, 0,
        1, -1.0, 0,
        1, 1.0, 0,
};

static float TEX_VERTEX[] = {
        0, 0,
        0, 1,
        1, 1,
        1, 0,
};


static bool checkGlError(const char *funcName) {
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGE("GL error after %s(): 0x%08x\n", funcName, err);
        return true;
    }
    return false;
}

static GLuint createShader(GLenum shaderType, const char *src) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        checkGlError("glCreateShader");
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);

    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen > 0) {
            GLchar *infoLog = (GLchar *) malloc(infoLogLen);
            if (infoLog) {
                glGetShaderInfoLog(shader, infoLogLen, NULL, infoLog);
                LOGE("Could not compile %s shader:\n%s\n",
                      shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment",
                      infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint createProgram(const char *vtxSrc, const char *fragSrc) {
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    vtxShader = createShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader)
        goto exit;

    fragShader = createShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader)
        goto exit;

    program = glCreateProgram();
    if (!program) {
        checkGlError("glCreateProgram");
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        LOGE("Could not link program");
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            GLchar *infoLog = (GLchar *) malloc(infoLogLen);
            if (infoLog) {
                glGetProgramInfoLog(program, infoLogLen, NULL, infoLog);
                LOGE("Could not link program:\n%s\n", infoLog);
                free(infoLog);
            }
        }
        glDeleteProgram(program);
        program = 0;
    }

    exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}


void GLShape::init() {
    mProgram = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    mPosition = (GLuint)glGetAttribLocation(mProgram, "inputPosition");
    mTextureCoordinate = (GLuint)glGetAttribLocation(mProgram, "inputTextureCoordinate");
    mImageTexture = (GLuint)glGetUniformLocation(mProgram, "inputImageTexture");
//    mMatrix = (GLuint)glGetUniformLocation(mProgram, "inputMatrix");
//    setIdentityM(mModelMatrix, 0);
    LOGI("sizeof(VERTEX_INDEX)/sizeof(VERTEX_INDEX[0]) %d",(sizeof(VERTEX_INDEX)/sizeof(VERTEX_INDEX[0])));
}


void GLShape::onDraw(GLuint texture) {
    glUseProgram(mProgram);
    glEnableVertexAttribArray(mPosition);
    glVertexAttribPointer(mPosition, 3,  GL_FLOAT, 0, 0, VERTEX_POS);
    glEnableVertexAttribArray(mTextureCoordinate);
    glVertexAttribPointer(mTextureCoordinate, 2, GL_FLOAT, 0, 0, TEX_VERTEX);
//    glUniformMatrix4fv(mMatrix, 1, 0, mModelMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(mImageTexture, 0);
    glDrawElements(GL_TRIANGLES, sizeof(VERTEX_INDEX)/sizeof(VERTEX_INDEX[0]),
            GL_UNSIGNED_SHORT, VERTEX_INDEX);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(mPosition);
    glDisableVertexAttribArray(mTextureCoordinate);
    glUseProgram(0);
}


