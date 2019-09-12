#include <jni.h>
#include <string>
#include "log.h"
#include "OpenSL.h"
#include "OpenGL.h"
#include <list>
#include <pthread.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

extern "C" {
#include <libavutil/log.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
}
#define FLOGI(FORMAT, ...) LOGI(FORMAT,##__VA_ARGS__);

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};


typedef struct Clock {
    double pts;           /* clock base */
//    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
} Clock;


static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static OpenSL openSL;
static OpenGL openGL;
static struct SwrContext *swr_context;
static struct SwsContext *sws_context;
static uint8_t *out_buffer;
static uint8_t *dst_data[4];
static int dst_linesize[4];
static int wanted_nb_samples;
static pthread_mutex_t c_mutex;
static pthread_mutex_t a_mutex;
static pthread_cond_t c_cond;
static pthread_cond_t a_cond;
static bool mustFeed = false;
static pthread_t p_a_tid;
static pthread_t p_v_tid;
static bool thread_flag = true;
static Clock vidclk;
static Clock audclk;
static Clock extclk;

static std::list<AVPacket *> audio_pkt_list;
static std::list<AVPacket *> video_pkt_list;

static ANativeWindow *mWindow;

static int open_codec_context(const char *src_filename, int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                              enum AVMediaType type) {
    int ret, stream_index;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    AVStream *st;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        FLOGI("Could not find %s stream in input file '%s'",
              av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            FLOGI("Failed to find %s codec",
                  av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            FLOGI("Failed to allocate the %s codec context",
                  av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            FLOGI("Failed to copy %s codec parameters to decoder context",
                  av_get_media_type_string(type));
            return ret;
        }
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            FLOGI("Failed to open %s codec",
                  av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
        LOGI("(*dec_ctx) => %d %d",  (*dec_ctx)->time_base.num, (*dec_ctx)->time_base.den);
    }
    return 0;
}

//static void set_clock(Clock *c, double pts) {
//    c->pts = pts;
////    c->last_updated = av_gettime_relative() / 1000000.0; // ms
//}

static double get_master_clock() {
    double time = av_gettime_relative() / 1000.0;
    if (extclk.last_updated == 0) {
        extclk.last_updated = time;
    }
    return time - extclk.last_updated; // ms
}

static double get_audio_clock() {
    if(audclk.pts == 0){
        return 0;
    }
    double time = av_gettime_relative() / 1000.0;
    if (audclk.last_updated == 0) {
        audclk.last_updated = time;
    }
    return audclk.pts + (time - audclk.last_updated); // ms
}

static double get_audio_pts_clock() {
    return audclk.pts; // ms
}

static void set_audio_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    audclk.pts = pts; // ms
    audclk.last_updated = time;
}

static double get_video_pts_clock() {
    return vidclk.pts; // ms
}

static void set_video_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    vidclk.pts = pts; // ms
    vidclk.last_updated = time;
}

static double get_video_clock() {
    return vidclk.pts; // ms
}

static int synchronize_audio(int nb_samples) {
    int wanted_nb_samples;
    double diff_ms;
    double nb_time_ms;
//    LOGI("get_audio_clock: %f get_master_clock: %f nb_samples: %d", get_audio_clock(), get_master_clock(), nb_samples);
    diff_ms = get_audio_pts_clock() - get_master_clock();
    nb_time_ms = 1.0 * nb_samples / audio_dec_ctx->sample_rate * 1000; // ms
//    LOGI("diff_ms: %f nb_time_ms: %f", diff_ms, nb_time_ms);
    if (fabs(diff_ms) > nb_time_ms * 0.2) { // 超过阀值
        if (diff_ms < 0) { // 音频落后时间
            if (-diff_ms < nb_time_ms) { // 音频比较慢，要丢弃帧，加快下一次pts超过时基
                wanted_nb_samples = (int) ((1 + diff_ms / nb_time_ms) * nb_samples);
            } else {
                wanted_nb_samples = 0;
            }
//            LOGI("wanted_nb_samples: %d diff_ms / nb_time_ms: %.6f,nb_samples: %d", wanted_nb_samples,(-diff_ms / nb_time_ms),nb_samples);
        } else { // 音频超过时间
            if (diff_ms < nb_time_ms) { // 音频比较快，要增加帧，减慢下一次的pts,让时基跟上
                wanted_nb_samples = (int) (diff_ms / nb_time_ms * nb_samples + nb_samples);
            } else {
                wanted_nb_samples = nb_samples + nb_samples;
            }
        }
        if (wanted_nb_samples < 0.2 * nb_samples) {  // 给上阀值，不能少于某一个采样率
            wanted_nb_samples = (int) (0.2 * nb_samples);
        }
    } else {
        wanted_nb_samples = nb_samples;
    }
//    wanted_nb_samples = ((wanted_nb_samples & 1) == 0) ? wanted_nb_samples : (wanted_nb_samples + 1);
    LOGI("diff_ms: %f wanted_nb_samples: %d", diff_ms, wanted_nb_samples);
    return wanted_nb_samples;
//    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
}

