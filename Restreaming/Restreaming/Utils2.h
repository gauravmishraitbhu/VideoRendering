//
//  Utils2.h
//  Restreaming
//
//  Created by gaurav on 09/09/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#include <stdio.h>

extern "C"{
    
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#ifndef __Restreaming__Utils2__
#define __Restreaming__Utils2__



//placeholder for input stream datastructure
class InputStream{
public:
    AVFormatContext *format_ctx;
    AVOutputFormat *output_fmt;
} ;

//placeholder for output stream datastructure
class OutputStream {
public:
    AVFormatContext *format_ctx;
    
};

#endif /* defined(__Restreaming__Utils2__) */



