//
//  myLib.c
//  Restreaming
//
//  Created by gaurav on 11/11/14.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

extern "C"{
    
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
    
    
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
    
    //#include <>
    
}


#include "Utils.h"
#include "Utils2.h"
#include <iostream>
using namespace std;




int open_input_file(const char *filename,AVFormatContext **ifmt_ctx)
{
    int ret;
    unsigned int i;
    
    
    if ((ret = avformat_open_input(ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    //setting the member variable
    
    if ((ret = avformat_find_stream_info(*ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    for (i = 0; i < (*ifmt_ctx)->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = (*ifmt_ctx)->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }
    
    
    av_dump_format(*ifmt_ctx, 0, filename, 0);
    return 0;
}

int open_outputfile_copy_codecs(const char *filename, OutputStream *outputStream,AVCodecContext *inputVideoCodec , AVCodecContext *inputAudioCodecCtx,AVFormatContext *inputFormatCtx){
    int ret = 1;
    
    AVFormatContext *ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , filename);
    
    outputStream->format_ctx = ofmt_ctx;
    
    AVCodec * videoCodec = avcodec_find_encoder(inputVideoCodec->codec_id);
    
    //create a video stream
    AVStream *outputVideoStream = avformat_new_stream(ofmt_ctx, videoCodec);
    if (!outputVideoStream) {
        fprintf(stderr, "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    
    
    //copy the settings
    //videoStream->codec = avcodec_alloc_context3(videoCodec);
    //    ret = avcodec_copy_context(outputVideoStream->codec, inputVideoCodec);
    //    if (ret < 0) {
    //        fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
    //        return ret;
    //    }
    
    int m_fps = 25;
    AVStream *inputVideoStream = inputFormatCtx->streams[0];
    outputVideoStream->sample_aspect_ratio.den = inputVideoStream->sample_aspect_ratio.den;
    outputVideoStream->sample_aspect_ratio.num = inputVideoStream->sample_aspect_ratio.num;
    outputVideoStream->codec->codec_id         = inputVideoStream->codec->codec_id;
    outputVideoStream->codec->time_base.num    = 1;
    outputVideoStream->codec->time_base.den =
    m_fps*(inputVideoStream->codec->ticks_per_frame);
    outputVideoStream->time_base.num           = 1;
    outputVideoStream->time_base.den           = 1000;
    outputVideoStream->r_frame_rate.num        = m_fps;
    outputVideoStream->r_frame_rate.den        = 1;
    outputVideoStream->avg_frame_rate.den      = 1;
    outputVideoStream->avg_frame_rate.num      = m_fps;
    //    m_out_vid_strm->duration = (m_out_end_time - m_out_start_time)*1000;
    //outputVideoStream->codec->bit_rate = 400000;
    
    
    /* Resolution must be a multiple of two. */
    outputVideoStream->codec->width    = inputFormatCtx->streams[0]->codec->width;
    outputVideoStream->codec->height   = inputFormatCtx->streams[0]->codec->height;
    
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    
    
    //    if(options.find("timebase_denominator") != options.end()){
    //        int denominator = boost::any_cast<int>(options["timebase_denominator"]);
    //        int numerator = boost::any_cast<int>(options["timebase_numerator"]);
    //        out_stream->time_base = (AVRational) {numerator,denominator};
    //    }else{
    //        out_stream->time_base = (AVRational){ 1, 25 };
    //    }
    //
    //    c->time_base       = out_stream->time_base;
    
    // c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    outputVideoStream->codec->pix_fmt       = AV_PIX_FMT_YUV420P;
    
    
    //    outputVideoStream->codec->codec_tag = 0;
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        outputVideoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    AVCodecContext *videoCodecCtx = outputVideoStream->codec;
    
    
    av_opt_set(videoCodecCtx->priv_data, "preset", "slow", 0);
    ret = avcodec_open2(videoCodecCtx,videoCodec , NULL);
    //av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec:\n");
        exit(1);
    }
    
    
    //add audio stream
    
    //    AVCodec *audioCodec = avcodec_find_encoder(inputAudioCodecCtx->codec_id);
    //    AVStream *audioStream = avformat_new_stream(ofmt_ctx, audioCodec);
    //    if (!audioStream) {
    //        fprintf(stderr, "Failed allocating output stream\n");
    //        ret = AVERROR_UNKNOWN;
    //        return ret;
    //    }
    //
    //    ret = avcodec_copy_context(audioStream->codec, inputAudioCodecCtx);
    //    if (ret < 0) {
    //        fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
    //        return ret;
    //    }
    //    audioStream->codec->codec_tag = 0;
    //    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    //        audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    
    av_dump_format(ofmt_ctx, 0, filename, 1);
    
    /* open the output file, if needed */
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            exit(1);
        }
    }
    
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    
    
    
    return ret;
}

int open_outputfile(const char *filename,OutputStream *out_stream,AVFormatContext *inputFormatCtx){
    
    
    
    //cout<< boost::any_cast<AVRational>(videoOptions1["timebase"]);
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *video_codec      = NULL;
    int ret                   = 0;
    AVCodec *audio_codec      = NULL;
    
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , filename);
    out_stream->format_ctx = ofmt_ctx;
    
    add_stream(ofmt_ctx, &video_codec, inputFormatCtx->streams[0]);
    open_video(ofmt_ctx, video_codec, NULL);
    
    if(inputFormatCtx->streams[1] != NULL) {
        add_stream_from_codec(ofmt_ctx,&audio_codec,inputFormatCtx->streams[1]->codec);
        //ret = avcodec_copy_context(ofmt_ctx->streams[1]->codec, inputAudioCodec);
        // open_audio(ofmt_ctx, audio_codec, NULL);
        
    }
    
    
    av_dump_format(ofmt_ctx, 0, filename, 1);
    
    /* open the output file, if needed */
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            
            av_log(NULL, AV_LOG_ERROR, "Could not open '%s'\n",filename);
            return ret;
        }
    }
    
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    return 0;
    
}

int add_stream_from_codec(AVFormatContext *oc,
                          AVCodec **codec,
                          AVCodecContext *inputCodec)
{
    int ret = 0;
    *codec = avcodec_find_encoder(inputCodec->codec_id);
    
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(inputCodec->codec_id));
        return -1;
    }
    
    AVStream *out_stream = avformat_new_stream(oc,*codec);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    
    ret = avcodec_copy_context(out_stream->codec, inputCodec);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
        return ret;
    }
    
    out_stream->codec->codec_tag = 0;
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    return 1;
}



