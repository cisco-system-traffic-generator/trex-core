/*
 Itay Marom
 Hanoch Haim
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
#include <trex_stream.h>

#include <bp_sim.h>


void CDpOneStream::Delete(CFlowGenListPerThread   * core){
    assert(m_node->get_state() == CGenNodeStateless::ss_INACTIVE);
    core->free_node((CGenNode *)m_node);
    delete m_dp_stream;
    m_node=0;
    m_dp_stream=0;
}

void CDpOneStream::DeleteOnlyStream(){
    assert(m_dp_stream);
    delete m_dp_stream;
    m_dp_stream=0;
}

int CGenNodeStateless::get_stream_id(){
    if (m_state ==CGenNodeStateless::ss_FREE_RESUSE) {
        return (-1); // not valid
    }
    assert(m_ref_stream_info);
    return ((int)m_ref_stream_info->m_stream_id);
}


void CGenNodeStateless::DumpHeader(FILE *fd){
    fprintf(fd," pkt_id, time, port , action , state, stream_id , stype , m-burst# , burst# \n");

}
void CGenNodeStateless::Dump(FILE *fd){
    fprintf(fd," %2.4f, %3lu, %s,%s, %3d, %s, %3lu, %3lu  \n",
            m_time,
            (ulong)m_port_id,
            "s-pkt", //action
            get_stream_state_str(m_state ).c_str(),
            get_stream_id(),   //stream_id
            TrexStream::get_stream_type_str(m_stream_type).c_str(), //stype
            (ulong)m_multi_bursts,
            (ulong)m_single_burst
            );
}


void CGenNodeStateless::refresh(){

    /* refill the stream info */
    m_single_burst    = m_single_burst_refill;
    m_multi_bursts    = m_ref_stream_info->m_num_bursts;
    m_state           = CGenNodeStateless::ss_ACTIVE;
}



void CGenNodeCommand::free_command(){
    assert(m_cmd);
    delete m_cmd;
}


std::string CGenNodeStateless::get_stream_state_str(stream_state_t stream_state){
    std::string res;

    switch (stream_state) {
    case CGenNodeStateless::ss_FREE_RESUSE :
         res="FREE    ";
        break;
    case CGenNodeStateless::ss_INACTIVE :
        res="INACTIVE ";
        break;
    case CGenNodeStateless::ss_ACTIVE :
        res="ACTIVE   ";
        break;
    default:
        res="Unknow   ";
    };
    return(res);
}


void CGenNodeStateless::free_stl_node(){
    /* if we have cache mbuf free it */
    rte_mbuf_t * m=get_cache_mbuf();
    if (m) {
        rte_pktmbuf_free(m);
        m_cache_mbuf=0;
    }
}


bool TrexStatelessDpPerPort::update_number_of_active_streams(uint32_t d){
    m_active_streams-=d; /* reduce the number of streams */
    if (m_active_streams == 0) {
        return (true);
    }
    return (false);
}


void TrexStatelessDpPerPort::stop_traffic(uint8_t port_id){

    assert(m_state==TrexStatelessDpPerPort::ppSTATE_TRANSMITTING);

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node; 
        assert(node->get_port_id() == port_id);
        if ( node->get_state() == CGenNodeStateless::ss_ACTIVE) {
            node->mark_for_free();
            m_active_streams--;
            dp_stream.DeleteOnlyStream();

        }else{
            dp_stream.Delete(m_core);
        }
    }

    /* active stream should be zero */
    assert(m_active_streams==0);
    m_active_nodes.clear();
    m_state=TrexStatelessDpPerPort::ppSTATE_IDLE;
}


void TrexStatelessDpPerPort::create(CFlowGenListPerThread   *  core){
    m_core=core;
    m_state=TrexStatelessDpPerPort::ppSTATE_IDLE;
    m_port_id=0;
    m_active_streams=0;
    m_active_nodes.clear();
}



void
TrexStatelessDpCore::create(uint8_t thread_id, CFlowGenListPerThread *core) {
    m_thread_id = thread_id;
    m_core = core;
    m_local_port_offset = 2*core->getDualPortId();

    CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(thread_id);
    m_ring_to_cp   = cp_dp->getRingDpToCp(thread_id);

    m_state = STATE_IDLE;

    int i;
    for (i=0; i<NUM_PORTS_PER_CORE; i++) {
        m_ports[i].create(core);
    }
}


