//
//  DecodeFromBinaryImage.cpp
//  Restreaming
//
//  Created by gaurav on 18/03/16.
//  Copyright © 2016 gaurav. All rights reserved.
//

extern "C"{
    
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
    
}

#include "DecodeFromBinaryImage.hpp"
#include <iostream>

using namespace std;



AVFrame *frame;
DecodeFromBinanyImage::DecodeFromBinanyImage(){
    cout << "DecodeFromBinanyImage instance created";
}

static int saveImage(const char *fileName ,int frameWidth , int frameHeight){
    
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
    
    
    
    //    outputRGBFrame->width = frameWidth;
    //    outputRGBFrame->height = frameHeight;
    //    outputRGBFrame->pts = 1;
    //    outputRGBFrame->format = AV_PIX_FMT_RGBA;
    
    //encode the frame
    AVPacket encodedPacket;
    av_init_packet(&encodedPacket);
    encodedPacket.data = NULL;
    encodedPacket.size = 0;
    int encode_success;
    ret = avcodec_encode_video2(c,
                                &encodedPacket,
                                frame,
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
        
    }else{
        av_log(NULL,AV_LOG_ERROR , "Couldnot decode frame");
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

int DecodeFromBinanyImage::decodeFromByteArray(uint8_t *inbuf , AVFrame **frame , int dataLength){
    AVCodecContext *codecCtx = NULL;
    
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_PNG);
    AVPacket pkt;
    int ret;
    
    codecCtx = avcodec_alloc_context3(codec);
    
    if(!codecCtx){
        printf("Cannot allocate codec ctx \n");
        return -1;
    }
    
    if(codec->capabilities&CODEC_CAP_TRUNCATED)
    {
        codecCtx->flags|= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
    }
    
    
    if (avcodec_open2(codecCtx, codec, NULL) < 0){
        printf("cannot open decoder \n");
        return -1;
    }
    
    av_init_packet(&pkt);
    
    pkt.size = dataLength;
    pkt.data = inbuf;
    
    int got_frame , len;
    
    len = avcodec_decode_video2(codecCtx, *frame, &got_frame, &pkt);
    
    if(got_frame){
        ret = 1;
    }else{
        ret = -1;
    }
    avcodec_free_context(&codecCtx);
    avcodec_close(codecCtx);
    
    return ret;
}

void DecodeFromBinanyImage::openInputFile(uint8_t *inbuf){
    const char * fileName = "/Users/apple/experiments/mergedData";
    FILE *f = fopen(fileName , "rb");
    
    //       uint8_t dummy[4];
    //    int dummyBytes = (int)fread(dummy , 1 , 4, f);
    //    cout << "number of dummy bytes--"<<dummyBytes;
    int bytesRead = (int)fread(inbuf , 1 , 4163 , f);
    cout << "first byte" << inbuf[0]<<"\n";
    
    cout << "number of bytes read--"<<bytesRead;
    
    fclose(f);
    
    
    
}

static int decodePngFile(const char *file ){
    
    //    const char * tempFIle = "/Users/apple/mnt-storage/experiments/temp.png";
    //    writeToTempFile(inbuf, dataLength, tempFIle);
    int ret;
    AVFormatContext * ifmt_ctx = NULL;
    
    if ((ret = avformat_open_input(&ifmt_ctx, file, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    av_dump_format(ifmt_ctx, 0, file, 0);
    
    for (int i = 0; i < (ifmt_ctx)->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = (ifmt_ctx)->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            /* Open decoder */
            codec_ctx->thread_count = 1; // this is required for some reason
            codec_ctx->refcounted_frames = 1;
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            
            int height = codec_ctx->height;
            int width = codec_ctx->width;
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }
    
    AVPacket packet;
    
    int frameDecoded = 0;
    frame = av_frame_alloc();
    while(1){
        av_init_packet(&packet);
        
        //get a packet out of container
        ret = av_read_frame(ifmt_ctx, &packet);
        
        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR,"no more frames left to be read");
            break;
        }
        
        int stream_index = packet.stream_index;
        
        if(stream_index != 0){
            //if not video frame just discard the frame.
            //in animation video audio frames are not expected
            continue;
        }
        
        
        AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
        
        
        ret = avcodec_decode_video2(in_stream->codec, (frame), &frameDecoded, &packet);
        if(ret<0){
            av_frame_free(&frame);
            av_log(NULL,AV_LOG_DEBUG,"could not decode a packet....trying next packet");
            continue;
        }
        
        if(frameDecoded){
            //at this point frame points to a decoded frame so
            //job done for this function
            av_log(NULL,AV_LOG_DEBUG,"success frame decoded---%p \n",frame);
            
            av_free_packet(&packet);
            
            
            frame->width = in_stream->codec->width;
            frame->height = in_stream->codec->height;
            break;
        }else{
            av_free_packet(&packet);
            continue;
        }
    }
    
    if(ifmt_ctx != NULL){
        avcodec_close(ifmt_ctx->streams[0]->codec);
        
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    
    return ret;
    
}


//int main()
//{
//    av_register_all();
//    
//    decodePngFile("/media/sf_mnt-storage/experiments/frame-9.png");
//    saveImage("/media/sf_mnt-storage/experiments/decoded.png",frame->width , frame->height);
//    
//}