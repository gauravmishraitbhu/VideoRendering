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
    /**
     *  @param type - 
        @param imageSeq - pointer to array of *imageSeq
        @param numImageSequence - num of template animations
        @param filename - input file name
        @param outputFile - outputfile name
        @param reportStatusEnabled - wheather to report the status of current job to a extrnal api
     */
    VideoFileInstance(int type,ImageSequence **imageSeq,int numImageSequence , const char *filename , const char *outputFile,int reportStatusEnabled );
    
    /**
     opens the provided input and output files. returns 0 is both are successfull. else 
     return < 0
     */
    int openInputAndOutputFiles();
    
    
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
    int lastReportedPercent = 0;
    
    AVFormatContext *ifmt_ctx = NULL;
    OutputStream out_stream;
    struct SwsContext *imgConvertCtxYUVToRGB = NULL;
    struct SwsContext *imgConvertCtxRGBToYUV = NULL;
    
    //pointer to array of *imageSequence
    ImageSequence **imageSequenceList;
    int numImageSequence = 0;
    //for opening file and decoder etc
    int openFile();
    //should apply for main video
    int openOutputFile();
    
    int convertToRGBFrame(AVFrame **,AVFrame **);
    int convertToYuvFrame(AVFrame ** , AVFrame **);
    
    
    /**
     *  simpley mux the audio packet from input container to output container.
     *
     *  @param AVPacket the audio packet which needs to be muxed
     *  @param AVStream source stream from which packet is taken
     *  @param AVStream destination stream where packet needs to be muxed
     *
     *  @return 0 on success , -1 if unsuccessfull
     */
    int processAudioPacket(AVPacket * , AVStream *, AVStream *);
    
    int64_t videoDuration;
    
    /**
     *  calls external rest api to inform about how much percent of current rendering is done.
     *
     *  @param percent value between 0 and 100
     */
    void reportStatus(int percent);
    
    
    /**
     @param AVPacket * - the video packet read from input container.
     if pkt.data = null then code will try to flush remaining frames from encoder.
     
     @param frameCount - total number of frames that have been encoded and muxed into container.
     
     @param framesLeftInEncoder - 0/1 indicating weather more calls to this function is required to 
     flush remaining packets.
     
     */
    int processVideoPacket(AVPacket *packet,int *frameCount,int *framesLeftInEncoder);
    
    /**
     *  encodes and write the current frame to output container.
     *
     *  @param contentFrame   the frame which contains the current data which needs to be muxed. 
                                can be NULL when flushing encoder
     *  @param packet         packet placholder where data will be encoded
     *  @param encode_success 0/1 depending on encode was success. sometimes even though no errors
                              this will be 0. this can happen in some encoders which have delay.
     *
     *  @return 0 if all is well.
     */
    int encodeWriteFrame(AVFrame *contentFrame,AVPacket *packet ,int *encode_success);
};


#endif /* defined(__Restreaming__OverlayAnimation__) */





