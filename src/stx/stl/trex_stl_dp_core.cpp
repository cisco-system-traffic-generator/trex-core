/*
 Itay Marom
 Hanoch Haim
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
#include "bp_sim.h"

#include "trex_stl_dp_core.h"
#include "trex_stl_stream_node.h"
#include "trex_stl_stream.h"
#include "trex_stl_streams_compiler.h"
#include "trex_stl_messaging.h"
#include "flow_stat_parser.h"
#include "mbuf.h"
#include "utl_mbuf.h"



/**
 * a wrapper for service mode 
 * it will move the fast send_node virtual call 
 * to send_node_service_mode which does capturing 
 * 
 */
class ServiceModeWrapper : public CVirtualIF {
public:

    ServiceModeWrapper() {
        m_wrapped = nullptr;
    }
    
    void set_wrapped_object(CVirtualIF *wrapped) {
        m_wrapped = wrapped;
    }
    
    CVirtualIF *get_wrapped_object() const {
        return m_wrapped;
    }
    
    virtual int close_file(void) {
        return m_wrapped->close_file();
    }
    
    virtual int flush_tx_queue(void) {
        return m_wrapped->flush_tx_queue();
    }
    
    virtual int open_file(std::string file_name) {
        return m_wrapped->open_file(file_name);
    }
    
    /* move to service mode */
    virtual int send_node(CGenNode *node) {
        return m_wrapped->send_node_service_mode(node);
    }
    
    virtual int update_mac_addr_from_global_cfg(pkt_dir_t dir, uint8_t *p) {
        return m_wrapped->update_mac_addr_from_global_cfg(dir, p);
    }
    
    virtual pkt_dir_t port_id_to_dir(uint8_t port_id) {
        return m_wrapped->port_id_to_dir(port_id);
    }
    
    virtual void send_one_pkt(pkt_dir_t dir, rte_mbuf_t *m) {
        m_wrapped->send_one_pkt(dir, m);
    }
    
    virtual void set_review_mode(CPreviewMode *preview_mode) {
        m_wrapped->set_review_mode(preview_mode);
    }

    virtual CVirtualIFPerSideStats * get_stats() {
        return m_wrapped->get_stats();
    }

    virtual uint16_t rx_burst(pkt_dir_t dir,
                              struct rte_mbuf **rx_pkts,
                              uint16_t nb_pkts){
        return m_wrapped->rx_burst(dir,rx_pkts,nb_pkts);
    }


    bool redirect_to_rx_core(pkt_dir_t   dir,
                             rte_mbuf_t * m){
        return m_wrapped->redirect_to_rx_core(dir,m);
    }


private:
    CVirtualIF *m_wrapped;
};


void CGenNodeStateless::cache_mbuf_array_init(){
    m_cache_size=0;
    m_cache_array_cnt=0;
}



void CGenNodeStateless::cache_mbuf_array_copy(CGenNodeCacheMbuf *obj,
                                              uint16_t size){

    int i;
    cache_mbuf_array_alloc(size);
    for (i=0; i<size; i++) {
        cache_mbuf_array_set(i,obj->m_array[i]);
    }
    cache_mbuf_array_set_const_mbuf(obj->m_mbuf_const);
}

           
rte_mbuf_t ** CGenNodeStateless::cache_mbuf_array_alloc(uint16_t size){

    uint32_t buf_size = CGenNodeCacheMbuf::get_object_size(size);
    /* TBD  replace with align, zero API */
    m_cache_mbuf = (void *)malloc(buf_size);
    assert(m_cache_mbuf);
    memset(m_cache_mbuf,0,buf_size);

    m_flags |= SL_NODE_CONST_MBUF_CACHE_ARRAY;
    m_cache_size=size;
    m_cache_array_cnt=0;
    return ((rte_mbuf_t **)m_cache_mbuf);
}

void CGenNodeStateless::cache_mbuf_array_free(){

    assert(m_cache_mbuf);
    int i;
    for (i=0; i<(int)m_cache_size; i++) {
        rte_mbuf_t * m=cache_mbuf_array_get((uint16_t)i);
        assert(m);
        rte_pktmbuf_free(m); 
    }

    /* free the const */
    rte_mbuf_t * m=cache_mbuf_array_get_const_mbuf() ;
    if (m) {
        rte_pktmbuf_free(m); 
    }

    free(m_cache_mbuf);
    m_cache_mbuf=0;
}


rte_mbuf_t * CGenNodeStateless::cache_mbuf_array_get(uint16_t index){

    CGenNodeCacheMbuf *p =(CGenNodeCacheMbuf *) m_cache_mbuf;
    return (p->m_array[index]);
}

void CGenNodeStateless::cache_mbuf_array_set_const_mbuf(rte_mbuf_t * m){
    CGenNodeCacheMbuf *p =(CGenNodeCacheMbuf *) m_cache_mbuf;
    p->m_mbuf_const=m;
}

rte_mbuf_t * CGenNodeStateless::cache_mbuf_array_get_const_mbuf(){
    CGenNodeCacheMbuf *p =(CGenNodeCacheMbuf *) m_cache_mbuf;
    return (p->m_mbuf_const);
}


void CGenNodeStateless::cache_mbuf_array_set(uint16_t index,
                                             rte_mbuf_t * m){
    CGenNodeCacheMbuf *p =(CGenNodeCacheMbuf *) m_cache_mbuf;
    p->m_array[index]=m;
}


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


void CGenNodeStateless::generate_random_seed() {
    /* seed can be provided by the user */
    uint32_t unique_seed;
    if (m_ref_stream_info->m_random_seed) {
        unique_seed = m_ref_stream_info->m_random_seed;
    } else {
        unsigned int tmp = (unsigned int)time(NULL);
        unique_seed = rand_r(&tmp);
    }

    /* per thread divergence */
    unique_seed = (unique_seed * ( (m_thread_id + 1) * 514229 ) ) & 0xFFFFFFFF;

    /* set random */
    set_random_seed(unique_seed);
}


