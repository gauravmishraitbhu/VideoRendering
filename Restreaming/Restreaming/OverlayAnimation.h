//
//  OverlayAnimation.h
//  Restreaming
//
//  Created by gaurav on 20/07/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//


extern "C"{
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>
}

#include "ImageSequence.h"
#include "Utils.h"
#include <stdio.h>

#ifndef __Restreaming__OverlayAnimation__


#define __Restreaming__OverlayAnimation__



//Each class instance will represent a video file. either content video or animation video.
class VideoFileInstance {
    
public:
    VideoFileInstance(int ,ImageSequence *, const char *,const char *,int );
    int startDecoding();
    
    /**
     reads packet from input format context and decode it to frame.
     @param frame the frame in which decoded data will be returned.
     */
    int getSingleFrame(AVFrame **frame);
    
    /**
     to be called for content video. this will run the main loop of program
     which will open video frame by frame and overlay respective animation frame
     on top of animation video.
     */
    int startOverlaying();
    
    
    //test function to test out remuxing code
    int remux();
    
    int getVideoHeight() {
        return ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->height;
    }
    
    int getVideoWidth() {
        return ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->width;
    }
    
    void setUniqueId(int _id){
        this->uniqueId = _id;
    }
    
    int getUniqueId(){
        return uniqueId;
    }
    
    /**
     for releasing context and closing codec etc.
     */
    int cleanup();
    
    
private:
    const char *fileName , *outputFilePath;
    //unique id of the ffmpeg job. external apis will use this id
    //to monitor status of job.
    int uniqueId = 1;
    int reportStatusEnabled;
    int VIDEO_STREAM_INDEX = 0;
    int VIDEO_TYPE_CONTENT = 1 , VIDEO_TYPE_ANIMATION = 2;
    int videoType; //1 = content video 2 = animation video
    AVFormatContext *ifmt_ctx = NULL;
    OutputStream out_stream;
    struct SwsContext *imgConvertCtxYUVToRGB = NULL;
    struct SwsContext *imgConvertCtxRGBToYUV = NULL;
    ImageSequence *imageSequence;
    //for opening file and decoder etc
    int openFile();
    //should apply for main video
    int openOutputFile();
    
    int convertToRGBFrame(AVFrame **,AVFrame **);
    int convertToYuvFrame(AVFrame ** , AVFrame **);
    
    int processAudioPacket(AVPacket * , AVStream *, AVStream *);
    
    int64_t videoDuration;
    
    void reportStatus(int percent);
};


#endif /* defined(__Restreaming__OverlayAnimation__) */





