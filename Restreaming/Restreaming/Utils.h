//
//  Utils.h
//  Restreaming
//
//  Created by gaurav on 24/07/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#ifndef __Restreaming__Utils__
#define __Restreaming__Utils__

#include <stdio.h>
#include <string>
#include <map>
#include <boost/any.hpp>

#endif /* defined(__Restreaming__Utils__) */

//placeholder for input stream datastructure
typedef struct InputStream{
    AVFormatContext *format_ctx;
    AVOutputFormat *output_fmt;
} InputStream;

//placeholder for output stream datastructure
typedef struct OutputStream {
    AVFormatContext *format_ctx;
    
}OutputStream;

int open_input_file(const char *filename,AVFormatContext ** ifmt_ctx);

int open_outputfile(const char *filename,OutputStream *out_stream,enum AVCodecID video_codec_id,
                    enum AVCodecID audio_codec_id,int video_width,int video_height,std::map<std::string,boost::any> options);

void add_stream(AVFormatContext *oc,
                AVCodec **codec,
                enum AVCodecID codec_id,
                int codecWidth,int codecHeight,std::map<std::string,boost::any> options);

void open_video(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);

void open_audio(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);

/**
 @param srcFrame - rgb24 frame from which pixels need to be copy.
 @param destFrame - rgb24 frame to which pixels needs to be copied.
 */
int copyVideoPixels(AVFrame **srcFrame, AVFrame **destFrame , int srcHeight , int srcWidth, int dstHeight,int destWidth);

/**
 here the srcFrame is RGBA
 @param srcFrame - rgba frame from which pixels need to be copy.
 @param destFrame - rgb24 frame to which pixels needs to be copied.
 */
int copyVideoPixelsRGBA(AVFrame **srcFrame, AVFrame **destFrame , int srcHeight , int srcWidth, int dstHeight,int destWidth);