void CGenNodeStateless::refresh_vm_bss() {
    if ( m_vm_flow_var ) {
        StreamVmDp  * vm_s=m_ref_stream_info->m_vm_dp;
        assert(vm_s);
        memcpy(m_vm_flow_var,vm_s->get_bss(),vm_s->get_bss_size());

        if ( vm_s->is_random_seed() ) {
            generate_random_seed();
        }
        
    }
}



/**
 * this function called when stream restart after it was inactive
 */
void CGenNodeStateless::refresh(){

    /* refill the stream info */
    m_single_burst    = m_single_burst_refill;
    m_multi_bursts    = m_ref_stream_info->m_num_bursts;
    m_state           = CGenNodeStateless::ss_ACTIVE;

    /* refresh init value */
#if 0
    /* TBD should add a JSON varible for that */
    refresh_vm_bss();
#endif
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

/*
 * Allocate mbuf for flow stat (and latency) info sending
 * m - Original mbuf (can be complicated mbuf data structure)
 * fsp_head - return pointer in which the flow stat info should be filled
 * is_const - is the given mbuf const
 * return new mbuf structure in which the fsp_head can be written. If needed, orginal mbuf is freed.
 */
rte_mbuf_t * CGenNodeStateless::alloc_flow_stat_mbuf(rte_mbuf_t *m, struct flow_stat_payload_header *&fsp_head
                                                     , bool is_const) {
    rte_mbuf_t *m_ret = NULL, *m_lat = NULL;
    uint16_t fsp_head_size = sizeof(struct flow_stat_payload_header);

    if (is_const) {
        // const mbuf case
        if (rte_pktmbuf_data_len(m) > 128) {
            m_ret = CGlobalInfo::pktmbuf_alloc_small_local(get_socket_id());
            assert(m_ret);
            // alloc mbuf just for the latency header
            m_lat = CGlobalInfo::pktmbuf_alloc_local( get_socket_id(), fsp_head_size);
            assert(m_lat);
            fsp_head = (struct flow_stat_payload_header *)rte_pktmbuf_append(m_lat, fsp_head_size);
            rte_pktmbuf_attach(m_ret, m);
            rte_pktmbuf_trim(m_ret, sizeof(struct flow_stat_payload_header));
            utl_rte_pktmbuf_add_after2(m_ret, m_lat);
            // ref count was updated when we took the (const) mbuf, and again in rte_pktmbuf_attach
            // so need do decrease now, to avoid leak.
            rte_pktmbuf_refcnt_update(m, -1);
            return m_ret;
        } else {
            // Short packet. Just copy all bytes.
            m_ret = CGlobalInfo::pktmbuf_alloc_local( get_socket_id(), rte_pktmbuf_data_len(m) );
            assert(m_ret);
            char *p = rte_pktmbuf_mtod(m, char*);
            char *p_new = rte_pktmbuf_append(m_ret, rte_pktmbuf_data_len(m));
            memcpy(p_new , p, rte_pktmbuf_data_len(m));
            fsp_head = (struct flow_stat_payload_header *)(p_new + rte_pktmbuf_data_len(m) - fsp_head_size);
            rte_pktmbuf_free(m);
            return m_ret;
        }
    } else {
        // Field engine (vm)
        if (rte_pktmbuf_is_contiguous(m)) {
            // one, r/w mbuf
            char *p = rte_pktmbuf_mtod(m, char*);
            fsp_head = (struct flow_stat_payload_header *)(p + rte_pktmbuf_data_len(m) - fsp_head_size);
            return m;
        } else {
            // Two options here.
            //   Normal case. We have: r/w --> read only.
            //     Changing to:
            //     (original) r/w -> (new) indirect (direct is original read_only, after trimming last bytes) -> (new) latency info
            //  In case of field engine with random packet size, we already have r/w->indirect(direct read only with packet data).
            //     Need to trim bytes from indirect, and make it point to new mbuf with latency data.
            m_lat = CGlobalInfo::pktmbuf_alloc_local( get_socket_id(), fsp_head_size);
            assert(m_lat);
            fsp_head = (struct flow_stat_payload_header *)rte_pktmbuf_append(m_lat, fsp_head_size);

            if (RTE_MBUF_INDIRECT(m->next)) {
                // Variable length field engine case
                rte_mbuf_t *m_indirect = m->next;
                utl_rte_pktmbuf_chain_to_indirect(m, m_indirect, m_lat);
                return m;
            } else {
                // normal case.
                rte_mbuf_t *m_read_only = m->next, *m_indirect;

                m_indirect = CGlobalInfo::pktmbuf_alloc_small_local(get_socket_id());
                assert(m_indirect);
                // alloc mbuf just for the latency header
                utl_rte_pktmbuf_chain_with_indirect(m, m_indirect, m_read_only, m_lat);
                m_indirect->data_len = (uint16_t)(m_indirect->data_len - fsp_head_size);
                return m;
            }
        }
    }
}

// test the const case of alloc_flow_stat_mbuf. The more complicated non const case is tested in the simulation.
bool CGenNodeStateless::alloc_flow_stat_mbuf_test_const() {
    rte_mbuf_t *m, *m_test;
    uint16_t sizes[2] = {64, 500};
    uint16_t size;
    struct flow_stat_payload_header *fsp_head;
    char *p;

    set_socket_id(0);
    for (int test_num = 0; test_num < sizeof(sizes)/sizeof(sizes[0]); test_num++) {
        size = sizes[test_num];
        m = CGlobalInfo::pktmbuf_alloc_local(get_socket_id(), size);
        p = rte_pktmbuf_append(m, size);
        for (int i = 0; i < size; i++) {
            p[i] = (char)i;
        }
        m_test = alloc_flow_stat_mbuf(m, fsp_head, true);
        p = rte_pktmbuf_mtod(m_test, char*);
        assert(rte_pktmbuf_pkt_len(m_test) == size);
        for (int i = 0; i < rte_pktmbuf_pkt_len(m_test) - sizeof(*fsp_head); i++) {
            assert(p[i] == (char)i);
        }
        // verify fsp_head points correctly
        if (size > 128) { // should match threshould in alloc_flow_stat_mbuf
            assert(rte_pktmbuf_data_len(m_test) == size - sizeof(*fsp_head));
            assert(rte_pktmbuf_data_len(m_test->next) == sizeof(*fsp_head));
            assert((char *)fsp_head == rte_pktmbuf_mtod((m_test->next), char*));
        } else {
            assert(rte_pktmbuf_data_len(m_test) == size);
            assert (((char *)fsp_head) + sizeof (*fsp_head) == p + rte_pktmbuf_data_len(m_test));
        }
        rte_pktmbuf_free(m_test);
    }
    return true;
}

rte_mbuf_t   * CGenNodeStateless::alloc_node_with_vm(){

    rte_mbuf_t        * m;
    /* alloc small packet buffer*/
    uint16_t prefix_size = prefix_header_size();
    m = CGlobalInfo::pktmbuf_alloc_local( get_socket_id(), prefix_size );
    if (m==0) {
        return (m);
    }
    /* TBD remove this, should handle cases of error */
    assert(m);
    char *p=rte_pktmbuf_append(m, prefix_size);
    memcpy( p ,m_original_packet_data_prefix, prefix_size);


    /* run the VM program */
    StreamDPVmInstructionsRunner runner;
    runner.set_mbuf(m);

    runner.run( (uint32_t*)m_vm_flow_var,
                m_vm_program_size,
                m_vm_program,
                m_vm_flow_var,
                (uint8_t*)p);

    uint16_t pkt_new_size=runner.get_new_pkt_size();
    if ( likely( pkt_new_size == 0) ) {
        /* no packet size change */
        rte_mbuf_t * m_const = get_const_mbuf();
        if (  m_const != NULL) {
            utl_rte_pktmbuf_add_after(m,m_const);
        }
        return (m);
    }

    /* packet size change there are a few changes */
    rte_mbuf_t * m_const = get_const_mbuf();
    if ( (m_const == 0 ) || (pkt_new_size<=prefix_size) ) {
        /* one mbuf , just trim it */
        m->data_len = pkt_new_size;
        m->pkt_len  = pkt_new_size;
        return (m);
    }

    rte_mbuf_t * mi= CGlobalInfo::pktmbuf_alloc_small_local(get_socket_id());
    assert(mi);
    rte_pktmbuf_attach(mi,m_const);
    utl_rte_pktmbuf_add_after2(m,mi);

    if ( pkt_new_size < m->pkt_len) {
        /* need to trim it */
        mi->data_len = (pkt_new_size - prefix_size);
        m->pkt_len   = pkt_new_size;
    }
    return (m);
}

void CGenNodeStateless::free_stl_vm_buf(){
        rte_mbuf_t * m ;
         m=get_const_mbuf();
         if (m) {
             rte_pktmbuf_free(m); /* reduce the ref counter */
             /* clear the const marker */
             clear_const_mbuf();
         }

         free_prefix_header();

         if (m_vm_flow_var) {
             /* free flow var */
             free(m_vm_flow_var);
             m_vm_flow_var=0;
         }
}



void CGenNodeStateless::free_stl_node(){

    if ( is_mask_for_free() ) {
        return;
    }

    if ( is_cache_mbuf_array() ){
        /* do we have cache of mbuf pre allocated */
        cache_mbuf_array_free();
    }else{
        /* if we have cache mbuf free it */
        rte_mbuf_t * m=get_cache_mbuf();
        if (m) {
                rte_pktmbuf_free(m);
                m_cache_mbuf=0;
        }
    }
    free_stl_vm_buf();
}

TrexStatelessDpPerPort::TrexStatelessDpPerPort() {
    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
            m_rfc2544[i].create();
        }
    m_fs_latency.create(m_rfc2544, &m_err_cntrs);
}

