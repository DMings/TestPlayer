cmake_minimum_required(VERSION 3.4.1)

set(ffmepg_src ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})
message("ffmepg_src:" ${ffmepg_src})

add_library(native-lib
        SHARED
        native-lib.cpp
        Opensl.cpp
        OpenGL.cpp
        GLRender.cpp
        GLUtils.cpp
        )
find_library(log-lib log)
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libavcodec.so)
add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libavfilter.so)
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libavformat.so)
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libavutil.so)
add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libswresample.so)
add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${ffmepg_src}/libswscale.so)

include_directories(${CMAKE_SOURCE_DIR}/ffmpeg/)

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
        native-lib
        swscale
        avformat
        swresample
        avcodec
        avutil
        avfilter
        log
        android
        OpenSLES
        EGL
        ${OPENGL_LIB})