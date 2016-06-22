

#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <thread>
#include "ImageSequence.h"
#include "ImageFrame.h"
#include "OverlayAnimation.h"
#include "Utils.h"

extern "C"{

#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>

#include <jni.h>
#include "com_typito_exporter_RenderJobThread.h"

}

using namespace std;

#define TRACE(msg)            wcout << msg
#define TRACE_ACTION(a, k, v) wcout << a << L" (" << k << L", " << v << L")\n"

std::string convertJStringToStr(JNIEnv* env, jstring s)
{
    if(s == NULL || env == NULL) return string("");
    const char* str = env->GetStringUTFChars(s, 0);
    string ret = str;
    env->ReleaseStringUTFChars(s, str);

    return ret;
}

jstring convertStrToJString(JNIEnv* env, std::string s)
{
    return env->NewStringUTF(s.c_str());
}

class RenderJobContext {
public:

    JNIEnv* env;
    jobject jobj;

    std::string jobId;

    std::string outputPath;
    std::vector<std::string> inputPaths;
    std::vector<double> inputClipStartTimes;
    std::vector<double> inputClipEndTimes;

    ImageSequence** imageSequenceList;
    int imageSequenceListLength;

    int canvasWidth;
    int canvasHeight;

    float job_duration;

    std::vector<AVFormatContext*> ifmt_ctx_v;
    AVFormatContext* ofmt_ctx;
    AVCodecContext* out_video_codec_ctx;
    AVCodecContext* out_audio_codec_ctx;
    AVStream* out_video_stream;
    AVStream* out_audio_stream;
    bool out_has_audio;
    int64_t last_out_video_pts;

    AVAudioFifo* out_audio_samples_fifo;

    AVFormatContext* current_ifmt_ctx;
    int current_ifmt_ctx_idx;
    AVCodecContext* current_in_video_codec_ctx;
    AVCodecContext* current_in_audio_codec_ctx;
    int current_in_video_stream_idx;
    int current_in_audio_stream_idx;
    bool current_in_has_audio;
    int64_t current_in_video_start_pts;
    int64_t current_in_video_end_pts;
    int64_t current_in_audio_start_pts;
    int64_t current_in_audio_end_pts;
    int current_in_audio_samples_encoded;
    int64_t current_in_audio_filled_pts;

    SwsContext* current_video_rgb24_sws_ctx;
    SwsContext* image_rgba_sws_ctx;
    SwsContext* rgb24_out_sws_ctx;

    SwrContext* current_swr_ctx;

    int64_t video_pts_offset;
    int64_t audio_pts_offset;

    bool isInputOpen;

    int lastReportedPercent;

    RenderJobContext();
};

RenderJobContext::RenderJobContext() :
canvasWidth (0),
canvasHeight (0),
job_duration (0),
current_ifmt_ctx_idx (-1),
current_in_video_stream_idx (-1),
current_in_audio_stream_idx (-1),
video_pts_offset (0),
audio_pts_offset (0),
current_in_video_start_pts (0),
current_in_video_end_pts (0),
current_in_audio_start_pts (0),
current_in_audio_end_pts (0),
current_in_audio_samples_encoded(0),
current_in_audio_filled_pts(0),
out_has_audio(false),
last_out_video_pts(0),
current_in_has_audio(false),
isInputOpen (false),
lastReportedPercent (0)
{
}

void reportStatus(RenderJobContext* rjob_ctx, int percent) {

    JNIEnv* env = rjob_ctx->env;
    jobject parentJavaThread = rjob_ctx->jobj;

    jclass myClass = env->FindClass("com/typito/exporter/RenderJobThread");

    if(env != NULL && myClass != NULL){
        jmethodID methodId = env->GetMethodID(myClass , "reportStatusNativeCB" , "(I)V");
        if(methodId == NULL){
            av_log(NULL, AV_LOG_ERROR, "no method found for reporting status");

        }else{
            env->CallVoidMethod(parentJavaThread,methodId,(jint)percent);
        }
    }else{
        cout << "null env";
    }
}

void reportJobDuration(RenderJobContext* rjob_ctx, int duration) {
    
    JNIEnv* env = rjob_ctx->env;
    jobject parentJavaThread = rjob_ctx->jobj;
    
    jclass myClass = env->FindClass("com/typito/exporter/RenderJobThread");
    
    if(env != NULL && myClass != NULL){
        jmethodID methodId = env->GetMethodID(myClass , "reportInputFileDurationNativeCB" , "(I)V");
        if(methodId == NULL){
            av_log(NULL, AV_LOG_ERROR, "no method found for reporting input duration");
            
        }else{
            env->CallVoidMethod(parentJavaThread,methodId,(jint)duration);
        }
    }else{
        cout << "null env";
    }
}

int setupInputs(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "setting up inputs start\n");

    int ret = 0;

    float job_duration = 0;
    
    bool allInputsHaveVideo = true;
    bool anyInputHasAudio = false;

    for (size_t i = 0; i < rjob_ctx->inputPaths.size(); i++) {
        
        std::string filePath = rjob_ctx->inputPaths[i];
        
        av_log(NULL, AV_LOG_DEBUG, "input %d filepath %s\n", (int)i, filePath.c_str());

        AVFormatContext* ifmt_ctx = NULL;

        if ((ret = avformat_open_input(&ifmt_ctx, filePath.c_str(), NULL, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open input file---%s\n", filePath.c_str());
            return ret;
        }

        if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot find stream information %s\n", filePath.c_str());
            return ret;
        }

        av_log(NULL, AV_LOG_INFO, "Dumping input format for %s\n", filePath.c_str());
        av_dump_format(ifmt_ctx, 0, filePath.c_str(), 0);

        float input_duration = (float)ifmt_ctx->duration / (float)AV_TIME_BASE;
        
        av_log(NULL, AV_LOG_DEBUG, "input duration %.3f\n", input_duration);
        
        float inputClipStartTime = (float)rjob_ctx->inputClipStartTimes[i];
        float inputClipEndTime = (float)rjob_ctx->inputClipEndTimes[i];

        job_duration += (inputClipEndTime - inputClipStartTime);

        rjob_ctx->ifmt_ctx_v.push_back(ifmt_ctx);
        
        bool foundVideoStream = false;
        bool foundAudioStream = false;
        
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            
            AVCodecContext* c = ifmt_ctx->streams[i]->codec;
            
            if (!foundVideoStream && (c->codec_type == AVMEDIA_TYPE_VIDEO)) {
                foundVideoStream = true;
            } else if (!foundAudioStream && (c->codec_type == AVMEDIA_TYPE_AUDIO)) {
                foundAudioStream = true;
            }
            
            if (foundVideoStream && foundAudioStream) {
                break;
            }
        }
        
        allInputsHaveVideo = allInputsHaveVideo && foundVideoStream;
        anyInputHasAudio = anyInputHasAudio || foundAudioStream;
    }
    
    if (!allInputsHaveVideo) {
        av_log(NULL, AV_LOG_ERROR, "All input streams do not have video\n");
        return -1;
    }
    
    rjob_ctx->out_has_audio = anyInputHasAudio;

    rjob_ctx->job_duration = job_duration;
    
    reportJobDuration(rjob_ctx, rjob_ctx->job_duration);

    av_log(NULL, AV_LOG_INFO, "Total job duration for %.3f\n", job_duration);

    av_log(NULL, AV_LOG_INFO, "setting up inputs done\n");

    return ret;
}

