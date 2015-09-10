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
//    ImageSequence *imageSequence  = new ImageSequence("/Users/apple/phantomjs/examples/frames/",1,0);
//    
//    VideoFileInstance *contentVideo = new VideoFileInstance(1,imageSequence,"/Users/apple/temp/Before_Vorator-2.mp4");
//    //imageSequence->getFrame(0);
//    
//    // animationFileInstance = new VideoFileInstance(2,"/Users/apple/temp/kinetic_small.mp4");
//    
//    //contentVideo->remux();
//    
//    contentVideo->startOverlaying();
//    //animationFileInstance->cleanup();
//    
//    //contentVideo->startDecoding();
//    contentVideo->cleanup();
//    
//    return 1;
//    
//}
//
//
