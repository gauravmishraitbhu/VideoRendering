//
//  myLib.h
//  Restreaming
//
//  Created by gaurav on 11/11/14.
//  Copyright (c) 2014 gaurav. All rights reserved.
//

#ifndef __Restreaming__myLib__
#define __Restreaming__myLib__

#include <stdio.h>

#endif /* defined(__Restreaming__myLib__) */

//placeholder for input stream datastructure
typedef struct InputStream{
    AVFormatContext *format_ctx;
    AVOutputFormat *output_fmt;
} InputStream;

//placeholder for output stream datastructure
typedef struct OutputStream {
    AVFormatContext *format_ctx;
    
}OutputStream;

int open_input_file(const char *filename,InputStream *input_stream);

int open_outputfile(const char *filename,OutputStream *out_stream,enum AVCodecID video_codec_id,
                     enum AVCodecID audio_codec_id,int video_width,int video_height);

void add_stream(AVFormatContext *oc,
                AVCodec **codec,
                enum AVCodecID codec_id,
                int codecWidth,int codecHeight);

void open_video(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);

void open_audio(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);