int setupAndOpenOutput(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "setting up and opening output start\n");

    int ret = 0;

    AVFormatContext* ofmt_ctx = NULL;
    AVCodec* out_video_codec = NULL;
    AVCodec* out_audio_codec = NULL;

    AVCodecContext* out_video_codec_ctx = NULL;
    AVCodecContext* out_audio_codec_ctx = NULL;

    AVStream* out_video_stream = NULL;
    AVStream* out_audio_stream = NULL;

    AVAudioFifo* out_audio_samples_fifo = NULL;
    
    // Allocate a format context
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , rjob_ctx->outputPath.c_str());

    // Get the video encoder
    out_video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!out_video_codec) {
        ret = -1;
        av_log(NULL, AV_LOG_ERROR, "Could not find encoder for '%s'\n",
                avcodec_get_name(AV_CODEC_ID_H264));
        return ret;
    }

    // Create new out video stream for the encoder
    out_video_stream = avformat_new_stream(ofmt_ctx, out_video_codec);
    if (!out_video_stream) {
        ret = -1;
        av_log(NULL, AV_LOG_ERROR, "Could not allocate video stream\n");
        return ret;
    }
    
    av_log(NULL, AV_LOG_DEBUG, "video codec and stream created\n");

    // Setup video stream and codec settings
    out_video_stream->id = ofmt_ctx->nb_streams - 1;

    out_video_codec_ctx = out_video_stream->codec;

    out_video_codec_ctx->width = 1920;
    out_video_codec_ctx->height = 1080;
    out_video_codec_ctx->gop_size = 24;

    out_video_codec_ctx->qmax = 51;
    out_video_codec_ctx->qmin = 10;
    
    av_log(NULL, AV_LOG_DEBUG, "video out settings width %d height %d bitrate %d gop_size %d qmax %d qmin %d\n",
           out_video_codec_ctx->width, out_video_codec_ctx->height, out_video_codec_ctx->bit_rate,
           out_video_codec_ctx->gop_size, out_video_codec_ctx->qmax, out_video_codec_ctx->qmin);

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        out_video_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    out_video_codec_ctx->time_base = (AVRational){1, 60};

    out_video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // x264 specific options
    AVDictionary* opt = NULL;
    av_dict_set(&opt, "preset", "veryfast", 0);
    av_dict_set_int(&opt, "crf", 21, 0);
    av_dict_set(&opt, "profile", "main", 0);
    av_dict_set(&opt, "level", "3.1", 0);
    av_opt_set(out_video_codec_ctx->priv_data, "preset", "veryfast", 0);
    av_opt_set_int(out_video_codec_ctx->priv_data, "crf", 21, 0);
    av_opt_set(out_video_codec_ctx->priv_data, "profile", "main", 0);
    av_opt_set(out_video_codec_ctx->priv_data, "level", "3.1", 0);

    // open the output video codec
    ret = avcodec_open2(out_video_codec_ctx, out_video_codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open output video codec\n");
        return ret;
    }
    
    av_log(NULL, AV_LOG_DEBUG, "opened video codec context\n");
    
    if (!rjob_ctx->out_has_audio) {
        
        av_log(NULL, AV_LOG_INFO, "Skipping setting up output audio stream/codec/codeccontext\n");
    } else {
        
        // Get the audio encoder
        out_audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
        if (!out_audio_codec) {
            ret = -1;
            av_log(NULL, AV_LOG_ERROR, "Could not find encoder for '%s'\n",
                   avcodec_get_name(AV_CODEC_ID_MP3));
            return ret;
        }
        
        // Create new out audio stream for the encoder
        out_audio_stream = avformat_new_stream(ofmt_ctx, out_audio_codec);
        if (!out_audio_stream) {
            ret = -1;
            av_log(NULL, AV_LOG_ERROR, "Could not allocate output audio stream\n");
            return ret;
        }
        
        av_log(NULL, AV_LOG_DEBUG, "audio codec and stream created\n");
        
        // Setup output audio codec and settings
        out_audio_stream->id = ofmt_ctx->nb_streams - 1;
        
        out_audio_codec_ctx = out_audio_stream->codec;
        
        out_audio_codec_ctx->bit_rate = 128000;
        out_audio_codec_ctx->sample_rate = 44100;
        
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_audio_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
        
        out_audio_codec_ctx->time_base = (AVRational){1, out_audio_codec_ctx->sample_rate };
        
        out_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;
        out_audio_codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
        out_audio_codec_ctx->channels = av_get_channel_layout_nb_channels(out_audio_codec_ctx->channel_layout);
        
        av_log(NULL, AV_LOG_DEBUG, "audio out settings samplerate %d sampleformat %s channel layout %s channels %d\n",
               out_audio_codec_ctx->sample_rate,
               av_get_sample_fmt_name(out_audio_codec_ctx->sample_fmt),
               "stereo", out_audio_codec_ctx->channels);
        
        // Open the output audio codec
        if ((ret = avcodec_open2(out_audio_codec_ctx, out_audio_codec, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output audio codec\n");
            return ret;
        }
        
        av_log(NULL, AV_LOG_DEBUG, "audio codec context opened\n");
        
        out_audio_samples_fifo = av_audio_fifo_alloc(out_audio_codec_ctx->sample_fmt,
                                                     out_audio_codec_ctx->channels, 1);
        
        av_log(NULL, AV_LOG_DEBUG, "audio fifo allocated\n");
    }

    av_log(NULL, AV_LOG_DEBUG, "opening output file\n");
    
    // open the output file
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&ofmt_ctx->pb, rjob_ctx->outputPath.c_str(), AVIO_FLAG_WRITE)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open '%s'\n", rjob_ctx->outputPath.c_str());
            return ret;
        }
    }
    
    av_log(NULL, AV_LOG_DEBUG, "writing header\n");

    // init muxer, write output file header
    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when writing header to output file\n");
        return ret;
    }

    av_log(NULL, AV_LOG_INFO, "Dumping output format\n");
    av_dump_format(ofmt_ctx, 0, rjob_ctx->outputPath.c_str(), 1);

    rjob_ctx->ofmt_ctx = ofmt_ctx;
    rjob_ctx->out_video_codec_ctx = out_video_codec_ctx;
    rjob_ctx->out_audio_codec_ctx = out_audio_codec_ctx;
    rjob_ctx->out_video_stream = out_video_stream;
    rjob_ctx->out_audio_stream = out_audio_stream;

    rjob_ctx->out_audio_samples_fifo = out_audio_samples_fifo;

    av_log(NULL, AV_LOG_INFO, "setting up and opening output done\n");

    return ret;
}

int setupSwsContexts(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "setting up output sws contexts\n");

    SwsContext* image_rgba_sws_ctx = NULL;
    SwsContext* rgb24_out_sws_ctx = NULL;

    image_rgba_sws_ctx = sws_getContext(rjob_ctx->canvasWidth,
        rjob_ctx->canvasHeight,
        AV_PIX_FMT_RGBA,
        rjob_ctx->out_video_codec_ctx->width,
        rjob_ctx->out_video_codec_ctx->height,
        AV_PIX_FMT_RGBA,
        SWS_BICUBIC, NULL, NULL, NULL);

    rgb24_out_sws_ctx = sws_getContext(rjob_ctx->out_video_codec_ctx->width,
        rjob_ctx->out_video_codec_ctx->height,
        AV_PIX_FMT_RGB24,
        rjob_ctx->out_video_codec_ctx->width,
        rjob_ctx->out_video_codec_ctx->height,
        rjob_ctx->out_video_codec_ctx->pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL);

    rjob_ctx->image_rgba_sws_ctx = image_rgba_sws_ctx;
    rjob_ctx->rgb24_out_sws_ctx = rgb24_out_sws_ctx;

    av_log(NULL, AV_LOG_INFO, "setting up output sws contexts done\n");

    return 0;
}

