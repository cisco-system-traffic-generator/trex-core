#include "utl_json.h"
#include <stdio.h>
/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/



std::string add_json(std::string name, uint32_t counter,bool last){
    char buff[200];
    sprintf(buff,"\"%s\":%lu",name.c_str(),counter);
    std::string s= std::string(buff);
    if (!last) {
        s+=",";
    }
    return (s);
}

std::string add_json(std::string name, uint64_t counter,bool last){
    char buff[200];
    sprintf(buff,"\"%s\":%llu",name.c_str(),counter);
    std::string s= std::string(buff);
    if (!last) {
        s+=",";
    }
    return (s);
}

std::string add_json(std::string name, double counter,bool last){
    char buff[200];
    sprintf(buff,"\"%s\":%.1f",name.c_str(),counter);
    std::string s= std::string(buff);
    if (!last) {
        s+=",";
    }
    return (s);
}





