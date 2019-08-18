cmake_minimum_required(VERSION 3.4.1)

set(ffmepg_src ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})
message("ffmepg_src:" ${ffmepg_src})

add_library(native-lib SHARED native-lib.cpp)
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

target_link_libraries(
        native-lib
        swscale
        avformat
        swresample
        avcodec
        avutil
        avfilter
        log
        android)