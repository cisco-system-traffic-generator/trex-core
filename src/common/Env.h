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

#ifndef OS_ENV
#define OS_ENV
                   
#ifdef __GNUC__
    #if  (_BYTE_ORDER == _BIG_ENDIAN)
      #define  BIG_EDIAN
    #else
      #undef  BIG_EDIAN
    #endif
#endif

#ifdef _MSC_VER
     #define BYTE_ORDER
     #undef BIG_EDIAN
#endif


#endif 
