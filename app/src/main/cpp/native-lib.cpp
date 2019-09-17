#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "log.h"
#include "FPlayer2.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_seek(JNIEnv *env, jobject instance, jfloat percent) {
    LOGE("percent: %f", percent);
    seek(percent);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_pause(JNIEnv *env, jobject instance) {
    pause();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_resume(JNIEnv *env, jobject instance) {
    resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_update_1surface(JNIEnv *env, jobject instance,
                                                   jobject surface) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    update_surface(window);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_release(JNIEnv *env, jobject instance) {
    release();
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
//    jvm = vm;
    LOGE("JNI_OnLoad");
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
        return JNI_VERSION_1_6;
    }
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) == JNI_OK) {
        return JNI_VERSION_1_4;
    }
    return JNI_ERR;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
//    jvm = NULL;
    LOGE("JNI_OnUnload");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_play(JNIEnv *env, jobject instance, jstring path_, jobject surface,
                                               jobject onProgressListener) {

    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    const char *path = env->GetStringUTFChars(path_, NULL);
    jclass plClass = env->GetObjectClass(onProgressListener);
    jmethodID onProgress = env->GetMethodID(plClass,"onProgress","(F)V");
    start_player(path, window,env,onProgressListener,onProgress);
    env->ReleaseStringUTFChars(path_, path);
}