//AVFrame *frame = av_frame_alloc();

static void decode_packet(AVPacket *pkt, bool clear_cache) {
    const char *stream_id = "-";
    if (clear_cache) {
        pkt->data = NULL;
        pkt->size = 0;
    }
    if (pkt->stream_index == video_stream_idx) {
        stream_id = "video";
        AVPacket *copy_pkg = av_packet_clone(pkt);
        if (copy_pkg != NULL) {
            video_pkt_list.push_back(copy_pkg);
        }
    } else if (pkt->stream_index == audio_stream_idx) {
        stream_id = "audio";
        AVPacket *copy_pkg = av_packet_clone(pkt);
        if (copy_pkg != NULL) {
            audio_pkt_list.push_back(copy_pkg);
        }
    }
    LOGI("stream_id ->: %s", stream_id)
    pthread_mutex_lock(&c_mutex);
    pthread_cond_broadcast(&c_cond);
    if (audio_pkt_list.size() >= 2 && video_pkt_list.size() >= 2) {
        pthread_cond_wait(&c_cond, &c_mutex);
    }
    pthread_mutex_unlock(&c_mutex);
}

static uint synchronize_video(double pkt_duration) { // us
    double wanted_delay = -1;
    double diff_ms;
    double duration = pkt_duration < 200 ? pkt_duration : 200;
    diff_ms = get_video_pts_clock() - get_audio_clock();
    if (diff_ms < 0) { // 视频落后时间
        if (duration > 0 && wanted_delay < duration * 0.3) {
            wanted_delay = 0;
        }
    } else { // 视频超过时间
        wanted_delay = diff_ms;
    }
//    LOGI("video diff_ms: %f wanted_delay: %f pkt_duration: %f get_video_pts_clock: %f", diff_ms, wanted_delay,pkt_duration,get_video_pts_clock());
    return (uint) wanted_delay;
}

double video_time;

void *videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    AVPacket *avPacket = NULL;
    int texture = openGL.init(mWindow, NULL, video_dec_ctx->width, video_dec_ctx->height);
    LOGI("texture: %d", texture)
    while (thread_flag) {
        avPacket = NULL;
        do {
            LOGE("video_pkt_list size: %d", video_pkt_list.size())
            if (!video_pkt_list.empty()) {
                avPacket = video_pkt_list.front();
                video_pkt_list.pop_front();
                if (video_pkt_list.size() <= 1) {
                    pthread_mutex_lock(&c_mutex);
                    pthread_cond_broadcast(&c_cond);
                    pthread_mutex_unlock(&c_mutex);
                }
            } else {
                pthread_mutex_lock(&c_mutex);
                pthread_cond_broadcast(&c_cond);
                pthread_cond_wait(&c_cond, &c_mutex);
                pthread_mutex_unlock(&c_mutex);
                if (!video_pkt_list.empty()) {
                    avPacket = video_pkt_list.front();
                    video_pkt_list.pop_front();
                }
            }
        } while (avPacket == NULL);
        LOGI("video decode ")
        // 解码
        ret = avcodec_send_packet(video_dec_ctx, avPacket);
        if (ret < 0) {
            LOGE("Error video sending a packet for decoding");
            continue;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(video_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) || ret == AVERROR_INPUT_CHANGED) {
                LOGE("video some err!");
                break;
            } else if (ret < 0) {
                LOGE("video legitimate decoding errors");
                break;
            }
            //  用 st 上的时基才对 video_stream
            double pts = (int64_t) (av_q2d(video_stream->time_base) * frame->pts * 1000.0); // ms
            double pkt_duration = (int64_t) (av_q2d(video_stream->time_base) * frame->pkt_duration * 1000.0); // ms
            ret = sws_scale(sws_context,
                            (const uint8_t *const *) frame->data, frame->linesize,
                            0, video_dec_ctx->height,
                            dst_data, dst_linesize);
            LOGI("video pts: %f get_audio_clock: %f get_master_clock: %f pkt_duration: %f", pts, get_audio_clock(),get_master_clock(),
                 pkt_duration);
            set_video_clock(pts);
            uint delay = synchronize_video(pkt_duration); // ms
//            LOGI("video show-> width: %d height: %d pts: %f delay: %f pkt_duration: %f", frame->width, frame->height,
//                 pts, delay, pkt_duration);

            if (delay >= 0) {
                av_usleep(delay * 1000); // us
                LOGI("video show->%d get_master_clock: %f video_time: %f", delay,get_master_clock(),(get_master_clock() - video_time));
            } else {
                LOGI("video show->skip get_master_clock: %f video_time: %f",get_master_clock(),(get_master_clock() - video_time));
            }
            video_time = get_master_clock();
//            LOGI("height: %d video_dec_ctx->height %d",ret,video_dec_ctx->height);
            openGL.draw(dst_data[0]);
//            env->CallVoidMethod(updateObject, updateMethod);
        }
        av_packet_free(&avPacket);
    }
    av_frame_free(&frame);
    openGL.release();
