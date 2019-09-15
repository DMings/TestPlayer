#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "log.h"
#include "FPlayer2.h"

extern "C" JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_testFF(
        JNIEnv *env,
        jobject, jstring path_, jobject surface) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    const char *path = env->GetStringUTFChars(path_, NULL);
    startPlayer(path, window);
    env->ReleaseStringUTFChars(path_, path);
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