int cleanupOutput(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "cleaning up output\n");

    if (rjob_ctx->out_has_audio && rjob_ctx->out_audio_samples_fifo)
    {
        av_log(NULL, AV_LOG_DEBUG, "freeing audio fifo\n");
        av_audio_fifo_free(rjob_ctx->out_audio_samples_fifo);
    }

    av_log(NULL, AV_LOG_DEBUG, "freeing video/audio output contexts\n");
    avcodec_close(rjob_ctx->out_video_codec_ctx);
    if (rjob_ctx->out_has_audio) {
        avcodec_close(rjob_ctx->out_audio_codec_ctx);
    }

    if (rjob_ctx->ofmt_ctx && !(rjob_ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        av_log(NULL, AV_LOG_DEBUG, "freeing output io context\n");
        avio_closep(&rjob_ctx->ofmt_ctx->pb);
    }
    
    av_log(NULL, AV_LOG_DEBUG, "freeing output format context\n");
    avformat_free_context(rjob_ctx->ofmt_ctx);

    av_log(NULL, AV_LOG_DEBUG, "freeing sws contexts\n");
    sws_freeContext(rjob_ctx->image_rgba_sws_ctx);
    sws_freeContext(rjob_ctx->rgb24_out_sws_ctx);
    
    rjob_ctx->out_has_audio = false;

    av_log(NULL, AV_LOG_INFO, "cleaning up output done\n");

    return 0;
}

int closeCurrentInput(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "closing current input %d\n", rjob_ctx->current_ifmt_ctx_idx);

    if (rjob_ctx->current_ifmt_ctx_idx < 0
        || rjob_ctx->current_ifmt_ctx_idx >= rjob_ctx->ifmt_ctx_v.size()) {
        av_log(NULL, AV_LOG_ERROR, "Invalid current index to close %d",
            rjob_ctx->current_ifmt_ctx_idx);
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "closing current video/audio codec contexts\n");
    avcodec_close(rjob_ctx->current_in_video_codec_ctx);
    if (rjob_ctx->current_in_has_audio) {
        avcodec_close(rjob_ctx->current_in_audio_codec_ctx);
    }

    av_log(NULL, AV_LOG_DEBUG, "closing input format context\n");
    avformat_close_input(&rjob_ctx->current_ifmt_ctx);

    av_log(NULL, AV_LOG_DEBUG, "freeing video rgb24 sws context\n");
    sws_freeContext(rjob_ctx->current_video_rgb24_sws_ctx);
    
    av_log(NULL, AV_LOG_DEBUG, "freeing audio resampler context\n");
    if (rjob_ctx->current_in_has_audio) {
        swr_free(&rjob_ctx->current_swr_ctx);
    }
    
    rjob_ctx->current_ifmt_ctx = NULL;
    rjob_ctx->current_in_video_codec_ctx = NULL;
    rjob_ctx->current_in_audio_codec_ctx = NULL;

    rjob_ctx->current_video_rgb24_sws_ctx = NULL;
    rjob_ctx->current_swr_ctx = NULL;

    rjob_ctx->current_in_video_stream_idx = -1;
    rjob_ctx->current_in_audio_stream_idx = -1;
    rjob_ctx->current_in_has_audio = false;
    rjob_ctx->current_in_video_start_pts = 0;
    rjob_ctx->current_in_video_end_pts = 0;
    rjob_ctx->current_in_audio_start_pts = 0;
    rjob_ctx->current_in_audio_end_pts = 0;
    
    rjob_ctx->current_in_audio_samples_encoded = 0;
    rjob_ctx->current_in_audio_filled_pts = 0;

    av_log(NULL, AV_LOG_INFO, "closing current input %d done\n", rjob_ctx->current_ifmt_ctx_idx);

    return 0;
}

int openCurrentInput(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "opening current input %d\n", rjob_ctx->current_ifmt_ctx_idx);

    if (rjob_ctx->current_ifmt_ctx_idx < 0
        || rjob_ctx->current_ifmt_ctx_idx >= rjob_ctx->ifmt_ctx_v.size()) {
        av_log(NULL, AV_LOG_ERROR, "Invalid current index to open %d",
            rjob_ctx->current_ifmt_ctx_idx);
        return -1;
    }

    int ret;

    AVFormatContext* current_ifmt_ctx = NULL;
    AVCodecContext* current_in_video_codec_ctx = NULL;
    AVCodecContext* current_in_audio_codec_ctx = NULL;
    int current_in_video_stream_idx = -1;
    int current_in_audio_stream_idx = -1;
    SwsContext* current_video_rgb24_sws_ctx = NULL;
    SwrContext* current_swr_ctx = NULL;
    bool current_in_has_audio = false;

    current_ifmt_ctx = rjob_ctx->ifmt_ctx_v[rjob_ctx->current_ifmt_ctx_idx];

    bool foundVideoStream = false;
    bool foundAudioStream = false;
    
    av_log(NULL, AV_LOG_DEBUG, "looking for video and audio streams\n");

    for (int i = 0; i < current_ifmt_ctx->nb_streams; i++) {

        AVCodecContext* c = current_ifmt_ctx->streams[i]->codec;

        if (!foundVideoStream && (c->codec_type == AVMEDIA_TYPE_VIDEO)) {
            current_in_video_stream_idx = i;
            current_in_video_codec_ctx = c;
            foundVideoStream = true;
        } else if (!foundAudioStream && (c->codec_type == AVMEDIA_TYPE_AUDIO)) {
            current_in_audio_stream_idx = i;
            current_in_audio_codec_ctx = c;
            foundAudioStream = true;
        }
        
        if (foundVideoStream && foundAudioStream) {
            break;
        }
    }
    
    current_in_has_audio = foundAudioStream;

    if (!current_in_has_audio) {
        av_log(NULL, AV_LOG_WARNING, "Couldn't find input audio stream\n");
    }
    
    av_log(NULL, AV_LOG_DEBUG, "found both video and audios streams\n");

    // Open input video decoder
    ret = avcodec_open2(current_in_video_codec_ctx,
                        avcodec_find_decoder(current_in_video_codec_ctx->codec_id), NULL);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open input video decoder\n");
        return ret;
    }

    if (current_in_has_audio) {
        // Open input audio decoder
        ret = avcodec_open2(current_in_audio_codec_ctx,
                            avcodec_find_decoder(current_in_audio_codec_ctx->codec_id), NULL);
        
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open input audio decoder\n");
            return ret;
        }
    }
    
    av_log(NULL, AV_LOG_DEBUG, "opened input video/audio codec contexts\n");

    current_video_rgb24_sws_ctx = sws_getContext(current_in_video_codec_ctx->width,
        current_in_video_codec_ctx->height,
        current_in_video_codec_ctx->pix_fmt,
        rjob_ctx->out_video_codec_ctx->width,
        rjob_ctx->out_video_codec_ctx->height,
        AV_PIX_FMT_RGB24,
        SWS_BICUBIC, NULL, NULL, NULL);
    
    av_log(NULL, AV_LOG_DEBUG, "created sws context for video scaling\n");
    
    if (current_in_has_audio) {

        current_swr_ctx = swr_alloc_set_opts(NULL,  // we're allocating a new context
              rjob_ctx->out_audio_codec_ctx->channel_layout,  // out_ch_layout
              rjob_ctx->out_audio_codec_ctx->sample_fmt,    // out_sample_fmt
              rjob_ctx->out_audio_codec_ctx->sample_rate,                // out_sample_rate
              current_in_audio_codec_ctx->channel_layout, // in_ch_layout
              current_in_audio_codec_ctx->sample_fmt,   // in_sample_fmt
              current_in_audio_codec_ctx->sample_rate,                // in_sample_rate
              0,                    // log_offset
              NULL);
        

        swr_init(current_swr_ctx);
        
        av_log(NULL, AV_LOG_DEBUG, "created swr context for audio resampling\n");
    
    }

    rjob_ctx->current_ifmt_ctx = current_ifmt_ctx;
    rjob_ctx->current_in_video_codec_ctx = current_in_video_codec_ctx;
    rjob_ctx->current_in_audio_codec_ctx = current_in_audio_codec_ctx;
    rjob_ctx->current_in_video_stream_idx = current_in_video_stream_idx;
    rjob_ctx->current_in_audio_stream_idx = current_in_audio_stream_idx;
    rjob_ctx->current_in_has_audio = current_in_has_audio;
    
    rjob_ctx->current_video_rgb24_sws_ctx = current_video_rgb24_sws_ctx;
    rjob_ctx->current_swr_ctx = current_swr_ctx;
    
    double inputStartTime = rjob_ctx->inputClipStartTimes[rjob_ctx->current_ifmt_ctx_idx];
    double inputEndTime = rjob_ctx->inputClipEndTimes[rjob_ctx->current_ifmt_ctx_idx];

    AVStream* current_in_video_stream = current_ifmt_ctx->streams[current_in_video_stream_idx];
    
    int64_t current_in_video_start_pts = (int64_t) av_q2d(av_div_q(av_d2q(inputStartTime, INT_MAX), current_in_video_stream->time_base));
    
    int64_t current_in_video_end_pts = (int64_t) av_q2d(av_div_q(av_d2q(inputEndTime, INT_MAX), current_in_video_stream->time_base));
    
    rjob_ctx->current_in_video_start_pts = current_in_video_start_pts;
    rjob_ctx->current_in_video_end_pts = current_in_video_end_pts;
    
    if (current_in_has_audio) {
        
        AVStream* current_in_audio_stream = current_ifmt_ctx->streams[current_in_audio_stream_idx];

        int64_t current_in_audio_start_pts = (int64_t) av_q2d(av_div_q(av_d2q(inputStartTime, INT_MAX), current_in_audio_stream->time_base));
        
        int64_t current_in_audio_end_pts = (int64_t) av_q2d(av_div_q(av_d2q(inputEndTime, INT_MAX), current_in_audio_stream->time_base));
        
        rjob_ctx->current_in_audio_start_pts = current_in_audio_start_pts;
        rjob_ctx->current_in_audio_end_pts = current_in_audio_end_pts;

    }

    av_log(NULL, AV_LOG_INFO, "opening current input %d done\n", rjob_ctx->current_ifmt_ctx_idx);

    return ret;
}

