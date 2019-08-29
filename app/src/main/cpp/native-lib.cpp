#include <jni.h>
#include <string>
#include "log.h"
#include "Opensl.h"
#include <list>
#include <pthread.h>

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
#define FLOGE(FORMAT, ...) LOGE(FORMAT,##__VA_ARGS__);
//#define FLOGI(FORMAT, ...) av_log(NULL, AV_LOG_INFO, FORMAT,##__VA_ARGS__);

#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AV_NOSYNC_THRESHOLD 10.0
#define AUDIO_DIFF_AVG_NB   20

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};


typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
//    double last_updated;
    double first_time;
} Clock;


static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVStream *video_stream = NULL, *audio_stream = NULL;
//static AVFrame* frame = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static Opensl opensl;
static struct SwrContext *swr_context;
static uint8_t *out_buffer;
static int wanted_nb_samples;
static pthread_mutex_t f_mutex;
static pthread_cond_t f_cond;
//static std::queue<AVFrame *> audio_queue;
static pthread_t p_tid;
static bool thread_flag = true;
static Clock audclk;
static Clock extclk;
static std::list<AVFrame *> audio_list;

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
    }
    return 0;
}

//static void set_clock(Clock *c, double pts) {
//    c->pts = pts;
////    c->last_updated = av_gettime_relative() / 1000000.0; // ms
//}

static double get_master_clock() {
    double time = av_gettime_relative() / 1000.0;
    if (extclk.first_time == 0) {
        extclk.first_time = time;
    }
    return extclk.pts_drift + time - extclk.first_time; // ms
}

static double get_audio_clock() {
    return audclk.pts_drift + audclk.pts; // ms
}

static int synchronize_audio(int nb_samples) {
    int wanted_nb_samples;
    double diff_ms;
    double nb_time_ms;
//    LOGI("get_audio_clock: %f get_master_clock: %f nb_samples: %d", get_audio_clock(), get_master_clock(), nb_samples);
    diff_ms = get_audio_clock() - get_master_clock();
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
//        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
//            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
//            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
//                /* not enough measures to have a correct estimate */
//                is->audio_diff_avg_count++;
//            } else {
//                /* estimate the A-V difference */
//                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
//
//                if (fabs(avg_diff) >= is->audio_diff_threshold) {
//                    wanted_nb_samples = nb_samples + (int) (diff * is->freq);
//                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
//                }
//                LOGI("diff=%f adiff=%f sample_diff=%d apts=%0.3f %f",
//                     diff, avg_diff, wanted_nb_samples - nb_samples,
//                     is->audio_clock, is->audio_diff_threshold);
//            }
//        } else {
//            /* too big difference : may be initial PTS errors, so
//               reset A-V filter */
//            is->audio_diff_avg_count = 0;
//            is->audio_diff_cum = 0;
//        }
    return wanted_nb_samples;
}


static void decode_packet(AVPacket *pkt, bool clear_cache) {
    int ret = 0;
    if (clear_cache) {
        pkt->data = NULL;
        pkt->size = 0;
    }
    if (pkt->stream_index == video_stream_idx) {
        av_usleep(10000); // us
        pthread_cond_signal(&f_cond);


//        // 解码
//        ret = avcodec_send_packet(video_dec_ctx, pkt);
//        if (ret < 0) {
//            LOGE("Error sending a packet for decoding");
//            return;
//        }
//        while (ret >= 0) {
//            AVFrame *frame = av_frame_alloc();
//            ret = avcodec_receive_frame(video_dec_ctx, frame);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                av_frame_free(&frame);
//                return;
//            } else if (ret < 0) {
//                LOGE("Error during decoding");
//                av_frame_free(&frame);
//                return;
//            }
//            av_usleep(10000); // us
//            av_frame_free(&frame);
//        }
    } else if (pkt->stream_index == audio_stream_idx) {
        // 解码
        ret = avcodec_send_packet(audio_dec_ctx, pkt);
        if (ret < 0) {
            LOGE("Error sending a packet for decoding");
            return;
        }
        while (ret >= 0) {
            AVFrame *frame = av_frame_alloc();
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frame);
                return;
            } else if (ret < 0) {
                LOGE("Error during decoding");
                av_frame_free(&frame);
                return;
            }
            pthread_mutex_lock(&f_mutex);
            if (audio_list.size() > 3) {
                pthread_cond_wait(&f_cond, &f_mutex); // 如果是视频解开的，实际上音频还有数据，这里就要处理一下，避免数据过多
//                LOGI("pthread wait push")

                int nb_samples_count = 0;
                for (std::list<AVFrame *>::iterator iterator = audio_list.begin();
                     iterator != audio_list.end(); ++iterator) {
                    nb_samples_count += (*iterator)->nb_samples;
                }
                if (nb_samples_count > audio_dec_ctx->sample_rate * 2.5) { // 音频已经累积了2.5秒的数据，有点多了
                    LOGI("nb_samples_count: %d,sample_rate: %d", nb_samples_count, audio_dec_ctx->sample_rate);
                    // 这里采用丢弃一半的策略，丢弃偶数索引；
                    int i = 0;
                    for (std::list<AVFrame *>::iterator iterator = audio_list.begin();
                         iterator != audio_list.end(); ++iterator) {
                        i++;
                        if (i % 2 == 0) {
                            audio_list.erase(iterator);
                            av_frame_free(&(*iterator));
                        }
                    }
                }
                audio_list.push_back(frame);
                pthread_cond_signal(&f_cond); // 假如消费过快，就需要通知，有数据到了
            } else {
                audio_list.push_back(frame);
                pthread_cond_signal(&f_cond);
//                LOGI("pthread push")
            }
            pthread_mutex_unlock(&f_mutex);
        }
    }
}

