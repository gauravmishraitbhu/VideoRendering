//
//  SaveRgbImageHelper.hpp
//  Restreaming
//
//  Created by gaurav on 29/01/16.
//  Copyright © 2016 gaurav. All rights reserved.
//

#ifndef SaveRgbImageHelper_hpp
#define SaveRgbImageHelper_hpp

#include <stdio.h>

extern "C"{
    
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class SaveImageHelper{
    
public:
    SaveImageHelper();

    int saveImageRGBImageToDisk(const char *fileName , AVFrame *outputRGBFrame,int frameWidth , int frameHeight);
    
    
private:
    
    
};

#endif /* SaveRgbImageHelper_hpp */
