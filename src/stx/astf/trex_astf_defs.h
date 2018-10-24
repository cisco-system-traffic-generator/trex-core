/*
 TRex team
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
#ifndef __TREX_ASTF_DEFS_H__
#define __TREX_ASTF_DEFS_H__

#include "utl_ip.h"

typedef struct {
    double      duration;
    double      mult;
    bool        nc;
    uint32_t    latency_pps;
    bool        ipv6;
    uint32_t    client_mask;
} start_params_t;


typedef struct {
    double      cps;
    uint32_t    ports_mask;
    ipaddr_t    client_ip;
    ipaddr_t    server_ip;
    uint32_t    dual_ip;
} lat_start_params_t;



#endif /* __TREX_ASTF_DEFS_H__ */