bool TrexStatelessDpPerPort::update_number_of_active_streams(uint32_t d){
    m_active_streams-=d; /* reduce the number of streams */
    if (m_active_streams == 0) {
        return (true);
    }
    return (false);
}

bool TrexStatelessDpPerPort::resume_traffic(uint8_t port_id, uint32_t profile_id){

    /* we are working with continues streams so we must be in transmit mode */
    assert((m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE) ||
           (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING));

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node;
        assert(node->get_port_id() == port_id);
        //assert(node->is_pause() == true); <- we can't be sure of this with ability to pause specific streams.
        if (((profile_id == 0) || (node->get_profile_id() == profile_id)) && node->is_pause()) {
            node->set_pause(false);
            m_paused_streams--;
        }
    }
    if (m_paused_streams < m_active_streams)
        m_state = TrexStatelessDpPerPort::ppSTATE_TRANSMITTING;
    return (true);
}

bool TrexStatelessDpPerPort::resume_streams(uint8_t port_id, stream_ids_t &stream_ids){

    assert( (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING) ||
            (m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE) );

    uint32_t done_count = 0;
    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node = dp_stream.m_node;
        assert(node->get_port_id() == port_id);
        if ( stream_ids.find(node->get_user_stream_id()) != stream_ids.end() ) {
            if (node->is_pause()) {
                node->set_pause(false);
                m_paused_streams--;
            }
            done_count++;
            if ( done_count == stream_ids.size() ) {
                break;
            }
        }
    }
    if (m_paused_streams < m_active_streams)
        m_state = TrexStatelessDpPerPort::ppSTATE_TRANSMITTING;
    // TODO: return feedback if done_count != stream_ids.size()
    return (true);
}

