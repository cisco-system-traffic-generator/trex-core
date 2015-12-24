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


/**
 * this function called when stream restart after it was inactive
 */
void CGenNodeStateless::refresh(){

    /* refill the stream info */
    m_single_burst    = m_single_burst_refill;
    m_multi_bursts    = m_ref_stream_info->m_num_bursts;
    m_state           = CGenNodeStateless::ss_ACTIVE;
}


void CGenNodeCommand::free_command(){
    
    assert(m_cmd);
    m_cmd->on_node_remove();
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


rte_mbuf_t   * CGenNodeStateless::alloc_node_with_vm(){

    rte_mbuf_t        * m;
    /* alloc small packet buffer*/
    uint16_t prefix_size = prefix_header_size();
    m = CGlobalInfo::pktmbuf_alloc( get_socket_id(), prefix_size );
    if (m==0) {
        return (m);
    }
    /* TBD remove this, should handle cases of error */
    assert(m);
    char *p=rte_pktmbuf_append(m, prefix_size);
    memcpy( p ,m_original_packet_data_prefix, prefix_size); 


    /* run the VM program */
    StreamDPVmInstructionsRunner runner;

    runner.run( (uint32_t*)m_vm_flow_var,
                m_vm_program_size, 
                m_vm_program,
                m_vm_flow_var,
                (uint8_t*)p);


    rte_mbuf_t * m_const = get_const_mbuf();
    if (  m_const != NULL) {
        utl_rte_pktmbuf_add_after(m,m_const);
    }
    return (m);
}


void CGenNodeStateless::free_stl_node(){
    /* if we have cache mbuf free it */
    rte_mbuf_t * m=get_cache_mbuf();
    if (m) {
        rte_pktmbuf_free(m);
        m_cache_mbuf=0;
    }else{
        /* non cache - must have an header */
         m=get_const_mbuf();
         if (m) {
             rte_pktmbuf_free(m); /* reduce the ref counter */
         }
         free_prefix_header();
    }
    if (m_vm_flow_var) {
        /* free flow var */
        free(m_vm_flow_var);
        m_vm_flow_var=0;
    }
}


bool TrexStatelessDpPerPort::update_number_of_active_streams(uint32_t d){
    m_active_streams-=d; /* reduce the number of streams */
    if (m_active_streams == 0) {
        return (true);
    }
    return (false);
}

bool TrexStatelessDpPerPort::resume_traffic(uint8_t port_id){

    /* we are working with continues streams so we must be in transmit mode */
    assert(m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE);

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node; 
        assert(node->get_port_id() == port_id);
        assert(node->is_pause() == true);
        node->set_pause(false);
    }
    m_state = TrexStatelessDpPerPort::ppSTATE_TRANSMITTING;
    return (true);
}

bool TrexStatelessDpPerPort::update_traffic(uint8_t port_id, double factor) {

    assert( (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING || 
            (m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE)) );

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node = dp_stream.m_node; 
        assert(node->get_port_id() == port_id);

        node->update_rate(factor);
    }

    return (true);
}

bool TrexStatelessDpPerPort::pause_traffic(uint8_t port_id){

    /* we are working with continues streams so we must be in transmit mode */
    assert(m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING);

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node; 
        assert(node->get_port_id() == port_id);
        assert(node->is_pause() == false);
        node->set_pause(true);
    }
    m_state = TrexStatelessDpPerPort::ppSTATE_PAUSE;
    return (true);
}


bool TrexStatelessDpPerPort::stop_traffic(uint8_t port_id,
                                          bool stop_on_id, 
                                          int event_id){


    if (m_state == TrexStatelessDpPerPort::ppSTATE_IDLE) {
        assert(m_active_streams==0);
        return false;
    }

    /* there could be race of stop after stop */
    if ( stop_on_id ) {
        if (event_id != m_event_id){
            /* we can't stop it is an old message */
            return false;
        }
    }

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
    return (true);
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
        if (state == CGenNodeStateless::ss_INACTIVE ) {

            /* refill start info and scedule, no update in active streams  */
            next_node->refresh();
            schedule = true;

        }else{
            to_stop_port = lp_port->update_number_of_active_streams(1);
        }
    }

    if ( to_stop_port ) {
        /* call stop port explictly to move the state */
        stop_traffic(cur_node->m_port_id,false,0);
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
    /* bail out in case of terminate */
    if (m_state != TrexStatelessDpCore::STATE_TERMINATE) {
        m_core->m_node_gen.close_file(m_core);
        m_state = STATE_IDLE; /* we exit from all ports and we have nothing to do, we move to IDLE state */
    }
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
                                       uint8_t port_id,
                                       int event_id){
    if (duration > 0.0) {
        CGenNodeCommand *node = (CGenNodeCommand *)m_core->create_node() ;

        node->m_type = CGenNode::COMMAND;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        TrexStatelessDpStop * cmd=new TrexStatelessDpStop(port_id);


        /* test this */
        m_core->m_non_active_nodes++;
        cmd->set_core_ptr(m_core);
        cmd->set_event_id(event_id);
        cmd->set_wait_for_event_id(true);

        node->m_cmd = cmd;

        m_core->m_node_gen.add_node((CGenNode *)node);
    }
}


