////
////  myLib.c
////  Restreaming
////
////  Created by gaurav on 11/11/14.
////  Copyright (c) 2014 gaurav. All rights reserved.
////
//
//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include <libavfilter/avfiltergraph.h>
//#include <libavfilter/avcodec.h>
//#include <libswresample/swresample.h>
//
//#include "myLib.h"
//
//
//
//
//
//
//int open_input_file(const char *filename,InputStream *input_stream)
//{
//    int ret;
//    unsigned int i;
//    
//    AVFormatContext *ifmt_ctx = NULL;
//    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
//        return ret;
//    }
//    
//    //setting the member variable
//    input_stream->format_ctx = ifmt_ctx;
//    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
//        return ret;
//    }
//    
//    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
//        AVStream *stream;
//        AVCodecContext *codec_ctx;
//        stream = ifmt_ctx->streams[i];
//        codec_ctx = stream->codec;
//        /* Reencode video & audio and remux subtitles etc. */
//        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
//            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
//            /* Open decoder */
//            ret = avcodec_open2(codec_ctx,
//                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
//            if (ret < 0) {
//                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
//                return ret;
//            }
//        }
//    }
//    
//    input_stream->output_fmt = ifmt_ctx->oformat;
//    av_dump_format(ifmt_ctx, 0, filename, 0);
//    return 0;
//}
//
//int open_outputfile(const char *filename,OutputStream *out_stream,enum AVCodecID video_codec_id,
//                     enum AVCodecID audio_codec_id,int video_width,int video_height){
//    AVFormatContext *ofmt_ctx = NULL;
//    AVCodec *video_codec = NULL;
//    int ret = 0;
//    AVCodec *audio_codec = NULL;
//    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , filename);
//    out_stream->format_ctx = ofmt_ctx;
//    
//    add_stream(ofmt_ctx, &video_codec, CODEC_ID_H264,video_width,video_height);
//    open_video(ofmt_ctx, video_codec, NULL);
//    
//    if(audio_codec_id != AV_CODEC_ID_NONE) {
//        add_stream(ofmt_ctx,&audio_codec,audio_codec_id,video_width,video_height);
//        open_audio(ofmt_ctx, audio_codec, NULL);
//
//    }
//    
//    
//    av_dump_format(ofmt_ctx, 0, filename, 1);
//    
//    /* open the output file, if needed */
//    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
//        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
//        if (ret < 0) {
//            fprintf(stderr, "Could not open '%s': %s\n", filename,
//                    av_err2str(ret));
//            exit(1);
//        }
//    }
//    
//    /* init muxer, write output file header */
//    ret = avformat_write_header(ofmt_ctx, NULL);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
//        return ret;
//    }
//    return 0;
//
//}
//
//
//void add_stream(AVFormatContext *oc,
//                       AVCodec **codec,
//                       enum AVCodecID codec_id,
//                       int width,int height)
//{
//    AVCodecContext *c;
//    int i;
//    
//    /* find the encoder */
//    *codec = avcodec_find_encoder(codec_id);
//    if (!(*codec)) {
//        fprintf(stderr, "Could not find encoder for '%s'\n",
//                avcodec_get_name(codec_id));
//        exit(1);
//    }
//    AVStream *out_stream;
//    
//    out_stream = avformat_new_stream(oc, *codec);
//    if (!out_stream) {
//        fprintf(stderr, "Could not allocate stream\n");
//        exit(1);
//    }
//    out_stream->id = oc->nb_streams-1;
//    c = out_stream->codec;
//    //    avcodec_copy_context(c, ifmt_ctx->streams[0]->codec);
//    switch ((*codec)->type) {
//            
//            
//        case AVMEDIA_TYPE_VIDEO:
//            c->codec_id = codec_id;
//            
//            c->bit_rate = 400000;
//            /* Resolution must be a multiple of two. */
//            c->width    = width ;//ifmt_ctx->streams[0]->codec->width;
//            c->height   = height;//ifmt_ctx->streams[0]->codec->height;
//            /* timebase: This is the fundamental unit of time (in seconds) in terms
//             * of which frame timestamps are represented. For fixed-fps content,
//             * timebase should be 1/framerate and timestamp increments should be
//             * identical to 1. */
//            out_stream->time_base = (AVRational){ 1, 25 };
//            c->time_base       = out_stream->time_base;
//            
//            // c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
//            c->pix_fmt       = PIX_FMT_YUV420P;
//            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
//                /* just for testing, we also add B frames */
//                c->max_b_frames = 2;
//            }
//            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
//                /* Needed to avoid using macroblocks in which some coeffs overflow.
//                 * This does not happen with normal video, it just happens here as
//                 * the motion of the chroma plane does not match the luma plane. */
//                c->mb_decision = 2;
//            }
//            break;
//            
//        case AVMEDIA_TYPE_AUDIO:
//            c->sample_fmt  = (*codec)->sample_fmts ?
//            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
//            c->bit_rate    = 64000;
//            c->sample_rate = 44100;
//            //c->frame_size =
//            if ((*codec)->supported_samplerates) {
//                c->sample_rate = (*codec)->supported_samplerates[0];
//                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
//                    if ((*codec)->supported_samplerates[i] == 44100)
//                        c->sample_rate = 44100;
//                }
//            }
//            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
//            c->channel_layout = AV_CH_LAYOUT_STEREO;
//            if ((*codec)->channel_layouts) {
//                c->channel_layout = (*codec)->channel_layouts[0];
//                for (i = 0; (*codec)->channel_layouts[i]; i++) {
//                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
//                        c->channel_layout = AV_CH_LAYOUT_STEREO;
//                }
//            }
//            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
//            out_stream->time_base = (AVRational){ 1, c->sample_rate };
//            break;
//            
//            
//        default:
//            break;
//    }
//    
//    /* Some formats want stream headers to be separate. */
//    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
//        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
//}
//
//
//void open_video(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg)
//{
//    int ret;
//    AVStream *stream = oc->streams[0];
//    AVCodecContext *c = stream->codec;
//    AVDictionary *opt = NULL;
//    
//    av_dict_copy(&opt, opt_arg, 0);
//    av_opt_set(c->priv_data, "preset", "slow", 0);
//    
//    /* open the codec */
//    ret = avcodec_open2(c, codec, &opt);
//    av_dict_free(&opt);
//    if (ret < 0) {
//        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
//        exit(1);
//    }
//}
//
//void open_audio(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg)
//{
//    AVStream *stream = oc->streams[1];
//    AVCodecContext *c = stream->codec;
//    int nb_samples;
//    int ret;
//    AVDictionary *opt = NULL;
//    
//    
//    
//    /* open it */
//    av_dict_copy(&opt, opt_arg, 0);
//    ret = avcodec_open2(c, codec, &opt);
//    av_dict_free(&opt);
//    if (ret < 0) {
//        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
//        exit(1);
//    }
//    
//    /* init signal generator */
//    // stream->t     = 0;
//    // ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
//    /* increment frequency by 110 Hz per second */
//    // ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
//    
//    if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE)
//        nb_samples = 10000;
//    else
//        nb_samples = c->frame_size;
//    
//    /* create resampler context */
////    swr_ctx = swr_alloc();
////    if (!swr_ctx) {
////        fprintf(stderr, "Could not allocate resampler context\n");
////        exit(1);
////    }
////    
////    /* set options */
////    av_opt_set_int       (swr_ctx, "in_channel_count",   c->channels,       0);
////    av_opt_set_int       (swr_ctx, "in_sample_rate",     c->sample_rate,    0);
////    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
////    av_opt_set_int       (swr_ctx, "out_channel_count",  c->channels,       0);
////    av_opt_set_int       (swr_ctx, "out_sample_rate",    c->sample_rate,    0);
////    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
////    
////    /* initialize the resampling context */
////    if ((ret = swr_init(swr_ctx)) < 0) {
////        fprintf(stderr, "Failed to initialize the resampling context\n");
////        exit(1);
////    }
//}
//
//
//
//int copyVideoPixels (AVFrame **fromFrame, AVFrame **destFrame, int srcHeight , int srcWidth,int dstHeight,int dstWidth){
//    int ret=0;
//    
//    uint8_t srcPixelRed , srcPixelGreen , srcPixelBlue;
//    int srcRow=0 , srcCol=0 ;   //srcRow from 0 to height
//    int dstRow = 0,dstCol = 0 ;
//    int startRow=0 , startCol = 0; //starting coordinates in content frame where
//                                    //animation needs to be put
//    
//
//    int srcLinesize=0,destLinesize=0;
//    srcLinesize = (*fromFrame)->linesize[0];
//    destLinesize = (*destFrame)->linesize[0];
//    //fromFrame will be the animation frame.
//    
//    for (srcRow = 0 ;srcRow < srcHeight ; srcRow++){
//        for (srcCol = 0 ; srcCol < srcWidth ; srcCol++){
//            
//            dstRow = srcRow + startRow;
//            dstCol = srcCol + startCol;
//            srcPixelRed = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol];
//            srcPixelGreen = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol + 1];
//            srcPixelBlue = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol + 2];
//            
//            if(srcPixelRed == 255 && srcPixelGreen == 255 && srcPixelBlue == 255){
//                continue;
//            }
//            
//            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol] = srcPixelRed;
//            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 1] = srcPixelGreen;
//            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 2] = srcPixelBlue;
//        }
//    }
//    
//
//    
//    return ret;
//}
//