bool TrexStatelessDpPerPort::update_traffic(uint8_t port_id, uint32_t profile_id, double factor) {

    assert( (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING ||
            (m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE)) );

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node = dp_stream.m_node;
        assert(node->get_port_id() == port_id);

        if (! node->is_latency_stream()) {
            if ((profile_id == 0) || (node->get_profile_id() == profile_id))
                node->update_rate(factor);
        }
    }

    return (true);
}

bool TrexStatelessDpPerPort::update_streams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream) {

    assert( (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING ||
            (m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE)) );

    uint32_t done_count = 0;
    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node = dp_stream.m_node;
        assert(node->get_port_id() == port_id);

        if ( !node->is_latency_stream() ) {
            stream_ipgs_map_it_t it = ipg_per_stream.find(node->get_user_stream_id());

            if ( it != ipg_per_stream.end() ) {
                node->set_ipg(it->second);
                done_count++;
                if ( done_count == ipg_per_stream.size() ) {
                    break;
                }
            }
        }
    }

    // TODO: return feedback if done_count != ipg_per_stream.size()
    return (true);
}

bool TrexStatelessDpPerPort::pause_traffic(uint8_t port_id, uint32_t profile_id){

    /* we are working with continues streams so we must be in transmit mode */
    assert(m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING);

    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node;
        assert(node->get_port_id() == port_id);
        //assert(node->is_pause() == false); <- we can't be sure of this with ability to pause specific streams.
        if (((profile_id == 0) || (node->get_profile_id() == profile_id)) && !node->is_pause()) {
            node->set_pause(true);
            m_paused_streams++;
        }
    }
    if (m_paused_streams == m_active_streams)
        m_state = TrexStatelessDpPerPort::ppSTATE_PAUSE;
    return (true);
}

bool TrexStatelessDpPerPort::pause_streams(uint8_t port_id, stream_ids_t &stream_ids){

    assert( (m_state == TrexStatelessDpPerPort::ppSTATE_TRANSMITTING) ||
            (m_state == TrexStatelessDpPerPort::ppSTATE_PAUSE) );

    uint32_t done_count = 0;
    for (auto dp_stream : m_active_nodes) {
        CGenNodeStateless * node = dp_stream.m_node;
        assert(node->get_port_id() == port_id);
        if ( stream_ids.find(node->get_user_stream_id()) != stream_ids.end() ) {
            if (!node->is_pause()) {
                node->set_pause(true);
                m_paused_streams++;
            }
            done_count++;
            if ( done_count == stream_ids.size() ) {
                break;
            }
        }
    }
    if (m_paused_streams == m_active_streams)
        m_state = TrexStatelessDpPerPort::ppSTATE_PAUSE;
    return (true);
}

bool TrexStatelessDpPerPort::push_pcap(uint8_t port_id,
                                       const std::string &pcap_filename,
                                       double ipg_usec,
                                       double min_ipg_sec,
                                       double speedup,
                                       uint32_t count,
                                       bool is_dual) {

    /* push pcap can only happen on an idle port from the core prespective */
    assert(m_state == TrexStatelessDpPerPort::ppSTATE_IDLE);

    CGenNodePCAP *pcap_node = (CGenNodePCAP *)m_core->create_node();
    if (!pcap_node) {
        return (false);
    }

    pkt_dir_t dir          = m_core->m_node_gen.m_v_if->port_id_to_dir(port_id);
    socket_id_t socket_id  = m_core->m_node_gen.m_socket_id;

    /* main port */
    uint8_t mac_addr[12];
    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir, mac_addr);

    /* for dual */
    uint8_t slave_mac_addr[12];
    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir ^ 0x1, slave_mac_addr);

    bool rc = pcap_node->create(port_id,
                                dir,
                                socket_id,
                                mac_addr,
                                slave_mac_addr,
                                pcap_filename,
                                ipg_usec,
                                min_ipg_sec,
                                speedup,
                                count,
                                is_dual);
    if (!rc) {
        m_core->free_node((CGenNode *)pcap_node);
        return (false);
    }

    /* schedule the node for now */
    pcap_node->m_time = m_core->m_cur_time_sec;
    m_core->m_node_gen.add_node((CGenNode *)pcap_node);

    /* hold a pointer to the node */
    assert(m_active_pcap_node == NULL);
    m_active_pcap_node = pcap_node;

    m_state = TrexStatelessDpPerPort::ppSTATE_PCAP_TX;
    return (true);
}


bool TrexStatelessDpPerPort::stop_traffic(uint8_t  port_id,
                                          uint32_t profile_id,
                                          bool     stop_on_id,
                                          int      event_id){


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

    for (auto& dp_stream : m_active_nodes) {
        CGenNodeStateless * node =dp_stream.m_node;
        assert(node->get_port_id() == port_id);
        if (profile_id && (node->get_profile_id() != profile_id))
            continue;

        if ( node->get_state() == CGenNodeStateless::ss_ACTIVE) {
            node->mark_for_free();
            m_active_streams--;
            dp_stream.DeleteOnlyStream();
        }else{
            dp_stream.Delete(m_core);
        }
    }
    /* remove all elements from m_active_nodes */
    auto it = std::remove_if(m_active_nodes.begin(), m_active_nodes.end(), [](CDpOneStream& x) {return x.m_dp_stream == NULL;});
    m_active_nodes.erase(it, m_active_nodes.end());

    /* check for active PCAP node */
    if (m_active_pcap_node) {
        /* when got async stop from outside or duration */
        if (m_active_pcap_node->is_active()) {
            m_active_pcap_node->mark_for_free();
        } else {
            /* graceful stop - node was put out by the scheduler */
            m_core->free_node( (CGenNode *)m_active_pcap_node);
        }

        m_active_pcap_node = NULL;
    }

    /* active stream should be zero */
    if (m_active_streams == 0) {
        m_active_nodes.clear();
        m_state=TrexStatelessDpPerPort::ppSTATE_IDLE;
    }
    return (true);
}


