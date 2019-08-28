#include <jni.h>
#include <string>
#include "log.h"
#include "Opensl.h"
#include <queue>
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

typedef struct VideoState {

//    Clock audclk;
////    Clock vidclk;
//    Clock extclk;

    int audio_stream;

    int av_sync_type;

    double audio_clock = NAN;
//    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    AVStream *video_st;
    //
    int channels;
    enum AVSampleFormat fmt;
    int freq;
} VideoState;

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
static VideoState videoState;
static int wanted_nb_samples;
static int64_t audio_callback_time;
static pthread_mutex_t f_mutex;
static pthread_cond_t f_cond;
static std::queue<AVFrame *> queue;
static pthread_t p_tid;
static bool thread_flag = true;
static Clock audclk;
static Clock extclk;

static int open_codec_context(const char *src_filename, int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                              AVStream *st,
                              enum AVMediaType type) {
    int ret, stream_index;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
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
//    LOGI("get_audio_clock: %f get_master_clock: %f", get_audio_clock(), get_master_clock());
    diff_ms = get_audio_clock() - get_master_clock();
    nb_time_ms = 1.0 * nb_samples / audio_dec_ctx->sample_rate * 1000; // ms
    if (fabs(diff_ms) > nb_time_ms * 0.2) { // 超过阀值
        if (diff_ms < 0) { // 音频落后时间
            if (-diff_ms < nb_time_ms) { // 音频比较迟，要丢帧追上
                wanted_nb_samples = (int) (( 1 + diff_ms / nb_time_ms) * nb_samples);
            } else {
                wanted_nb_samples = 0;
            }
//            LOGI("diff_ms: %f nb_time_ms: %f", diff_ms, nb_time_ms);
//            LOGI("wanted_nb_samples: %d diff_ms / nb_time_ms: %.6f,nb_samples: %d", wanted_nb_samples,(-diff_ms / nb_time_ms),nb_samples);
            if (wanted_nb_samples < 0.2 * nb_samples) {  // 给上阀值，不能少于某一个采样率
                wanted_nb_samples = (int) (0.2 * nb_samples);
            }
        } else { // 音频超过时间
            if (diff_ms < nb_time_ms) { // 音频比较快，要增加帧，变慢
                wanted_nb_samples = (int) (diff_ms / nb_time_ms * nb_samples + nb_samples);
            } else {
                wanted_nb_samples = nb_samples + nb_samples;
            }
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


static void decode_packet(AVPacket *pkt, bool cache) {
    int ret = 0;
    if (cache) {
        pkt->data = NULL;
        pkt->size = 0;
    }
//    int decoded = pkt.size;
//    if (pkt.stream_index == video_stream_idx && false) {
////        avcodec_send_packet(video_dec_ctx,&pkt);
////        avcodec_receive_frame(video_dec_ctx,frame);
//        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
//        if (ret < 0) {
//            FLOGI("Error decoding video frame (%s)\n", av_err2str(ret));
//            return ret;
//        }
//        if (*got_frame) {
//            if (frame->width != width || frame->height != height ||
//                frame->format != pix_fmt) {
//                /* To handle this change, one could call av_image_alloc again and
//                 * decode the following frames into another rawvideo file. */
//                FLOGE("Error: Width, height and pixel format have to be "
//                      "constant in a rawvideo file, but the width, height or "
//                      "pixel format of the input video changed:\n"
//                      "old: width = %d, height = %d, format = %s\n"
//                      "new: width = %d, height = %d, format = %s\n",
//                      width, height, av_get_pix_fmt_name(pix_fmt),
//                      frame->width, frame->height,
//                      av_get_pix_fmt_name((AVPixelFormat) frame->format));
//                return -1;
//            }
//            FLOGI("video_frame%s n:%d coded_n:%d\n",
//                  cached ? "(cached)" : "",
//                  video_frame_count++, frame->coded_picture_number);
//            /* copy decoded frame to destination buffer:
//             * this is required since rawvideo expects non aligned data */
////            av_image_copy(video_dst_data, video_dst_linesize,
////                          (const uint8_t **)(frame->data), frame->linesize,
////                          pix_fmt, width, height);
//            /* write to rawvideo file */
//        }
//    } else
    if (pkt->stream_index == audio_stream_idx) {
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
            if (queue.size() > 3) {
                pthread_cond_wait(&f_cond, &f_mutex);
//                LOGI("pthread wait push")
                queue.push(frame);
                pthread_cond_signal(&f_cond);
            } else {
                queue.push(frame);
                pthread_cond_signal(&f_cond);
//                LOGI("pthread push")
            }
            pthread_mutex_unlock(&f_mutex);
        }

//        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
//        if (ret < 0) {
//            FLOGE("Error decoding audio frame (%s)\n", av_err2str(ret));
//            return ret;
//        }
//        /* Some audio decoders decode only part of the packet, and have to be
//         * called again with the remainder of the packet data.
//         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
//         * Also, some decoders might over-read the packet. */
//        decoded = FFMIN(ret, pkt.size);
//        if (*got_frame) {
////            int unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat) frame->format);
////            FLOGI("audio_frame%s n:%d nb_samples:%d pts:%s\n",
////                  cached ? "(cached)" : "",
////                  audio_frame_count++, frame->nb_samples,
////                  av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
//
////            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
//        }
    }
}

static void **slBufferCallback() {
    int ret = 0;
    pthread_mutex_lock(&f_mutex);
    AVFrame *frame = NULL;
    uint32_t bufferSize;
    void *result[2];
    if (!queue.empty()) {
        frame = queue.front();
        queue.pop();
        pthread_cond_signal(&f_cond);
    }
    if (queue.empty()) {
        pthread_cond_wait(&f_cond, &f_mutex);
        if (!queue.empty()) {
            frame = queue.front();
            queue.pop();
        }
    }

    if (frame == NULL) {
        bufferSize = 0;
        result[0] = &bufferSize;
        LOGE("frame null !!!");
        return result;
    }

    pthread_mutex_unlock(&f_mutex);

    audclk.pts = av_q2d(audio_dec_ctx->time_base) * frame->pts * 1000; // ms
    wanted_nb_samples = synchronize_audio(frame->nb_samples);
//            int out_count = (int64_t) wanted_nb_samples * is->freq / frame->sample_rate + 256;
//            int out_size = av_samples_get_buffer_size(NULL, is->channels, out_count, is->fmt, 0);
//    int delay = 0;
//    if (wanted_nb_samples != frame->nb_samples) {
//        ret = swr_set_compensation(swr_context,
//                                   (wanted_nb_samples - frame->nb_samples) * is->freq / frame->sample_rate,
//                                   wanted_nb_samples * is->freq / frame->sample_rate);
//        if (delay < 0) {
//            LOGE("swr_set_compensation() failed");
//            bufferSize = 0;
//            result[0] = &bufferSize;
//            return result;
//        } else {
//            delay = ret;
//        }
//    }
    ret = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
                      (const uint8_t **) frame->data, frame->nb_samples);
//            len2 = swr_convert(swr_context, out, out_count, frame->data, frame->nb_samples);
//            if (cache) {
//                int fifo_size = swr_get_out_samples(swr_context, 0);
//                if (fifo_size >= frame->nb_samples) {
//                    res = swr_convert(swr_context, frame->data, wanted_nb_samples, NULL, 0);
//                }
//            } else {
//                res = swr_convert(swr_context, &out_buffer, wanted_nb_samples,
//                                  (const uint8_t **) frame->data, frame->nb_samples);
//            }
    if (ret >= 0) {
        LOGI("swr_convert len: %d wanted_nb_samples: %d", ret,wanted_nb_samples);
    } else {
        LOGE("swr_convert err = %d", ret);
    }
//    audio_callback_time = av_gettime_relative();
////            AVRational tb = (AVRational) {1, frame->sample_rate};
////            av_rescale_q(frame->pts, audio_dec_ctx->pkt_timebase, tb);
//    double pts = av_q2d(audio_dec_ctx->pkt_timebase) * frame->pts;
//    double costTime = (double) 1000000.0 * wanted_nb_samples / frame->sample_rate;
//
//    LOGI("costTime: %f wanted_nb_samples: %d len: %d delay: %d pts: %f frame->pts: %d", costTime,
//         wanted_nb_samples, ret,
//         delay, pts, frame->pts);
//    av_usleep((unsigned int) costTime + 10000);
//
//    if (isnan(is->audio_clock)) {
//        is->audio_clock = pts + costTime;
//    }
    // 播放音频
//                swr_convert(swr_context, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
//                int size = av_samples_get_buffer_size(NULL, out_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
//            LOGI("nb_samples: %d channels: %d sample_rate: %d", frame->nb_samples, frame->channels,
//                 frame->sample_rate);
    av_frame_free(&frame);
    bufferSize = (uint32_t) ret * 4;
    result[0] = &bufferSize;
    result[1] = out_buffer;
    return result;
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
    int n_channels = 0;
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
                           videoState.video_st,
                           AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        FLOGI("Video open_codec_context");
    }
    if (open_codec_context(src_filename, &audio_stream_idx, &audio_dec_ctx, fmt_ctx,
                           videoState.audio_st,
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
        n_channels = audio_dec_ctx->channels;
//        LOGI(">>>n_channels: %d", n_channels);
//        const char *fmt;
//        if (av_sample_fmt_is_planar(sfmt)) {
//            const char *packed = av_get_sample_fmt_name(sfmt);
//            FLOGI("Warning: the sample format the decoder produced is planar (%s). "
//                  "This example will output the first channel only.",
//                  packed ? packed : "?");
//            sfmt = av_get_packed_sample_fmt(sfmt);
//            n_channels = 1;
//        }
//        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
//            goto end;
        FLOGI("ffplay -ac %d -ar %d byte %d", n_channels, audio_dec_ctx->sample_rate, av_get_bytes_per_sample(sfmt));
//        FLOGI("ffplay -f %s -ac %d -ar %d byte %d",
//              fmt, n_channels, audio_dec_ctx->sample_rate, av_get_bytes_per_sample(sfmt));
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
        videoState.audio_diff_threshold = out_sample_rate * 0.05;
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
    videoState.channels = n_channels;
    videoState.freq = out_sample_rate;
    videoState.fmt = AV_SAMPLE_FMT_S16;
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
