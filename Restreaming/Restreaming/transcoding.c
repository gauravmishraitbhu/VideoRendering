/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example transcoding.c
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include "myLib.h"

//static AVFormatContext *ifmt_ctx;
//static AVFormatContext *ofmt_ctx;



typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

static struct SwsContext *sws_ctx;
//static struct SwrContext *swr_ctx;



static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt,int is_input)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    if(is_input){
        printf("input--");
    }else{
        printf("output--");
    }
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}




static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                       AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                 dec_ctx->time_base.num, dec_ctx->time_base.den,
                 dec_ctx->sample_aspect_ratio.num,
                 dec_ctx->sample_aspect_ratio.den);
        
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }
        
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                             (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        
        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
            av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                 dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }
        
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                             (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                             (uint8_t*)&enc_ctx->channel_layout,
                             sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                             (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    
    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    
    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}

static int init_filters(AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;
        if (!(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
              || ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;
        
//        if(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
//            continue;
//        }
        
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = "crop=50:50:50:50"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
                          ofmt_ctx->streams[i]->codec, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame,AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
    (ifmt_ctx->streams[stream_index]->codec->codec_type ==
     AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;
    
    if (!got_frame)
        got_frame = &got_frame_local;
    
    av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
                   filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;
    
    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    av_packet_rescale_ts(&enc_pkt,
                         ifmt_ctx->streams[stream_index]->codec->time_base,
                         ofmt_ctx->streams[stream_index]->time_base);
    
    log_packet(ofmt_ctx, &enc_pkt, 0);
    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index,AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx)
{
    int ret;
    AVFrame *filt_frame;
    
    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
                                       frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }
    
    /* pull filtered frames from the filtergraph */
    while (1) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
                                      filt_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }
        
        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(filt_frame, stream_index, NULL,ifmt_ctx,ofmt_ctx);
        if (ret < 0)
            break;
    }
    
    return ret;
}






static int flush_encoder(unsigned int stream_index,AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx)
{
    int ret;
    int got_frame;
    
    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
          CODEC_CAP_DELAY))
        return 0;
    
    while (1) {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index, &got_frame,ifmt_ctx,ofmt_ctx);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}





int main(int argc, char **argv)
{
    int ret;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    OutputStream out_stream;
    AVFormatContext *ofmt_ctx;
    InputStream input_stream = {.format_ctx = NULL , .output_fmt = NULL};
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    

    
    av_register_all();
    avformat_network_init();
    avfilter_register_all();
    char *input_file = "rtmp://localhost:1935/vod/sample.mp4";
    if ((ret = open_input_file(input_file,&input_stream)) < 0)
        goto end;
    

    
    AVFormatContext *ifmt_ctx = input_stream.format_ctx;

    //ifmt_ctx->streams[1]->codec->codec_id
    char * output_filename = "rtmp://localhost:1935/live/myStream";
    open_outputfile(output_filename, &out_stream,CODEC_ID_H264,ifmt_ctx->streams[1]->codec->codec_id , ifmt_ctx->streams[0]->codec->width,ifmt_ctx->streams[0]->codec->height);
    ofmt_ctx = out_stream.format_ctx;
    
        
        /* Write the stream header, if any. */
        ret = avformat_write_header(ofmt_ctx, NULL);
        if (ret < 0) {
            exit(1);
        }
    
    if ((ret = init_filters(input_stream.format_ctx,out_stream.format_ctx)) < 0)
        goto end;
    
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
               stream_index);
        
        if (filter_ctx[stream_index].filter_graph) {
            av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
            frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            log_packet(ifmt_ctx, &packet, 1);
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ifmt_ctx->streams[stream_index]->codec->time_base);
            dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
            avcodec_decode_audio4;
            ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
                           &got_frame, &packet);
            if (ret < 0) {
                av_frame_free(&frame);
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }
            
            if (got_frame) {

                frame->pts = av_frame_get_best_effort_timestamp(frame);
                ret = filter_encode_write_frame(frame, stream_index,ifmt_ctx,ofmt_ctx);
                av_frame_free(&frame);
                int got = 0;
//                ret = encode_write_frame(frame,stream_index,&got);

                if (ret < 0)
                    goto end;
            } else {
                av_frame_free(&frame);
            }
        } else {
            /* remux this frame without reencoding */
//            av_packet_rescale_ts(&packet,
//                                 ifmt_ctx->streams[stream_index]->time_base,
//                                 ofmt_ctx->streams[stream_index]->time_base);
//            
//            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
//            if (ret < 0)
//                goto end;
        }
        av_free_packet(&packet);
    }
    
    /* flush filters and encoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i,ifmt_ctx,ofmt_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }
        
        /* flush encoder */
        ret = flush_encoder(i,ifmt_ctx,ofmt_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }
    
    av_write_trailer(ofmt_ctx);
end:
    av_free_packet(&packet);
    av_frame_free(&frame);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        avcodec_close(ifmt_ctx->streams[i]->codec);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
            avcodec_close(ofmt_ctx->streams[i]->codec);
        if (filter_ctx && filter_ctx[i].filter_graph)
            avfilter_graph_free(&filter_ctx[i].filter_graph);
    }
    av_free(filter_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    
    return ret ? 1 : 0;
}
