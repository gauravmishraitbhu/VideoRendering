//
//  ImageFrame.cpp
//  Restreaming
//
//  Created by gaurav on 26/10/15.
//  Copyright © 2015 gaurav. All rights reserved.
//

#include "ImageFrame.h"


ImageFrame::ImageFrame(){
    this->frameIndex = 0;
    this->skipFrame = false;
    this->top = 0;
    this->left = 0;
    this->bottom = 0;
    this->right = 0;
    this->frameFileName = "";
    this->decodedFrame = NULL;
}

int ImageFrame::getHeight(){
    return this->bottom - this->top;
    //return this->bottom - this->top + 1;
}

int ImageFrame::getWidth() {
    return this->right - this->left;
    //return this->right - this->left + 1;
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
