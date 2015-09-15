////
////  Main.cpp
////  Restreaming
////
////  Created by gaurav on 09/09/15.
////  Copyright (c) 2015 gaurav. All rights reserved.
////
//
//#include "Main.h"
//using namespace std;
//
//
//extern "C"{
//#include <libavformat/avformat.h>
//#include <libavfilter/avfilter.h>
//#include <libavfilter/avcodec.h>
//}
//
//#include "OverlayAnimation.h"
//#include "ImageSequence.h"
//
//
//int main(int argc, char **argv) {
//    av_register_all();
//    avformat_network_init();
//    avfilter_register_all();
//    
//    
//    
//    ImageSequence *imageSequence  = new ImageSequence("/Users/apple/temp/frames/",1,0,12,653); //12,653
//    
//            VideoFileInstance *contentVideo = new VideoFileInstance(1,imageSequence,
//                                                                    "/Users/apple/temp/Before_Vorator.mp4",
//                                                                    "/Users/apple/temp/sample_output.mp4",
//                                                                    1);
//    
//    
//            contentVideo->setUniqueId(2);
//            contentVideo->startOverlaying();
//    
//            contentVideo->cleanup();
//    
//    return 1;
//    
//}
//
//