static void slBufferCallback(uint8_t **buffer, uint32_t *bufferSize) {
    int ret = 0;
    AVFrame *frame = NULL;

    pthread_mutex_lock(&f_mutex);
    if (!audio_list.empty()) {
        frame = audio_list.front();
        audio_list.pop_front();
        pthread_cond_signal(&f_cond);
    }
    if (audio_list.empty()) {
        pthread_cond_wait(&f_cond, &f_mutex);
        if (!audio_list.empty()) {
            frame = audio_list.front();
            audio_list.pop_front();
        }
    }
    pthread_mutex_unlock(&f_mutex);

    if (frame == NULL) {
        *bufferSize = 0;
        LOGE("frame null !!!");
        return;
    }

    audclk.pts = av_q2d(audio_dec_ctx->time_base) * frame->pts * 1000.0; // ms
    wanted_nb_samples = synchronize_audio(frame->nb_samples);
    ret = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
                      (const uint8_t **) frame->data, frame->nb_samples);
    av_frame_free(&frame);
//            if (cache) {
//                int fifo_size = swr_get_out_samples(swr_context, 0);
//                if (fifo_size >= frame->nb_samples) {
//                    res = swr_convert(swr_context, frame->data, wanted_nb_samples, NULL, 0);
//                }
//            }
    if (ret >= 0) {
        *bufferSize = (uint32_t) ret * 4;
        *buffer = out_buffer;
//        LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
    } else {
        *bufferSize = 0;
        LOGE("swr_convert err = %d", ret);
    }
    return;
}

//void *process(void *arg) {
//    opensl.play();
//    while (thread_flag){
//
//    }
//    opensl.pause();
//    opensl.release();
//    return 0;
//}

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
    SLConfigure slConfigure;

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
        FLOGI("ffplay -ac %d -ar %d byte %d", audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
              av_get_bytes_per_sample(sfmt));
    }

    pthread_mutex_init(&f_mutex, NULL);
    pthread_cond_init(&f_cond, NULL);

    if (audio_stream) {
        out_sample_rate = audio_dec_ctx->sample_rate;
//    out_buffer = (uint8_t *) av_malloc(static_cast<size_t>(out_sample_rate * out_channel * 2));
        int s = av_samples_alloc(&out_buffer, NULL,
                                 av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                 out_sample_rate,
                                 AV_SAMPLE_FMT_S16, 0);
        LOGI("out_sample_rate: %d s: %d", out_sample_rate, s);
        slConfigure.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        slConfigure.sampleRate = out_sample_rate;
        slConfigure.slBufferCallback = slBufferCallback;
        opensl.createPlayer(&slConfigure);
        swr_context = swr_alloc();
        swr_alloc_set_opts(swr_context,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                           audio_dec_ctx->channel_layout, audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate,
                           0, NULL);
        ret = swr_init(swr_context);
        if (ret != 0) {
            LOGE("swr_init failed");
        } else {
            LOGI("swr_init success");
        }
    }
    //
//    init_clock(&videoState.audclk);
    //
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    opensl.play();
//    pthread_create(&p_tid, 0, process, 0);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        decode_packet(&pkt, false);
        av_packet_unref(&orig_pkt);
    }
    /* flush cached frames */
//    avcodec_flush_buffers(audio_dec_ctx);
    FLOGI("flush cached frames.");
    decode_packet(&pkt, true);
    FLOGI("Demuxing succeeded.");
    if (audio_stream) {
        delete[](out_buffer);
        swr_free(&swr_context);
    }
    end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);

    opensl.pause();
    opensl.release();
    pthread_cond_destroy(&f_cond);
    pthread_mutex_destroy(&f_mutex);
    thread_flag = false;
//    pthread_join(p_tid, 0);
}


extern "C" JNIEXPORT void JNICALL
Java_com_dming_testplayer_MainActivity_testFF(
        JNIEnv *env,
        jobject, jstring path_) {
//    av_log_set_callback(&ffmpegCallback);
    const char *path = env->GetStringUTFChars(path_, NULL);
    testPlayer(path);
    env->ReleaseStringUTFChars(path_, path);

}