bool hasNextInput(RenderJobContext* rjob_ctx) {
    return (rjob_ctx->current_ifmt_ctx_idx < (int)(rjob_ctx->ifmt_ctx_v.size() - 1));
}

int encodeAndWriteFrame(RenderJobContext* rjob_ctx, AVFrame* dataFrame, int out_stream_index, int* areFramesLeftInEncoder) {

    av_log(NULL, AV_LOG_DEBUG, "ready to encode and write frame to output\n");
    
    int ret = 0;

    bool flushingEncoder = (dataFrame == NULL);
    
    av_log(NULL, AV_LOG_DEBUG, "flushing encoder %s\n", flushingEncoder ? "yes" : "no");

    AVPacket encodedPacket;
    av_init_packet(&encodedPacket);
    encodedPacket.data = NULL;
    encodedPacket.size = 0;

    AVStream* out_stream = rjob_ctx->ofmt_ctx->streams[out_stream_index];

    int got_encoded_packet = 0;

    if (out_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        
        av_log(NULL, AV_LOG_DEBUG, "encoding video frame\n");

        ret = avcodec_encode_video2(rjob_ctx->out_video_codec_ctx,
                                    &encodedPacket,
                                    dataFrame,
                                    &got_encoded_packet);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error encoding video frame\n");
            return ret;
        }

    } else if (out_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
        
        av_log(NULL, AV_LOG_DEBUG, "encoding audio frame\n");

        ret = avcodec_encode_audio2(rjob_ctx->out_audio_codec_ctx,
                                    &encodedPacket,
                                    dataFrame,
                                    &got_encoded_packet);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error encoding audio frame\n");
            return ret;
        }
    }

    if (got_encoded_packet) {
        
        av_log(NULL, AV_LOG_DEBUG, "got encoded packet\n");

        *areFramesLeftInEncoder = 1;

        encodedPacket.stream_index = out_stream_index;

        av_packet_rescale_ts(&encodedPacket,
                             out_stream->codec->time_base,
                             out_stream->time_base);
        
        av_log(NULL, AV_LOG_DEBUG, "av_interleaved_write_frame encoded packet\n");

        ret = av_interleaved_write_frame(rjob_ctx->ofmt_ctx, &encodedPacket);

        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR , "Error writing encoded packet to out\n");
            return ret;
        }

    } else {
        *areFramesLeftInEncoder = 0;
    }

    av_free_packet(&encodedPacket);

    return ret;
}

