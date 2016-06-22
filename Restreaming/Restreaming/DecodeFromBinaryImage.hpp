//
//  DecodeFromBinaryImage.hpp
//  Restreaming
//
//  Created by gaurav on 18/03/16.
//  Copyright © 2016 gaurav. All rights reserved.
//

#ifndef DecodeFromBinaryImage_hpp
#define DecodeFromBinaryImage_hpp

#include <stdio.h>


extern "C"{
    
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class DecodeFromBinanyImage{
    
public:
    DecodeFromBinanyImage();

    void decodeFile();
    void openInputFile(uint8_t *inbuf);
    int decodeFromByteArray(uint8_t *inbuf,AVFrame **frame , int);
};

#endif /* DecodeFromBinaryImage_hpp */
