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
    

}

#include<cpprest/json.h>
#include <stdio.h>
#include <string>
#include "ImageFrame.h"


#ifndef IMAGESEQUENCE_HPP
#define IMAGESEQUENCE_HPP

class ImageSequence{
    
public:
    ImageSequence(std::string filename);
    
    /**
     returns pointer to next frame in sequence.
     */
    ImageFrame * getFrame(float contentVideoTimeBase ,float ptsFactor, int contentVideoPts);
    
    int getZIndex(){
        return zIndex;
    }
    
    void setZIndex(int num){
        zIndex = num;
    }
    
    int getVideoHeight();
    int getVideoWidth();
    
    float getOffetTime();
    
    void cleanup();
private:
    //the decoded frame number, ie if currently the instance is holding decoded data for frame number 3
    //then this variable will be 3. starting from 1.
    int currentFrameNum=0;
    
    //should be read from some meta file
    int maxNumofFrames = 240;
    
//    //file name suffix. fo eg the folder contains img11.png img12.png then this count should be 11.
//    int intitialFileSeqCnt = 11;
    
    
    //will be used while overlapping multiple images on top of video
    int zIndex = 1;
    
    //time after which animation video will start on content video timeline. 0 both video will start at same time
    float offsetTime = 0;
    
    
    /**
     fps of the animation sequence. ie how many images represent 1 sec worth of animation.
     */
    int fps;
    
    std::map<std::string,ImageFrame*> frameMap ;
    
    int height,width;
    
    std::string baseFileName;
    ImageFrame *currFrame = NULL;
    
    //will be used again and again for various png files
    AVFormatContext *ifmt_ctx = NULL;
    
    
    
    int openFile(const char * fileName);
    
    
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
    
    void closeFile();
    
    void parseMetaFile(const char *);
    
    void readJson(const web::json::value& );
};

#endif



