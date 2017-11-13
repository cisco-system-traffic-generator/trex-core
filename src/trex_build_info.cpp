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
#include "trex_build_info.h"
#include <gnu/libc-version.h>

#include <sstream>
#include <features.h>
#include <iostream>

bool TrexBuildInfo::is_sanitized() {
    #ifdef __SANITIZE_ADDRESS__
        return true;
    #else
        return false;
    #endif
}


std::string TrexBuildInfo::get_gcc_built_with_vesrion() {
    return __VERSION__;
}


std::string TrexBuildInfo::get_glibc_built_with_version() {
    std::stringstream ss;
    ss << __GLIBC__ << "." << __GLIBC_MINOR__;
    
    return ss.str();
}


std::string TrexBuildInfo::get_host_glibc_version() {
    return gnu_get_libc_version();
}


void TrexBuildInfo::show() {
    std::cout << "\n";
    std::cout << "Compiled with GCC     :   " << get_gcc_built_with_vesrion() << "\n";
    std::cout << "Compiled with glibc   :   " << get_glibc_built_with_version() << " (host: " << get_host_glibc_version() << ")\n";
    std::cout << "Sanitized image       :   " << (is_sanitized() ? "yes" : "no") << "\n";
}