int processInputPacket(RenderJobContext* rjob_ctx, AVPacket* inputPacket, int* areFramesLeftInDecoder) {

    int ret = 0;

    AVStream* in_stream = rjob_ctx->current_ifmt_ctx->streams[inputPacket->stream_index];
    
    if (in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO && !rjob_ctx->out_has_audio) {
        // Output has no audio but input has. Don't need to process
        av_free_packet(inputPacket);
        return 0;
    }

    if (in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
        
        av_log(NULL, AV_LOG_DEBUG, "processing input audio packet\n");

        AVPacket origInputPacket = *inputPacket;

        do {

            AVFrame* audioFrame = av_frame_alloc();

            int got_frame;

            ret = avcodec_decode_audio4(in_stream->codec,
                                        audioFrame,
                                        &got_frame,
                                        inputPacket
                                        );

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Couldn't decode audio packet\n");
                return ret;
            }

            inputPacket->data += ret;
            inputPacket->size -= ret;

            if (got_frame) {

                *areFramesLeftInDecoder = 1;
                
                // The timestamp of this frame should be the same as the corresponding input frame.
                int64_t frame_pts = av_frame_get_best_effort_timestamp(audioFrame);
                
                bool useFrame = true;
                
                if (frame_pts < 0) {
                    useFrame = false;
                }
                
                if (frame_pts < rjob_ctx->current_in_audio_start_pts) {
                    useFrame = false;
                }
                
                if (frame_pts > rjob_ctx->current_in_audio_end_pts) {
                    useFrame = false;
                }
                
                if (useFrame) {
                    // First we need to convert the samples from the input frame
                    // to the desired output audio parameters
                    uint8_t **converted_samples = NULL;
                    
                    int inputSampleCount = audioFrame->nb_samples;
                    
                    av_log(NULL, AV_LOG_DEBUG, "audio frame input sample count %d\n", inputSampleCount);
                    
                    // Compute the number of output samples we'll have after resampling
                    int delay = swr_get_delay(rjob_ctx->current_swr_ctx, in_stream->codec->sample_rate);
                    
                    av_log(NULL, AV_LOG_DEBUG, "audio frame delay samples %d\n", delay);
                    
                    int outputSampleMaxCount = swr_get_out_samples(rjob_ctx->current_swr_ctx, delay + inputSampleCount);
                    
                    av_log(NULL, AV_LOG_DEBUG, "audio frame output sample max count %d\n", outputSampleMaxCount);
                    
                    // Allocate the space to store the samples after resampling
                    converted_samples = (uint8_t **)calloc(rjob_ctx->out_audio_codec_ctx->channels, sizeof(*converted_samples));
                    
                    av_samples_alloc(converted_samples, NULL, rjob_ctx->out_audio_codec_ctx->channels,
                                     outputSampleMaxCount, rjob_ctx->out_audio_codec_ctx->sample_fmt, 0);

                    // Resample
                    int actualOutputSampleCount = swr_convert(rjob_ctx->current_swr_ctx, converted_samples, outputSampleMaxCount, (const uint8_t **)audioFrame->extended_data, inputSampleCount);

                    av_log(NULL, AV_LOG_DEBUG, "audio frame output sample actual count %d\n", actualOutputSampleCount);
                    
                    
                    // Expand fifo size to accomodate the new samples
                    av_audio_fifo_realloc(rjob_ctx->out_audio_samples_fifo, av_audio_fifo_size(rjob_ctx->out_audio_samples_fifo) + actualOutputSampleCount);
                    
                    // Push new samples into the fifo
                    av_audio_fifo_write(rjob_ctx->out_audio_samples_fifo, (void**) converted_samples, actualOutputSampleCount);
                    
                    // We don't need this anymore
                    if (converted_samples) {
                        av_freep(&converted_samples[0]);
                        free(converted_samples);
                    }
                    
                    int output_frame_size = rjob_ctx->out_audio_codec_ctx->frame_size;
                    
                    av_log(NULL, AV_LOG_DEBUG, "audio frame output frame size %d\n", output_frame_size);
                    
                    av_log(NULL, AV_LOG_DEBUG, "audio frame fifo current samples %d\n", av_audio_fifo_size(rjob_ctx->out_audio_samples_fifo));
                    
                    // If we have enough samples for the output frame, create and encode
                    while(av_audio_fifo_size(rjob_ctx->out_audio_samples_fifo) >= output_frame_size) {
                        
                        AVFrame *outputFrame = av_frame_alloc();
                        
                        outputFrame->nb_samples = output_frame_size;
                        outputFrame->channel_layout = rjob_ctx->out_audio_codec_ctx->channel_layout;
                        outputFrame->format = rjob_ctx->out_audio_codec_ctx->sample_fmt;
                        outputFrame->sample_rate = rjob_ctx->out_audio_codec_ctx->sample_rate;
                        
                        // Allocate the buffer for the frame data
                        av_frame_get_buffer(outputFrame, 0);
                        
                        // Pull out samples from the fifo into the frame
                        int num_read_samples = av_audio_fifo_read(rjob_ctx->out_audio_samples_fifo, (void **)outputFrame->data, output_frame_size);
                        
                        double out_frame_time = ((double)rjob_ctx->current_in_audio_samples_encoded / (double)rjob_ctx->out_audio_codec_ctx->sample_rate);
                        
                        av_log(NULL, AV_LOG_DEBUG, "audio frame output read samples %d\n", num_read_samples);
                        av_log(NULL, AV_LOG_DEBUG, "audio frame output frame time %.2f\n", out_frame_time);
                        
                        int64_t out_frame_pts = (int64_t) av_q2d(av_div_q(av_d2q(out_frame_time, INT_MAX), rjob_ctx->out_audio_codec_ctx->time_base));
                        
                        av_log(NULL, AV_LOG_DEBUG, "audio frame output frame pts %d\n", (int)out_frame_pts);
                        
                        out_frame_pts += rjob_ctx->audio_pts_offset;
                        
                        av_log(NULL, AV_LOG_DEBUG, "audio frame output frame pts after offset %d\n", (int)out_frame_pts);
                        
                        outputFrame->pts = out_frame_pts;
                        
                        rjob_ctx->current_in_audio_samples_encoded += num_read_samples;
                        
                        double filled_duration = ((double)rjob_ctx->current_in_audio_samples_encoded / (double)rjob_ctx->out_audio_codec_ctx->sample_rate);
                        
                        rjob_ctx->current_in_audio_filled_pts = (int64_t) av_q2d(av_div_q(av_d2q(filled_duration, INT_MAX), rjob_ctx->out_audio_codec_ctx->time_base));
                        
                        av_log(NULL, AV_LOG_DEBUG, "audio frame output filled pts %d\n", (int)rjob_ctx->current_in_audio_filled_pts);
                        
                        int got_encoded_packet;
                        ret = encodeAndWriteFrame(rjob_ctx, outputFrame, 1, &got_encoded_packet);
                        
                        if (ret < 0) {
                            av_log(NULL,AV_LOG_ERROR , "Error encoding and writing audio frame\n");
                            return ret;
                        }
                        
                        av_frame_free(&outputFrame);
                    }

                }

            } else {
                *areFramesLeftInDecoder = 0;
            }
            
            av_frame_unref(audioFrame);
            av_frame_free(&audioFrame);

        } while (inputPacket->size > 0);
        
        av_log(NULL, AV_LOG_DEBUG, "processing input audio packet complete\n");

        av_free_packet(&origInputPacket);

    } else if (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        
        av_log(NULL, AV_LOG_DEBUG, "processing input video packet\n");

        AVFrame* videoFrame = av_frame_alloc();

        int got_frame;

        ret = avcodec_decode_video2(in_stream->codec,
                                    videoFrame,
                                    &got_frame,
                                    inputPacket
                                    );

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Couldn't decode video packet\n");
            return ret;
        }

        if (got_frame) {

            *areFramesLeftInDecoder = 1;
            
            bool useFrame = true;
            
            AVCodecContext* out_video_codec_ctx = rjob_ctx->out_video_codec_ctx;
            SwsContext* current_video_rgb24_sws_ctx = rjob_ctx->current_video_rgb24_sws_ctx;
            
            int64_t frame_pts = av_frame_get_best_effort_timestamp(videoFrame);
            
            if (frame_pts < rjob_ctx->current_in_video_start_pts) {
                useFrame = false;
            }
            
            if (frame_pts > rjob_ctx->current_in_video_end_pts) {
                useFrame = false;
            }
            
            av_log(NULL, AV_LOG_DEBUG, "video frame pts %d\n", (int)frame_pts);
            
            frame_pts -= rjob_ctx->current_in_video_start_pts;
            
            av_log(NULL, AV_LOG_DEBUG, "video frame pts clipped to start %d\n", (int)frame_pts);
            
            frame_pts = av_rescale_q(frame_pts, in_stream->time_base, in_stream->codec->time_base);
            
           av_log(NULL, AV_LOG_DEBUG, "video frame pts rescaled to in codec %d\n", (int)frame_pts);
            
            frame_pts = av_rescale_q(frame_pts, in_stream->codec->time_base, out_video_codec_ctx->time_base);
            
           av_log(NULL, AV_LOG_DEBUG, "video frame pts rescaled to out codec %d\n", (int)frame_pts);
            
            frame_pts += rjob_ctx->video_pts_offset;
            
            av_log(NULL, AV_LOG_DEBUG, "video frame pts after offset %d\n", (int)frame_pts);
            
            av_log(NULL, AV_LOG_DEBUG, "video frame pts last out pts %d\n", (int)rjob_ctx->last_out_video_pts);
            
            if (frame_pts <= rjob_ctx->last_out_video_pts) {
                useFrame = false;
            }

            if (useFrame) {
                
                // First convert the video frame to an output sized rgb frame
                AVFrame* scaledVideoRGBFrame = av_frame_alloc();

                av_image_alloc(scaledVideoRGBFrame->data, scaledVideoRGBFrame->linesize,
                                out_video_codec_ctx->width, out_video_codec_ctx->height,
                                AV_PIX_FMT_RGB24, 16);

                sws_scale(current_video_rgb24_sws_ctx, videoFrame->data, videoFrame->linesize,
                                0, in_stream->codec->height,
                                scaledVideoRGBFrame->data, scaledVideoRGBFrame->linesize);

                scaledVideoRGBFrame->format = AV_PIX_FMT_RGB24;
                scaledVideoRGBFrame->width = out_video_codec_ctx->width;
                scaledVideoRGBFrame->height = out_video_codec_ctx->height;
                
                
                
                av_log(NULL, AV_LOG_DEBUG, "video frame rescaled pts %d\n", (int)frame_pts);

                float frame_time = (float)(frame_pts * av_q2d(out_video_codec_ctx->time_base));

                int progressPercent = (int)((frame_time / rjob_ctx->job_duration) * 100);

                // Max is 99 because we fire completion at the end of the job
                // Also, our total job duration may not be as accurate as the time stamps we get here
                progressPercent = FFMIN(99, FFMAX(0, progressPercent));

                // Round to nearest multiple of five
                progressPercent = (progressPercent / 5) * 5;

                if (rjob_ctx->lastReportedPercent != progressPercent) {
                    rjob_ctx->lastReportedPercent = progressPercent;
                    reportStatus(rjob_ctx, progressPercent);
                }

                for (int k = 0; k < rjob_ctx->imageSequenceListLength ; k++) {

                    ImageSequence* imageSequence = rjob_ctx->imageSequenceList[k];
                    ImageFrame* imageFrame = imageSequence->getFrameAtTime(frame_time);

                    if (imageFrame != NULL && imageFrame->hasDecodedFrame()) {

                        AVFrame* overlayFrame = av_frame_alloc();

                        av_image_alloc(overlayFrame->data, overlayFrame->linesize,
                                        rjob_ctx->canvasWidth, rjob_ctx->canvasHeight,
                                        AV_PIX_FMT_RGBA, 16);

                        overlayFrame->format = AV_PIX_FMT_RGBA;
                        overlayFrame->width = rjob_ctx->canvasWidth;
                        overlayFrame->height = rjob_ctx->canvasHeight;

                        copyImageFrameToCanvasSizeFrame(imageFrame, overlayFrame);

                        AVFrame* overlayScaledFrame = av_frame_alloc();

                        av_image_alloc(overlayScaledFrame->data, overlayScaledFrame->linesize,
                                        out_video_codec_ctx->width, out_video_codec_ctx->height,
                                        AV_PIX_FMT_RGBA, 16);

                        overlayScaledFrame->format = AV_PIX_FMT_RGBA;
                        overlayScaledFrame->width = out_video_codec_ctx->width;
                        overlayScaledFrame->height = out_video_codec_ctx->height;

                        sws_scale(rjob_ctx->image_rgba_sws_ctx, overlayFrame->data, overlayFrame->linesize,
                                        0, overlayFrame->height,
                                        overlayScaledFrame->data, overlayScaledFrame->linesize);

                        overlayFrameOnOutputFrame(overlayScaledFrame, scaledVideoRGBFrame);

                        av_freep(overlayFrame->data);
                        av_frame_free(&overlayFrame);
                        
                        av_freep(overlayScaledFrame->data);
                        av_frame_free(&overlayScaledFrame);

                    }else{
                        av_log(NULL,AV_LOG_DEBUG , "null frame recieved from image sequence\n");
                    }
                }

                AVFrame* videoOutputFrame = av_frame_alloc();

                av_image_alloc(videoOutputFrame->data, videoOutputFrame->linesize,
                                out_video_codec_ctx->width, out_video_codec_ctx->height,
                                out_video_codec_ctx->pix_fmt, 16);

                videoOutputFrame->format = out_video_codec_ctx->pix_fmt;
                videoOutputFrame->width = out_video_codec_ctx->width;
                videoOutputFrame->height = out_video_codec_ctx->height;

                sws_scale(rjob_ctx->rgb24_out_sws_ctx, scaledVideoRGBFrame->data, scaledVideoRGBFrame->linesize,
                                0, out_video_codec_ctx->height,
                                videoOutputFrame->data, videoOutputFrame->linesize);

                videoOutputFrame->pts = frame_pts;
                
                rjob_ctx->last_out_video_pts = frame_pts;

                int got_encoded_packet = 0;
                ret = encodeAndWriteFrame(rjob_ctx, videoOutputFrame, 0, &got_encoded_packet);

                if (ret < 0) {
                    av_log(NULL,AV_LOG_ERROR , "Error encoding and writing video frame\n");
                    return ret;
                }

                av_freep(scaledVideoRGBFrame->data);
                av_frame_free(&scaledVideoRGBFrame);
                
                av_freep(videoOutputFrame->data);
                av_frame_free(&videoOutputFrame);
            }
            
            av_frame_unref(videoFrame);

        } else {
            *areFramesLeftInDecoder = 0;
        }
        
        av_log(NULL, AV_LOG_DEBUG, "processing input video packet complete\n");

        av_frame_free(&videoFrame);

        av_free_packet(inputPacket);
    }

    return ret;
}