void TrexStatelessDpPerPort::create(CFlowGenListPerThread   *  core){
    m_core=core;
    m_state=TrexStatelessDpPerPort::ppSTATE_IDLE;
    m_active_streams=0;
    m_paused_streams=0;
    m_active_nodes.clear();
    m_active_pcap_node = NULL;
}


TrexStatelessDpCore::TrexStatelessDpCore(uint8_t thread_id, CFlowGenListPerThread *core) : TrexDpCore(thread_id, core, STATE_IDLE) {
  
    m_duration        = -1;
    m_is_service_mode = NULL;
    m_wrapper         = new ServiceModeWrapper();
    m_need_to_rx = false;
    
    m_local_port_offset = 2 * core->getDualPortId();

    int i;
    for (i=0; i<NUM_PORTS_PER_CORE; i++) {
        m_ports[i].create(core);
    }
    m_parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
}

TrexStatelessDpCore::~TrexStatelessDpCore() {
    delete m_wrapper;
    delete m_parser;
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

            if (cur_node->m_action_counter > 0) {
                cur_node->m_action_counter--;
                if (cur_node->m_action_counter==0) {
                    to_stop_port = lp_port->update_number_of_active_streams(1);
                }else{
                    /* refill start info and scedule, no update in active streams  */
                    next_node->refresh();
                    schedule = true;
                }
            }else{
                /* refill start info and scedule, no update in active streams  */
                next_node->refresh();
                schedule = true;
            }

        }else{
            to_stop_port = lp_port->update_number_of_active_streams(1);
        }
    }

    if ( to_stop_port ) {
        /* call stop port explictly to move the state */
        stop_traffic(cur_node->m_port_id,cur_node->m_profile_id,false,0);
    }

    return ( schedule );
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

    if ( get_dpdk_mode()->dp_rx_queues() ){
        // add rx node if needed 
        CGenNode * node_rx = m_core->create_node() ;
        node_rx->m_type = CGenNode::STL_RX_FLUSH;
        node_rx->m_time = now_sec(); /* NOW to warm thing up */
        m_core->m_node_gen.add_node(node_rx);
    }

    double old_offset = 0.0;
    m_core->m_node_gen.flush_file(-1, 0.0, false, m_core, old_offset);
    /* bail out in case of terminate */
    if (m_state != TrexStatelessDpCore::STATE_TERMINATE) {
        m_core->m_node_gen.close_file(m_core);
        m_state = STATE_IDLE; /* we exit from all ports and we have nothing to do, we move to IDLE state */
    }
}


void TrexStatelessDpCore::_rx_handle_packet(int dir,
                                           rte_mbuf_t * m,
                                           bool is_idle,
                                           bool &drop){
    /* parse the packet, if it has TOS=1, forward it */
    //utl_rte_pktmbuf_dump_k12(stdout,m);
    if (m_is_service_mode){
        drop=false;
        return;
    }
    drop=true;

    uint8_t *p = rte_pktmbuf_mtod(m, uint8_t*);
    uint16_t pkt_size= rte_pktmbuf_data_len(m);
    CFlowStatParser_err_t res=m_parser->parse(p,pkt_size);

    if (res != FSTAT_PARSER_E_OK){
        drop=false;
        return;
    }

    uint8_t proto = m_parser->get_protocol();
    bool tcp_udp=false;

    if ((proto == IPPROTO_TCP) || (proto == IPPROTO_UDP)){
        tcp_udp = true;
    }

    if (m_parser->is_fs_latency()) {
        m_ports[dir].m_fs_latency.handle_pkt(m);
    }

    if ( !tcp_udp && (is_idle==false)){
        drop=false;
    }

}

void TrexStatelessDpCore::set_need_to_rx(bool enable){
    m_need_to_rx = enable;
}


bool TrexStatelessDpCore::rx_for_idle(void){
    if (m_need_to_rx){
        return (m_core->handle_stl_pkts(true) > 0?true:false);
    }else{
        return(false);
    }
}

void TrexStatelessDpCore::rx_handle_packet(int dir,
                                           rte_mbuf_t * m,
                                           bool is_idle,
                                           tvpid_t port_id){
    bool drop;
    _rx_handle_packet(dir,m,is_idle,drop);

    if (drop) {
        //TrexCaptureMngr::getInstance().handle_pkt_rx_dp(m, port_id);
        rte_pktmbuf_free(m);
    }else{
        /* redirect to rx core */
        m_core->m_node_gen.m_v_if->redirect_to_rx_core(dir, m);
    }
}


/* add per port exit */
void
TrexStatelessDpCore::add_port_duration(double duration,
                                       uint8_t port_id,
                                       uint32_t profile_id,
                                       int event_id){
    if (duration > 0.0) {
        CGenNodeCommand *node = (CGenNodeCommand *)m_core->create_node() ;

        node->m_type = CGenNode::COMMAND;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        TrexStatelessDpStop * cmd=new TrexStatelessDpStop(port_id, profile_id);


        /* test this */
        m_core->m_non_active_nodes++;
        cmd->set_core_ptr(m_core);
        cmd->set_event_id(event_id);
        cmd->set_wait_for_event_id(true);

        node->m_cmd = cmd;

        m_core->m_node_gen.add_node((CGenNode *)node);
    }
}


