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
#include <trex_streams_compiler.h>
#include <trex_stream_node.h>

#include <bp_sim.h>

static inline double
usec_to_sec(double usec) {
    return (usec / (1000 * 1000));
}


void
TrexStatelessDpCore::create(uint8_t thread_id, CFlowGenListPerThread *core) {
    m_thread_id = thread_id;
    m_core = core;

    CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(thread_id);
    m_ring_to_cp   = cp_dp->getRingDpToCp(thread_id);

    m_state = STATE_IDLE;
}

/**
 * in idle state loop, the processor most of the time sleeps 
 * and periodically checks for messages 
 * 
 * @author imarom (01-Nov-15)
 */
void 
TrexStatelessDpCore::idle_state_loop() {

    while (m_state == STATE_IDLE) {
        periodic_check_for_cp_messages();
        delay(200);
    }
}

/**
 * scehduler runs when traffic exists 
 * it will return when no more transmitting is done on this 
 * core 
 * 
 * @author imarom (01-Nov-15)
 */
void
TrexStatelessDpCore::start_scheduler() {
    /* creates a maintenace job using the scheduler */
    CGenNode * node_sync = m_core->create_node() ;
    node_sync->m_type = CGenNode::FLOW_SYNC;
    node_sync->m_time = m_core->m_cur_time_sec + SYNC_TIME_OUT;
    m_core->m_node_gen.add_node(node_sync);

    double old_offset = 0.0;
    m_core->m_node_gen.flush_file(-1, 0.0, false, m_core, old_offset);
}

void
TrexStatelessDpCore::start() {

    while (true) {
        idle_state_loop();

        start_scheduler();
    }
}

void
TrexStatelessDpCore::add_cont_stream(uint8_t port_id,
                                     double isg_usec,
                                     double pps,
                                     const uint8_t *pkt,
                                     uint16_t pkt_len) {

    CGenNodeStateless *node = m_core->create_node_sl();

    /* add periodic */
    node->m_type = CGenNode::STATELESS_PKT;

    node->m_time = m_core->m_cur_time_sec + usec_to_sec(isg_usec);

    pkt_dir_t dir = m_core->m_node_gen.m_v_if->port_id_to_dir(port_id);
    node->m_flags = 0; 

    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */
    uint16_t pkt_size = pkt_len;
    const uint8_t *stream_pkt = pkt;

    /* stateless specific fields */
    node->m_next_time_offset = 1.0 / pps;
    node->m_is_stream_active = 1;
    node->m_port_id = port_id;

    /* allocate const mbuf */
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), pkt_size);
    assert(m);

    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* copy the packet */
    memcpy(p,stream_pkt,pkt_size);

    /* set dir 0 or 1 client or server */
    node->set_mbuf_cache_dir(dir);

    /* TBD repace the mac if req we should add flag  */
    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir, m);

    /* set the packet as a readonly */
    node->set_cache_mbuf(m);

    /* keep track */
    m_active_nodes.push_back(node);

    /* schedule */
    m_core->m_node_gen.add_node((CGenNode *)node);

    m_state = TrexStatelessDpCore::STATE_TRANSMITTING;

}

void
TrexStatelessDpCore::start_traffic(TrexStreamsCompiledObj *obj) {
    for (auto single_stream : obj->get_objects()) {
        add_cont_stream(single_stream.m_port_id,
                        single_stream.m_isg_usec,
                        single_stream.m_pps,
                        single_stream.m_pkt,
                        single_stream.m_pkt_len);
    }
}

void
TrexStatelessDpCore::stop_traffic(uint8_t port_id) {
    /* we cannot remove nodes not from the top of the queue so
       for every active node - make sure next time
       the scheduler invokes it, it will be free */
    for (auto node : m_active_nodes) {
        if (node->m_port_id == port_id) {
            node->m_is_stream_active = 0;
        }
    }

    /* remove all the non active nodes */
    auto pred = std::remove_if(m_active_nodes.begin(),
                               m_active_nodes.end(), 
                               [](CGenNodeStateless *node) { return (!node->m_is_stream_active); });

    m_active_nodes.erase(pred, m_active_nodes.end());

    if (m_active_nodes.size() == 0) {
        m_state = STATE_IDLE;
        /* stop the scheduler */

        CGenNode *node = m_core->create_node() ;

        node->m_type = CGenNode::EXIT_SCHED;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + 0.0001;

        m_core->m_node_gen.add_node(node);
    }
    
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

