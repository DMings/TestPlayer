#include <jni.h>
#include <string>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include "log.h"

void testPlayer(const char* path) {
    LOGI("testPlayer>>>");
        av_register_all();
//    avformat_network_init();
        AVFormatContext *ifmt_ctx = 0;
        LOGI("set url:%s", path);
        avformat_open_input(&ifmt_ctx, path, 0, 0);
        avformat_find_stream_info(ifmt_ctx, 0);
        //查找流
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {

            AVCodecParameters *parameters = ifmt_ctx->streams[i]->codecpar;
            AVCodec *dec = avcodec_find_decoder(parameters->codec_id);

            if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
                AVCodecContext *codec = avcodec_alloc_context3(dec);
                avcodec_parameters_to_context(codec, parameters);
                avcodec_open2(codec, dec, 0);
                audio->setCodec(codec);
                audio->index = i;
                audio->time_base = ifmt_ctx->streams[i]->time_base;
            } else if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
                AVCodecContext *codec = avcodec_alloc_context3(dec);
                avcodec_parameters_to_context(codec, parameters);
                validate_thread_parameters(codec);
                avcodec_open2(codec, dec, 0);
                video->setCodec(codec);
                video->index = i;
                video->time_base = ifmt_ctx->streams[i]->time_base;
                if (window)
                    ANativeWindow_setBuffersGeometry(window, video->codec->width,
                                                     video->codec->height,
                                                     WINDOW_FORMAT_RGBA_8888);
            }
        }
        video->duration = (int) (ifmt_ctx->duration / 1000000);
        video->play(audio);
        audio->play();
        AVPacket *packet = av_packet_alloc();
        unsigned int video_delay = 0;
        unsigned int audio_delay = 0;
        int ret = 0;
        while (isPlay) {

            if (ff_pause) {
                pthread_mutex_lock(&ff_pause_mutex); //增加暂停
                pthread_cond_wait(&ff_pause_cond, &ff_pause_mutex);
                pthread_mutex_unlock(&ff_pause_mutex);
            }
            if (ff_stop)break;
            if (seekSet) {
                seekSet = false;
                video->clearQueue(&video->queue);
                audio->clearQueue(&audio->queue);
                avformat_flush(ifmt_ctx);//清空缓存数据
                seekFrame(ifmt_ctx, seekProgress);
                pthread_mutex_lock(&pause_mutex); //清除暂停
                pthread_cond_broadcast(&pause_cond);
                pthread_mutex_unlock(&pause_mutex);
            }
//        av_seek_frame(ifmt_ctx, -1 , 20 * AV_TIME_BASE, AVSEEK_FLAG_ANY);
            ret = av_read_frame(ifmt_ctx, packet);
            if (ret == 0) {
                if (video && packet->stream_index == video->index) {
                    video_delay = video->enQueue(packet);
                } else if (audio && packet->stream_index == audio->index) {
                    audio_delay = audio->enQueue(packet);
                }
                av_packet_unref(packet);
                if (audio_delay > 0) {
                    av_usleep((video_delay + audio_delay) * (1000 / 2)); //两者取平均 us
                }
//            if (video_delay > 0) {
//////                av_usleep((video_delay + audio_delay) * (1000 / 2)); //两者取平均 us
//                av_usleep(video_delay); //两者取平均 us
//            }
//            LOGI("video_delay: %d ms audio_delay: %d ms total: %d ms", video_delay, audio_delay,
//                 ((video_delay + audio_delay) / 2));

            } else if (ret == AVERROR_EOF) {
                //读取完毕 但是不一定播放完毕
                while (isPlay) {
                    if (video->queue.empty() && audio->queue.empty()) {
                        break;
                    }
//                LOGI("等待播放完成");
                    av_usleep(10000);
                }
                av_packet_unref(packet);
                break;
            } else {
                av_packet_unref(packet);
                break;
            }
        }
        isPlay = 0;
        if (path) {
            free(path);
            path = 0;
        }
        if (video && video->isPlay)
            video->stop();
        if (audio && audio->isPlay)
            audio->stop();
        clearPlayer();
        avformat_free_context(ifmt_ctx);
        av_packet_free(&packet);
        LOGI("PROCESS EXIT");
        JNIEnv *env;
        callBack.jvm->AttachCurrentThread(&env,NULL);
        env->DeleteGlobalRef(callBack.anPlayer);
        callBack.jvm->DetachCurrentThread();
        callBack.anPlayer = NULL;
        callBack.timeMethod = NULL;
        callBack.env = NULL;

}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dming_testplayer_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {


    testPlayer();


    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
