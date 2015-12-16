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
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "ImageSequence.h"
#include "OverlayAnimation.h"
#include "GlobalData.h"

extern "C"{
#include <libavformat/avformat.h>
    
}

using namespace std;

#define TRACE(msg)            wcout << msg
#define TRACE_ACTION(a, k, v) wcout << a << L" (" << k << L", " << v << L")\n"

map<utility::string_t, utility::string_t> dictionary;

class Task{
private:
    vector<string> animationPathList;
    string videoPath,outputFilePath;
    int reportingEnabled,uniqueId;
    int numAnimations;
    
    
    public :
    Task(string _videoPath,string _outputFilePath, vector<string> _animationPathList,int _reportingEnabled,int _uniqueId){
        cout << "Starting a task";
        
        videoPath         = _videoPath;
        animationPathList = _animationPathList;
        numAnimations     = (int)animationPathList.size();

        outputFilePath    = _outputFilePath;
        reportingEnabled  = _reportingEnabled;
        uniqueId          = _uniqueId;
    }
    
    void operator()() const
    {
        try{
            
            ImageSequence * imageSequenceList[numAnimations];
            int i = 0;
            for (i=0;i<animationPathList.size();i++){
                string animationPath = animationPathList[i];
                boost::trim_left(animationPath);
                boost::trim_right(animationPath);
                ImageSequence *imageSequence  = new ImageSequence(animationPath.c_str());
                imageSequenceList[i] = imageSequence;
            }
            
            
           
            VideoFileInstance *contentVideo = new VideoFileInstance(1,imageSequenceList,numAnimations,
                                                                    videoPath.c_str(),
                                                                    outputFilePath.c_str(),
                                                                    reportingEnabled);
            int ret = contentVideo->openInputAndOutputFiles();
            
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR ,"Error occured while openong input or output files");
                return;
            }
            
            contentVideo->setUniqueId(uniqueId);
            ret = contentVideo->startOverlaying();
            
            if(ret < 0){
                av_log(NULL,AV_LOG_ERROR ,"Error occured while overlaying");
            }
            
            contentVideo->cleanup();
        }catch(exception const & e){
            wcout << e.what() << endl;
        }
    }
};


void handle_get(http_request request)
{
    TRACE(L"\nhandle GET\n");
    
    string s = request.to_string();
    cout << request.relative_uri().to_string();
    
    if(boost::starts_with(request.relative_uri().to_string() , "/status")){
        auto http_get_vars = uri::split_query(request.request_uri().query());
        
        auto foundId = http_get_vars.find("uniqueId");
        
        if (foundId == end(http_get_vars)) {
            
            request.reply(status_codes::BadRequest, "uniqueId missing");

        }else{
            auto uniqueId = foundId->second;
            int id = atoi(uniqueId.c_str());
            if(GlobalData::jobStatusMap.find(id) == GlobalData::jobStatusMap.end()){
                //key not found
                request.reply(status_codes::OK, "percent=0");
            }else{
                //cout << GlobalData::jobStatusMap[id];
                request.reply(status_codes::OK, "percent="+std::to_string(GlobalData::jobStatusMap[id]));
            }
            
            
        }

    }else{
        //TRACE(s.c_str());
        request.reply(status_codes::OK, "Hello WOrld");
    }
    
    
    
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
    
    
   
    
    if(postParams.find("uniqueId") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["uniqueId"]);
        uniqueId = std::stoi (val.as_string());
    }else{
        uniqueId = 15;
    }
    
    
    if(postParams.find("reportingEnabled") != postParams.end()){
        val = boost::any_cast<json::value> (postParams["reportingEnabled"]);
        reportingEnabled = std::stoi(val.as_string());
    }else{
        reportingEnabled = 1;
    }
    
    
    std::vector<string> animationList;
    split(animationList,animationPath,boost::is_any_of(","),boost::token_compress_on);
    
    Task task(videoPath,outputFilePath,animationList,reportingEnabled,uniqueId);
    std::thread thread(task);
    thread.detach();
    
    
    //cout << request.to_string();
    
    request.reply(status_codes::OK, "Job Started");
}

int main()
{
    
    av_register_all();
    
    //    avfilter_register_all();
    //utility::string_t s(L"http://0.0.0.0:8000/render");
    http_listener listener("http://0.0.0.0:8000/render");
    
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