int flushDecoders(RenderJobContext* rjob_ctx) {

    av_log(NULL, AV_LOG_INFO, "flushing decoders\n");

    int ret;

    int areFramesLeftInDecoder = 0;
    // Flush all video packets
    do {

        AVPacket dummyPkt;
        av_init_packet(&dummyPkt);
        dummyPkt.data = NULL;
        dummyPkt.size = 0;
        dummyPkt.stream_index = rjob_ctx->current_in_video_stream_idx;

        ret = processInputPacket(rjob_ctx, &dummyPkt, &areFramesLeftInDecoder);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error processing input packet\n");
            return ret;
        }

    } while(areFramesLeftInDecoder);

    
    if (rjob_ctx->current_in_has_audio) {
        
        areFramesLeftInDecoder = 0;
        // Flush all audio packets
        do {
            AVPacket dummyPkt;
            av_init_packet(&dummyPkt);
            dummyPkt.data = NULL;
            dummyPkt.size = 0;
            dummyPkt.stream_index = rjob_ctx->current_in_audio_stream_idx;
            
            ret = processInputPacket(rjob_ctx, &dummyPkt, &areFramesLeftInDecoder);
            
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error processing input packet\n");
                return ret;
            }
            
        } while(areFramesLeftInDecoder);
    }

    av_log(NULL, AV_LOG_INFO, "flushing decoders done\n");

    return ret;
}

