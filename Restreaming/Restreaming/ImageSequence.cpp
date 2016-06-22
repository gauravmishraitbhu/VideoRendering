//
//  ImageSequence.cpp
//  Restreaming
//
//  Created by gaurav on 03/08/15.
//  Copyright (c) 2015 gaurav. All rights reserved.
//
extern "C"{
#include<jni.h>
}

#include "ImageSequence.h"
#include "ImageFrame.h"
#include <string>
#include "Utils.h"



using namespace std;

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};


// couple of helper methods
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

int saveImageRGBImageToDisk(const char *fileName , AVFrame *outputRGBFrame,int frameWidth , int frameHeight);

ImageSequence::ImageSequence(JNIEnv * env, jobject obj){
    
    this->env = env;
    this->javaObj = obj;
    
}

static int writeToTempFile(uint8_t *inbuf ,int bufferSize, const char * file){
    FILE *write_ptr;
    write_ptr = fopen(file,"wb");
    fwrite(inbuf,bufferSize,1,write_ptr);
    fclose(write_ptr);
    return 1;
}

void ImageSequence::convertJavaImageFrameToNativeImageFrame(JNIEnv * env , jobject imageFrameJava , ImageFrame ** imgFrameNative,
                                                            int *gotNewFrame){
    
    //frameIndex
    jmethodID frameIndexGetter = getMethodId(env , imageFrameJava , "getFrameIndex" , "()I");
    jint frameIndex = env->CallIntMethod(imageFrameJava, frameIndexGetter);
    
    if(currFrame != NULL && currFrame->hasDecodedFrame()){
        
        //check if the new frame returned from java is the same as the one which we already
        // decoded in prev iteration
        int prevFrameIndex = currFrame->getFrameIndex();
        if((int)frameIndex == prevFrameIndex){
            *gotNewFrame = 0;
            av_log(NULL ,AV_LOG_DEBUG,"got a duplicate frame hence returning");
            return;
        }
    }
    
    //this is a new frame so we can proceed with setting the member variables
    *gotNewFrame = 1;
    
    (*imgFrameNative)->setFrameIndex(frameIndex);
    
    //data bytearray
    AVFrame *frame = NULL;
    jmethodID byteArrayGetter = getMethodId(env , imageFrameJava, "getData", "()[B");
    jbyteArray imageBytes = (jbyteArray)env->CallObjectMethod(imageFrameJava, byteArrayGetter);
    jsize dataLength = env->GetArrayLength(imageBytes);
    jbyte* databytes = env->GetByteArrayElements(imageBytes,NULL);
    
    //add 32 bytes of padding
    uint8_t inputBuffer[dataLength + AV_INPUT_BUFFER_PADDING_SIZE];
    
    
    //uint8_t inputBuffer[dataLength + FF_INPUT_BUFFER_PADDING_SIZE];
    memset(inputBuffer + dataLength, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
    av_log(NULL,AV_LOG_DEBUG , "length=%d \n" , dataLength);
    memcpy(inputBuffer , (uint8_t *)databytes , dataLength);
    env->ReleaseByteArrayElements(imageBytes,databytes,JNI_ABORT);
    
    //std::string base64Str = base64_encode(inputBuffer , dataLength);
    //av_log(NULL,AV_LOG_ERROR , "base64str ---- %s \n",base64Str.c_str() );
    
    std::string basePath = "/Users/apple/mnt-storage";
    
    int result = decodeFromByteArray(inputBuffer, &frame,dataLength);
    
    av_log(NULL,AV_LOG_DEBUG , "function returned frame ---- %p \n",frame );
    
    
    (*imgFrameNative)->decodedFrame = frame;
    (*imgFrameNative)->setHeight(frame->height);
    (*imgFrameNative)->setWidth(frame->width);
    
    //    std::string testfileName = basePath + "/experiments/out/"+std::to_string(zIndex)+ "/" + std::to_string(frameIndex) + ".png";
    //    saveImageRGBImageToDisk(testfileName.c_str(), frame, frame->width, frame->height);
    
    if(result < 0){
        av_log( NULL , AV_LOG_ERROR , "error decoding bytearray");
    }
    
    
    //left
    jmethodID leftGetter = getMethodId(env, imageFrameJava, "getLeft", "()I");
    jint left = env->CallIntMethod(imageFrameJava, leftGetter);
    (*imgFrameNative)->setLeft(left);
    
    //top
    jmethodID topGetter = getMethodId(env , imageFrameJava , "getTop" , "()I");
    jint top = env->CallIntMethod(imageFrameJava , topGetter);
    (*imgFrameNative)->setTop(top);
    
    
}

ImageFrame* ImageSequence::getFrameForNum(int nextFrameNum) {
    int ret = 0;
    
    if(nextFrameNum < 0){
        return NULL;
    }
    av_log( NULL , AV_LOG_DEBUG , "\ngoing to get frame num--%d",nextFrameNum);
    av_log( NULL , AV_LOG_DEBUG , "\ncurrFrameNum-%d",currentFrameNum);
    
    currentFrameNum = nextFrameNum;
    
    //release the prev currFrame
    
    
    //make native call here and get byte array
    jclass myClass = env->FindClass("com/typito/exporter/RenderJobThread");
    
    if(env != NULL && myClass != NULL){
        jmethodID methodId = env->GetMethodID(myClass , "getFrame" , "(ILjava/lang/String;)Lcom/typito/exporter/beans/ImageFrame;");
        if(methodId == NULL){
            av_log( NULL , AV_LOG_ERROR , "cant find getFrame");
            
        }else{
            const char *seqIdChar = this->getSeqId().c_str();
            jstring seqIdJstr = env->NewStringUTF(seqIdChar);
            jobject imageFrameJava = env->CallObjectMethod(javaObj,methodId,nextFrameNum , seqIdJstr);
            
            if(imageFrameJava != NULL){
                int newDecodedFrame = 0;
                ImageFrame *imgFrame = new ImageFrame();
                convertJavaImageFrameToNativeImageFrame(env , imageFrameJava , &imgFrame ,&newDecodedFrame);
                if(newDecodedFrame){
                    //we got a new frame so free up the memory reference by prev frame
                    if(currFrame != NULL){
                        currFrame->freeDecodedFrame();
                        delete currFrame;
                    }
                    currFrame = imgFrame;
                }else{
                    // dont do anything prev frame ie this->currFrame will be returned and
                    // caller will use that frame
                    av_log( NULL , AV_LOG_DEBUG , "returning old");
                }
                
                
            }else{
                currFrame = NULL;
            }
        }
    } //end of env and thread class check
    
    return currFrame;
}

ImageFrame* ImageSequence::getFrameAtTime(float videoTime) {
    
    int nextFrameNum = this->calculateNextFrameNumberForTime(videoTime);
    
    return this->getFrameForNum(nextFrameNum);
}

ImageFrame* ImageSequence::getFrame(float contentVideoTimeBase , float ptsFactor,int contentVideoPts){
    int nextFrameNum = this->calculateNextFrameNumber(contentVideoTimeBase,  ptsFactor,contentVideoPts);
    
    return this->getFrameForNum(nextFrameNum);
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    
    av_log( NULL , AV_LOG_DEBUG , "goign to read some data from input bytearray \n");
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);
    //    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    av_log( NULL , AV_LOG_DEBUG , "buf size at end of read-- %d\n" , bd->size);
    return buf_size;
}


