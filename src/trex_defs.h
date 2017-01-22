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
#include <queue>
#include <vector>
#include <string>

#ifndef __TREX_DEFS_H__
#define __TREX_DEFS_H__

#define TREX_MAX_PORTS 16

// maximum number of IP ID type flow stats we support
#define MAX_FLOW_STATS 127
// maximum number of payload type flow stats we support
#define MAX_FLOW_STATS_PAYLOAD 128

#ifndef UINT8_MAX
    #define UINT8_MAX 255
#endif

#ifndef UINT16_MAX
    #define UINT16_MAX 0xFFFF
#endif

#ifndef UINT64_MAX
    #define UINT64_MAX 0xFFFFFFFFFFFFFFFF
#endif

struct cpu_vct_st {
    cpu_vct_st() {
        m_port1 = -1;
        m_port2 = -1;
    }
    std::vector<uint8_t> m_history;
    int m_port1;
    int m_port2;
};

typedef std::set<uint32_t> flow_stat_active_t;
typedef std::set<uint32_t>::iterator flow_stat_active_it_t;
typedef std::vector<cpu_vct_st> cpu_util_full_t;
typedef std::vector<std::string> xstats_names_t;
typedef std::vector<uint64_t> xstats_values_t;
typedef std::vector<uint32_t> supp_speeds_t;

#endif