int flushEncodersAndWriteTrailer(RenderJobContext* rjob_ctx) {
    //some codecs can delay delay some packets

    av_log(NULL, AV_LOG_INFO, "flushing encoders\n");

    int ret = 0;

    // Video encoders first
    if ((rjob_ctx->out_video_stream->codec->codec->capabilities & AV_CODEC_CAP_DELAY)) {

        int got_encoded_packet = 0;

        do {

            got_encoded_packet = 0;
            ret = encodeAndWriteFrame(rjob_ctx, NULL, 0, &got_encoded_packet);

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error writing input packet\n");
                return ret;
            }
        } while(got_encoded_packet);
    }
    
    if (rjob_ctx->out_has_audio) {
        // audio encoders flush
        if ((rjob_ctx->out_audio_stream->codec->codec->capabilities & AV_CODEC_CAP_DELAY)) {
            
            int got_encoded_packet = 0;
            
            do {
                
                ret = encodeAndWriteFrame(rjob_ctx, NULL, 1, &got_encoded_packet);
                
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error writing input packet\n");
                    return ret;
                }
            } while(got_encoded_packet);
        }
    }

    av_log(NULL, AV_LOG_INFO, "writing trailer\n");

    ret = av_write_trailer(rjob_ctx->ofmt_ctx);

    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "Error writing a trailer to container\n");
    }

    av_log(NULL, AV_LOG_INFO, "flushing encoders done\n");

    return ret;
}

