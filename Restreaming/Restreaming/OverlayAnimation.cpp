//
//  OverlayAnimation.cpp
//  Restreaming
//
//  Created by gaurav on 20/07/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#include "OverlayAnimation.h"

extern "C"{
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>


#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

#include <libavutil/time.h>
#include "myLib.h"
}

#include <iostream>
#include <string>

using namespace std;

////////////////////////////////////////CLASS DECLARATION/////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//Each class instance will represent a video file. either content video or animation video.
class VideoFileInstance {
    
public:
    VideoFileInstance(int , const char *);
    int startDecoding();
    
    /**
     reads packet from input format context and decode it to frame.
     @param frame the frame in which decoded data will be returned.
     */
    int getSingleFrame(AVFrame **frame);
    
private:
    const char *fileName;
    int VIDEO_STREAM_INDEX = 0;
    int type; //1 = content video 2 = animation video
    AVFormatContext *ifmt_ctx = NULL;
    OutputStream out_stream;
    struct SwsContext *imgConvertCtxYUVToRGB = NULL;
    struct SwsContext *imgConvertCtxRGBToYUV = NULL;
    
    //for opening file and decoder etc
    int openFile();
    int openOutputFile();
    
    int convertToRGBFrame(AVFrame **,AVFrame **);
    int convertToYuvFrame(AVFrame ** , AVFrame **);
    
};

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////


int VideoFileInstance::openOutputFile() {
  
    int ret = open_outputfile("/Users/apple/temp/sample_output.mp4", &out_stream , CODEC_ID_H264, AV_CODEC_ID_NONE, ifmt_ctx->streams[0]->codec->width,ifmt_ctx->streams[0]->codec->height);
    
    
    return ret;
    
}

int VideoFileInstance::getSingleFrame(AVFrame **frame) {
    int ret;
    
    
    
    return ret;
}

