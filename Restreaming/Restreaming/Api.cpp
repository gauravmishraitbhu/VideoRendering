#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#pragma comment(lib, "cpprest110_1_1")

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <boost/any.hpp>

#include "ImageSequence.h"
#include "OverlayAnimation.h"

extern "C"{
#include <libavformat/avformat.h>

}

using namespace std;

#define TRACE(msg)            wcout << msg
#define TRACE_ACTION(a, k, v) wcout << a << L" (" << k << L", " << v << L")\n"

map<utility::string_t, utility::string_t> dictionary;

class Task{
    private:
    
    string videoPath,animationPath,outputFilePath;
    int fps , maxFrames,reportingEnabled,uniqueId;
    
    
    public :
    Task(string _videoPath,string _outputFilePath, string _animationPath , int _fps, int _maxFrames,int _reportingEnabled,int _uniqueId){
        cout << "Starting a task";
        fps = _fps;
        videoPath = _videoPath;
        animationPath = _animationPath;
        maxFrames = _maxFrames;
        outputFilePath = _outputFilePath;
        reportingEnabled = _reportingEnabled;
        uniqueId = _uniqueId;
    }
    
    void operator()() const
    {
        try{
        
        ImageSequence *imageSequence  = new ImageSequence(animationPath.c_str(),1,0,fps,maxFrames); //12,653
        
        VideoFileInstance *contentVideo = new VideoFileInstance(1,imageSequence,
                                                                videoPath.c_str(),
                                                                outputFilePath.c_str(),
                                                                reportingEnabled);
        
            
        contentVideo->setUniqueId(uniqueId);
        contentVideo->startOverlaying();
        
        contentVideo->cleanup();
        }catch(exception const & e){
           wcout << e.what() << endl;
        }
    }
};


void handle_get(http_request request)
{
    TRACE(L"\nhandle GET\n");
//    Task task;
//    std::thread thread(task);
//    thread.detach();
    cout << request.to_string();
    request.reply(status_codes::OK, "Hello WOrld");
}




void handle_post(http_request request)
{
    TRACE("\nhandle POST\n");
    
    std::map<string,boost::any> postParams ;
    request.extract_json().then([&postParams ](pplx::task<json::value> task){
        
        try{
            json::value val = task.get();
            cout <<val;
            
            cout << val.is_string();
            
            for(auto iter  = val.as_object().cbegin() ; iter != val.as_object().cend() ; ++iter){
                const std::string &str = iter->first;
                const json::value &v = iter->second;
                
                postParams[str] = v;
                
                //cout << str << v <<"\n";
            }
            

        }catch (exception const & e){
            wcout << e.what() << endl;
        }

    }).wait();
    
    string videoPath,animationPath,outputFilePath;
    int fps,max_frames,reportingEnabled,uniqueId;
    
    json::value val;
    if(postParams.find("videoPath") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["videoPath"]);
        videoPath = val.as_string();
    }else{
        videoPath = "/Users/apple/temp/Before_Vorator.mp4";
    }
    
    if(postParams.find("animationPath") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["animationPath"]);
        animationPath = val.as_string();
    }else{
        animationPath = "/Users/apple/phantomjs/examples/frames/";
    }
    
    if(postParams.find("outputFilePath") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["outputFilePath"]);
        outputFilePath = val.as_string();
    }else{
        outputFilePath = "/Users/apple/temp/sample_output.mp4";
    }
    
    
    if(postParams.find("fps") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["fps"]);
        fps = std::stoi (val.as_string());
    }else{
        fps = 12;
    }
    
    if(postParams.find("uniqueId") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["uniqueId"]);
        uniqueId = std::stoi (val.as_string());
    }else{
        uniqueId = 15;
    }
    
    if(postParams.find("maxFrames") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["maxFrames"]);
        max_frames = std::stoi(val.as_string());
    }else{
        max_frames = 653;
    }
    
    if(postParams.find("reportingEnabled") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["reportingEnabled"]);
        reportingEnabled = std::stoi(val.as_string());
    }else{
        reportingEnabled = 1;
    }
    
    Task task(videoPath,outputFilePath,animationPath,fps,max_frames,reportingEnabled,uniqueId);
    std::thread thread(task);
    thread.detach();

    
    //cout << request.to_string();

    request.reply(status_codes::OK, "Job Started");
}

int main()
{
    
    av_register_all();
    
    avfilter_register_all();
    //utility::string_t s(L"http://localhost:8000/render");
    http_listener listener("http://192.168.0.108:8000/render");
    
    listener.support(methods::GET, handle_get);
   listener.support(methods::POST, handle_post);
//    listener.support(methods::PUT, handle_put);
//    listener.support(methods::DEL, handle_del);
    
    try
    {
        listener
        .open()
        .then([&listener](){TRACE("\nstarting to listen\n");})
        .wait();
        
        while (true);
    }
    catch (exception const & e)
    {
        wcout << e.what() << endl;
    }
    
    return 0;
}