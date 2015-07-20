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
#include <libavutil/samplefmt.h>

#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

#include <libavutil/time.h>
#include "myLib.h"
}

#include <iostream>
#include <string>

using namespace std;




class VideoFileInstance {
    
public:
    VideoFileInstance(int , const char *);
    int startDecoding();
    
private:
    const char *fileName;
    int type; //1 = content video 2 = animation video
    AVFormatContext *ifmt_ctx = NULL;
    OutputStream out_stream;
    
    //for opening file and decoder etc
    int openFile();
    int openOutputFile();
    
};

int VideoFileInstance::openOutputFile() {
    open_outputfile("/Users/apple/temp/sample_output.mp4", &out_stream , CODEC_ID_H264, AV_CODEC_ID_NONE, ifmt_ctx->streams[0]->codec->width,ifmt_ctx->streams[0]->codec->height);
    
    
    
    return 0;
}

int VideoFileInstance::startDecoding() {
    
    AVPacket packet = { .data = NULL, .size = 0 };
    
    AVFrame *frame;
    
    int ret = 0;
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
        int got_frame = 0;
        frame = av_frame_alloc();
        ret = avcodec_decode_video2(in_stream->codec, frame, &got_frame, &packet);

        
        
        if(ret<0){
            av_frame_free(&frame);
            printf("could not decode a packet....");
            return ret;
        }
        
        
        
    }
    
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
    
    
    VideoFileInstance *contentVideo = new VideoFileInstance(1,"/Users/apple/temp/sample.mp4");
    contentVideo->startDecoding();

}
