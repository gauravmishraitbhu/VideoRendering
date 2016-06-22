//
//  ImageSequence.h
//  Restreaming
//
//  Created by gaurav on 03/08/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

extern "C"{

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <jni.h>

}

#include <stdio.h>
#include <string>
#include <map>
#include "ImageFrame.h"


#ifndef IMAGESEQUENCE_HPP
#define IMAGESEQUENCE_HPP

class ImageSequence{

public:
    ImageSequence(JNIEnv * env, jobject obj);

    /**
     returns pointer to next frame in sequence.
     */
    ImageFrame * getFrame(float contentVideoTimeBase ,float ptsFactor, int contentVideoPts);

    ImageFrame* getFrameAtTime(float videoTime);

    int getZIndex(){
        return zIndex;
    }

    void setZIndex(int num){
        zIndex = num;
    }

    int getFps(){
        return fps;
    }

    void setFps(int _fps){
        fps = _fps;
    }

    float getOffsetTime(){
        return offsetTime;
    }

    void setOffsetTime(float time){
        offsetTime = time;
    }

    int getNumFrames(){
        return maxNumofFrames;
    }

    void setNumFrames(int num){
        maxNumofFrames = num;
    }

    std::string getSeqId(){
        return seqId;
    }

    void setSeqId(std::string _seqId){
        seqId = _seqId;
    }


    void cleanup();
private:
    //the decoded frame number, ie if currently the instance is holding decoded data for frame number 3
    //then this variable will be 3. starting from 1.
    int currentFrameNum=-1;

    //this holds the image url which was last decoded
    std::string currentDecodedImageUrl = "";

    //should be read from some meta file
    int maxNumofFrames = 240;


    //will be used while overlapping multiple images on top of video
    int zIndex = 1;

    //time after which animation video will start on content video timeline. 0 both video will start at same time
    float offsetTime = 0;

    std::string seqId;

    //JNI specific stuff will be used to call parent java thread
    JNIEnv * env;
    jobject javaObj;

    /**
     fps of the animation sequence. ie how many images represent 1 sec worth of animation.
     */
    int fps=30;



    ImageFrame *currFrame = NULL;

    //handle for png decoder
//    AVCodecContext *codecCtx = NULL;

    /**
     *  calculates the next animation frame which needs to be displayed on content video at any time

     *
     *  @param contentVideoTimeBase timebase used in content video
     *  @param ptsFactor            ptsFactor =  1 /(frameRate * timebase) ;

     *  @param contentVideoPts       pts of current frame in content video
     *
     *  @return returns the frame num which should be rendered from this animation
      -1 if the the startTime for the current animation has not passed yet.
     */
    int calculateNextFrameNumber(float contentVideoTimeBase , float ptsFactor,int contentVideoPts);

    int calculateNextFrameNumberForTime(float videoTime);

    ImageFrame* getFrameForNum(int nextFrameNum);

    int decodeFromByteArray(uint8_t *inbuf , AVFrame **frame , int dataLength);

    void convertJavaImageFrameToNativeImageFrame(JNIEnv * env , jobject imageFrameJava , ImageFrame ** imgFrameNative,
                                                               int *gotNewFrame);
};

#endif
