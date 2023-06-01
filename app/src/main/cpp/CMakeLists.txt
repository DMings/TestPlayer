cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 17)

add_library(
        fplayer
        SHARED
        OpenSL.cpp
        OpenGL.cpp
        GLRender.cpp
        GLUtils.cpp
        FPacket.c
        AVClock.cpp
        BaseAV.cpp
        Audio.cpp
        Video.cpp
        FPlayer.cpp
        PthreadSleep.cpp
        GLThread.cpp
        AudioSpeed.cpp
        FPlayerJni.cpp
        )
find_library(log-lib log)
set(ffmepgSrc ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI})
add_library(avcodec STATIC IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libavcodec.a)
add_library(avformat STATIC IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libavformat.a)
add_library(avutil STATIC IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libavutil.a)
add_library(swresample STATIC IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libswresample.a)
add_library(swscale STATIC IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libswscale.a)
add_library(x264 STATIC IMPORTED)
set_target_properties(x264 PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libx264.a)
add_library(fdk-aac STATIC IMPORTED)
set_target_properties(fdk-aac PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libfdk-aac.a)
add_library(soundtouch STATIC IMPORTED)
set_target_properties(soundtouch PROPERTIES IMPORTED_LOCATION ${ffmepgSrc}/libsoundtouch.a)

include_directories(${CMAKE_SOURCE_DIR}/libs/include/)

#如果当前系统版本过低设置拒绝编译
#if (${ANDROID_PLATFORM_LEVEL} LESS 11)
#    message(FATAL_ERROR "OpenGL 2 is not supported before API level 11 (currently using ${ANDROID_PLATFORM_LEVEL}).")
#    return()
#elseif (${ANDROID_PLATFORM_LEVEL} LESS 18)
#    add_definitions("-DDYNAMIC_ES3")
    set(OPENGL_LIB GLESv2)
#else ()
#    set(OPENGL_LIB GLESv3)
#endif (${ANDROID_PLATFORM_LEVEL} LESS 11)

target_link_libraries(
        fplayer
        avformat
        avcodec
        avutil
        swresample
        swscale
        soundtouch
        x264
        fdk-aac
        log
        android
        OpenSLES
        EGL
        z
        ${OPENGL_LIB})