void TrexStatelessDpCore::update_mac_addr(TrexStream * stream,
                                          CGenNodeStateless *node,
                                          pkt_dir_t dir,
                                          char *raw_pkt){
    bool              ov_src = stream->get_override_src_mac_by_pkt_data();
    TrexStream::stream_dst_mac_t  ov_dst = stream->get_override_dst_mac_mode();


    if ( (ov_src == true) && (ov_dst == TrexStream::stPKT) ) {
        /* nothing to do, take from the packet both */
        return;
    }

        /* take from cfg_file */
    if ( (ov_src == false) &&
         (ov_dst == TrexStream::stCFG_FILE) ){

          m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir,(uint8_t*)raw_pkt);
          return;
    }

    /* save the pkt*/
    char tmp_pkt[12];
    memcpy(tmp_pkt,raw_pkt,12);

    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir,(uint8_t*)raw_pkt);

    if ((ov_src == true) && (ov_dst == TrexStream::stCFG_FILE)) {
        memcpy(raw_pkt+6,tmp_pkt+6,6);
    }

    if ((ov_src == false) && (ov_dst == TrexStream::stPKT)) {
        memcpy(raw_pkt,tmp_pkt,6);
    }
}


void TrexStatelessDpCore::replay_vm_into_cache(TrexStream * stream, 
                                               CGenNodeStateless *node){

    uint16_t      cache_size = stream->m_cache_size;
    assert(cache_size>0);
    rte_mbuf_t * m=0;

    uint32_t buf_size = CGenNodeCacheMbuf::get_object_size(cache_size);
    CGenNodeCacheMbuf * p = (CGenNodeCacheMbuf *)malloc(buf_size);
    assert(p);
    memset(p,0,buf_size);

    int i;
    for (i=0; i<cache_size; i++) {
        p->m_array[i] =  node->alloc_node_with_vm();
    }
    /* save const */
    m=node->get_const_mbuf();
    if (m) {
        p->m_mbuf_const=m;
        rte_pktmbuf_refcnt_update(m,1);
    }

    /* free all VM and const mbuf */
    node->free_stl_vm_buf();

    /* copy to local node meory */
    node->cache_mbuf_array_copy(p,cache_size);

    /* free the memory */
    free(p);
}


