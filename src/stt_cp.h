#ifndef STT_CP_H
#define STT_CP_H

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

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

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <assert.h>
#include "44bsd/tcp_var.h"
#include "44bsd/flow_table.h"
#include "utl_counter.h"
#include "utl_dbl_human.h"

/* TCP Stateful CP object for counters and such */    

typedef enum {
    TCP_CLIENT_SIDE = 0,    
    TCP_SERVER_SIDE = 1,    
    TCP_CS_NUM = 2,
    TCP_CS_INVALID = 255
} tcp_dir_enum_t;

typedef uint8_t tcp_dir_t;

class CSTTCpPerDir {
public:
    bool Create();
    void Delete();
    void update_counters();
    void create_clm_counters();

public:
    tcpstat             m_tcp;
    CSttFlowTableStats  m_ft;

    /* externation counters */
    uint64_t            m_active_flows;
    uint64_t            m_est_flows;

    CBwMeasure          m_tx_bw_l7;
    double              m_tx_bw_l7_r;
    CBwMeasure          m_rx_bw_l7;
    double              m_rx_bw_l7_r;
    CPPSMeasure         m_tx_pps;
    double              m_tx_pps_r;
    CPPSMeasure         m_rx_pps;
    double              m_rx_pps_r;

    double              avg_size;

    /* externation counters --end */

    CGTblClmCounters    m_clm; /* utility for dump */

    std::vector<CTcpPerThreadCtx*>  m_tcp_ctx; /* vectors contexts*/
};

class CSTTCp {

public:
    void Create();
    void Delete();
    void Add(tcp_dir_t dir,CTcpPerThreadCtx* ctx);
    void Init();
    void Update();
    void DumpTable();

public:
    CSTTCpPerDir   m_sts[TCP_CS_NUM];
    CTblGCounters  m_dtbl;
    bool           m_init;
private:

};


#endif
