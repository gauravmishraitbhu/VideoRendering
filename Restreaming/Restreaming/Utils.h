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

extern "C"{
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>
}

#include "Utils2.h"
#include "ImageFrame.h"


int open_input_file(const char *filename,AVFormatContext ** ifmt_ctx);


/**
 *  creates and open a file for muxing audio and video packets.
 *
 *  @param filename    outfile name. absolute path.
 *  @param out_stream  handle for output file's AVFormatCtx
 *  @param inputFmtCtx handle of input AVFormatCtx
 *
 *  @return <#return value description#>
 */
int open_outputfile(const char *filename,OutputStream *out_stream,AVFormatContext *inputFmtCtx);



/**
 *  adds a single stream to a container using the codec id provided
 *
 *  @param oc          outputfile's formatCtx instance
 *  @param codec       codec instance whose value will be set by this function.
 *  @param inputStream corresponsing input stream . if you want to add a video stream to container
                        value should represent video stream of input file.
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
 *  creates a file with the codecs and codec settings copied from the input params.
 *
 *  @param filename       <#filename description#>
 *  @param outputStream   <#outputStream description#>
 *  @param videoCodec     <#videoCodec description#>
 *  @param audioCodecCtx  <#audioCodecCtx description#>
 *  @param inputFormatCtx <#inputFormatCtx description#>
 *
 *  @return <#return value description#>
 */
int open_outputfile_copy_codecs(const char *filename, OutputStream *outputStream,AVCodecContext *videoCodec , AVCodecContext *audioCodecCtx , AVFormatContext * inputFormatCtx);

void open_video(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);

void open_audio(AVFormatContext *oc, AVCodec *codec, AVDictionary *opt_arg);

/**
 @param srcFrame - rgb24 frame from which pixels need to be copy.
 @param destFrame - rgb24 frame to which pixels needs to be copied.
 */
int copyVideoPixels(AVFrame **srcFrame, AVFrame **destFrame , int srcHeight , int srcWidth, int dstHeight,int destWidth);

void copyImageFrameToCanvasSizeFrame(ImageFrame *imageFrame, AVFrame *outFrame);

void overlayFrameOnOutputFrame(AVFrame* overlayFrame, AVFrame* outFrame);

/**
 here the srcFrame is RGBA
 @param srcFrame - rgba frame from which pixels need to be copy.
 @param destFrame - rgb24 frame to which pixels needs to be copied.
 */
int copyVideoPixelsRGBA(AVFrame *srcFrame, AVFrame **destFrame , int srcHeight , int srcWidth, int dstHeight,int destWidth,
                        int startRow , int startCol);

int copyVideoPixelsRGBA (ImageFrame *imageFrame , AVFrame **destFrame , int dstHeight , int dstWidth);

jmethodID getMethodId(JNIEnv * env , jobject javaObj,const char* methodName , const char * signature );
