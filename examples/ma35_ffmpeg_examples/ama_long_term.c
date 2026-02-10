/*
 * AMD AMA Long-term Looped Playback Example
 *
 * This example demonstrates continuous looped video decoding and scaling using
 * AMD AMA hardware. When the input reaches EOF, it automatically seeks back to
 * the beginning and continues decoding, adjusting timestamps for seamless playback.
 *
 * Key concepts demonstrated:
 * - Hardware device initialization for AMD AMA
 * - Hardware-accelerated H.264 decoding
 * - Hardware video scaling using scaler_ama filter
 * - Looped playback with timestamp adjustment
 * - Long-term continuous processing
 *
 * Usage:
 *   ./ama_long_term [options] input_file
 *
 * Options:
 *   -d <level>  Set debug level (quiet, panic, fatal, error, warning, info, verbose, debug, trace)
 *   -s <size>   Scale output size WxH (e.g., 1280x720) [default: 1280x720]
 *   -l <count>  Number of loops (0 = infinite) [default: 0]
 *   -h          Show this help message
 *
 * Example:
 *   ./ama_long_term -d info -s 1920x1080 -l 5 input.mp4
 *
 * Requirements:
 *   - AMD AMA hardware and drivers
 *   - FFmpeg built with AMA support
 *   - VPI libraries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/log.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
#include <libavutil/time.h>
#include "vpi_api.h"

/* Default configuration */
static const char *DEFAULT_DEVICE = "/dev/ama_transcoder0";
static const char *DEFAULT_SCALE_SIZE = "1280x720";

/* Global flag for signal handling */
static volatile int keep_running = 1;

/* Application context structure */
typedef struct {
    /* Hardware context */
    AVBufferRef *hw_device_ctx;
    
    /* Input/Output contexts */
    AVFormatContext *fmt_ctx;
    AVCodecContext *dec_ctx;
    int video_stream_index;
    
    /* Filter graph contexts */
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
    
    /* Configuration */
    const char *input_file;
    const char *scale_size;
    const char *device_name;
    int log_level;
    int loop_count;  /* 0 = infinite */
    
    /* Loop management */
    int64_t base_time;
    int64_t last_packet_pts;
    int32_t last_packet_duration;
    int current_loop;
    
    /* Statistics */
    int64_t frames_decoded;
    int64_t frames_scaled;
} AppContext;

/* Function prototypes */
static void print_usage(const char *prog_name);
static int parse_arguments(int argc, char **argv, AppContext *ctx);
static void signal_handler(int sig);
static int init_device(AppContext *ctx);
static int init_input(AppContext *ctx);
static int init_decoder(AppContext *ctx);
static int init_filters(AppContext *ctx);
static int decode_packet(AppContext *ctx, AVPacket *packet);
static int filter_frame(AppContext *ctx, AVFrame *frame);
static void unref_frame(AVFrame *frame);
static void cleanup(AppContext *ctx);
static enum AVPixelFormat get_hw_format(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts);

/* Print usage information */
static void print_usage(const char *prog_name)
{
    printf("AMD AMA Long-term Looped Playback Example\n\n");
    printf("Usage: %s [options] input_file\n\n", prog_name);
    printf("Options:\n");
    printf("  -d <level>  Set debug level (quiet, panic, fatal, error, warning, info, verbose, debug, trace)\n");
    printf("  -s <size>   Scale output size WxH (e.g., 1280x720) [default: %s]\n", DEFAULT_SCALE_SIZE);
    printf("  -l <count>  Number of loops (0 = infinite) [default: 0]\n");
    printf("  -D <device> Device name [default: %s]\n", DEFAULT_DEVICE);
    printf("  -h          Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -d info -s 1920x1080 -l 5 input.mp4\n", prog_name);
    printf("\nPress Ctrl+C to stop playback\n");
}

