#ifndef __TREX_DEFS_H__
#define __TREX_DEFS_H__

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
#include <map>
#include <unordered_map>
#include <functional>
#include <json/json.h>

#define TREX_MAX_PORTS 16

#define MAX_SOCKETS_SUPPORTED   (4)
#define MAX_THREADS_SUPPORTED   (120)


// maximum number of IP ID type flow stats we support. Must be in the form 2^x - 1
#define MAX_FLOW_STATS 1023
#define MAX_FLOW_STATS_X710 127
#define MAX_FLOW_STATS_XL710 255
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


enum pgid_types_e {
    PGID_FLOW_STAT,
    PGID_LATENCY
};

struct active_pgid {
    uint32_t m_pg_id;
    pgid_types_e m_type;
};

struct lengthed_str_t {
    uint16_t m_len;
    char *m_str;
};

typedef std::vector<active_pgid> flow_stat_active_t_new;
typedef std::vector<active_pgid>::iterator flow_stat_active_it_t_new;
typedef std::set<uint32_t> flow_stat_active_t;
typedef std::set<uint32_t>::iterator flow_stat_active_it_t;
typedef std::vector<cpu_vct_st> cpu_util_full_t;
typedef std::vector<std::string> xstats_names_t;
typedef std::vector<uint64_t> xstats_values_t;
typedef std::vector<uint32_t> supp_speeds_t;
typedef std::set<uint32_t> stream_ids_t;
typedef std::unordered_map<uint32_t,double> stream_ipgs_map_t;
typedef std::unordered_map<uint32_t,double>::const_iterator stream_ipgs_map_it_t;
typedef std::unordered_map<uint8_t, bool> uint8_to_bool_map_t;

// stack related
typedef std::function<void(void)> stack_task_t;
typedef std::vector<stack_task_t> task_list_t;
typedef std::unordered_map<std::string,std::string> err_per_mac_t;
typedef std::set<std::string> str_set_t;
typedef std::vector<uint16_t> vlan_list_t;
typedef struct {
    bool            is_ready;
    err_per_mac_t   err_per_mac;
} stack_result_t;
typedef std::map<uint64_t,stack_result_t> stack_result_map_t;
// end stack related

// async related
typedef std::function<void(Json::Value &result)> async_result_func_t;
typedef std::function<void(void)> async_cancel_func_t;
typedef struct {
    async_result_func_t result_func;
    async_cancel_func_t cancel_func;
} async_ticket_task_t;
typedef std::map<uint64_t,async_ticket_task_t> async_ticket_map_t;
// end async related

#endif