void
TrexStatelessDpCore::add_stream(TrexStatelessDpPerPort * lp_port,
                                TrexStream * stream,
                                TrexStreamsCompiledObj *comp) {

    CGenNodeStateless *node = m_core->create_node_sl();

    /* add periodic */
    node->m_cache_mbuf=0;
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
    node->m_src_port =0;
    node->m_original_packet_data_prefix = 0;



    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */
    
    uint16_t pkt_size = stream->m_pkt.len;
    const uint8_t *stream_pkt = stream->m_pkt.binary;

    node->m_pause =0;
    node->m_stream_type = stream->m_type;
    node->m_next_time_offset = 1.0 / stream->get_pps();

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

    /* set dir 0 or 1 client or server */
    node->set_mbuf_cache_dir(dir);


    if (stream->is_vm()  == false ) {
        /* no VM */

        node->m_vm_flow_var =  NULL;
        node->m_vm_program  =  NULL;
        node->m_vm_program_size =0;

                /* allocate const mbuf */
        rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), pkt_size);
        assert(m);
    
        char *p = rte_pktmbuf_append(m, pkt_size);
        assert(p);
        /* copy the packet */
        memcpy(p,stream_pkt,pkt_size);
    
        /* TBD repace the mac if req we should add flag  */
        m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir,(uint8_t*) p);
    
        /* set the packet as a readonly */
        node->set_cache_mbuf(m);

        node->m_original_packet_data_prefix =0;
    }else{

        /* set the program */
        TrexStream * local_mem_stream = node->m_ref_stream_info;

        StreamVmDp  * lpDpVm = local_mem_stream->getDpVm();

        node->m_vm_flow_var =  lpDpVm->clone_bss(); /* clone the flow var */
        node->m_vm_program  =  lpDpVm->get_program(); /* same ref to the program */
        node->m_vm_program_size =lpDpVm->get_program_size();


        /* we need to copy the object */
        if ( pkt_size > stream->m_vm_prefix_size  ) {
            /* we need const packet */
            uint16_t const_pkt_size  = pkt_size - stream->m_vm_prefix_size ;
            rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), const_pkt_size );
            assert(m);

            char *p = rte_pktmbuf_append(m, const_pkt_size);
            assert(p);

            /* copy packet data */
            memcpy(p,(stream_pkt+ stream->m_vm_prefix_size),const_pkt_size);

            node->set_const_mbuf(m);
        }


        if (stream->m_vm_prefix_size > pkt_size ) {
            stream->m_vm_prefix_size = pkt_size;
        }
        /* copy the headr */
        uint16_t header_size = stream->m_vm_prefix_size;
        assert(header_size);
        node->alloc_prefix_header(header_size);
        uint8_t *p=node->m_original_packet_data_prefix;
        assert(p);

        memcpy(p,stream_pkt , header_size);
        /* TBD repace the mac if req we should add flag  */
        m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir, p);
    }


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
                                   double duration,
                                   int event_id) {


    TrexStatelessDpPerPort * lp_port=get_port_db(obj->get_port_id());
    lp_port->m_active_streams = 0;
    lp_port->set_event_id(event_id);

    /* no nodes in the list */
    assert(lp_port->m_active_nodes.size()==0);

    for (auto single_stream : obj->get_objects()) {
        /* all commands should be for the same port */
        assert(obj->get_port_id() == single_stream.m_stream->m_port_id);
        add_stream(lp_port,single_stream.m_stream,obj);
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
        add_port_duration( duration ,obj->get_port_id(),event_id );
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
TrexStatelessDpCore::resume_traffic(uint8_t port_id){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    lp_port->resume_traffic(port_id);
}


void 
TrexStatelessDpCore::pause_traffic(uint8_t port_id){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    lp_port->pause_traffic(port_id);
}

void 
TrexStatelessDpCore::update_traffic(uint8_t port_id, double factor) {

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    lp_port->update_traffic(port_id, factor);
}


void
TrexStatelessDpCore::stop_traffic(uint8_t port_id,
                                  bool stop_on_id, 
                                  int event_id) {
    /* we cannot remove nodes not from the top of the queue so
       for every active node - make sure next time
       the scheduler invokes it, it will be free */

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    if ( lp_port->stop_traffic(port_id,stop_on_id,event_id) == false){
        /* nothing to do ! already stopped */
        //printf(" skip .. %f\n",m_core->m_cur_time_sec);
        return;
    }

#if 0
    if ( are_all_ports_idle() ) {
        /* just a place holder if we will need to do somthing in that case */
    }
#endif
 
    /* inform the control plane we stopped - this might be a async stop
       (streams ended)
     */
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(m_core->m_thread_id);
    TrexStatelessDpToCpMsgBase *event_msg = new TrexDpPortEventMsg(m_core->m_thread_id,
                                                                   port_id,
                                                                   TrexDpPortEvent::EVENT_STOP,
                                                                   lp_port->get_event_id());
    ring->Enqueue((CGenNode *)event_msg);

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