/* Parse command line arguments */
static int parse_arguments(int argc, char **argv, AppContext *ctx)
{
    int opt;
    
    /* Set defaults */
    ctx->scale_size = DEFAULT_SCALE_SIZE;
    ctx->device_name = DEFAULT_DEVICE;
    ctx->log_level = AV_LOG_INFO;
    ctx->loop_count = 0;  /* Infinite by default */
    
    while ((opt = getopt(argc, argv, "d:s:l:D:h")) != -1) {
        switch (opt) {
            case 'd':
                if (strcmp(optarg, "quiet") == 0) ctx->log_level = AV_LOG_QUIET;
                else if (strcmp(optarg, "panic") == 0) ctx->log_level = AV_LOG_PANIC;
                else if (strcmp(optarg, "fatal") == 0) ctx->log_level = AV_LOG_FATAL;
                else if (strcmp(optarg, "error") == 0) ctx->log_level = AV_LOG_ERROR;
                else if (strcmp(optarg, "warning") == 0) ctx->log_level = AV_LOG_WARNING;
                else if (strcmp(optarg, "info") == 0) ctx->log_level = AV_LOG_INFO;
                else if (strcmp(optarg, "verbose") == 0) ctx->log_level = AV_LOG_VERBOSE;
                else if (strcmp(optarg, "debug") == 0) ctx->log_level = AV_LOG_DEBUG;
                else if (strcmp(optarg, "trace") == 0) ctx->log_level = AV_LOG_TRACE;
                else {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return -1;
                }
                break;
            case 's':
                ctx->scale_size = optarg;
                break;
            case 'l':
                ctx->loop_count = atoi(optarg);
                if (ctx->loop_count < 0) {
                    fprintf(stderr, "Invalid loop count: %s\n", optarg);
                    return -1;
                }
                break;
            case 'D':
                ctx->device_name = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n\n");
        print_usage(argv[0]);
        return -1;
    }
    
    ctx->input_file = argv[optind];
    return 0;
}

/* Signal handler for graceful shutdown */
static void signal_handler(int sig)
{
    av_log(NULL, AV_LOG_INFO, "\nReceived signal %d, stopping...\n", sig);
    keep_running = 0;
}

/* Hardware format callback for decoder */
static enum AVPixelFormat get_hw_format(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    AppContext *ctx = avctx->opaque;
    if (ctx->hw_device_ctx)
        return AV_PIX_FMT_VPE;
    else
        return AV_PIX_FMT_NONE;
}

/* Initialize AMD AMA hardware device */
static int init_device(AppContext *ctx)
{
    int ret;
    AVDictionary *opts = NULL;
    
    av_log(NULL, AV_LOG_INFO, "Initializing AMD AMA device: %s\n", ctx->device_name);
    
    av_dict_set(&opts, "priority", "vod", 0);
    ret = av_hwdevice_ctx_create(&ctx->hw_device_ctx, AV_HWDEVICE_TYPE_AMA, 
                                  ctx->device_name, opts, 0);
    av_dict_free(&opts);
    
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to create AMA device. Error: %s\n", av_err2str(ret));
        return ret;
    }
    
    av_log(NULL, AV_LOG_INFO, "AMA device initialized successfully\n");
    return 0;
}

