

#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include "ImageSequence.h"
#include "OverlayAnimation.h"
#include "Utils.h"

extern "C"{
#include <libavformat/avformat.h>

#include <jni.h>
#include "com_typito_exporter_RenderJobThread.h"
}

using namespace std;

#define TRACE(msg)            wcout << msg
#define TRACE_ACTION(a, k, v) wcout << a << L" (" << k << L", " << v << L")\n"



std::string convertToStr(JNIEnv* env, jstring s)
{
    if(s == NULL || env == NULL) return string("");
    const char* str = env->GetStringUTFChars(s, 0);
    string ret = str;
    env->ReleaseStringUTFChars(s, str);

    return ret;
}

//
// /**
//  takes in java version of ImageSequence object and sets the appropriate values native copy
//  */
// void convertFromImageImageSequenceObject(JNIEnv * env , jobject javaImageSeqObj , ImageSequence *imageSeqNative){
//     //fps
//     jmethodID fpsGetter = getMethodId(env, javaImageSeqObj, "getFps","()I" );
//     jint fps = env->CallIntMethod(javaImageSeqObj, fpsGetter);
//     imageSeqNative->setFps((int)fps);
//
//     //startTime
//     jmethodID startTimeGetter = getMethodId(env, javaImageSeqObj, "getStartTime", "()F");
//     jfloat startTime = env->CallFloatMethod(javaImageSeqObj, startTimeGetter);
//     imageSeqNative->setOffsetTime(startTime);
//
//     //zIndex
//     jmethodID zIndexGetter = getMethodId(env, javaImageSeqObj, "getzIndex", "()I");
//     jint zIndex = env->CallIntMethod(javaImageSeqObj, zIndexGetter);
//     imageSeqNative->setZIndex((int)zIndex);
//
//     //num of frames
//     jmethodID numFramesGetter = getMethodId(env, javaImageSeqObj, "getNumFrames", "()I");
//     jint numFrames = env->CallIntMethod(javaImageSeqObj, numFramesGetter);
//     imageSeqNative->setNumFrames(numFrames);
//
//     //seqId
//     jmethodID idGetter = getMethodId(env, javaImageSeqObj, "getId", "()Ljava/lang/String;");
//     jstring seqId = (jstring)env->CallObjectMethod(javaImageSeqObj, idGetter);
//     const char* seqIdChar = env->GetStringUTFChars(seqId,NULL);
//     string seqIdStr(seqIdChar);
//     imageSeqNative->setSeqId(seqIdStr);
//     env->ReleaseStringUTFChars(seqId, seqIdChar);
//
// }


// JNIEXPORT jint JNICALL Java_com_typito_exporter_RenderJobThread_startJobNative
// (JNIEnv * env, jobject obj, jstring videoPathJStr, jstring outputFilePathJStr, jstring uniqueIdJStr,jobjectArray imageSeqJObjects){
//
//     std::string videoPath = convertToStr(env , videoPathJStr);
//
//     std::string outputFilePath = convertToStr(env , outputFilePathJStr);
//     std::string uniqueId = convertToStr(env , uniqueIdJStr);
//
//     int numAnimations = env->GetArrayLength(imageSeqJObjects);
//
//     av_log(NULL,AV_LOG_DEBUG , "starting job native");
//
//     try{
//
//         ImageSequence * imageSequenceList[numAnimations];
//         int i = 0;
//         for (i=0;i<numAnimations;i++){
//             jobject javaImageSeqObj = env->GetObjectArrayElement(imageSeqJObjects, i);
//             ImageSequence *imageSequence  = new ImageSequence(env,obj);
//             convertFromImageImageSequenceObject(env , javaImageSeqObj , imageSequence);
//             imageSequenceList[i] = imageSequence;
//         }
//
//
//         time_t startTime= std::time(0);
//
//         VideoFileInstance *contentVideo = new VideoFileInstance(1,imageSequenceList,numAnimations,
//                                                                 videoPath.c_str(),
//                                                                 outputFilePath.c_str(),
//                                                                 1,env,obj);
//         int ret = contentVideo->openInputAndOutputFiles();
//
//         if(ret < 0){
//             av_log(NULL,AV_LOG_ERROR ,"Error occured while openong input or output files");
//             return -1;
//         }
//
//         contentVideo->setUniqueId(uniqueId);
//         ret = contentVideo->startOverlaying();
//
//
//         if(ret < 0){
//
//             av_log(NULL,AV_LOG_ERROR ,"Error occured while overlaying");
//                         return -1;
//         }
//
//         // contentVideo->cleanup();
//     }catch(exception const & e){
//         wcout << e.what() << endl;
//         return -1;
//     }
//
//     return 1;
// }

JNIEXPORT jstring JNICALL Java_com_typito_exporter_RenderJobThread_getJobStatusNative
(JNIEnv *env, jobject object, jstring uniqueId){


    return NULL;
}
// 
//
// JNIEXPORT jint JNICALL Java_com_typito_exporter_RenderJobThread_setupLibNative
// (JNIEnv *env, jobject object){
//     av_register_all();
//     return 1;
// }

//int main(){
//    JavaVM *jvm;
//    JNIEnv *env;
//    JavaVMInitArgs vm_args;
//    JavaVMOption* options = new JavaVMOption[1];
//    options[0].optionString = "-Djava.class.path=/Users/apple/Vorator/vorator_codebase/vorator/ExporterApp/target/classes";
//
//    vm_args.version = JNI_VERSION_1_6;             // minimum Java version
//    vm_args.nOptions = 1;                          // number of options
//    vm_args.options = options;
//    vm_args.ignoreUnrecognized = false;
//
//    jint rc = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);  // YES !!
//    delete options;    // we then no longer need the initialisation options.
//    if (rc != JNI_OK) {
//        // TO DO: error processing...
//        cin.get();
//        exit(EXIT_FAILURE);
//    }
//    //=============== Display JVM version =======================================
//    cout << "JVM load succeeded: Version ";
//    jint ver = env->GetVersion();
//    cout << ((ver>>16)&0x0f) << "."<<(ver&0x0f) << endl;
//
//    jclass TestClazz = env->FindClass("com/typito/exporter/Test");
//    jobject obj;
//    if(TestClazz != nullptr){
//        cout <<"found the test class" << "\n";
//        jmethodID jmethodID = env->GetStaticMethodID(TestClazz , "test" , "()Lcom/typito/exporter/RenderJobThread;");
//
//         obj = env->CallStaticObjectMethod(TestClazz, jmethodID);
//        if(obj != nullptr){
//            cout << "got java object";
//        }
//
//    }
//
//
//
//    // TO DO: add the code that will use JVM <============  (see next steps)
//    cin.get();
//    jvm->DestroyJavaVM();
//}
