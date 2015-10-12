//
//  OverlayAnimation.cpp
//  Restreaming
//
//  Created by gaurav on 20/07/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//

#include "OverlayAnimation.h"

extern "C"{
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>


#include <libavutil/timestamp.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
    
    
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
    
    
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>    
#include <libavutil/time.h>
    
}

#include "Utils.h"
#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <boost/any.hpp>
#include "ImageSequence.h"

#include <cpprest/http_client.h>
#include <cpprest/json.h>
#pragma comment(lib, "cpprest110_1_1")

using namespace web;
using namespace web::http;
using namespace web::http::client;

using namespace std;



class ReportStatusTask{
private:
    int percent;
    int uniqueId;
public:
    ReportStatusTask(int _uniqueId,int _percent){
        percent = _percent;
        uniqueId = _uniqueId;
    }
    
    
    void operator()() const{
        try{
           
            json::value postData;
            //postData.
            utility::string_t unique("uniqueId");
            utility::string_t percent_str("percent");
            postData[unique] = json::value::number(uniqueId);
            postData[percent_str] = json::value::number(percent);
            
            http_client client("http://localhost:3000/api/v1/reportstatus");
            client.request(methods::POST,"",postData ).then([](http_response resp){
                
                if(resp.status_code() == status_codes::OK){
                    cout << "successfull reported status for video id "<<endl;
                }
                
            });
        }catch(exception &e1){
            cout << e1.what();
        }

        
    }
};



int VideoFileInstance::cleanup(){
    
    avcodec_close(out_stream.format_ctx->streams[0]->codec);
    avcodec_close(ifmt_ctx->streams[0]->codec);
    
    avformat_close_input(&ifmt_ctx);
    
    
    
    if(videoType == VIDEO_TYPE_CONTENT){
        
        if (out_stream.format_ctx && !(out_stream.format_ctx->oformat->flags & AVFMT_NOFILE))
            avio_close(out_stream.format_ctx->pb);
        
        
        avformat_free_context(out_stream.format_ctx);
    }
    
    
    
    
    return 0;
}

int VideoFileInstance::openOutputFile() {
    
    std::map<string,boost::any> videoOptions ;
    AVCodecContext *videoCodecCtx , *audioCodecCtx;
    videoCodecCtx = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec;
    videoOptions["bitrate"] = (int)(videoCodecCtx->bit_rate);
    videoOptions["timebase_denominator"] = videoCodecCtx->time_base.den;
    videoOptions["timebase_numerator"] = videoCodecCtx->time_base.num;
    
    audioCodecCtx = ifmt_ctx->streams[1]->codec;
    videoOptions["frame_size"] = audioCodecCtx->frame_size;
    videoOptions["audio_bitrate"] = audioCodecCtx->bit_rate;
    videoOptions["audio_sample_rate"] = audioCodecCtx->sample_rate;
    videoOptions["channel_layout"] = audioCodecCtx->channel_layout;
    
    cout << boost::any_cast<int>(videoOptions["bitrate"]);
    
    int ret = open_outputfile(outputFilePath,
                              &out_stream,ifmt_ctx);
    
//    int ret = open_outputfile_copy_codecs(outputFilePath , &out_stream , ifmt_ctx->streams[0]->codec,ifmt_ctx->streams[1]->codec,ifmt_ctx);
    
    
    return ret;
    
}


int VideoFileInstance::processAudioPacket(AVPacket *packet , AVStream *in_stream , AVStream * output_stream){
    packet->pts = av_rescale_q(packet->pts, in_stream->time_base, output_stream->time_base);
    packet->dts = av_rescale_q(packet->dts, in_stream->time_base, output_stream->time_base);
    packet->duration = av_rescale_q(packet->duration, in_stream->time_base, output_stream->time_base);
    int ret = 0;
    //packet.pts =
    //packet.pos = -1;
    //log_packet(ofmt_ctx, &pkt, "out");
    //packet.pts = frameCnt ++;
    ret = av_interleaved_write_frame(out_stream.format_ctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error muxing packet\n");
        
    }
    av_free_packet(packet);
    
    return ret;
}

