//
//  ImageSequence.cpp
//  Restreaming
//
//  Created by gaurav on 03/08/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#include "ImageSequence.h"

#include <string>


using namespace std;



ImageSequence::ImageSequence(const char *fileName,int initialFile,float offsetTime,int _fps,int maxNumFrames){
    this->baseFileName = fileName;
    this->intitialFileSeqCnt = initialFile;
    
    currentFrameNum = -1;
    fps = _fps;
    maxNumofFrames = maxNumFrames;
    this->offsetTime = offsetTime;
    
}

float ImageSequence::getOffetTime(){
    return offsetTime;
}

void ImageSequence::closeFile(){
    
    if(ifmt_ctx != NULL){
        avcodec_close(ifmt_ctx->streams[0]->codec);
        
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
}

int ImageSequence::openFile(const char *fileName){
    int ret = 0;
    unsigned int i;
    
   
    
    
    if ((ret = avformat_open_input(&ifmt_ctx, fileName, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    //setting the member variable
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    for (i = 0; i < (ifmt_ctx)->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = (ifmt_ctx)->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            /* Open decoder */
            codec_ctx->thread_count = 1; // this is required for some reason
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            
            height = codec_ctx->height;
            width = codec_ctx->width;
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }
    
    
    
    //av_dump_format(ifmt_ctx, 0, fileName, 0);
    return ret;
    
    
   // av_log(NULL,AV_LOG_INFO,"opened first file");
}

AVFrame * ImageSequence::getFrame(float contentVideoTimeBase , float ptsFactor,int contentVideoPts){
    
    
    int nextFrameNum = calculateNextFrameNumber(contentVideoTimeBase,  ptsFactor,contentVideoPts);
    int ret = 0;
    //responsility of caller to handle NULL
    if(nextFrameNum > maxNumofFrames){
        return NULL;
    }
    
    //decode next frame only when required.
    if(nextFrameNum > currentFrameNum){
        currentFrameNum = nextFrameNum;
        int currSeq = intitialFileSeqCnt + currentFrameNum - 1;
        
        std::string completeName = std::string(baseFileName) + std::string("frame")+std::to_string(currSeq) + std::string(".png");
        ret = openFile(completeName.c_str());
        if(ret < 0){
            av_log(NULL, AV_LOG_ERROR, "Error opening a image file.... %s" , completeName.c_str());
            return NULL;
        }
        //openFile("/Users/apple/temp/frame11.png");
        
        AVPacket packet;
        
        int frameDecoded = 0;
        bool gotFrame = false;
        int ret;
        AVFrame *frame;
        
        
        
        while(1){
            av_init_packet(&packet);
            
            //get a packet out of container
            ret = av_read_frame(ifmt_ctx, &packet);
            
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR,"no more frames left to be read");
                currFrame = NULL;
                return currFrame ;
            }
            
            int stream_index = packet.stream_index;
            
            if(stream_index != 0){
                //if not video frame just discard the frame.
                //in animation video audio frames are not expected
                continue;
            }
            
            
            AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
            frame = av_frame_alloc();
            
            
            ret = avcodec_decode_video2(in_stream->codec, (frame), &frameDecoded, &packet);
            
            if(ret<0){
                av_frame_free(&frame);
                printf("could not decode a packet....trying next packet");
                continue;
            }
            
            if(frameDecoded){
                //at this point frame points to a decoded frame so
                //job done for this function
                
                av_free_packet(&packet);
                gotFrame = true;
                //copy frame and return;
                
                //cleanup older frame
                if(currFrame != NULL){
                    
                    av_frame_unref(currFrame);
                    av_frame_free(&currFrame);
                }
                
                currFrame = av_frame_clone(frame);
                av_frame_free(&frame);
                break;
            }else{
                av_free_packet(&packet);
                av_frame_free(&frame);
                continue;
            }
            
            
        }
    
    } //end of if (nextFrame>currFrame)
    
    closeFile();
    
    return currFrame;
}

int ImageSequence::calculateNextFrameNumber(float contentVideoTimeBase ,float ptsFactor ,int contentVideoPts){
    if(currentFrameNum == -1){
        //first time call.
        return 1;
    }
    
    float wallClockTimeContentVideo = contentVideoTimeBase * contentVideoPts * ptsFactor;
    
    //if animation video is supposed to be delayed by some time then the above time needs to be
    //adjusted by subtracting the offset time eg if offset is 5 sec and absolute walltime comes
    //out to be 6 secs then we need to show the frame which is supposed to be at t=1 secs
    
    wallClockTimeContentVideo = wallClockTimeContentVideo - offsetTime;
    
    //the time till which current frame needs to be displayed
    float wallClockTimeForCurrentAnimationFrame = float(1/(float)fps) * currentFrameNum;
    
    if(wallClockTimeContentVideo > wallClockTimeForCurrentAnimationFrame){
        //time for next animation frame
        return (currentFrameNum+1);
    }else{
        //no need for decoding next image . simply return current one.
        return currentFrameNum;
    }
}

int ImageSequence::getVideoHeight() {
    return height;
}

int ImageSequence::getVideoWidth() {
    return width;
}