int doRenderJob(JNIEnv* env, jobject parentRenderJobThreadObj, std::string jobId,
    std::vector<std::string> inputPaths, std::string outputPath,
    std::vector<double> inputClipStartTimes, std::vector<double> inputClipEndTimes,
    int canvasWidth, int canvasHeight,
    ImageSequence** imageSequenceList, int imageSequenceListLength) {

    av_log(NULL, AV_LOG_INFO, "doRenderJob start\n");

    RenderJobContext* rjob_ctx = new RenderJobContext();
    rjob_ctx->env = env;
    rjob_ctx->jobj = parentRenderJobThreadObj;

    rjob_ctx->jobId = jobId;
    rjob_ctx->inputPaths = inputPaths;
    rjob_ctx->outputPath = outputPath;
    rjob_ctx->inputClipStartTimes = inputClipStartTimes;
    rjob_ctx->inputClipEndTimes = inputClipEndTimes;
    rjob_ctx->canvasWidth = canvasWidth;
    rjob_ctx->canvasHeight = canvasHeight;

    for (int i = 0; i < imageSequenceListLength; i++) {
        for (int j = i+1; j < imageSequenceListLength; j++){
            if(imageSequenceList[i]->getZIndex() > imageSequenceList[j]->getZIndex()){
                //swap the pointers

                ImageSequence *temp = imageSequenceList[i];
                imageSequenceList[i] = imageSequenceList[j];
                imageSequenceList[j] = temp;
            }
        }
    }

    rjob_ctx->imageSequenceList = imageSequenceList;
    rjob_ctx->imageSequenceListLength = imageSequenceListLength;

    int ret = 0;

    if ((ret = setupInputs(rjob_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open inputs\n");
        return ret;
    }

    if ((ret = setupAndOpenOutput(rjob_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot setup output\n");
        return ret;
    }

    if ((ret = setupSwsContexts(rjob_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot setup sws contexts\n");
        return ret;
    }

    rjob_ctx->current_ifmt_ctx_idx = -1;
    rjob_ctx->isInputOpen = false;

    av_log(NULL, AV_LOG_INFO, "starting frame processing loop\n");

    while (1) {

        if (!rjob_ctx->isInputOpen) {
            av_log(NULL, AV_LOG_INFO, "no input open\n");
            if (hasNextInput(rjob_ctx)) {

                rjob_ctx->current_ifmt_ctx_idx++;

                av_log(NULL, AV_LOG_INFO, "opening next input\n");

                ret = openCurrentInput(rjob_ctx);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Couldn't open current input\n");
                    return ret;
                }
                rjob_ctx->isInputOpen = true;
            } else {
                av_log(NULL, AV_LOG_INFO, "no inputs left to open\n");
                break;
            }
        }

        // Every iteration we read a frame
        AVPacket inputPacket;
        av_init_packet(&inputPacket);
        inputPacket.pts = AV_NOPTS_VALUE;
        inputPacket.dts = AV_NOPTS_VALUE;

        ret = av_read_frame(rjob_ctx->current_ifmt_ctx, &inputPacket);

        if (ret < 0) {
            // Either error reading frame or end of file. Let's assume it's the latter

            ret = flushDecoders(rjob_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Couldn't flush input decoder\n");
                return ret;
            }

            if (!hasNextInput(rjob_ctx)) {
                // Flush encoder
                ret = flushEncodersAndWriteTrailer(rjob_ctx);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Couldn't flush encoder\n");
                    return ret;
                }
            }

            if (rjob_ctx->out_has_audio) {
                
                if (rjob_ctx->current_in_has_audio) {
                    
                    AVStream* in_video_stream = rjob_ctx->current_ifmt_ctx->streams[rjob_ctx->current_in_video_stream_idx];
                    AVStream* in_audio_stream = rjob_ctx->current_ifmt_ctx->streams[rjob_ctx->current_in_audio_stream_idx];
                    
                    // Figures out the higher of audio and video and uses that for the offset
                    int64_t videoOffset = rjob_ctx->current_in_video_end_pts - rjob_ctx->current_in_video_start_pts;
                    videoOffset = av_rescale_q(videoOffset, in_video_stream->time_base, rjob_ctx->out_video_codec_ctx->time_base);
                    videoOffset += FFMAX(rjob_ctx->out_video_codec_ctx->ticks_per_frame, 1);
                    
//                    int64_t audioOffset = rjob_ctx->current_in_audio_end_pts - rjob_ctx->current_in_audio_start_pts;
                    int64_t audioOffset = rjob_ctx->current_in_audio_filled_pts;
//                    audioOffset = av_rescale_q(audioOffset, in_audio_stream->time_base, rjob_ctx->out_audio_codec_ctx->time_base);
//                    audioOffset += FFMAX(rjob_ctx->out_audio_codec_ctx->ticks_per_frame, 1);
                    
                    bool isVideoOffsetHigher = av_compare_ts(videoOffset, rjob_ctx->out_video_codec_ctx->time_base,
                                                             audioOffset, rjob_ctx->out_audio_codec_ctx->time_base) > 0;
                    
                    int64_t newVideoOffset;
                    int64_t newAudioOffset;
                    
                    if (isVideoOffsetHigher) {
                        newVideoOffset = videoOffset;
                        newAudioOffset = av_rescale_q(videoOffset, rjob_ctx->out_video_codec_ctx->time_base, rjob_ctx->out_audio_codec_ctx->time_base);
                    } else {
                        newVideoOffset = av_rescale_q(audioOffset, rjob_ctx->out_audio_codec_ctx->time_base, rjob_ctx->out_video_codec_ctx->time_base);
                        newAudioOffset = audioOffset;
                    }
                    
                    av_log(NULL, AV_LOG_DEBUG, "new video offset %d\n", (int)newVideoOffset);
                    av_log(NULL, AV_LOG_DEBUG, "new audio offset %d\n", (int)newAudioOffset);
                    
                    rjob_ctx->video_pts_offset += newVideoOffset;
                    rjob_ctx->audio_pts_offset += newAudioOffset;
                } else {
                    
                    // Current input doesn't have audio. But we still need to update both the offsets based on the
                    // video since subsequent streams can still have audio
                    AVStream* in_video_stream = rjob_ctx->current_ifmt_ctx->streams[rjob_ctx->current_in_video_stream_idx];
                    int64_t videoOffset = rjob_ctx->current_in_video_end_pts - rjob_ctx->current_in_video_start_pts;
                    videoOffset = av_rescale_q(videoOffset, in_video_stream->time_base, rjob_ctx->out_video_codec_ctx->time_base);
                    videoOffset += FFMAX(rjob_ctx->out_video_codec_ctx->ticks_per_frame, 1);
                    
                    rjob_ctx->video_pts_offset += videoOffset;
                    rjob_ctx->audio_pts_offset += av_rescale_q(videoOffset, rjob_ctx->out_video_codec_ctx->time_base, rjob_ctx->out_audio_codec_ctx->time_base);
                }
                
            } else {
                // When there is no output audio stream, audio_pts_offset isn't relevant at all
                AVStream* in_video_stream = rjob_ctx->current_ifmt_ctx->streams[rjob_ctx->current_in_video_stream_idx];

                int64_t videoOffset = rjob_ctx->current_in_video_end_pts - rjob_ctx->current_in_video_start_pts;
                videoOffset = av_rescale_q(videoOffset, in_video_stream->time_base, rjob_ctx->out_video_codec_ctx->time_base);
                videoOffset += FFMAX(rjob_ctx->out_video_codec_ctx->ticks_per_frame, 1);
                
                rjob_ctx->video_pts_offset += videoOffset;
            }
            

            ret = closeCurrentInput(rjob_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Couldn't close current input\n");
                return ret;
            }

            rjob_ctx->isInputOpen = false;

            continue;
        }

        int areFramesLeftInDecoder;

        ret = processInputPacket(rjob_ctx, &inputPacket, &areFramesLeftInDecoder);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error processing input packet\n");
            return ret;
        }
    }

    av_log(NULL, AV_LOG_INFO, "end of frame processing loop\n");

    cleanupOutput(rjob_ctx);
    
    for (int i = 0; i < imageSequenceListLength; i++) {
        imageSequenceList[i]->cleanup();
        delete imageSequenceList[i];
    }

    av_log(NULL, AV_LOG_INFO, "doRenderJob complete\n");

    reportStatus(rjob_ctx, 100);

    return ret;

}

/**
 takes in java version of ImageSequence object and sets the appropriate values native copy
 */
void convertFromImageImageSequenceObject(JNIEnv * env , jobject javaImageSeqObj , ImageSequence *imageSeqNative){
    //fps
    jmethodID fpsGetter = getMethodId(env, javaImageSeqObj, "getFps","()I" );
    jint fps = env->CallIntMethod(javaImageSeqObj, fpsGetter);
    imageSeqNative->setFps((int)fps);

    //startTime
    jmethodID startTimeGetter = getMethodId(env, javaImageSeqObj, "getStartTime", "()F");
    jfloat startTime = env->CallFloatMethod(javaImageSeqObj, startTimeGetter);
    imageSeqNative->setOffsetTime(startTime);

    //zIndex
    jmethodID zIndexGetter = getMethodId(env, javaImageSeqObj, "getzIndex", "()I");
    jint zIndex = env->CallIntMethod(javaImageSeqObj, zIndexGetter);
    imageSeqNative->setZIndex((int)zIndex);

    //num of frames
    jmethodID numFramesGetter = getMethodId(env, javaImageSeqObj, "getNumFrames", "()I");
    jint numFrames = env->CallIntMethod(javaImageSeqObj, numFramesGetter);
    imageSeqNative->setNumFrames(numFrames);

    //seqId
    jmethodID idGetter = getMethodId(env, javaImageSeqObj, "getId", "()Ljava/lang/String;");
    jstring seqId = (jstring)env->CallObjectMethod(javaImageSeqObj, idGetter);
    const char* seqIdChar = env->GetStringUTFChars(seqId,NULL);
    string seqIdStr(seqIdChar);
    imageSeqNative->setSeqId(seqIdStr);
    env->ReleaseStringUTFChars(seqId, seqIdChar);

}

JNIEXPORT jint JNICALL Java_com_typito_exporter_RenderJobThread_doRenderJob
(JNIEnv * env, jobject jobj, jstring jobIdJStr, jobjectArray videoPathJStrings,
    jstring outputFilePathJStr, jdoubleArray videoClipStartTimes, jdoubleArray videoClipEndTimes,
    jint canvasWidth, jint canvasHeight,
    jobjectArray imageSeqJObjects){

    int inputVideoCount = env->GetArrayLength(videoPathJStrings);

    std::vector<std::string> inputVideoPaths;
    std::vector<double> inputVideoStartTimes( inputVideoCount );
    std::vector<double> inputVideoEndTimes( inputVideoCount );
    
    env->GetDoubleArrayRegion(videoClipStartTimes, 0, inputVideoCount, &inputVideoStartTimes[0]);
    env->GetDoubleArrayRegion(videoClipEndTimes, 0, inputVideoCount, &inputVideoEndTimes[0]);
    
    for (int i = 0; i < inputVideoCount; i++) {
        jstring videoPathJStr = (jstring) env->GetObjectArrayElement(videoPathJStrings, i);
        std::string videoPath = convertJStringToStr(env , videoPathJStr);
        inputVideoPaths.push_back(videoPath);
    }

    std::string outputFilePath = convertJStringToStr(env , outputFilePathJStr);

    std::string jobId = convertJStringToStr(env , jobIdJStr);

    int numOverlayImages = env->GetArrayLength(imageSeqJObjects);

    av_log(NULL, AV_LOG_DEBUG , "starting job native\n");

    try {

        ImageSequence* imageSequenceList[numOverlayImages];
        for (int i = 0; i < numOverlayImages; i++) {
            jobject javaImageSeqObj = env->GetObjectArrayElement(imageSeqJObjects, i);
            ImageSequence *imageSequence  = new ImageSequence(env, jobj);
            convertFromImageImageSequenceObject(env, javaImageSeqObj, imageSequence);
            imageSequenceList[i] = imageSequence;
        }

        int ret = doRenderJob(env, jobj, jobId, inputVideoPaths, outputFilePath,
            inputVideoStartTimes, inputVideoEndTimes,
            canvasWidth, canvasHeight, imageSequenceList, numOverlayImages);

        if (ret < 0) {
            av_log(NULL,AV_LOG_ERROR ,"Error occured while overlaying\n");
            return -1;
        }

        return 1;

    } catch(exception const & e){
        wcout << e.what() << endl;
        return -1;
    }
}

JNIEXPORT jint JNICALL Java_com_typito_exporter_RenderJobThread_setupLibNative
(JNIEnv *env, jclass clazz, jstring nativeLogLevelStr){
    av_register_all();
    
    std::string nativeLogLevel(convertJStringToStr(env, nativeLogLevelStr));
    
    if (nativeLogLevel == "debug") {
        av_log_set_level(AV_LOG_DEBUG);
    } else if (nativeLogLevel == "info") {
        av_log_set_level(AV_LOG_INFO);
    } else if (nativeLogLevel == "warn") {
        av_log_set_level(AV_LOG_WARNING);
    } else if (nativeLogLevel == "error") {
        av_log_set_level(AV_LOG_ERROR);
    } else {
        // Our default level
        av_log_set_level(AV_LOG_INFO);
    }

    return 1;
}