int ImageSequence::decodeFromByteArray(uint8_t *inbuf , AVFrame **decodedFrame , int dataLength){
    
    int ret;
    struct buffer_data bd = { 0 };
    bd.ptr = inbuf;
    size_t avio_ctx_buffer_size = 4096;
    bd.size = dataLength;
    
    AVFormatContext * inputFormatCtx = NULL;
    AVIOContext *avIOCtx = NULL;
    
    if (!(inputFormatCtx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    
    uint8_t * avio_ctx_buffer = (uint8_t *)av_malloc(avio_ctx_buffer_size);
    
    avIOCtx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                 0, &bd, &read_packet, NULL, NULL);
    
    inputFormatCtx->pb = avIOCtx;
    
    ret = avformat_open_input(&inputFormatCtx, NULL, NULL, NULL);
    if (ret < 0) {
        av_log( NULL , AV_LOG_ERROR , "could not open format context \n");
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(inputFormatCtx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    av_dump_format(inputFormatCtx, 0, "", 0);
    
    AVStream *stream;
    AVCodecContext *codecCtx;
    int decodedFrameHeight=0 , decodedFrameWidth = 0;
    stream = inputFormatCtx->streams[0];
    codecCtx = stream->codec;
    
    if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        codecCtx->thread_count = 1;
        codecCtx->refcounted_frames = 1;
        /* Open decoder */
        ret = avcodec_open2(codecCtx,
                            avcodec_find_decoder(codecCtx->codec_id), NULL);
        
        decodedFrameHeight = codecCtx->height;
        decodedFrameWidth = codecCtx->width;
        
        av_log(NULL,AV_LOG_DEBUG,"height of frame--%d",codecCtx->height);
        av_log(NULL,AV_LOG_DEBUG,"widht of frame--%d",codecCtx->width);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for image data\n");
            return ret;
        }
    }
    
    AVPacket packet;
    AVFrame *frame;
    int got_frame , len;
    
    while(1){
        av_init_packet(&packet);
        frame = av_frame_alloc();
        ret = av_read_frame(inputFormatCtx , &packet);
        
        if(ret  < 0){
            av_log( NULL , AV_LOG_DEBUG , "no more data left to be read\n");
            break;
        }
        
        av_log( NULL , AV_LOG_DEBUG , "reading packet successfull \n");
        
        int stream_index = packet.stream_index;
        
        if(stream_index != 0){
            //if not video frame just discard the frame.
            //in animation video audio frames are not expected
            continue;
        }
        
        
        len = avcodec_decode_video2(codecCtx, frame, &got_frame, &packet);
        
        if(ret<0){
            av_log( NULL , AV_LOG_DEBUG , "could not decode a packet....trying next packet\n");
            continue;
        }
        
        if(got_frame){
            av_free_packet(&packet);
            (frame)->height = decodedFrameHeight;
            (frame)->width = decodedFrameWidth;
            *decodedFrame = frame;
            av_log( NULL , AV_LOG_DEBUG , "decoded a frame so job done\n");
            break;
        }else{
            av_free_packet(&packet);
            av_frame_free(&frame);
            continue;
        }
        
    }
    
    
    av_log( NULL , AV_LOG_DEBUG , "successfully decoded a frame \n");
    
    
    
    av_freep(avIOCtx);
    //free decoder resources
    avcodec_close(codecCtx);
    avcodec_free_context(&codecCtx);
    //avformat_free_context(inputFormatCtx);
    
    return ret;
}

int ImageSequence::calculateNextFrameNumberForTime(float videoTime) {
    
    if(videoTime < offsetTime){
        return -1;
    }
    
    
    //if animation video is supposed to be delayed by some time then the above time needs to be
    //adjusted by subtracting the offset time eg if offset is 5 sec and absolute walltime comes
    //out to be 6 secs then we need to show the frame which is supposed to be at t=1 secs
    
    videoTime = videoTime - offsetTime;
    
    
    
    if(currentFrameNum == -1){
        //first time call.
        return 1;
    }
    av_log(NULL,AV_LOG_DEBUG,"fps----%d",fps);
    
    //start from currentframe number and find a animation frame whose pts is just greater than current video pts
    int nextBestFrameNum = currentFrameNum;
    
    while(1){
        //the time till which current frame needs to be displayed
        float wallClockTimeForCurrentAnimationFrame = float(1/(float)fps) * nextBestFrameNum;
        
        if(wallClockTimeForCurrentAnimationFrame >= videoTime){
            break;
        }else{
            nextBestFrameNum ++ ;
        }
    }
    
    av_log(NULL,AV_LOG_DEBUG,"\nwallclockTimeForAnimation--%f",float(1/(float)fps) * nextBestFrameNum);
    av_log(NULL,AV_LOG_DEBUG,"\nwallClockTimeContentVideo--%f",videoTime);
    return nextBestFrameNum;
}


int ImageSequence::calculateNextFrameNumber(float contentVideoTimeBase ,float ptsFactor ,int contentVideoPts){
    
    
    float wallClockTimeContentVideo = contentVideoTimeBase * contentVideoPts * ptsFactor;
    
    //the time for rendering this animation has not come yet
    //return -1
    //for eg - wallClockTimeContentVideo = 5 and animation is supposed to start at 6
    //then dont return any frame
    
    return calculateNextFrameNumberForTime(wallClockTimeContentVideo);
}



void ImageSequence::cleanup(){
    if(currFrame != NULL){
        currFrame->freeDecodedFrame();
    }
    
    
}

/////////////////////////////////////////////////////////End Of Class methods////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int saveImageRGBImageToDisk(const char *fileName , AVFrame *outputRGBFrame,int frameWidth , int frameHeight){
    
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *video_codec      = NULL;
    int ret                   = 0;
    AVCodecContext *c = NULL;
    
    av_log(NULL,AV_LOG_DEBUG , "goign to save frame to disk---%s \n" , fileName);
    
    
    //create a context
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL , fileName);
    
    video_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    
    if (!(video_codec)) {
        fprintf(stderr, "Could not find encoder for \n");
        return -1;
    }
    
    c = avcodec_alloc_context3(video_codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    
    
    AVStream *out_stream = avformat_new_stream(ofmt_ctx,video_codec);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    
    out_stream->id = ofmt_ctx->nb_streams-1;
    
    out_stream->codec = c;
    //    c = out_stream->codec;
    
    switch ((video_codec)->type) {
            
        case AVMEDIA_TYPE_VIDEO:
            
            
            c->codec_id                = AV_CODEC_ID_PNG;
            
            /* Resolution must be a multiple of two. */
            c->width                   = frameWidth;
            c->height                  = frameHeight;
            
            
            c->time_base               = (AVRational){1,25};
            out_stream->time_base = (AVRational){1,25};
            
            //this depends on the input frame format.for a video frame this will be RGB24
            c->pix_fmt                 = AV_PIX_FMT_RGBA;
            
            break;
            
            
    }
    
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    //write header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    
    
    //open the encoder
    ret = avcodec_open2(c, video_codec, NULL);
    
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec:\n");
        exit(1);
    }
    
    
    
    outputRGBFrame->width = frameWidth;
    outputRGBFrame->height = frameHeight;
    outputRGBFrame->pts = 1;
    outputRGBFrame->format = AV_PIX_FMT_RGBA;
    
    //encode the frame
    AVPacket encodedPacket;
    av_init_packet(&encodedPacket);
    encodedPacket.data = NULL;
    encodedPacket.size = 0;
    int encode_success;
    ret = avcodec_encode_video2(c,
                                &encodedPacket,
                                outputRGBFrame,
                                &encode_success);
    if(ret < 0){
        av_log(NULL,AV_LOG_ERROR , "Error encoding a frame");
        
    }
    
    if(encode_success){
        encodedPacket.stream_index = 0;
        
        
        //        av_packet_rescale_ts(encodedPacket,
        //                             this->out_stream.format_ctx->streams[0]->codec->time_base,
        //                             this->out_stream.format_ctx->streams[0]->time_base);
        
        encodedPacket.pts = 1;
        encodedPacket.dts = 1;
        ret = av_write_frame(ofmt_ctx, &encodedPacket);
        
        if(ret < 0){
            av_log(NULL,AV_LOG_ERROR , "Error writing  a frame to container...");
            return ret;
        }
        
    }
    
    ret = av_write_trailer(ofmt_ctx);
    
    if(ret < 0){
        av_log(NULL,AV_LOG_ERROR , "Error writing  a trailer to container...");
    }
    
    
    avcodec_close(c);
    //    avcodec_free_context(&c);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    
    
    avformat_free_context(ofmt_ctx);
    
    return 1;
}

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i)
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
        
    }
    
    return ret;
    
}


//int main(){
//    ImageSequence *s = new ImageSequence("/Users/apple/temp/html-renderer/output/4");
//
//}