//    env->DeleteGlobalRef(initObject);
//    env->DeleteGlobalRef(updateObject);
//    jvm->DetachCurrentThread();
    return 0;
}

static bool test = false;
static int last_wanted_nb_samples = 0;

static void slBufferCallback() {
    pthread_mutex_lock(&a_mutex);
    mustFeed = true;
    pthread_cond_signal(&a_cond); //通知
    pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
    pthread_mutex_unlock(&a_mutex);
    //在这里之后必须要有数据
}


void *audioProcess(void *arg) {
    int ret = 0;
    AVPacket *avPacket = NULL;
    AVFrame *frame = av_frame_alloc();
    while (thread_flag) {
        avPacket = NULL;
        do {
            LOGI("audio_pkt_list size: %d", audio_pkt_list.size())
            if (!audio_pkt_list.empty()) {
                avPacket = audio_pkt_list.front();
                audio_pkt_list.pop_front();
                if (audio_pkt_list.size() <= 1) {
                    pthread_mutex_lock(&c_mutex);
                    pthread_cond_broadcast(&c_cond);
                    pthread_mutex_unlock(&c_mutex);
                }
            } else {
                pthread_mutex_lock(&c_mutex);
                pthread_cond_broadcast(&c_cond);
                pthread_cond_wait(&c_cond, &c_mutex);
                pthread_mutex_unlock(&c_mutex);
                if (!audio_pkt_list.empty()) {
                    avPacket = audio_pkt_list.front();
                    audio_pkt_list.pop_front();
                }
            }
        } while (avPacket == NULL);

        // 解码
        ret = avcodec_send_packet(audio_dec_ctx, avPacket);
        if (ret < 0) {
            LOGE("Error audio sending a packet for decoding");
            av_frame_free(&frame);
            return NULL;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) || ret == AVERROR_INPUT_CHANGED) {
                LOGE("audio some err!");
                break;
            } else if (ret < 0) {
                LOGE("audio legitimate decoding errors");
                break;
            }
            pthread_mutex_lock(&a_mutex);
            if (!mustFeed) {
                pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
            }
            mustFeed = false;

            // 到这里必须要有sl数据
//            set_audio_clock(frame->best_effort_timestamp);// ms
            double pts = av_q2d(audio_stream->time_base) * frame->pts * 1000.0;
            set_audio_clock(pts);// ms
            LOGI("audio pts: %f best_effort_timestamp: %lld get_master_clock: %f", pts, frame->best_effort_timestamp,
                 get_master_clock());
            wanted_nb_samples = frame->nb_samples;
//            wanted_nb_samples = synchronize_audio(frame->nb_samples);
//            if (!test) {
//                test = true;
//                wanted_nb_samples = 1024;
//            } else {
//                test = false;
//                wanted_nb_samples = 768;
//            }
//            wanted_nb_samples = 768;
//            if (wanted_nb_samples != last_wanted_nb_samples) {  // 没有这个，输入通道数量不会变
//                last_wanted_nb_samples = wanted_nb_samples;
//                LOGE("wanted_nb_samples != frame->nb_samples %d", wanted_nb_samples);
//                if (swr_set_compensation(swr_context,
//                                         (int) (1.0 * (wanted_nb_samples - frame->nb_samples) *
//                                         audio_dec_ctx->sample_rate / frame->sample_rate),
//                                         wanted_nb_samples *
//                                         audio_dec_ctx->sample_rate / frame->sample_rate) < 0) {
//                    LOGE("swr_set_compensation() failed");
//                    continue;
//                }
//            }
            ret = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
                              (const uint8_t **) frame->data, frame->nb_samples);
            if (ret > 0) {
                openSL.setEnqueueBuffer(out_buffer, (uint32_t) ret * 4);
                pthread_cond_signal(&a_cond);
//                LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
            } else {
                LOGE("swr_convert err = %d", ret);
            }
            pthread_mutex_unlock(&a_mutex);
        }
        av_packet_free(&avPacket);
    }
    av_frame_free(&frame);
    return 0;
}