void
TrexStatelessDpCore::add_stream(TrexStatelessDpPerPort * lp_port,
                                uint32_t profile_id,
                                TrexStream * stream,
                                TrexStreamsCompiledObj *comp,
                                double start_at_ts) {

    CGenNodeStateless *node = (CGenNodeStateless*)m_core->create_node();

    node->m_thread_id = m_thread_id;
    node->m_profile_id = profile_id;
    node->cache_mbuf_array_init();
    node->m_batch_size=0;

    /* add periodic */
    node->m_cache_mbuf=0;
    node->m_type = CGenNode::STATELESS_PKT;

    node->m_action_counter = stream->m_action_count;

    /* clone the stream from control plane memory to DP memory */
    node->m_ref_stream_info = stream->clone();
    /* no need for this memory anymore on the control plane memory */
    stream->release_dp_object();

    node->m_next_stream=0; /* will be fixed later */

    if ( stream->m_self_start ){
        /* if self start it is in active mode */
        node->m_state =CGenNodeStateless::ss_ACTIVE;
        lp_port->m_active_streams++;
    }else{
        node->m_state =CGenNodeStateless::ss_INACTIVE;
    }

    if ( unlikely(start_at_ts) ) {
        node->m_time = start_at_ts + stream->get_start_delay_sec();
    } else {
        node->m_time = m_core->m_cur_time_sec + stream->get_start_delay_sec();
    }

    pkt_dir_t dir = m_core->m_node_gen.m_v_if->port_id_to_dir(stream->m_port_id);
    node->m_flags = 0;
    node->m_src_port =0;
    node->m_original_packet_data_prefix = 0;

    if (stream->m_rx_check.m_enabled) {
        uint16_t hw_id = stream->m_rx_check.m_hw_id;
        if (hw_id < MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
            node->set_stat_needed();
            node->set_stat_hw_id(hw_id);
            // no support for cache with flow stat payload rules
            if ((TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                stream->m_cache_size = 0;
            }
        }
    }

    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */

    uint16_t pkt_size = stream->m_pkt.len;
    const uint8_t *stream_pkt = stream->m_pkt.binary;

    node->m_stream_type = stream->m_type;
    node->m_next_time_offset_backup = 1.0 / stream->get_pps();
    node->m_null_stream = (stream->m_null_stream ? 1 : 0);
    node->set_pause(stream->m_start_paused);

    /* stateless specific fields */
    switch ( stream->m_type ) {

    case TrexStream::stCONTINUOUS :
        node->m_single_burst=0;
        node->m_single_burst_refill=0;
        node->m_multi_bursts=0;
        break;

    case TrexStream::stSINGLE_BURST :
        node->m_stream_type             = TrexStream::stMULTI_BURST;
        node->m_single_burst            = stream->m_burst_total_pkts;
        node->m_single_burst_refill     = stream->m_burst_total_pkts;
        node->m_multi_bursts            = 1;  /* single burst in multi burst of 1 */
        break;

    case TrexStream::stMULTI_BURST :
        node->m_single_burst        = stream->m_burst_total_pkts;
        node->m_single_burst_refill = stream->m_burst_total_pkts;
        node->m_multi_bursts        = stream->m_num_bursts;
        break;
    default:

        assert(0);
    };

    node->m_port_id = stream->m_port_id;

    /* set dir 0 or 1 client or server */
    node->set_mbuf_cache_dir(dir);


    if (node->m_ref_stream_info->getDpVm() == NULL) {
        /* no VM */

        node->m_vm_flow_var =  NULL;
        node->m_vm_program  =  NULL;
        node->m_vm_program_size =0;

                /* allocate const mbuf */
        rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(), pkt_size);
        assert(m);

        char *p = rte_pktmbuf_append(m, pkt_size);
        assert(p);
        /* copy the packet */
        memcpy(p,stream_pkt,pkt_size);

        update_mac_addr(stream,node,dir,p);

        /* set the packet as a readonly */
        node->set_cache_mbuf(m);

        node->m_original_packet_data_prefix =0;
    }else{

        /* set the program */
        TrexStream * local_mem_stream = node->m_ref_stream_info;

        StreamVmDp  * lpDpVm = local_mem_stream->getDpVm();

        node->m_vm_flow_var      = lpDpVm->clone_bss(); /* clone the flow var */
        node->m_vm_program       = lpDpVm->get_program(); /* same ref to the program */
        node->m_vm_program_size  = lpDpVm->get_program_size();

        /* generate random seed if needed*/
        if (lpDpVm->is_random_seed()) {
            node->generate_random_seed();
        }

        /* we need to copy the object */
        if ( pkt_size > lpDpVm->get_prefix_size() ) {
            /* we need const packet */
            uint16_t const_pkt_size  = pkt_size - lpDpVm->get_prefix_size() ;
            rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(), const_pkt_size );
            assert(m);

            char *p = rte_pktmbuf_append(m, const_pkt_size);
            assert(p);

            /* copy packet data */
            memcpy(p,(stream_pkt + lpDpVm->get_prefix_size()),const_pkt_size);

            node->set_const_mbuf(m);
        }


        if ( lpDpVm->is_pkt_size_var() ) {
            // mark the node as varible size
            node->set_var_pkt_size();
        }


        if (lpDpVm->get_prefix_size() > pkt_size ) {
            lpDpVm->set_prefix_size(pkt_size);
        }

        /* copy the headr */
        uint16_t header_size = lpDpVm->get_prefix_size();
        assert(header_size);
        node->alloc_prefix_header(header_size);
        uint8_t *p=node->m_original_packet_data_prefix;
        assert(p);

        memcpy(p,stream_pkt , header_size);

        update_mac_addr(stream,node,dir,(char *)p);

        if (stream->m_cache_size > 0 ) {
            /* we need to create cache of objects */
            replay_vm_into_cache(stream, node);
        }
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
                                   uint32_t profile_id,
                                   double duration,
                                   int event_id,
                                   double start_at_ts) {


    TrexStatelessDpPerPort * lp_port=get_port_db(obj->get_port_id());
    lp_port->set_event_id(event_id);

    double schd_offset = get_dpdk_mode()->dp_rx_queues()?
        SCHD_OFFSET_DTIME_RX_ENABLED:
        SCHD_OFFSET_DTIME;

    /* update cur time */
    if ( CGlobalInfo::is_realtime() ){
        m_core->m_cur_time_sec = now_sec() + schd_offset;
    }

    /* no nodes in the list */
    uint32_t base_node = lp_port->m_active_nodes.size();

    for (auto single_stream : obj->get_objects()) {
        /* all commands should be for the same port */
        assert(obj->get_port_id() == single_stream.m_stream->m_port_id);
        add_stream(lp_port,profile_id,single_stream.m_stream,obj,start_at_ts);
    }

    uint32_t nodes = lp_port->m_active_nodes.size() - base_node;
    /* find next stream */
    assert(nodes == obj->get_objects().size());

    int cnt=0;

    /* set the next_stream pointer  */
    for (auto single_stream : obj->get_objects()) {

        if (single_stream.m_stream->is_dp_next_stream() ) {
            int stream_id = single_stream.m_stream->m_next_stream_id;
            assert(stream_id<nodes);
            /* point to the next stream , stream_id is fixed */
            lp_port->m_active_nodes[base_node+cnt].m_node->m_next_stream = lp_port->m_active_nodes[base_node+stream_id].m_node ;
        }
        cnt++;
    }

    lp_port->m_state =TrexStatelessDpPerPort::ppSTATE_TRANSMITTING;
    m_state = TrexStatelessDpCore::STATE_TRANSMITTING;


    if ( duration > 0.0 ){
        add_port_duration( duration ,obj->get_port_id(), profile_id, event_id );
    }

}


bool TrexStatelessDpCore::are_all_ports_idle() {

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
TrexStatelessDpCore::resume_traffic(uint8_t port_id, uint32_t profile_id){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->resume_traffic(port_id, profile_id);

}


void
TrexStatelessDpCore::resume_streams(uint8_t port_id, stream_ids_t &stream_ids){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->resume_streams(port_id, stream_ids);

}


void
TrexStatelessDpCore::pause_traffic(uint8_t port_id, uint32_t profile_id){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->pause_traffic(port_id, profile_id);

}

void
TrexStatelessDpCore::pause_streams(uint8_t port_id, stream_ids_t &stream_ids){

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->pause_streams(port_id, stream_ids);

}

void 
TrexStatelessDpCore::push_pcap(uint8_t port_id,
                               int event_id,
                               const std::string &pcap_filename,
                               double ipg_usec,
                               double m_min_ipg_sec,
                               double speedup,
                               uint32_t count,
                               double duration,
                               bool is_dual) {

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);

    lp_port->set_event_id(event_id);

    /* delegate the command to the port */
    bool rc = lp_port->push_pcap(port_id, pcap_filename, ipg_usec, m_min_ipg_sec, speedup, count, is_dual);
    if (!rc) {
        /* report back that we stopped */
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(m_core->m_thread_id);
        TrexDpToCpMsgBase *event_msg = new TrexDpPortEventMsg(m_core->m_thread_id,
                                                              port_id,
                                                              event_id,
                                                              false);
        ring->Enqueue((CGenNode *)event_msg);
        return;
    }


    if (duration > 0.0) {
        add_port_duration(duration, port_id, 0, event_id);
    }

     m_state = TrexStatelessDpCore::STATE_PCAP_TX;
}

void
TrexStatelessDpCore::update_traffic(uint8_t port_id, uint32_t profile_id, double factor) {

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->update_traffic(port_id, profile_id, factor);

}