/* Open and analyze input file */
static int init_input(AppContext *ctx)
{
    int ret;
    
    av_log(NULL, AV_LOG_INFO, "Opening input file: %s\n", ctx->input_file);
    
    if ((ret = avformat_open_input(&ctx->fmt_ctx, ctx->input_file, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file: %s\n", av_err2str(ret));
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(ctx->fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information: %s\n", av_err2str(ret));
        return ret;
    }
    
    /* Find video stream */
    ctx->video_stream_index = -1;
    for (unsigned int i = 0; i < ctx->fmt_ctx->nb_streams; i++) {
        if (ctx->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            ctx->video_stream_index = i;
            av_log(NULL, AV_LOG_INFO, "Found video stream at index %d\n", i);
            break;
        }
    }
    
    if (ctx->video_stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "No video stream found in input file\n");
        return AVERROR_STREAM_NOT_FOUND;
    }
    
    /* Print stream information */
    AVStream *stream = ctx->fmt_ctx->streams[ctx->video_stream_index];
    av_log(NULL, AV_LOG_INFO, "Video stream: %dx%d, duration: %ld\n",
           stream->codecpar->width, stream->codecpar->height,
           stream->duration);
    
    return 0;
}

/* Initialize hardware decoder */
static int init_decoder(AppContext *ctx)
{
    int ret;
    const AVCodec *decoder;
    AVStream *stream;
    AVDictionary *opts = NULL;
    
    av_log(NULL, AV_LOG_INFO, "Initializing h264_ama hardware decoder\n");
    
    decoder = avcodec_find_decoder_by_name("h264_ama");
    if (!decoder) {
        av_log(NULL, AV_LOG_ERROR, "h264_ama decoder not found\n");
        return AVERROR_DECODER_NOT_FOUND;
    }
    
    stream = ctx->fmt_ctx->streams[ctx->video_stream_index];
    ctx->dec_ctx = avcodec_alloc_context3(decoder);
    if (!ctx->dec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }
    
    /* Copy codec parameters from input stream */
    if ((ret = avcodec_parameters_to_context(ctx->dec_ctx, stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters: %s\n", av_err2str(ret));
        return ret;
    }
    
    /* Configure hardware decoding */
    ctx->dec_ctx->hw_device_ctx = av_buffer_ref(ctx->hw_device_ctx);
    ctx->dec_ctx->get_format = get_hw_format;
    ctx->dec_ctx->opaque = ctx;
    ctx->dec_ctx->framerate = av_guess_frame_rate(ctx->fmt_ctx, stream, NULL);
    
    /* Set decoder options */
    av_dict_set(&opts, "transcode", "0", 0);
    av_dict_set(&opts, "out_fmt", "nv12", 0); /* Request linear format output */
    
    if ((ret = avcodec_open2(ctx->dec_ctx, decoder, &opts)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder: %s\n", av_err2str(ret));
        av_dict_free(&opts);
        return ret;
    }
    
    av_dict_free(&opts);
    
    av_log(NULL, AV_LOG_INFO, "Decoder initialized: %dx%d\n", 
           ctx->dec_ctx->width, ctx->dec_ctx->height);
    
    return 0;
}

/* Initialize filter graph for hardware scaling */
static int init_filters(AppContext *ctx)
{
    char args[512];
    char filter_desc[1024];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVRational time_base = ctx->fmt_ctx->streams[ctx->video_stream_index]->time_base;
    
    av_log(NULL, AV_LOG_INFO, "Initializing filter graph for scaling to %s\n", ctx->scale_size);
    
    ctx->filter_graph = avfilter_graph_alloc();
    
    if (!outputs || !inputs || !ctx->filter_graph) {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate filter graph resources\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* Get buffer source and sink filters */
    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");
    if (!buffersrc || !buffersink) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find buffer filters\n");
        ret = AVERROR_FILTER_NOT_FOUND;
        goto end;
    }
    
    /* Configure buffer source with VPE format */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ctx->dec_ctx->width, ctx->dec_ctx->height,
             AV_PIX_FMT_VPE,
             time_base.num, time_base.den,
             ctx->dec_ctx->sample_aspect_ratio.num,
             ctx->dec_ctx->sample_aspect_ratio.den);
    
    ret = avfilter_graph_create_filter(&ctx->buffersrc_ctx, buffersrc, "in",
                                        args, NULL, ctx->filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source: %s\n", av_err2str(ret));
        goto end;
    }
    
    /* Set hardware frames context */
    if (ctx->dec_ctx->hw_frames_ctx) {
        AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
        if (par) {
            par->hw_frames_ctx = av_buffer_ref(ctx->dec_ctx->hw_frames_ctx);
            ret = av_buffersrc_parameters_set(ctx->buffersrc_ctx, par);
            av_free(par);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot set hw_frames_ctx: %s\n", av_err2str(ret));
                goto end;
            }
        }
    }
    
    /* Create buffer sink */
    ret = avfilter_graph_create_filter(&ctx->buffersink_ctx, buffersink, "out",
                                        NULL, NULL, ctx->filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink: %s\n", av_err2str(ret));
        goto end;
    }
    
    /* Keep VPE format for output (hardware to hardware scaling) */
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_VPE, AV_PIX_FMT_NONE};
    av_opt_set_int_list(ctx->buffersink_ctx, "pix_fmts", pix_fmts,
                        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    /* Build filter description for scaler_ama */
    snprintf(filter_desc, sizeof(filter_desc),
             "scaler_ama=outputs=1:out_res=(%s)",
             ctx->scale_size);
    
    av_log(NULL, AV_LOG_INFO, "Filter graph: %s\n", filter_desc);
    
    /* Configure filter graph connections */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = ctx->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    
    inputs->name = av_strdup("out");
    inputs->filter_ctx = ctx->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(ctx->filter_graph, filter_desc,
                                         &inputs, &outputs, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot parse filter graph: %s\n", av_err2str(ret));
        goto end;
    }
    
    /* Set hardware device context for all filters */
    for (unsigned int i = 0; i < ctx->filter_graph->nb_filters; i++) {
        ctx->filter_graph->filters[i]->hw_device_ctx = av_buffer_ref(ctx->hw_device_ctx);
        if (!ctx->filter_graph->filters[i]->hw_device_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set hw_device_ctx for filter\n");
            ret = AVERROR(ENOMEM);
            goto end;
        }
    }
    
    if ((ret = avfilter_graph_config(ctx->filter_graph, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot configure filter graph: %s\n", av_err2str(ret));
        goto end;
    }
    
    av_log(NULL, AV_LOG_INFO, "Filter graph configured successfully\n");
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

/* Process frame through filter graph */
static int filter_frame(AppContext *ctx, AVFrame *frame)
{
    int ret;
    AVFrame *filtered_frame;
    
    /* Add frame to filter graph */
    ret = av_buffersrc_add_frame_flags(ctx->buffersrc_ctx, frame,
                                        AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error feeding filter graph: %s\n", av_err2str(ret));
        return ret;
    }
    
    filtered_frame = av_frame_alloc();
    if (!filtered_frame)
        return AVERROR(ENOMEM);
    
    /* Get all frames from filter graph */
    while (1) {
        ret = av_buffersink_get_frame(ctx->buffersink_ctx, filtered_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error getting filtered frame: %s\n", av_err2str(ret));
            av_frame_free(&filtered_frame);
            return ret;
        }
        
        /* Successfully scaled frame */
        ctx->frames_scaled++;
        av_log(NULL, AV_LOG_DEBUG,
               "Scaled frame %ld: %dx%d pts=%ld (loop %d)\n",
               ctx->frames_scaled,
               filtered_frame->width, filtered_frame->height,
               filtered_frame->pts, ctx->current_loop);
        
        /* Here you could process the scaled frame further */
        /* For example: encode, save, or send to display */
        
        unref_frame(filtered_frame);
    }
    
    av_frame_free(&filtered_frame);
    return 0;
}

/* Unreference VPE frame properly */
static void unref_frame(AVFrame *frame)
{
    if (frame && frame->data[0] && frame->format == AV_PIX_FMT_VPE) {
        vpi_frame_unref((VpiFrm *)frame->data[0]);
    }
    av_frame_unref(frame);
}

/* Decode packet and process frames */
static int decode_packet(AppContext *ctx, AVPacket *packet)
{
    int ret;
    AVFrame *frame = av_frame_alloc();
    
    if (!frame)
        return AVERROR(ENOMEM);
    
    ret = avcodec_send_packet(ctx->dec_ctx, packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error sending packet to decoder: %s\n", av_err2str(ret));
        av_frame_free(&frame);
        return ret;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx->dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error receiving frame from decoder: %s\n", av_err2str(ret));
            av_frame_free(&frame);
            return ret;
        }
        
        ctx->frames_decoded++;
        av_log(NULL, AV_LOG_DEBUG,
               "Decoded frame %ld: %dx%d pts=%ld\n",
               ctx->frames_decoded,
               frame->width, frame->height, frame->pts);
        
        /* Process frame through filter graph for scaling */
        if (filter_frame(ctx, frame) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error filtering frame\n");
        }
        
        unref_frame(frame);
    }
    
    av_frame_free(&frame);
    return 0;
}

/* Cleanup resources */
static void cleanup(AppContext *ctx)
{
    if (ctx->buffersink_ctx)
        avfilter_free(ctx->buffersink_ctx);
    if (ctx->buffersrc_ctx)
        avfilter_free(ctx->buffersrc_ctx);
    if (ctx->filter_graph)
        avfilter_graph_free(&ctx->filter_graph);
    if (ctx->dec_ctx)
        avcodec_free_context(&ctx->dec_ctx);
    if (ctx->fmt_ctx)
        avformat_close_input(&ctx->fmt_ctx);
    if (ctx->hw_device_ctx)
        av_buffer_unref(&ctx->hw_device_ctx);
}

int main(int argc, char **argv)
{
    AppContext ctx = {0};
    AVPacket packet = {0};
    int ret = 0;
    
    /* Parse command line arguments */
    if (parse_arguments(argc, argv, &ctx) < 0)
        return 1;
    
    /* Set log level */
    av_log_set_level(ctx.log_level);
    
    /* Set up signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize components */
    if ((ret = init_device(&ctx)) < 0)
        goto cleanup;
    
    if ((ret = init_input(&ctx)) < 0)
        goto cleanup;
    
    if ((ret = init_decoder(&ctx)) < 0)
        goto cleanup;
    
    if ((ret = init_filters(&ctx)) < 0)
        goto cleanup;
    
    av_log(NULL, AV_LOG_INFO, "Starting looped playback...\n");
    if (ctx.loop_count == 0) {
        av_log(NULL, AV_LOG_INFO, "Infinite loop mode - press Ctrl+C to stop\n");
    } else {
        av_log(NULL, AV_LOG_INFO, "Will play %d loops\n", ctx.loop_count);
    }
    
    /* Initialize loop tracking */
    ctx.last_packet_pts = AV_NOPTS_VALUE;
    ctx.last_packet_duration = 0;
    ctx.base_time = 0;
    ctx.current_loop = 1;
    
    /* Main processing loop */
    while (keep_running) {
        ret = av_read_frame(ctx.fmt_ctx, &packet);
        
        if (ret == AVERROR_EOF) {
            /* End of file reached - check if we should loop */
            if (ctx.loop_count > 0 && ctx.current_loop >= ctx.loop_count) {
                av_log(NULL, AV_LOG_INFO, "Completed %d loops, stopping\n", ctx.current_loop);
                break;
            }
            
            /* Seek back to beginning */
            av_log(NULL, AV_LOG_INFO, "Loop %d complete, rewinding...\n", ctx.current_loop);
            
            if (av_seek_frame(ctx.fmt_ctx, ctx.video_stream_index, 0, AVSEEK_FLAG_BACKWARD) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to seek to beginning\n");
                break;
            }
            
            /* Flush decoder */
            avcodec_flush_buffers(ctx.dec_ctx);
            
            /* Adjust base time for continuous timestamps */
            ctx.base_time = ctx.last_packet_pts + ctx.last_packet_duration;
            ctx.current_loop++;
            
            continue;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error reading frame: %s\n", av_err2str(ret));
            break;
        }
        
        /* Process only video packets */
        if (packet.stream_index == ctx.video_stream_index && packet.size > 0) {
            /* Adjust timestamps for looped playback */
            packet.pts = ctx.base_time + packet.pts;
            packet.dts = ctx.base_time + packet.dts;
            ctx.last_packet_pts = packet.pts;
            ctx.last_packet_duration = packet.duration;
            
            av_log(NULL, AV_LOG_DEBUG, 
                   "Processing packet pts=%ld size=%d (loop %d)\n",
                   packet.pts, packet.size, ctx.current_loop);
            
            decode_packet(&ctx, &packet);
        }
        
        av_packet_unref(&packet);
    }
    
    /* Flush decoder */
    av_log(NULL, AV_LOG_INFO, "Flushing decoder...\n");
    decode_packet(&ctx, NULL);
    
    /* Print statistics */
    av_log(NULL, AV_LOG_INFO, "\n=== Statistics ===\n");
    av_log(NULL, AV_LOG_INFO, "Loops completed: %d\n", 
           ctx.current_loop - (ret == AVERROR_EOF ? 0 : 1));
    av_log(NULL, AV_LOG_INFO, "Frames decoded: %ld\n", ctx.frames_decoded);
    av_log(NULL, AV_LOG_INFO, "Frames scaled: %ld\n", ctx.frames_scaled);
    
cleanup:
    cleanup(&ctx);
    
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }
    
    av_log(NULL, AV_LOG_INFO, "Program completed successfully\n");
    return 0;
}
