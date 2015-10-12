//
//  Utils.h
//  Restreaming
//
//  Created by gaurav on 24/07/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#ifndef __Restreaming__Utils__
#define __Restreaming__Utils__


#endif /* defined(__Restreaming__Utils__) */

#include <stdio.h>
#include <string>
#include <map>
#include <boost/any.hpp>

extern "C"{
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>
}

#include "Utils2.h"


int open_input_file(const char *filename,AVFormatContext ** ifmt_ctx);

int open_outputfile(const char *filename,OutputStream *out_stream,AVFormatContext *inputFmtCtx);


/**
 adds a single stream to a container using the codec id provided
 @param oc - output format context
 @param codec - the reference to codec.
 @param codec_id - requested codec id
 */
void add_stream(AVFormatContext *oc,
                AVCodec **codec,
                AVStream *inputStream);


/**
 same function as add_stream with only difference being the stream is created with exact same settings as 
 the input codec.
 
 */

int add_stream_from_codec(AVFormatContext *oc,
                          AVCodec **codec,
                          AVCodecContext *inputCodec);


/**
 creates a file with the codecs and codec settings copied from the input params.
 */
int open_outputfile_copy_codecs(const char *filename, OutputStream *outputStream,AVCodecContext *videoCodec , AVCodecContext *audioCodecCtx , AVFormatContext * inputFormatCtx);

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
