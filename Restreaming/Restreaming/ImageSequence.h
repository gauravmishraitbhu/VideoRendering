//
//  ImageSequence.h
//  Restreaming
//
//  Created by gaurav on 03/08/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

extern "C"{
    
#include <libavformat/avformat.h>
#include <libavfilter/avcodec.h>
    
}
#include <stdio.h>



#ifndef IMAGESEQUENCE_HPP
#define IMAGESEQUENCE_HPP

class ImageSequence{
    
public:
    ImageSequence(char const *,int initialFile,float offsetTime,int fps,int maxFrames);
    
    /**
     returns pointer to next frame in sequence.
     */
    AVFrame * getFrame(float contentVideoTimeBase ,float ptsFactor, int contentVideoPts);
    
    int getVideoHeight();
    int getVideoWidth();
private:
    //the decoded frame number, ie if currently the instance is holding decoded data for frame number 3
    //then this variable will be 3. starting from 1.
    int currentFrameNum=0;
    
    //should be read from some meta file
    int maxNumofFrames = 240;
    
    //file name suffix. fo eg the folder contains img11.png img12.png then this count should be 11.
    int intitialFileSeqCnt = 11;
    
    //time after which animation video will start on content video timeline. 0 both video will start at same time
    float offsetTime = 0;
    
    /**
     fps of the animation sequence. ie how many images represent 1 sec worth of animation.
     */
    int fps;
    
    int height,width;
    
    const char *baseFileName;
    AVFrame *currFrame = NULL;
    AVFormatContext *ifmt_ctx = NULL;
    
    void openFile(const char * fileName);
    
    int calculateNextFrameNumber(float contentVideoTimeBase , float ptsFactor,int contentVideoPts);
    
    void closeFile();
    
};

#endif



