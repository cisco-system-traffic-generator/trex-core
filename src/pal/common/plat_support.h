#ifndef PLATFORM_CHECK_SUPPORT_H
#define PLATFORM_CHECK_SUPPORT_H

/*
Copyright (c) 2016-2017 Cisco Systems, Inc.

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

/* check that we are 64bit platform*/

#if defined(__x86_64__) || defined(__aarch64__)
 // supported 
#else
    #error("NOT supported platform");
#endif


#endif