/* move to the next stream, old stream move to INACTIVE */
bool TrexStatelessDpCore::set_stateless_next_node(CGenNodeStateless * cur_node,
                                                  CGenNodeStateless * next_node){

    assert(cur_node);
    TrexStatelessDpPerPort * lp_port = get_port_db(cur_node->m_port_id);
    bool schedule =false;

    bool to_stop_port=false;

    if (next_node == NULL) {
        /* there is no next stream , reduce the number of active streams*/
        to_stop_port = lp_port->update_number_of_active_streams(1);

    }else{
        uint8_t state=next_node->get_state();

        /* can't be FREE_RESUSE */
        assert(state != CGenNodeStateless::ss_FREE_RESUSE);
        if (next_node->get_state() == CGenNodeStateless::ss_INACTIVE ) {

            /* refill start info and scedule, no update in active streams  */
            next_node->refresh();
            schedule = true;

        }else{
            to_stop_port = lp_port->update_number_of_active_streams(1);
        }
    }

    if ( to_stop_port ) {
        /* call stop port explictly to move the state */
        stop_traffic(cur_node->m_port_id);
    }

    return ( schedule );
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



void TrexStatelessDpCore::quit_main_loop(){
    m_core->set_terminate_mode(true); /* mark it as terminated */
    m_state = STATE_TERMINATE;
    add_global_duration(0.0001);
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
    /* TBD do we need that ? */
    m_core->m_node_gen.close_file(m_core);
}


void 
TrexStatelessDpCore::run_once(){

    idle_state_loop();

    if ( m_state == STATE_TERMINATE ){
        return;
    }

    start_scheduler();
}


void
TrexStatelessDpCore::start() {

    while (true) {
        run_once();

        if ( m_core->is_terminated_by_master() ) {
            break;
        }
    }
}

/* only if both port are idle we can exit */
void 
TrexStatelessDpCore::schedule_exit(){

    CGenNodeCommand *node = (CGenNodeCommand *)m_core->create_node() ;

    node->m_type = CGenNode::COMMAND;

    node->m_cmd = new TrexStatelessDpCanQuit();

    /* make sure it will be scheduled after the current node */
    node->m_time = m_core->m_cur_time_sec ;

    m_core->m_node_gen.add_node((CGenNode *)node);
}


void 
TrexStatelessDpCore::add_global_duration(double duration){
    if (duration > 0.0) {
        CGenNode *node = m_core->create_node() ;

        node->m_type = CGenNode::EXIT_SCHED;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        m_core->m_node_gen.add_node(node);
    }
}

/* add per port exit */
void
TrexStatelessDpCore::add_port_duration(double duration,
                                  uint8_t port_id){
    if (duration > 0.0) {
        CGenNodeCommand *node = (CGenNodeCommand *)m_core->create_node() ;

        node->m_type = CGenNode::COMMAND;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        node->m_cmd = new TrexStatelessDpStop(port_id);

        m_core->m_node_gen.add_node((CGenNode *)node);
    }
}


void
TrexStatelessDpCore::add_cont_stream(TrexStatelessDpPerPort * lp_port,
                                     TrexStream * stream,
                                     TrexStreamsCompiledObj *comp) {

    CGenNodeStateless *node = m_core->create_node_sl();

    /* add periodic */
    node->m_type = CGenNode::STATELESS_PKT;

    node->m_ref_stream_info  =   stream->clone_as_dp();

    node->m_next_stream=0; /* will be fixed later */


    if ( stream->m_self_start ){
        /* if self start it is in active mode */
        node->m_state =CGenNodeStateless::ss_ACTIVE;
        lp_port->m_active_streams++;
    }else{
        node->m_state =CGenNodeStateless::ss_INACTIVE;
    }

    node->m_time = m_core->m_cur_time_sec + usec_to_sec(stream->m_isg_usec);

    pkt_dir_t dir = m_core->m_node_gen.m_v_if->port_id_to_dir(stream->m_port_id);
    node->m_flags = 0; 

    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */
    
    uint16_t pkt_size = stream->m_pkt.len;
    const uint8_t *stream_pkt = stream->m_pkt.binary;

    node->m_stream_type = stream->m_type;
    node->m_next_time_offset =  1.0 / (stream->get_pps() * comp->get_multiplier()) ;


    /* stateless specific fields */
    switch ( stream->m_type ) {

    case TrexStream::stCONTINUOUS :
        node->m_single_burst=0;
        node->m_single_burst_refill=0;
        node->m_multi_bursts=0;
        node->m_ibg_sec                 = 0.0;
        break;

    case TrexStream::stSINGLE_BURST :
        node->m_stream_type             = TrexStream::stMULTI_BURST;
        node->m_single_burst            = stream->m_burst_total_pkts;
        node->m_single_burst_refill     = stream->m_burst_total_pkts;
        node->m_multi_bursts            = 1;  /* single burst in multi burst of 1 */
        node->m_ibg_sec                 = 0.0;
        break;

    case TrexStream::stMULTI_BURST :
        node->m_single_burst        = stream->m_burst_total_pkts;
        node->m_single_burst_refill = stream->m_burst_total_pkts;
        node->m_multi_bursts        = stream->m_num_bursts;
        node->m_ibg_sec             = usec_to_sec( stream->m_ibg_usec );
        break;
    default:

        assert(0);
    };

    node->m_port_id = stream->m_port_id;

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

    CDpOneStream one_stream;

    one_stream.m_dp_stream = node->m_ref_stream_info;
    one_stream.m_node =node;

    lp_port->m_active_nodes.push_back(one_stream);

    /* schedule only if active */
    if (node->m_state == CGenNodeStateless::ss_ACTIVE) {
        m_core->m_node_gen.add_node((CGenNode *)node);
    }
}

void
TrexStatelessDpCore::start_traffic(TrexStreamsCompiledObj *obj, 
                                   double duration) {

#if 0
    /* TBD to remove ! */
    obj->Dump(stdout);
#endif

    TrexStatelessDpPerPort * lp_port=get_port_db(obj->get_port_id());
    lp_port->m_active_streams = 0;
    /* no nodes in the list */
    assert(lp_port->m_active_nodes.size()==0);

    for (auto single_stream : obj->get_objects()) {
        /* all commands should be for the same port */
        assert(obj->get_port_id() == single_stream.m_stream->m_port_id);
        add_cont_stream(lp_port,single_stream.m_stream,obj);
    }

    uint32_t nodes = lp_port->m_active_nodes.size();
    /* find next stream */
    assert(nodes == obj->get_objects().size());

    int cnt=0;

    /* set the next_stream pointer  */
    for (auto single_stream : obj->get_objects()) {

        if (single_stream.m_stream->is_dp_next_stream() ) {
            int stream_id = single_stream.m_stream->m_next_stream_id;
            assert(stream_id<nodes);
            /* point to the next stream , stream_id is fixed */
            lp_port->m_active_nodes[cnt].m_node->m_next_stream = lp_port->m_active_nodes[stream_id].m_node ;
        }
        cnt++;
    }

    lp_port->m_state =TrexStatelessDpPerPort::ppSTATE_TRANSMITTING;
    m_state = TrexStatelessDpCore::STATE_TRANSMITTING;


    if ( duration > 0.0 ){
        add_port_duration( duration ,obj->get_port_id() );
    }
}


bool TrexStatelessDpCore::are_all_ports_idle(){

    bool res=true;
    int i;
    for (i=0; i<NUM_PORTS_PER_CORE; i++) {
        if ( m_ports[i].m_state != TrexStatelessDpPerPort::ppSTATE_IDLE ){
            res=false;
        }
    }
    return (res);
}


void
TrexStatelessDpCore::stop_traffic(uint8_t port_id) {
    /* we cannot remove nodes not from the top of the queue so
       for every active node - make sure next time
       the scheduler invokes it, it will be free */

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    lp_port->stop_traffic(port_id);

    if ( are_all_ports_idle() ) {

        schedule_exit();
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