void ffmpegCallback(void *ptr, int level, const char *fmt, va_list vl) {

    static int print_prefix = 1;
    char line[1024];
    av_log_format_line2(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);
    if (level <= AV_LOG_DEBUG) {
        LOGI("%s", line);
    }
}

void testPlayer(const char *src_filename) {
    int ret = 0;
    AVPacket pkt;
    int out_sample_rate = 0;
    SLConfigure slConfigure = {0};

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        FLOGI("Could not open source file %s", src_filename);
        exit(1);
    }
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        FLOGI("Could not find stream information");
        exit(1);
    }
    if (open_codec_context(src_filename, &video_stream_idx, &video_dec_ctx, fmt_ctx,
                           AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        FLOGI("Video open_codec_context");
    }
    if (open_codec_context(src_filename, &audio_stream_idx, &audio_dec_ctx, fmt_ctx,
                           AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        FLOGI("Audio open_codec_context");
    }
    if (!audio_stream && !video_stream) {
        FLOGI("Could not find audio or video stream in the input, aborting");
        goto end;
    }
    if (video_stream) {
        FLOGI("ffplay -f rawvideo -pix_fmt %s -video_size %d x %d",
              av_get_pix_fmt_name(pix_fmt), width, height);
    }

    if (audio_stream) {
        enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
        FLOGI("ffplay -ac %d -ar %d byte %d fmt_name:%s", audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
              av_get_bytes_per_sample(sfmt), av_get_sample_fmt_name(sfmt));

    }

    pthread_mutex_init(&c_mutex, NULL);
    pthread_mutex_init(&a_mutex, NULL);
    pthread_cond_init(&a_cond, NULL);
    pthread_cond_init(&c_cond, NULL);

    if (audio_stream) {
        out_sample_rate = audio_dec_ctx->sample_rate;
        int s = av_samples_alloc(&out_buffer, NULL,
                                 av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                 out_sample_rate,
                                 AV_SAMPLE_FMT_S16, 1);
        LOGI("out_sample_rate: %d s: %d nb_channels:%d", out_sample_rate, s,
             av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO));
        slConfigure.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        slConfigure.sampleRate = out_sample_rate;
        slConfigure.signSlBufferCallback = slBufferCallback;
        openSL.createPlayer(&slConfigure);
        swr_context = swr_alloc();
        swr_alloc_set_opts(swr_context,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                           audio_dec_ctx->channel_layout, audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate,
                           1, NULL);
        ret = swr_init(swr_context);
        if (ret != 0) {
            LOGE("swr_init failed");
        } else {
            LOGI("swr_init success");
        }
    }

    if (video_stream) {
        sws_context = sws_getContext(
                video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
                video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, NULL, NULL, NULL);
        LOGI("video_dec_ctx width: %d height: %d", video_dec_ctx->width, video_dec_ctx->height);
        if ((ret = av_image_alloc(dst_data, dst_linesize,
                                  video_dec_ctx->width, video_dec_ctx->height,
                                  AV_PIX_FMT_RGBA, 1)) < 0) {
            LOGE("Could not allocate destination image: %d", ret);
        } else {
            LOGI("dst_data size: %d", ret);
        }
    }
    //av_packet_ref
//    init_clock(&videoState.audclk);
    //
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    openSL.play();
    pthread_create(&p_a_tid, 0, audioProcess, 0);
    pthread_create(&p_v_tid, 0, videoProcess, 0);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        decode_packet(&pkt, false);
    }
    decode_packet(&pkt, true);
    /* flush cached frames */