void add_stream(AVFormatContext *oc,
                AVCodec **codec,
                AVStream *inputStream)
{
    AVCodecContext *c;
    int i;
    
    /* find the encoder */
    *codec = avcodec_find_encoder(inputStream->codec->codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(inputStream->codec->codec_id));
        exit(1);
    }
    AVStream *out_stream;
    
    out_stream = avformat_new_stream(oc, *codec);
    if (!out_stream) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    out_stream->id = oc->nb_streams-1;
    c = out_stream->codec;
    //    avcodec_copy_context(c, ifmt_ctx->streams[0]->codec);
    switch ((*codec)->type) {
            
            
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = inputStream->codec->codec_id;
            
            
            c->bit_rate = inputStream->codec->bit_rate;
            
            
            /* Resolution must be a multiple of two. */
            c->width    = inputStream->codec->width;
            c->height   = inputStream->codec->height;
            c->gop_size = inputStream->codec->gop_size;
            
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            
            
            c->time_base       = inputStream->codec->time_base;
            out_stream->time_base = inputStream->time_base;
            
            // c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
            c->pix_fmt       = AV_PIX_FMT_YUV420P;
            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                /* just for testing, we also add B frames */
                c->max_b_frames = 2;
            }
            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                 * This does not happen with normal video, it just happens here as
                 * the motion of the chroma plane does not match the luma plane. */
                c->mb_decision = 2;
            }
            break;
            
        case AVMEDIA_TYPE_AUDIO:
            c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            
            
            c->sample_rate = inputStream->codec->sample_rate;
            c->bit_rate    = inputStream->codec->bit_rate;
            
            
            
            if ((*codec)->supported_samplerates) {
                c->sample_rate = (*codec)->supported_samplerates[0];
                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100)
                        c->sample_rate = 44100;
                }
            }
            
            
            
            c->frame_size = inputStream->codec->frame_size;
            
            
            // c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            
            
            c->channel_layout = inputStream->codec->channel_layout;
            
            
            
            if ((*codec)->channel_layouts) {
                c->channel_layout = (*codec)->channel_layouts[0];
                for (i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                        c->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }
            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            out_stream->time_base = (AVRational){ 1, c->sample_rate };
            break;
            
            
        default:
            break;
    }
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
}


void open_video(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg)
{
    int ret;
    AVStream *stream = oc->streams[0];
    AVCodecContext *c = stream->codec;
    AVDictionary *opt = NULL;
    
    av_dict_copy(&opt, opt_arg, 0);
    av_opt_set(c->priv_data, "preset", "slow", 0);
    
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec:\n");
        exit(1);
    }
}

