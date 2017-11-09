/*
 Itay Marom
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

#ifndef __TREX_BUILD_INFO_H__
#define __TREX_BUILD_INFO_H__

#include <string>

/**
 * Build Information
 * 
 * @author imarom (11/7/2017)
 */
class TrexBuildInfo {
    
public:
    
    /**
     * returns true if the image is sanitized
     * 
     */
    static bool is_sanitized();
    
    /**
     * get GCC version used to build the binary
     */
    static std::string get_gcc_built_with_vesrion();
    
    /**
     * 
     * 
     * @author imarom (11/6/2017)
     * 
     * @return std::string 
     */
    static std::string get_glibc_built_with_version();
    
    
    /**
     * machine GLIBC version
     * 
     * @author imarom (11/6/2017)
     * 
     * @return std::string 
     */
    static std::string get_host_glibc_version();
    
    
    /**
     * print all information to stdout
     * 
     * @author imarom (11/6/2017)
     */
    static void show();
};

#endif /* __TREX_BUILD_INFO_H__ */

