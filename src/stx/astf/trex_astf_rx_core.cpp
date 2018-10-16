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

#include "trex_astf_rx_core.h"

CRxAstfCore::CRxAstfCore(void) : CRxCore() {
    m_rx_dp = CMsgIns::Ins()->getRxDp();
}

bool CRxAstfCore::work_tick(void) {
    bool did_something = CRxCore::work_tick();
    did_something |= handle_msg_packets();
    return did_something;
}

/* check for messages from DP cores */
uint32_t CRxAstfCore::handle_msg_packets(void) {
    uint32_t pkts = 0;
    for (uint8_t thread_id=0; thread_id<m_tx_cores; thread_id++) {
        CNodeRing *r = m_rx_dp->getRingDpToCp(thread_id);
        pkts += handle_rx_one_queue(thread_id, r);
    }
    return pkts;
}

uint32_t CRxAstfCore::handle_rx_one_queue(uint8_t thread_id, CNodeRing *r) {
    CGenNode * node;
    uint32_t got_pkts;
    CFlowGenListPerThread* lpt = get_platform_api().get_fl()->m_threads_info[thread_id];
    uint8_t port1, port2, port_pair_offset;
    lpt->get_port_ids(port1, port2);
    port_pair_offset = port1 & port2;

    for ( got_pkts=0; got_pkts<64; got_pkts++ ) { // read 64 packets at most
        if ( r->Dequeue(node)!=0 ){
            break;
        }
        assert(node);

        CGenNodeLatencyPktInfo *pkt_info = (CGenNodeLatencyPktInfo *) node;
        assert(pkt_info->m_msg_type==CGenNodeMsgBase::LATENCY_PKT);
        assert(pkt_info->m_latency_offset==0xdead);
        uint8_t dir = pkt_info->m_dir & 1;
        m_rx_port_mngr[port_pair_offset + dir]->handle_pkt((rte_mbuf_t *)pkt_info->m_pkt);

        CGlobalInfo::free_node(node);
    }
    return got_pkts;
}


