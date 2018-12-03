/*
  hhaim
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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

#ifndef __TREX_STACK_COUNTERS_H__
#define __TREX_STACK_COUNTERS_H__

#include <string>
#include <json/json.h>
#include "utl_counter.h"
#include "utl_dbl_human.h"


class CRxCounters {

public:
    enum {
        /* TX/RX */
        CNT_RX                  =0,
        CNT_TX                  =1,
        CNT_RX_TX_SIZE          =2,

        /* PKT/BYTE*/
        CNT_PKT                 =0,
        CNT_BYTE                =1,
        CNT_TYPE                =2,

        /* type of packet */
        CNT_UNICAST             =0,
        CNT_MULTICAST           =1,
        CNT_BROADCAST           =2,
        CNT_UMB_TYPE            =3,
    };


    bool Create();
    void Delete();

    void clear_counters();
    void dump();
    void dump_meta(std::string name,
                   Json::Value & json);

    void dump_values(std::string name,
                     bool zeros,
                     Json::Value & obj);

public:
    uint64_t            m_gen_cnt[CNT_RX_TX_SIZE][CNT_TYPE][CNT_UMB_TYPE];
    uint64_t            m_tx_err_small_pkt;
    uint64_t            m_tx_err_big_9k;
    uint64_t            m_rx_err_invalid_pkt;

    CPPSMeasure         m_rx_pps;
    CPPSMeasure         m_tx_pps;

    CGTblClmCounters    m_clm; /* utility for dump */
    CTblGCounters       m_tbl;
};



#endif /* __TREX_STACK_COUNTERS_H__ */