int VideoFileInstance::processVideoFrame(AVPacket *packet , int *frameEncodedCount){
    
    AVFrame *contentVideoFrame, *contentVideoRGB , *contentVideoFinalYUV;
    AVFrame *animationVideoFrame , *animationVideoRGB;
    int lastReportedPercent  = 0,percentGaps;
    AVPacket encodedPacket;
    
    int ret = 0;
    int stream_index = packet->stream_index;
    
    int frame_decoded = 0,encode_success = 0;
    contentVideoFrame = av_frame_alloc();
    AVStream *in_stream = ifmt_ctx->streams[stream_index];
    
    //pts should be multiple of 1/fps / timebase
    //eg fps = 24 and timebase is 1/48 then pts should be 2,4,6 etc
    double frameRate = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->r_frame_rate.num/ifmt_ctx->streams[VIDEO_STREAM_INDEX]->r_frame_rate.den;
    double timebase = (double)ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->time_base.num / (double)ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->time_base.den;
    
    double ptsFactorFloat =  1 /(frameRate * timebase) ;
    double ptsFactor = round(ptsFactorFloat);
    
    if(videoDuration < 10){
        percentGaps = 50;
    }else if(videoDuration >=10 && videoDuration < 30){
        percentGaps = 25;
    }else{
        percentGaps = 10;
    }

    
    int64_t pts;
    
    ret = avcodec_decode_video2(in_stream->codec,
                                contentVideoFrame,
                                &frame_decoded,
                                packet
                                );
    
    
    
    if(ret<0){
        av_frame_free(&contentVideoFrame);
        fprintf(stderr,"could not decode a packet....");
        return ret;
    }
    
    if(frame_decoded){
        
        pts = av_frame_get_best_effort_timestamp(contentVideoFrame);
        contentVideoFrame->pts = pts;
        
        
        // now we have frame from content video
        
        contentVideoRGB = av_frame_alloc();
        animationVideoFrame = av_frame_alloc();
        animationVideoRGB = av_frame_alloc();
        contentVideoFinalYUV = av_frame_alloc();
        
        //convert the frame to rgb
        convertToRGBFrame(&contentVideoFrame, &contentVideoRGB);
        
        int contentFrameHeight = this->getVideoHeight();
        int contentFrameWidth = this->getVideoWidth();
        
        float wallClockTimeContentVideo = timebase * ((*frameEncodedCount)+1) * ptsFactor;
        float animatonTimeOffset = imageSequence->getOffetTime();
        
        float currentPercent = wallClockTimeContentVideo/(float)videoDuration;
        
        currentPercent *= 100;
        
        int nearestPercent = currentPercent - (int)currentPercent % percentGaps;
        
        if(nearestPercent != 0 && nearestPercent != lastReportedPercent){
            
            reportStatus(nearestPercent);
            lastReportedPercent = nearestPercent;
        }

        if(wallClockTimeContentVideo > animatonTimeOffset){
            
                AVFrame * imageFrame = imageSequence->getFrame(timebase,ptsFactor,*frameEncodedCount+1);
                
                if(imageFrame != NULL){
                    int animationFrameHeight = imageSequence->getVideoHeight();
                    int animationFrameWidth = imageSequence->getVideoWidth();
                    
                    copyVideoPixelsRGBA(&imageFrame ,
                                        &contentVideoRGB,
                                        animationFrameHeight , animationFrameWidth,
                                        contentFrameHeight , contentFrameWidth);
                    
                    
            
                
                
                }
            
        }
        
        //convert the content rgb to yuv
        convertToYuvFrame(&contentVideoRGB, &contentVideoFinalYUV);
        
        AVCodecContext *codecCtx = this->out_stream.format_ctx->streams[0]->codec;
        contentVideoFinalYUV->pts = ptsFactor * (*frameEncodedCount) ;
        
        contentVideoFinalYUV->format = codecCtx->pix_fmt;
        contentVideoFinalYUV->height = codecCtx->height;
        contentVideoFinalYUV->width = codecCtx->width;
        
        (*frameEncodedCount)++;
        //av_log(NULL,AV_LOG_INFO,"frame numer %d",frameEncodedCount);
        //encode the packet
        av_init_packet(&encodedPacket);
        encode_success = 0;
        ret = avcodec_encode_video2(this->out_stream.format_ctx->streams[0]->codec,
                                    &encodedPacket,
                                    contentVideoFinalYUV,
                                    &encode_success);
        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR , "Error encoding a frame");
            
        }
        
        
        
        // encode the packet and mux it into the container
        if(encode_success){
            encodedPacket.stream_index = VIDEO_STREAM_INDEX;
            
            
            av_packet_rescale_ts(&encodedPacket,
                                 this->out_stream.format_ctx->streams[0]->codec->time_base,
                                 this->out_stream.format_ctx->streams[0]->time_base);
            
            
            ret = av_interleaved_write_frame(this->out_stream.format_ctx, &encodedPacket);
            
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR , "Error writing  a frame to container...");
                return ret;
            }
            
        }else{
            //av_log(NULL,AV_LOG_ERROR , "Error encoding a frame");
            
        }

        //cleanup
        av_freep(contentVideoRGB->data);
        av_frame_free(&contentVideoRGB);
        
        av_frame_free(&contentVideoFrame);
        
        
        av_freep(animationVideoRGB->data);
        av_frame_free(&animationVideoRGB);
        
        
        av_freep(contentVideoFinalYUV->data);
        av_frame_free(&contentVideoFinalYUV);
        
        av_free_packet(&encodedPacket);

        
    }else{
        //decode unsuccessfull
        
        av_frame_free(&contentVideoFrame);
    }//end of if(frame_decoded)
    
    av_free_packet(packet);
    
    return ret;

}

