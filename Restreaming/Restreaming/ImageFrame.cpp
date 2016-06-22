//
//  ImageFrame.cpp
//  Restreaming
//
//  Created by gaurav on 26/10/15.
//  Copyright © 2015 gaurav. All rights reserved.
//

#include "ImageFrame.h"


ImageFrame::ImageFrame(){
    
    this->top = 0;
    this->left = 0;
   
    this->decodedFrame = NULL;
}


int ImageFrame::getHeight(){
    return height;
}

int ImageFrame::getWidth() {
    return width;
}

void ImageFrame::setHeight(int h){
    this->height = h;
}

void ImageFrame::setWidth(int w){
    this->width = w;
}

bool ImageFrame::hasDecodedFrame(){
    if (this->decodedFrame == NULL) {
        return false;
    } else {
        return true;
    }
//    return !(decodedFrame == NULL);
}

void ImageFrame::freeDecodedFrame(){
    av_frame_unref(decodedFrame);
    av_frame_free(&decodedFrame);
    decodedFrame = NULL;
}
