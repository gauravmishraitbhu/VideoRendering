//
//  SaveRgbImageHelper.cpp
//  Restreaming
//
//  Created by gaurav on 29/01/16.
//  Copyright © 2016 gaurav. All rights reserved.
//


extern "C"{
    
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
    
}

#include "SaveRgbImageHelper.hpp"
#include <iostream>
using namespace std;

SaveImageHelper::SaveImageHelper(){
    
    cout <<"save image helper instance";
}


int SaveImageHelper::saveImageRGBImageToDisk(const char *fileName , AVFrame *outputRGBFrame,int frameWidth , int frameHeight){
    
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *video_codec      = NULL;
    int ret                   = 0;
    AVCodecContext *c = NULL;
    
    //create a context 
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , fileName);
    
    video_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    
    if (!(video_codec)) {
        fprintf(stderr, "Could not find encoder for \n");
        return -1;
    }
    
    c = avcodec_alloc_context3(video_codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    
    
    AVStream *out_stream = avformat_new_stream(ofmt_ctx,video_codec);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    
    out_stream->id = ofmt_ctx->nb_streams-1;
    
    out_stream->codec = c;
//    c = out_stream->codec;
    
    switch ((video_codec)->type) {
            
        case AVMEDIA_TYPE_VIDEO:
            
            
            c->codec_id                = AV_CODEC_ID_PNG;
            
            /* Resolution must be a multiple of two. */
            c->width                   = frameWidth;
            c->height                  = frameHeight;

            
            c->time_base               = (AVRational){1,25};
            out_stream->time_base = (AVRational){1,25};
            
            //this depends on the input frame format.for a video frame this will be RGB24
            c->pix_fmt                 = AV_PIX_FMT_RGBA;
            
            break;

            
    }
    
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    //write header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    
    //open the encoder
    ret = avcodec_open2(c, video_codec, NULL);

    if (ret < 0) {
        fprintf(stderr, "Could not open video codec:\n");
        exit(1);
    }
    
    
    
    outputRGBFrame->width = frameWidth;
    outputRGBFrame->height = frameHeight;
    outputRGBFrame->pts = 1;
    outputRGBFrame->format = AV_PIX_FMT_RGB24;
    
    //encode the frame
    AVPacket encodedPacket;
    av_init_packet(&encodedPacket);
    encodedPacket.data = NULL;
    encodedPacket.size = 0;
    int encode_success;
    ret = avcodec_encode_video2(c,
                                &encodedPacket,
                                outputRGBFrame,
                                &encode_success);
    if(ret < 0){
        av_log(NULL,AV_LOG_ERROR , "Error encoding a frame");
        
    }

    if(encode_success){
        encodedPacket.stream_index = 0;
        
        
//        av_packet_rescale_ts(encodedPacket,
//                             this->out_stream.format_ctx->streams[0]->codec->time_base,
//                             this->out_stream.format_ctx->streams[0]->time_base);
        
        encodedPacket.pts = 1;
        encodedPacket.dts = 1;
        ret = av_write_frame(ofmt_ctx, &encodedPacket);
        
        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR , "Error writing  a frame to container...");
            return ret;
        }
        
    }
    
    ret = av_write_trailer(ofmt_ctx);
    
    if(ret < 0){
        av_log(NULL,AV_LOG_ERROR , "Error writing  a trailer to container...");
    }

    
    avcodec_close(c);
//    avcodec_free_context(&c);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    
    
    avformat_free_context(ofmt_ctx);
    
    return 1;
}