#ifndef C_COMMON
#define C_COMMON
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
#include <stdint.h>
#include <assert.h>

typedef uint8_t   uint8;
typedef int8_t     int8;

typedef uint16_t  uint16;
typedef int16_t    int16;

typedef uint32_t   uint32; //32 bit
typedef int32_t    int32;


typedef uint32_t    uint;
typedef uint8_t    uchar;
typedef uint16_t   ushort;

typedef void*           c_pvoid;            

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else  /* __cplusplus */
#define NULL    ((void *)0)
#endif  /* __cplusplus */
#endif  /* NULL */


#ifdef _DEBUG
    #define BP_ASSERT(a) assert(a)
#else
    #define BP_ASSERT(a) (void (a))
#endif

#endif