int VideoFileInstance::startOverlaying(){
    

    if(videoType == VIDEO_TYPE_ANIMATION){
        av_log(NULL,AV_LOG_ERROR,"startOverlaying should not be called on animation video.");
        return -1;
    }

    
    int ret;
    AVPacket packet;
   

    int frameEncodedCount=0;
    
    
    
    while(1){
        
        
        av_init_packet(&packet);
        ret = av_read_frame(ifmt_ctx, &packet);
        
        if(ret < 0) {
            reportStatus(100);
            av_log( NULL , AV_LOG_ERROR , "error reading frame or end of file");
            break;
        }
        
        AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
        AVStream *output_stream;
        
        
        if(in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            continue;
            //for audio packets simply mux them into output container.
            //no need to decode and encode them.
            output_stream = out_stream.format_ctx->streams[packet.stream_index];
            
            
            ret = processAudioPacket(&packet, in_stream, output_stream);
            if(ret < 0) {
                av_log( NULL , AV_LOG_ERROR , "error while processing audio packet");
                return ret;
            }
            
            continue;
        }else{
            //for now ignore audio packets later will just mux audio packets
            //into the container currently the contaner doesnt have the
            //audi channel
            if(in_stream->codec->codec_type != AVMEDIA_TYPE_VIDEO){
                continue;
            }
            
            ret = processVideoFrame(&packet, &frameEncodedCount);
            if(ret < 0){
                av_log( NULL , AV_LOG_ERROR , "error while processing video packet");
                continue;
            }
        }
        
        
        
    }
    
    ret = av_write_trailer(out_stream.format_ctx);
    if(ret < 0){
        av_log(NULL,AV_LOG_ERROR , "Error writing  a trailer to container...");
    }
    
    return ret;
}