//    avcodec_flush_buffers(audio_dec_ctx);
//    FLOGI("flush cached frames.");
//    decode_packet(&pkt, true);
    FLOGI("Demuxing succeeded.");
    if (audio_stream) {
//        delete[](out_buffer);
        av_freep(&out_buffer);
        swr_free(&swr_context);
    }
    if (video_stream) {
        sws_freeContext(sws_context);
        av_freep(&dst_data[0]);
    }
    end:

    thread_flag = false;
    pthread_join(p_a_tid, 0);
    pthread_join(p_v_tid, 0);

    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);

    openSL.pause();
    openSL.release();
    pthread_cond_destroy(&a_cond);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
    pthread_mutex_destroy(&a_mutex);
}


extern "C" JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_testFF(
        JNIEnv *env,
        jobject, jstring path_, jobject surface) {
//    av_log_set_callback(&ffmpegCallback);

//    jclass classInit = env->GetObjectClass(init);
//    initMethod = env->GetMethodID(classInit, "run", "()V");
//    initObject = env->NewGlobalRef(init);
////    env->CallVoidMethod(runnable, runMethod);
//    jclass classUpdate = env->GetObjectClass(update);
//    updateMethod = env->GetMethodID(classUpdate, "run", "()V");
//    updateObject = env->NewGlobalRef(update);

    mWindow = ANativeWindow_fromSurface(env, surface);

    const char *path = env->GetStringUTFChars(path_, NULL);
    testPlayer(path);
    env->ReleaseStringUTFChars(path_, path);

}


//                if (audio_frame_list.size() > 5) {
//                    LOGE("pthread_cond_wait audio_frame_list %d", audio_frame_list.size());
//                }
//                int nb_samples_count = 0;
//                for (std::list<AVFrame *>::iterator iterator = audio_frame_list.begin();
//                     iterator != audio_frame_list.end(); ++iterator) {
//                    nb_samples_count += (*iterator)->nb_samples;
//                }
//                if (nb_samples_count > audio_dec_ctx->sample_rate * 1.5) { // 音频已经累积了1.5秒的数据，有点多了,低概率
//                    LOGE("nb_samples_count: %d,sample_rate: %d", nb_samples_count, audio_dec_ctx->sample_rate);
//                    // 这里采用丢弃一半的策略，丢弃偶数索引；
//                    int i = 0;
//                    for (std::list<AVFrame *>::iterator iterator = audio_frame_list.begin();
//                         iterator != audio_frame_list.end();) {
//                        i++;
//                        if (i % 2 == 0) {
//                            iterator = audio_frame_list.erase(iterator);
//                            av_frame_free(&(*iterator));
//                        } else {
//                            iterator++;
//                        }
//                    }
//                }

//            if (cache) {
//                int fifo_size = swr_get_out_samples(swr_context, 0);
//                if (fifo_size >= frame->nb_samples) {
//                    res = swr_convert(swr_context, frame->data, wanted_nb_samples, NULL, 0);
//                }
//            }
// $
extern "C"
JNIEXPORT void JNICALL
Java_com_dming_testplayer_gl_TestActivity_startPlay(JNIEnv *env, jobject instance, jstring path_, jobject runnable) {
    const char *path = env->GetStringUTFChars(path_, 0);
    LOGI("path: %s", path);
    jclass classRunnable = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(classRunnable, "run", "()V");
    env->CallVoidMethod(runnable, runMethod);

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
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_2) == JNI_OK) {
        return JNI_VERSION_1_2;
    }
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_1) == JNI_OK) {
        return JNI_VERSION_1_1;
    }
    return JNI_ERR;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
//    jvm = NULL;
    LOGE("JNI_OnUnload");
}