int VideoFileInstance::convertToRGBFrame(AVFrame **yuvframe,AVFrame **rgbPictInfo) {
    int ret;
    int width = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->width;
    int height = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->height;
    
    
    
    
    //init context if not done already.
    if (imgConvertCtxYUVToRGB == NULL) {
        //init once
        imgConvertCtxYUVToRGB = sws_getContext(width, height, PIX_FMT_YUV420P, width, height, PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
        
        if(imgConvertCtxYUVToRGB == NULL) {
            av_log(NULL,AV_LOG_ERROR,"error creating img context");
            return -1;
        }
        
    }
    

    // call av_freep(rgbPictInfo->data) to free memory
    
    av_image_alloc( (*rgbPictInfo)->data,   //data to be filled
                   (*rgbPictInfo)->linesize,//line sizes to be filled
                   width, height,
                   PIX_FMT_RGB24,           //pixel format
                   32                       //aling
                   );
    

    
    ret = sws_scale(imgConvertCtxYUVToRGB, (*yuvframe)->data, (*yuvframe)->linesize, 0, height,
              (*rgbPictInfo)->data, (*rgbPictInfo)->linesize);
    
    
    return ret;
}

int VideoFileInstance::convertToYuvFrame (AVFrame **rgbFrame , AVFrame ** yuvFrame) {
    int ret = 0;
    int width = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->width;
    int height = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->height;

    if(imgConvertCtxRGBToYUV == NULL) {
        imgConvertCtxRGBToYUV = sws_getContext(width, height, PIX_FMT_RGB24, width, height, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
        
        if(imgConvertCtxRGBToYUV == NULL){
            av_log(NULL,AV_LOG_ERROR,"error creating img context");
            return -1;
        }
    }
    
   
    av_image_alloc( (*yuvFrame)->data,   //data to be filled
                   (*yuvFrame)->linesize, //line sizes to be filled
                   width, height,
                   PIX_FMT_YUV420P,        //pixel format
                   32                      //aling
                   );
    
    sws_scale(imgConvertCtxRGBToYUV,(*rgbFrame)->data , (*rgbFrame)->linesize, 0, height,
              (*yuvFrame)->data , (*yuvFrame)->linesize);
    

    
    return ret;
}

int VideoFileInstance::startDecoding() {
    
    AVPacket packet = { .data = NULL, .size = 0 };
    
    AVFrame *frame , *rgbFrame , *yuvFrame;
    AVPacket encoded_packet;
    
    int ret = 0;
     int frame_decoded = 0;
    int stream_index;
     int encode_success;
    while(1) {
        
        ret = av_read_frame(ifmt_ctx, &packet);
        
        if(ret < 0) {
            av_log( NULL , AV_LOG_ERROR , "error reading frame or end of file");
            break;
        }
        
        AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
        
        //for now ignore audio packets
        if(in_stream->codec->codec_type != AVMEDIA_TYPE_VIDEO){
            continue;
        }
        
        stream_index = packet.stream_index;
        frame_decoded = 0;
        frame = av_frame_alloc();
        
        av_packet_rescale_ts(&packet,
                             ifmt_ctx->streams[stream_index]->time_base,
                             ifmt_ctx->streams[stream_index]->codec->time_base);
        
        ret = avcodec_decode_video2(in_stream->codec, frame, &frame_decoded, &packet);

        if(ret<0){
            av_frame_free(&frame);
            printf("could not decode a packet....");
            return ret;
        }
        
        if(frame_decoded){
            
            
            rgbFrame = av_frame_alloc();
            yuvFrame = av_frame_alloc();
            
            convertToRGBFrame(&frame, &rgbFrame);
            convertToYuvFrame(&rgbFrame, &yuvFrame);
            
            
            frame->pts = av_frame_get_best_effort_timestamp(frame);
            yuvFrame->pts = frame->pts;
            
            av_init_packet(&encoded_packet);
           
            encode_success = 0;
            
            ret = avcodec_encode_video2(this->out_stream.format_ctx->streams[0]->codec,
                                  &encoded_packet,
                                  yuvFrame,
                                  &encode_success);
            
            encoded_packet.stream_index = 0;

            if(encode_success){
                av_packet_rescale_ts(&encoded_packet,
                                     this->out_stream.format_ctx->streams[0]->codec->time_base,
                                     this->out_stream.format_ctx->streams[0]->time_base);
            
                //encoded_packet.pts = frame->pts;
               // encoded_packet.dts = frame->pts;
                ret = av_interleaved_write_frame(this->out_stream.format_ctx, &encoded_packet);
            }
            
            
            av_free_packet(&encoded_packet);
            av_freep(rgbFrame->data);
            av_freep(yuvFrame->data);
            av_frame_free(&rgbFrame);
            av_frame_free(&yuvFrame);
            av_frame_free(&frame);
            
        }else{
            //frame was not decoded properly
            av_frame_free(&frame);
            av_log(NULL,AV_LOG_DEBUG,"frame was not fully decoded");
        }
        
       av_free_packet(&packet);

        
    }
     av_write_trailer(out_stream.format_ctx);
    cout<<"successfully decoded all video frames";
    return 0;
    
}

int VideoFileInstance::openFile() {
    int ret;
    if ((ret = avformat_open_input(&ifmt_ctx, fileName, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    //setting the member variable

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    av_dump_format(ifmt_ctx, 0, fileName, 0);
    int i;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            ) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }

    
    return 0;

}




VideoFileInstance::VideoFileInstance(int type,const char *filename){
    cout<< "creating instance of video file. of type  " << type << "\n";
    this->type = type;
    this->fileName = filename;
    openFile();
    
    openOutputFile();
    
    
}

int main(int argc, char **argv) {
    av_register_all();
    avformat_network_init();
    avfilter_register_all();
    
    
    VideoFileInstance *contentVideo = new VideoFileInstance(1,"/Users/apple/temp/merged.mp4");
    contentVideo->startDecoding();

}