int VideoFileInstance::startDecoding() {
    
    AVPacket packet;
    av_init_packet(&packet);
    
    AVFrame *frame , *rgbFrame , *yuvFrame;
    AVPacket encoded_packet;
    
    int ret = 0;
    int frame_decoded = 0;
    int stream_index;
    int encode_success;
    int frameEncodedCount = 0;
    
    //pts should be multiple of 1/fps / timebase
    //eg fps = 24 and timebase is 1/48 then pts should be 2,4,6 etc
    double frameRate = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->r_frame_rate.num/ifmt_ctx->streams[VIDEO_STREAM_INDEX]->r_frame_rate.den;
    double timebase = (double)ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->time_base.num / (double)ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->time_base.den;
    
    double ptsFactorFloat =  1 /(frameRate * timebase) ;
    int ptsFactor = round(ptsFactorFloat);
    while(1) {
        
        ret = av_read_frame(ifmt_ctx, &packet);
        
        if(ret < 0) {
            av_log( NULL , AV_LOG_ERROR , "error reading frame or end of file");
            break;
        }
        
        AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
        AVStream *output_stream = out_stream.format_ctx->streams[packet.stream_index];
        
        //for now ignore audio packets
        if(in_stream->codec->codec_type != AVMEDIA_TYPE_VIDEO && in_stream->codec->codec_type != AVMEDIA_TYPE_AUDIO){
            continue;
        }
        
        if(in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            continue;
            
            //            packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, output_stream->time_base
            //                                          , AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            //                    packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            
            
            packet.duration = av_rescale_q(packet.duration, in_stream->time_base, output_stream->time_base);
            packet.pos = -1;
            //log_packet(ofmt_ctx, &pkt, "out");
            
            ret = av_interleaved_write_frame(out_stream.format_ctx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error muxing packet\n");
                break;
            }
            av_free_packet(&packet);
            
            
            
        }else{
            
            stream_index = packet.stream_index;
            frame_decoded = 0;
            frame = av_frame_alloc();
            
//            av_packet_rescale_ts(&packet,
//                                 ifmt_ctx->streams[stream_index]->time_base,
//                                 ifmt_ctx->streams[stream_index]->codec->time_base);
            
            ret = avcodec_decode_video2(in_stream->codec, frame, &frame_decoded, &packet);
            
            if(ret<0){
                av_frame_free(&frame);
                fprintf(stderr,"could not decode a packet....");
                return ret;
            }
            
            if(frame_decoded){
                
                
                rgbFrame = av_frame_alloc();
                yuvFrame = av_frame_alloc();
                
                // convertToRGBFrame(&frame, &rgbFrame);
                //convertToYuvFrame(&rgbFrame, &yuvFrame);
                
                
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                frame->pts = ptsFactor*frameEncodedCount ;
                frameEncodedCount++;
                
                av_init_packet(&encoded_packet);
                
                encode_success = 0;
                
                ret = avcodec_encode_video2(this->out_stream.format_ctx->streams[0]->codec,
                                            &encoded_packet,
                                            frame,
                                            &encode_success);
                
                encoded_packet.stream_index = 0;
                
                if(encode_success){
                    
                    av_packet_rescale_ts(&encoded_packet,
                                         this->out_stream.format_ctx->streams[0]->codec->time_base,
                                         this->out_stream.format_ctx->streams[0]->time_base);
                    
                    //encoded_packet.pts = frame->pts;
                    // encoded_packet.dts = frame->pts;
                    ret = av_interleaved_write_frame(this->out_stream.format_ctx, &encoded_packet);
                }
                
                
                av_free_packet(&encoded_packet);
                av_freep(rgbFrame->data);
                av_freep(yuvFrame->data);
                av_frame_free(&rgbFrame);
                av_frame_free(&yuvFrame);
                av_frame_free(&frame);
                
            }else{
                //frame was not decoded properly
                av_frame_free(&frame);
                av_log(NULL,AV_LOG_DEBUG,"frame was not fully decoded");
            }
            
            av_free_packet(&packet);
            
            
            
        }
        
    }
    av_write_trailer(out_stream.format_ctx);
    cout<<"successfully decoded all video frames";
    return 0;
    
}


//audio frames will be discarded this function should only be called for animation video.
int VideoFileInstance::getSingleFrame(AVFrame **frame) {
    
    
    if(videoType == VIDEO_TYPE_CONTENT){
        av_log(NULL,AV_LOG_ERROR,"getSingleFrame should not be called on content video. this can cause loss of audio data");
        return -1;
    }
    
    int ret = 0;
    AVPacket packet;
    av_init_packet(&packet);
    int frameDecoded = 0;
    bool gotFrame = false;
    
    while(!gotFrame){
        av_init_packet(&packet);
        
        //get a packet out of container
        ret = av_read_frame(ifmt_ctx, &packet);
        
        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR,"no more frames left to be read");
            return ret ;
        }
        
        int stream_index = packet.stream_index;
        
        if(stream_index != VIDEO_STREAM_INDEX){
            //if not video frame just discard the frame.
            //in animation video audio frames are not expected
            continue;
        }
        
        AVStream *in_stream = ifmt_ctx->streams[packet.stream_index];
        *frame = av_frame_alloc();
        
        av_packet_rescale_ts(&packet,
                             ifmt_ctx->streams[stream_index]->time_base,
                             ifmt_ctx->streams[stream_index]->codec->time_base);
        
        ret = avcodec_decode_video2(in_stream->codec, (*frame), &frameDecoded, &packet);
        
        if(ret<0){
            av_frame_free(frame);
            printf("could not decode a packet....trying next packet");
            continue;
        }
        
        if(frameDecoded){
            //at this point frame points to a decoded frame so
            //job done for this function
            ret = 0;
            av_free_packet(&packet);
            gotFrame = true;
        }else{
            av_free_packet(&packet);
            av_frame_free(frame);
            continue;
        }
        
        
    }
    
    return ret;
}

