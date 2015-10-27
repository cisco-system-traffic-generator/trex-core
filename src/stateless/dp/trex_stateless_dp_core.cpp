/*
 Itay Marom
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
#include <trex_stateless_dp_core.h>
#include <trex_stateless_messaging.h>

#include <bp_sim.h>

typedef struct dp_node_extended_info_ {
    double next_time_offset;
} dp_node_extended_info_st;

TrexStatelessDpCore::TrexStatelessDpCore(uint8_t thread_id, CFlowGenListPerThread *core) {
    m_thread_id = thread_id;
    m_core = core;

    m_state = STATE_IDLE;

    CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(thread_id);
    m_ring_to_cp   = cp_dp->getRingDpToCp(thread_id);
}

void
TrexStatelessDpCore::start() {

    /* creates a maintenace job using the scheduler */
    CGenNode * node_sync = m_core->create_node() ;
    node_sync->m_type = CGenNode::FLOW_SYNC;
    node_sync->m_time = m_core->m_cur_time_sec + SYNC_TIME_OUT;
    m_core->m_node_gen.add_node(node_sync);

    double old_offset = 0.0;
    m_core->m_node_gen.flush_file(100000000, 0.0, false, m_core, old_offset);

}

void
TrexStatelessDpCore::handle_pkt_event(CGenNode *node) {

    /* if port has stopped - no transmition */
    if (m_state == STATE_IDLE) {
        return;
    }

    m_core->m_node_gen.m_v_if->send_node(node);

    CGenNodeStateless *node_sl = (CGenNodeStateless *)node;

    /* in case of continues */
    dp_node_extended_info_st *opaque = (dp_node_extended_info_st *)node_sl->get_opaque_storage();
    node->m_time += opaque->next_time_offset;

    /* insert a new event */
    m_core->m_node_gen.m_p_queue.push(node);
}

void
TrexStatelessDpCore::start_const_traffic(const uint8_t *pkt,
                                         uint16_t pkt_len,
                                         double pps) {

    CGenNodeStateless *node = m_core->create_node_sl();

    /* add periodic */
    node->m_type = CGenNode::STATELESS_PKT;
    node->m_time = m_core->m_cur_time_sec + 0.0 /* STREAM ISG */;
    node->m_flags = 0; 

    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */
    uint16_t pkt_size = pkt_len;
    const uint8_t *stream_pkt = pkt;

    dp_node_extended_info_st *opaque = (dp_node_extended_info_st *)node->get_opaque_storage();
    opaque->next_time_offset = 1.0 / pps;

    /* allocate const mbuf */
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), pkt_size);
    assert(m);

    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* copy the packet */
    memcpy(p,stream_pkt,pkt_size);

    /* set dir 0 or 1 client or server */
    pkt_dir_t dir = 0;
    node->set_mbuf_cache_dir(dir);

    /* TBD repace the mac if req we should add flag  */
    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir, m);

    /* set the packet as a readonly */
    node->set_cache_mbuf(m);

    m_state = TrexStatelessDpCore::STATE_TRANSMITTING;

    m_core->m_node_gen.add_node((CGenNode *)node);

}

void
TrexStatelessDpCore::stop_traffic() {
    m_state = STATE_IDLE;
}

/**
 * handle a message from CP to DP
 * 
 */
void 
TrexStatelessDpCore::handle_cp_msg(TrexStatelessCpToDpMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