void open_audio(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg)
{
    AVStream *stream = oc->streams[1];
    AVCodecContext *c = stream->codec;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    
    
    
    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec:\n");
        exit(1);
    }
    
    if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;
    
}


int copyVideoPixelsRGBA(AVFrame **fromFrame, AVFrame **destFrame, int srcHeight , int srcWidth,int dstHeight,int dstWidth) {
    int ret=0;
    
    uint8_t srcPixelRed , srcPixelGreen , srcPixelBlue;
    
    float srcPixelAlpha;
    
    uint8_t dstPixelRed , dstPixelGreen , dstPixelBlue;
    int srcRow=0 , srcCol=0 ;   //srcRow from 0 to height
    int dstRow = 0,dstCol = 0 ;
    int startRow=0 , startCol = 0; //starting coordinates in content frame where
    
    int srcLinesize=0,destLinesize=0;
    srcLinesize = (*fromFrame)->linesize[0];
    destLinesize = (*destFrame)->linesize[0];
    
    
    uint8_t resultRed , resultGreen , resultBlue;
    
    
    for (srcRow = 0 ;srcRow < srcHeight ; srcRow++){
        for (srcCol = 0 ; srcCol < srcWidth ; srcCol++){
            dstRow = srcRow + startRow;
            dstCol = srcCol + startCol;
            
            if(dstRow > dstHeight || dstCol > dstWidth){
                continue;
            }
            
            srcPixelRed = (*fromFrame)->data[0][srcRow * srcLinesize + 4*srcCol];
            srcPixelGreen = (*fromFrame)->data[0][srcRow * srcLinesize + 4*srcCol + 1];
            srcPixelBlue = (*fromFrame)->data[0][srcRow * srcLinesize + 4*srcCol + 2];
            
            
            //            if(srcPixelRed == 255 && srcPixelGreen == 255 && srcPixelBlue == 255){
            //                continue;
            //            }
            srcPixelAlpha = float( (*fromFrame)->data[0][srcRow * srcLinesize + 4*srcCol + 3]);
            srcPixelAlpha = (float)srcPixelAlpha / (float)255;
            
            
            dstPixelRed = (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol];
            dstPixelGreen = (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 1];
            dstPixelBlue = (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 2];
            
            
            
            resultRed = (int)srcPixelRed * srcPixelAlpha + (int)dstPixelRed * (1-srcPixelAlpha);
            resultGreen = (int)srcPixelGreen * srcPixelAlpha + (int)dstPixelGreen * (1-srcPixelAlpha);
            resultBlue = (int)srcPixelBlue * srcPixelAlpha + (int)dstPixelBlue * (1-srcPixelAlpha);
            
            
            
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol] = (int)resultRed;
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 1] = (int)resultGreen;
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 2] = (int)resultBlue;
            
        }
    }
    
    return ret;
    
}


int copyVideoPixels (AVFrame **fromFrame, AVFrame **destFrame, int srcHeight , int srcWidth,int dstHeight,int dstWidth){
    int ret=0;
    
    uint8_t srcPixelRed , srcPixelGreen , srcPixelBlue;
    int srcRow=0 , srcCol=0 ;   //srcRow from 0 to height
    int dstRow = 0,dstCol = 0 ;
    int startRow=0 , startCol = 0; //starting coordinates in content frame where
    //animation needs to be put
    
    
    int srcLinesize=0,destLinesize=0;
    srcLinesize = (*fromFrame)->linesize[0];
    destLinesize = (*destFrame)->linesize[0];
    //fromFrame will be the animation frame.
    
    for (srcRow = 0 ;srcRow < srcHeight ; srcRow++){
        for (srcCol = 0 ; srcCol < srcWidth ; srcCol++){
            
            dstRow = srcRow + startRow;
            dstCol = srcCol + startCol;
            srcPixelRed = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol];
            srcPixelGreen = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol + 1];
            srcPixelBlue = (*fromFrame)->data[0][srcRow * srcLinesize + 3*srcCol + 2];
            
            if(srcPixelRed == 255 && srcPixelGreen == 255 && srcPixelBlue == 255){
                continue;
            }
            
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol] = srcPixelRed;
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 1] = srcPixelGreen;
            (*destFrame)->data[0][dstRow * destLinesize + 3*dstCol + 2] = srcPixelBlue;
        }
    }
    
    
    
    return ret;
}