int VideoFileInstance::convertToRGBFrame(AVFrame **yuvframe,AVFrame **rgbPictInfo) {
    int ret;
    int width = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->width;
    int height = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->height;
    
    
    //init context if not done already.
    if (imgConvertCtxYUVToRGB == NULL) {
        //init once
        imgConvertCtxYUVToRGB = sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
        
        if(imgConvertCtxYUVToRGB == NULL) {
            av_log(NULL,AV_LOG_ERROR,"error creating img context");
            return -1;
        }
        
    }
    
    
    // call av_freep(rgbPictInfo->data) to free memory
    
    av_image_alloc( (*rgbPictInfo)->data,   //data to be filled
                   (*rgbPictInfo)->linesize,//line sizes to be filled
                   width, height,
                   AV_PIX_FMT_RGB24,           //pixel format
                   32                       //aling
                   );
    
    
    
    ret = sws_scale(imgConvertCtxYUVToRGB, (*yuvframe)->data, (*yuvframe)->linesize, 0, height,
                    (*rgbPictInfo)->data, (*rgbPictInfo)->linesize);
    
    
    return ret;
}

int VideoFileInstance::convertToYuvFrame (AVFrame **rgbFrame , AVFrame ** yuvFrame) {
    int ret = 0;
    int width = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->width;
    int height = ifmt_ctx->streams[VIDEO_STREAM_INDEX]->codec->height;
    
    if(imgConvertCtxRGBToYUV == NULL) {
        imgConvertCtxRGBToYUV = sws_getContext(width, height, AV_PIX_FMT_RGB24, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
        
        if(imgConvertCtxRGBToYUV == NULL){
            av_log(NULL,AV_LOG_ERROR,"error creating img context");
            return -1;
        }
    }
    
    
    av_image_alloc( (*yuvFrame)->data,   //data to be filled
                   (*yuvFrame)->linesize, //line sizes to be filled
                   width, height,
                   AV_PIX_FMT_YUV420P,        //pixel format
                   32                      //aling
                   );
    
    sws_scale(imgConvertCtxRGBToYUV,(*rgbFrame)->data , (*rgbFrame)->linesize, 0, height,
              (*yuvFrame)->data , (*yuvFrame)->linesize);
    
    
    
    return ret;
}


int VideoFileInstance::openFile() {
    int ret;
    if ((ret = avformat_open_input(&ifmt_ctx, fileName, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    //setting the member variable
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    av_dump_format(ifmt_ctx, 0, fileName, 0);
    
    videoDuration = ifmt_ctx->duration / 1000000;
    int i;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            ) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }
    
    
    return 0;
    
}




VideoFileInstance::VideoFileInstance(int type,ImageSequence * imageSeq , const char *filename , const char *outputFile,int reportStatusEnabled){
    cout<< "creating instance of video file. of type  " << type << "\n";
    this->videoType = type;
    this->fileName = filename;
    this->imageSequence = imageSeq;
    this->outputFilePath = outputFile;
    this->reportStatusEnabled = reportStatusEnabled;
    
    
}

int VideoFileInstance::openInputAndOutputFiles(){
    int ret = openFile();
    
    if(ret < 0){
        return ret;
    }else{
        
        if(this->videoType == VIDEO_TYPE_CONTENT){
            ret = openOutputFile();
        }
        
    }
    return ret;

}

void VideoFileInstance::reportStatus(int percent) {
    
    if(reportStatusEnabled){
    
        string s = "reporting status for video id" + std::to_string( uniqueId ) + "percent == " + std::to_string(percent);

        cout << s;
        av_log(NULL, AV_LOG_DEBUG,s.c_str());
        ReportStatusTask task(uniqueId,percent);
        std::thread thread(task);
        thread.detach();
    }
}

//int main(){
//    
//    try{
//        
//    }catch (exception &e){
//        cout << e.what();
//    }
//    
//    
//    getchar();
//    //thread.detach();
//}

