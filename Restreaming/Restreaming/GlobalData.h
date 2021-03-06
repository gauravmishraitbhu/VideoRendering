//
//  GlobalData.h
//  Restreaming
//
//  Created by gaurav on 15/12/15.
//  Copyright © 2015 gaurav. All rights reserved.
//


#ifndef GlobalData_h
#define GlobalData_h

#include <map>

namespace GlobalData {
    extern std::map<std::string,int> jobStatusMap;
    extern std::map<std::string , time_t> startTimes ;
    extern std::map<std::string , time_t> endTimes ;
}


#endif /* GlobalData_h */
