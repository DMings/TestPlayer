#include <jni.h>
#include <string>
#include "log.h"
#include "Opensl.h"

extern "C" {
#include <libavutil/log.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/samplefmt.h>
}
#define FLOGI(FORMAT, ...) LOGI(FORMAT,##__VA_ARGS__);
#define FLOGE(FORMAT, ...) LOGE(FORMAT,##__VA_ARGS__);
//#define FLOGI(FORMAT, ...) av_log(NULL, AV_LOG_INFO, FORMAT,##__VA_ARGS__);


static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static AVFrame *frame = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
//Opensl opensl;
struct SwrContext *swr_context;
uint8_t *out_buffer;

static int open_codec_context(const char *src_filename, int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                              enum AVMediaType type) {
    int ret, stream_index;
    AVStream *st;
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
        /* find decoder for the stream */
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
        /* Copy codec parameters from input stream to output codec context */
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


static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt) {
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt;
        const char *fmt_be, *fmt_le;
    }
            sample_fmt_entries[] = {
            {AV_SAMPLE_FMT_U8,  "u8",    "u8"},
            {AV_SAMPLE_FMT_S16, "s16be", "s16le"},
            {AV_SAMPLE_FMT_S32, "s32be", "s32le"},
            {AV_SAMPLE_FMT_FLT, "f32be", "f32le"},
            {AV_SAMPLE_FMT_DBL, "f64be", "f64le"},
    };
//    Opensl *sl = new Opensl();
//    sl->createPlayer(44100, 2);
//    sl->release();
//    delete sl;
    *fmt = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    FLOGI("sample format %s is not supported as output format",
          av_get_sample_fmt_name(sample_fmt));
    return -1;
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
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return;
            } else if (ret < 0) {
                LOGE("Error during decoding");
                return;
            }
            int res = -1;
            if (cache) {
                int fifo_size = swr_get_out_samples(swr_context, 0);
                if (fifo_size >= frame->nb_samples) {
                    res = swr_convert(swr_context, frame->data, frame->nb_samples,
                                      NULL, 0);
                }
            } else {
                res = swr_convert(swr_context, &out_buffer, frame->nb_samples,
                                  (const uint8_t **) frame->data, frame->nb_samples);
            }
            if (res >= 0) {
                LOGI("swr_convert = %d", res);
            } else {
                LOGI("swr_convert err = %d", res);
            }
            // 播放音频
//                swr_convert(swr_context, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
//                int size = av_samples_get_buffer_size(NULL, out_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
//            LOGI("nb_samples: %d channels: %d sample_rate: %d", frame->nb_samples, frame->channels,
//                 frame->sample_rate);
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
    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
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
    int n_channels = 0;

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
    if (open_codec_context(src_filename, &video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        FLOGI("Video open_codec_context");
    }
    if (open_codec_context(src_filename, &audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
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
        LOGI(">>>n_channels: %d", n_channels);
//        const char *fmt;
        if (av_sample_fmt_is_planar(sfmt)) {
            const char *packed = av_get_sample_fmt_name(sfmt);
            FLOGI("Warning: the sample format the decoder produced is planar (%s). "
                  "This example will output the first channel only.",
                  packed ? packed : "?");
            sfmt = av_get_packed_sample_fmt(sfmt);
            n_channels = 1;
        }
//        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
//            goto end;
        FLOGI("ffplay -ac %d -ar %d byte %d", n_channels, audio_dec_ctx->sample_rate, av_get_bytes_per_sample(sfmt));
//        FLOGI("ffplay -f %s -ac %d -ar %d byte %d",
//              fmt, n_channels, audio_dec_ctx->sample_rate, av_get_bytes_per_sample(sfmt));
    }
    frame = av_frame_alloc();
    if (!frame) {
        FLOGI("Could not allocate frame");
        goto end;
    }
    if (audio_stream) {
        // 输出的采样率必须与输入相同
        out_sample_rate = audio_dec_ctx->sample_rate;
//    out_buffer = (uint8_t *) av_malloc(static_cast<size_t>(out_sample_rate * out_channel * 2));
        av_samples_alloc(&out_buffer, NULL,
                         n_channels,
//                             av_get_channel_layout_nb_channels(audio_dec_ctx->channel_layout),
                         out_sample_rate,
                         AV_SAMPLE_FMT_S16, 0);
        LOGI("out_sample_rate: %d", out_sample_rate);
//    opensl.createPlayer(out_sample_rate, out_channel);
//    av_get_default_channel_layout(2)
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
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        decode_packet(&pkt, false);
        av_packet_unref(&orig_pkt);
    }
    /* flush cached frames */

    FLOGI("flush cached frames.");
    decode_packet(&pkt, true);
    FLOGI("Demuxing succeeded.");
//    opensl.release();
    if (audio_stream) {
        delete[](out_buffer);
        swr_free(&swr_context);
    }
    end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);


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
