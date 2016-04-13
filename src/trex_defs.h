/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#include <set>

#ifndef __TREX_DEFS_H__
#define __TREX_DEFS_H__

#define TREX_MAX_PORTS 12

#define MAX_FLOW_STATS 127

#ifndef UINT8_MAX
    #define UINT8_MAX 255
#endif

#ifndef UINT16_MAX
    #define UINT16_MAX 0xFFFF
#endif


typedef std::set<uint32_t> flow_stat_active_t;
typedef std::set<uint32_t>::iterator flow_stat_active_it_t;

#endif