void
TrexStatelessDpCore::update_streams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream) {

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    lp_port->update_streams(port_id, ipg_per_stream);

}

void
TrexStatelessDpCore::stop_traffic(uint8_t  port_id,
                                  uint32_t profile_id,
                                  bool     stop_on_id,
                                  int      event_id) {
    /* we cannot remove nodes not from the top of the queue so
       for every active node - make sure next time
       the scheduler invokes it, it will be free */

    TrexStatelessDpPerPort * lp_port = get_port_db(port_id);
    if ( lp_port->stop_traffic(port_id,profile_id,stop_on_id,event_id) == false){
        return;
    }

    if (lp_port->m_active_streams == 0) {
        /* flush the TX queue before sending done message to the CP */
        m_core->flush_tx_queue();

        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(m_core->m_thread_id);
        TrexDpToCpMsgBase *event_msg = new TrexDpPortEventMsg(m_core->m_thread_id,
                                                              port_id,
                                                              lp_port->get_event_id());
        ring->Enqueue((CGenNode *)event_msg);
    }
}



void
TrexStatelessDpCore::set_service_mode(uint8_t port_id, bool enabled) {
    /* ignore the same message */
    if (enabled == m_is_service_mode) {
        return;
    }
    
    if (enabled) {
        /* sanity */
        assert(m_core->m_node_gen.m_v_if != m_wrapper);
        
        /* set the wrapper object and make the VIF point to it */
        m_wrapper->set_wrapped_object(m_core->m_node_gen.m_v_if);
        m_core->m_node_gen.m_v_if = m_wrapper;
        m_is_service_mode = true;
        
    } else {
        /* sanity */
        assert(m_core->m_node_gen.m_v_if == m_wrapper);
        
        /* restore the wrapped object and make the VIF point to it */
        m_core->m_node_gen.m_v_if = m_wrapper->get_wrapped_object();
        m_is_service_mode = false;
    }
}

void
TrexStatelessDpCore::clear_fs_latency_stats(uint8_t dir) {
    m_ports[dir].m_fs_latency.reset_stats();
}

void
TrexStatelessDpCore::clear_fs_latency_stats_partial(uint8_t dir, int min, int max, TrexPlatformApi::driver_stat_cap_e type) {
    m_ports[dir].m_fs_latency.reset_stats_partial(min, max, type);
}

void
TrexStatelessDpCore::rfc2544_stop_and_sample(int min, int max, bool reset, bool period_switch) {
    for (uint8_t dir = 0; dir < NUM_PORTS_PER_CORE; dir++) {
        for (int hw_id = min; hw_id <= max; hw_id++) {
            CRFC2544Info &curr_rfc2544 = m_ports[dir].m_rfc2544[hw_id];

            if (reset) {
                // need to stop first, so count will be consistent
                curr_rfc2544.stop();
            }

            if (period_switch) {
                curr_rfc2544.sample_period_end();
            }
        }
    }
}

void
TrexStatelessDpCore::rfc2544_reset(int min, int max) {
    for (uint8_t dir = 0; dir < NUM_PORTS_PER_CORE; dir++) {
        for (int hw_id = min; hw_id <= max; hw_id++) {
            CRFC2544Info &curr_rfc2544 = m_ports[dir].m_rfc2544[hw_id];
            curr_rfc2544.reset();
        }
    }
}

/**
 * PCAP node
 */
bool CGenNodePCAP::create(uint8_t port_id,
                          pkt_dir_t dir,
                          socket_id_t socket_id,
                          const uint8_t *mac_addr,
                          const uint8_t *slave_mac_addr,
                          const std::string &pcap_filename,
                          double ipg_usec,
                          double min_ipg_sec,
                          double speedup,
                          uint32_t count,
                          bool is_dual) {
    std::stringstream ss;

    m_type       = CGenNode::PCAP_PKT;
    m_flags      = 0;
    m_src_port   = 0;
    m_port_id    = port_id;
    m_count      = count;
    m_is_dual    = is_dual;
    m_dir        = dir;
    m_min_ipg_sec    = min_ipg_sec;

    /* increase timeout of WD due to io */
    TrexWatchDog::IOFunction::io_begin();

    /* mark this node as slow path */
    set_slow_path(true);

    if (ipg_usec != -1) {
        /* fixed IPG */
        m_ipg_sec = std::max(min_ipg_sec, usec_to_sec(ipg_usec / speedup));
        m_speedup = 0;
    } else {
        /* packet IPG */
        m_ipg_sec = -1;
        m_speedup  = speedup;
    }

    /* copy MAC addr info */
    memcpy(m_mac_addr, mac_addr, 12);
    memcpy(m_slave_mac_addr, slave_mac_addr, 12);


    set_socket_id(socket_id);

    /* create the PCAP reader */
    m_reader = CCapReaderFactory::CreateReader((char *)pcap_filename.c_str(), 0, ss);
    if (!m_reader) {
        return false;
    }

    m_raw_packet = new CCapPktRaw();
    if ( m_reader->ReadPacket(m_raw_packet) == false ){
        return false;
    }

    /* set the dir */
    set_mbuf_dir(dir);

    /* update the direction (for dual mode) */
    update_pkt_dir();

    /* this is the reference time */
    m_last_pkt_time = m_raw_packet->get_time();

    /* ready */
    m_state = PCAP_ACTIVE;

    return true;
}

/**
 * cleanup for PCAP node
 * 
 * @author imarom (08-May-16)
 */
void CGenNodePCAP::destroy() {

    if (m_raw_packet) {
        delete m_raw_packet;
        m_raw_packet = NULL;
    }

    if (m_reader) {
        delete m_reader;
        m_reader = NULL;
    }

    /* end of io, return normal timeout of WD */
    TrexWatchDog::IOFunction::io_end();

    m_state = PCAP_INVALID;
}




