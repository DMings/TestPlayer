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
static int width, height;
static enum AVPixelFormat pix_fmt;
static Opensl opensl;
static struct SwrContext *swr_context;
static uint8_t *out_buffer;
static int wanted_nb_samples;
static pthread_mutex_t c_mutex;
static pthread_cond_t ac_cond;
static pthread_cond_t vc_cond;
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
    LOGI("diff_ms: %f wanted_nb_samples: %d", diff_ms, wanted_nb_samples);
    return wanted_nb_samples;
}

AVFrame *frame = av_frame_alloc();

static void decode_packet(AVPacket *pkt, bool clear_cache) {
    int ret = 0;
    if (clear_cache) {
        pkt->data = NULL;
        pkt->size = 0;
    }
    if (pkt->stream_index == video_stream_idx) {
//        double t = av_gettime_relative();
//        AVPacket *copy_pkg = av_packet_clone(pkt);
//        if (copy_pkg != NULL) {
//            pthread_mutex_lock(&c_mutex);
////            if (video_pkt_list.size() >= 2) {
////                // 就算视频消耗过快，也会被音频卡住，这里音频实际上不是太多
////                pthread_cond_wait(&c_cond, &c_mutex); // 如果是视频解开的，实际上音频还有数据，这里就要处理一下，避免数据过多
////                if (video_pkt_list.size() > 4) {
//                    LOGE("video_pkt_list %d", video_pkt_list.size());
////                }
////                video_pkt_list.push_back(copy_pkg);
////                pthread_cond_signal(&c_cond); // 假如消费过快，就需要通知，有数据到了
////            } else {
//                video_pkt_list.push_back(copy_pkg);
//                pthread_cond_signal(&vc_cond);
////            }
//            pthread_mutex_unlock(&c_mutex);
//        }
//        LOGE("video cost time: %f", (av_gettime_relative() - t) / 1000);
    } else if (pkt->stream_index == audio_stream_idx) {
        double t = av_gettime_relative();
//        AVPacket *copy_pkg = pkt;
//        // 解码
//        ret = avcodec_send_packet(audio_dec_ctx, pkt);
//        if (ret < 0) {
//            LOGE("Error audio sending a packet for decoding");
////            av_frame_free(&frame);
//            return ;
//        }
//        while (ret >= 0) {
//            ret = avcodec_receive_frame(audio_dec_ctx, frame);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                LOGE("ret == AVERROR(EAGAIN) || ret == AVERROR_EOF %d",ret);
////                av_frame_free(&frame);
//                return ;
//            } else if (ret < 0) {
//                LOGE("Error audio during decoding");
////                av_frame_free(&frame);
//                return ;
//            }
//            LOGI("avcodec_receive_frame");
//
//            bool isFeed;
//            pthread_mutex_lock(&c_mutex);
//            isFeed = mustFeed;
//            mustFeed = false;
//            pthread_mutex_unlock(&c_mutex);
//            LOGE("mustFeed %d",isFeed);
//            if (!isFeed) {
//                pthread_mutex_lock(&c_mutex);
//                pthread_cond_wait(&a_cond, &c_mutex); // 等待回调
//                pthread_mutex_unlock(&c_mutex);
//            }
//            // 到这里必须要有sl数据
//            audclk.pts = av_q2d(audio_dec_ctx->time_base) * frame->pts * 1000.0; // ms
//            wanted_nb_samples = synchronize_audio(frame->nb_samples);
//            if (wanted_nb_samples != frame->nb_samples) {  // 没有这个，输入通道数量不会变
//                if (swr_set_compensation(swr_context,
//                                         (wanted_nb_samples - frame->nb_samples) * audio_dec_ctx->sample_rate /
//                                         frame->sample_rate,
//                                         wanted_nb_samples * audio_dec_ctx->sample_rate / frame->sample_rate) < 0) {
//                    LOGE("swr_set_compensation() failed");
//                    continue;
//                }
//            }
//            ret = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
//                              (const uint8_t **) frame->data, frame->nb_samples);
//            if (ret >= 0) {
//                opensl.setEnqueueBuffer(out_buffer, (uint32_t) ret * 4);
//                LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
//            } else {
//                LOGE("swr_convert err = %d", ret);
//            }
////        }
////        av_packet_free(&avPacket);
//        LOGE("audio cost time: %f", (av_gettime_relative() - t) / 1000);
//        }

        AVPacket *copy_pkg = av_packet_clone(pkt);
        if (copy_pkg != NULL) {
            pthread_mutex_lock(&c_mutex);
            if (audio_pkt_list.size() >= 2) {
                // 就算视频消耗过快，也会被音频卡住，这里音频实际上不是太多
                pthread_cond_wait(&ac_cond, &c_mutex); // 如果是视频解开的，实际上音频还有数据，这里就要处理一下，避免数据过多
                if (audio_pkt_list.size() > 4) {
                    LOGE("pthread_cond_wait audio_pkt_list %d", audio_pkt_list.size());
                }
                audio_pkt_list.push_back(copy_pkg);
                pthread_cond_signal(&ac_cond); // 假如消费过快，就需要通知，有数据到了
            } else {
                audio_pkt_list.push_back(copy_pkg);
                pthread_cond_signal(&ac_cond);
            }
            pthread_mutex_unlock(&c_mutex);
        }
        LOGE("audio cost time: %f", (av_gettime_relative() - t) / 1000);
    }
}

