//
//  ImageFrame.hpp
//  Restreaming
//
//  Created by gaurav on 26/10/15.
//  Copyright © 2015 gaurav. All rights reserved.
//

#ifndef ImageFrame_hpp
#define ImageFrame_hpp

extern "C"{
    
#include <libavformat/avformat.h>

    
    
}

#include <stdio.h>
#include <string>



class ImageFrame {
private:
    
    //dimention of the image
    //image files will always have integer height and width
    int height = 0;
    int width = 0;
    
public:
    int frameIndex        = 0;
    bool skipFrame        = false;
    std::string frameFileName;
    int left              = 0;
    int top               = 0;
    
    AVFrame *decodedFrame = NULL;
    
    ImageFrame();
    
    
    /**
     *
     *
     *  @return width of current frame
     */
    int getWidth();
    
    /**
     *  <#Description#>
     *
     *  @return height of current frame
     */
    int getHeight();
    
    void setWidth(int w);
    void setHeight(int h);
    
    void freeDecodedFrame();
    
    bool hasDecodedFrame();
    
};

#endif /* ImageFrame_hpp */