static void slBufferCallback() {
    pthread_mutex_lock(&c_mutex);
    mustFeed = true;
    pthread_cond_signal(&a_cond);
    LOGI("slBufferCallback mustFeed")
    pthread_mutex_unlock(&c_mutex);
}

void *videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    AVPacket *avPacket = NULL;
    while (thread_flag) {
        avPacket = NULL;
        pthread_mutex_lock(&c_mutex);
        if (!video_pkt_list.empty()) {
            avPacket = video_pkt_list.front();
            video_pkt_list.pop_front();
//            if (video_pkt_list.size() <= 2 && !video_pkt_list.empty()) {
//                pthread_cond_signal(&c_cond);
//            }
        } else {
//            pthread_cond_signal(&c_cond);
            pthread_cond_wait(&vc_cond, &c_mutex);
            if (!video_pkt_list.empty()) {
                avPacket = video_pkt_list.front();
                video_pkt_list.pop_front();
            }
        }
        pthread_mutex_unlock(&c_mutex);

        if (avPacket == NULL) {
            continue;
        }

        // 解码
        ret = avcodec_send_packet(video_dec_ctx, avPacket);
        if (ret < 0) {
            LOGE("Error video sending a packet for decoding");
            continue;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(video_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                LOGE("Error video during decoding");
                continue;
            }
            LOGI("video show->");
            av_usleep(5000); // us
        }
        av_packet_free(&avPacket);
    }
    av_frame_free(&frame);
    return 0;
}

void *audioProcess(void *arg) {
    int ret = 0;
    AVPacket *avPacket = NULL;
    AVFrame *frame = av_frame_alloc();
    while (thread_flag) {
        avPacket = NULL;
        pthread_mutex_lock(&c_mutex);
        if (!audio_pkt_list.empty()) {
            avPacket = audio_pkt_list.front();
            audio_pkt_list.pop_front();
            pthread_cond_signal(&ac_cond);
        } else {
            pthread_cond_signal(&ac_cond);
            pthread_cond_wait(&ac_cond, &c_mutex);
            if (!audio_pkt_list.empty()) {
                avPacket = audio_pkt_list.front();
                audio_pkt_list.pop_front();
            }
        }
        pthread_mutex_unlock(&c_mutex);

        if (avPacket == NULL) {
            continue;
        }
        double t = av_gettime_relative();
        // 解码
        ret = avcodec_send_packet(audio_dec_ctx, avPacket);
        if (ret < 0) {
            LOGE("Error audio sending a packet for decoding");
            av_frame_free(&frame);
            return NULL;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                LOGE("ret == AVERROR(EAGAIN) || ret == AVERROR_EOF");
                break;
            } else if (ret < 0) {
                LOGE("Error audio during decoding");
                av_frame_free(&frame);
                return NULL;
            }
            bool isFeed;
            pthread_mutex_lock(&c_mutex);
            isFeed = mustFeed;
            mustFeed = false;
            pthread_mutex_unlock(&c_mutex);
            LOGE("mustFeed %d",isFeed);
            if (!isFeed) {
                pthread_mutex_lock(&c_mutex);
                pthread_cond_wait(&a_cond, &c_mutex); // 等待回调
                pthread_mutex_unlock(&c_mutex);
            }
            // 到这里必须要有sl数据
            audclk.pts = av_q2d(audio_dec_ctx->time_base) * frame->pts * 1000.0; // ms
            wanted_nb_samples = frame->nb_samples;
//            wanted_nb_samples = synchronize_audio(frame->nb_samples);
            if (wanted_nb_samples != frame->nb_samples) {  // 没有这个，输入通道数量不会变
                if (swr_set_compensation(swr_context,
                                         (wanted_nb_samples - frame->nb_samples) * audio_dec_ctx->sample_rate /
                                         frame->sample_rate,
                                         wanted_nb_samples * audio_dec_ctx->sample_rate / frame->sample_rate) < 0) {
                    LOGE("swr_set_compensation() failed");
                    continue;
                }
            }
            ret = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
                              (const uint8_t **) frame->data, frame->nb_samples);
            if (ret >= 0) {
                opensl.setEnqueueBuffer(out_buffer, (uint32_t) ret * 4);
                LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
            } else {
                LOGE("swr_convert err = %d", ret);
            }
        }
        av_packet_free(&avPacket);
        LOGE("audio cost time: %f", (av_gettime_relative() - t) / 1000);
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
        FLOGI("ffplay -ac %d -ar %d byte %d", audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
              av_get_bytes_per_sample(sfmt));
    }

    pthread_mutex_init(&c_mutex, NULL);
    pthread_cond_init(&a_cond, NULL);
    pthread_cond_init(&vc_cond, NULL);
    pthread_cond_init(&ac_cond, NULL);

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
        slConfigure.signSlBufferCallback = slBufferCallback;
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
    //av_packet_ref
//    init_clock(&videoState.audclk);
    //
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    opensl.play();
    pthread_create(&p_a_tid, 0, audioProcess, 0);
    pthread_create(&p_v_tid, 0, videoProcess, 0);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        decode_packet(&pkt, false);
    }
    /* flush cached frames */
//    avcodec_flush_buffers(audio_dec_ctx);
//    FLOGI("flush cached frames.");
//    decode_packet(&pkt, true);
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
    pthread_cond_destroy(&a_cond);
    pthread_cond_destroy(&vc_cond);
    pthread_cond_destroy(&ac_cond);
    pthread_mutex_destroy(&c_mutex);
    thread_flag = false;
    pthread_join(p_a_tid, 0);
    pthread_join(p_v_tid, 0);
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