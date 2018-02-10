/*
 Hanoh Haim
 Ido Barnea
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
#include "utl_json.h"
#include "trex_watchdog.h"
#include "pkt_gen.h"
#include "common/basic_utils.h"
#include "stateful_rx_core.h"

const uint8_t sctp_pkt[]={

    0x00,0x04,0x96,0x08,0xe0,0x40,
    0x00,0x0e,0x2e,0x24,0x37,0x5f,
    0x08,0x00,

    0x45,0x03,0x00,0x30,
    0x00,0x00,0x40,0x00,
    0xff,0x84,0xbd,0x04,
    0x9b,0xe6,0x18,0x9b, //sIP
    0xcb,0xff,0xfc,0xc2, //DIP

    0x80,0x44,//SPORT
    0x00,0x50,//DPORT

    0x00,0x00,0x00,0x00, //checksum

    0x11,0x22,0x33,0x44, // magic
    0x00,0x00,0x00,0x00, //64 bit counter
    0x00,0x00,0x00,0x00,
    0x00,0x01,0xa0,0x00, //seq
    0x00,0x00,0x00,0x00,

};

const uint8_t icmp_pkt[]={
    0x00,0x04,0x96,0x08,0xe0,0x40,
    0x00,0x0e,0x2e,0x24,0x37,0x5f,
    0x08,0x00,

    0x45,0x03,0x00,0x30,
    0x00,0x00,0x40,0x00,
    0xff,0x01,0xbd,0x04,
    0x9b,0xe6,0x18,0x9b, //SIP
    0xcb,0xff,0xfc,0xc2, //DIP

    0x08, 0x00,
    0x01, 0x02,  //checksum
    0xaa, 0xbb,  // id
    0x00, 0x00,  // Sequence number

    0x11,0x22,0x33,0x44, // magic
    0x00,0x00,0x00,0x00, //64 bit counter
    0x00,0x00,0x00,0x00,
    0x00,0x01,0xa0,0x00, //seq
    0x00,0x00,0x00,0x00,

};


void CLatencyPktInfo::Create(class CLatencyPktMode *m_l_pkt_info){
    uint8_t pkt_size = m_l_pkt_info->getPacketLen();

    m_packet = new CCapPktRaw( pkt_size);
    m_packet->pkt_cnt=0;
    m_packet->time_sec=0;
    m_packet->time_nsec=0;
    memcpy(m_packet->raw, m_l_pkt_info->getPacketData(), pkt_size);
    m_packet->pkt_len=pkt_size;

    m_pkt_indication.m_packet  =m_packet;

    m_pkt_indication.m_ether = (EthernetHeader *)m_packet->raw;
    m_pkt_indication.l3.m_ipv4=(IPHeader       *)(m_packet->raw+14);
    m_pkt_indication.m_is_ipv6 = false;
    m_pkt_indication.m_is_ipv6_converted = false;

    m_pkt_indication.l4.m_icmp=(ICMPHeader *)m_packet->raw+14+20;
    m_pkt_indication.m_payload=(uint8_t *)m_packet->raw+14+20+16;
    m_pkt_indication.m_payload_len=0;
    m_pkt_indication.m_packet_padding=4;


    m_pkt_indication.m_ether_offset =0;
    m_pkt_indication.m_ip_offset =14;
    m_pkt_indication.m_udp_tcp_offset = 34;
    m_pkt_indication.m_payload_offset = 34+8;

    CPacketDescriptor * lpd=&m_pkt_indication.m_desc;
    lpd->Clear();
    lpd->SetInitSide(true);
    lpd->SetSwapTuple(false);
    lpd->SetIsValidPkt(true);
    lpd->SetIsIcmp(true);
    lpd->SetIsLastPkt(true);
    m_pkt_info.Create(&m_pkt_indication);

    memset(&m_dummy_node,0,sizeof(m_dummy_node));

    m_dummy_node.set_socket_id( CGlobalInfo::m_socket.port_to_socket(0) );

    m_dummy_node.m_time =0.1;
    m_dummy_node.m_pkt_info = &m_pkt_info;
    m_dummy_node.m_dest_ip  = 0;
    m_dummy_node.m_src_ip   = 0;
    m_dummy_node.m_src_port =  0x11;
    m_dummy_node.m_flow_id =0;
    m_dummy_node.m_flags =CGenNode::NODE_FLAGS_LATENCY;
}

rte_mbuf_t * CLatencyPktInfo::generate_pkt(int port_id, uint32_t extern_ip) {
    bool is_client_to_server = (port_id % 2 == 0) ? true:false;
    int dual_port_index = port_id >> 1;
    uint32_t mask = dual_port_index * m_dual_port_mask;
    uint32_t c = m_client_ip.v4;
    uint32_t s = m_server_ip.v4;

    if (is_client_to_server) {
        if ( extern_ip ) {
            m_dummy_node.m_src_ip = extern_ip;
        } else {
            m_dummy_node.m_src_ip = c + mask;
        }
        m_dummy_node.m_dest_ip = s + mask;
    } else {
        if ( extern_ip ) {
            m_dummy_node.m_dest_ip = extern_ip;
        } else {
            m_dummy_node.m_dest_ip = c + mask;
        }
        m_dummy_node.m_src_ip = s + mask;
    }

    rte_mbuf_t *m = m_pkt_info.generate_new_mbuf(&m_dummy_node);
    
    if (m_client_cfg) {
        m_client_cfg[dual_port_index].apply(m, (is_client_to_server ? CLIENT_SIDE : SERVER_SIDE));
    }
    
    return m;
}

void CLatencyPktInfo::set_ip(uint32_t                src,
                             uint32_t                dst,
                             uint32_t                dual_port_mask) {
    m_client_ip.v4   = src;
    m_server_ip.v4   = dst;
    m_dual_port_mask = dual_port_mask;
    
    if (m_client_cfg) {
        delete [] m_client_cfg;
    }
    
    /* no client cluster cfg */
    m_client_cfg = NULL;
}


void CLatencyPktInfo::set_ip(uint32_t        src,
                             uint32_t        dst,
                             uint32_t        dual_port_mask,
                             uint8_t         port_cnt,
                             ClientCfgDB    &client_cfg_db) {
    m_client_ip.v4   = src;
    m_server_ip.v4   = dst;
    m_dual_port_mask = dual_port_mask;
    
    if (m_client_cfg) {
        delete [] m_client_cfg;
    }
    
    /* allocate one client config for each pair of ports */
    int dual_port_cnt = port_cnt >> 1;
    m_client_cfg = new ClientCfgBase[dual_port_cnt];
    
    /* for each IP - lookup the client cluster */
    for (int i = 0; i < dual_port_cnt; i++) {
        uint32_t ip = src + (dual_port_mask * i);
        
        ClientCfgEntry *entry = client_cfg_db.lookup(ip);
        if (!entry) {
            std::stringstream ss;
            ss << "client configuration error: could not map IP '" << ip_to_str(ip) << "' to a group\n";
            std::cout << ss.str();
            exit(-1);
        }
        
        entry->assign(m_client_cfg[i], ip);
    }
}


void CLatencyPktInfo::Delete(){
    m_pkt_info.Delete();
    delete m_packet;
    
    if (m_client_cfg) {
        delete [] m_client_cfg;
        m_client_cfg = NULL;
    }
}

void CCPortLatency::reset(){
    m_rx_seq    =m_tx_seq;
    m_pad       = 0;
    m_tx_pkt_err=0;
    m_tx_pkt_ok =0;
    m_ign_stats.clear();
    m_ign_stats_prev.clear();
    m_pkt_ok=0;
    m_rx_check=0;
    m_no_magic=0;
    m_unsup_prot=0;
    m_no_id=0;
    m_seq_error=0;
    m_l3_cs_err=0;
    m_l4_cs_err=0;
    m_length_error=0;
    m_no_ipv4_option=0;
    m_hist.Reset();
}

static uint8_t nat_is_port_can_send(uint8_t port_id){
    uint8_t client_index = (port_id %2);
    return (client_index ==0 ?1:0);
}

bool CCPortLatency::Create(CLatencyManager * parent,
                           uint8_t id,
                           uint16_t payload_offset,
                           uint16_t l4_offset,
                           uint16_t pkt_size,
                           CCPortLatency * rx_port){
    m_parent    = parent;
    m_id        = id;
    m_tx_seq    =0x12345678;
    m_icmp_tx_seq = 1;
    m_icmp_rx_seq = 0;
    m_l4_offset = l4_offset;
    m_payload_offset    = payload_offset;
    m_pkt_size  = pkt_size;
    m_rx_port   = rx_port;
    m_nat_can_send = nat_is_port_can_send(m_id);
    m_nat_learn    = m_nat_can_send;
    m_nat_external_ip=0;

    m_hist.Create();
    reset();
    return (true);
}

void CCPortLatency::Delete(){
    m_hist.Delete();
}

void CCPortLatency::update_packet(rte_mbuf_t * m, int port_id){
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    bool is_client_to_server=(port_id%2==0)?true:false;

    /* update mac addr dest/src 12 bytes */
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(m_id),12);

    latency_header * h=(latency_header *)(p+m_payload_offset);
    h->magic = LATENCY_MAGIC | m_id ;
    h->time_stamp = os_get_hr_tick_64();
    h->seq = m_tx_seq;
    m_tx_seq++;

    CLatencyPktMode *c_l_pkt_mode = m_parent->c_l_pkt_mode;
    c_l_pkt_mode->update_pkt(p + m_l4_offset, is_client_to_server, m_pkt_size - m_l4_offset, &m_icmp_tx_seq);
}


void CCPortLatency::DumpShortHeader(FILE *fd){
    fprintf(fd," if|   tx_ok , rx_ok  , rx check ,error,       latency (usec) ,    Jitter          max window \n");
    fprintf(fd,"   |         ,        ,          ,     ,   average   ,   max  ,    (usec)                     \n");
    fprintf(fd," ---------------------------------------------------------------------------------------------------------------- \n");
}



std::string CCPortLatency::get_field(std::string name,float f){
    char buff[200];
    sprintf(buff,"\"%s-%d\":%.1f,",name.c_str(),m_id,f);
    return (std::string(buff));
}


void CCPortLatency::dump_json_v2(std::string & json ){
    char buff[200];
    sprintf(buff,"\"port-%d\": {",m_id);
    json+=std::string(buff);
    m_hist.dump_json("hist",json);
    dump_counters_json(json);
    json+="},";
}

void CCPortLatency::dump_json(std::string & json ){
    json += get_field("avg",m_hist.get_average_latency() );
    json += get_field("max",m_hist.get_max_latency() );
    json += get_field("c-max",m_hist.get_max_latency_last_update() );
    json += get_field("error",(float)(m_unsup_prot+m_no_magic+m_no_id+m_seq_error+m_length_error+m_l3_cs_err+m_l4_cs_err));
    json += get_field("jitter",(float)get_jitter_usec() );
}


void CCPortLatency::DumpShort(FILE *fd){

//	m_hist.update(); <- moved to CLatencyManager::update()
    fprintf(fd,"%8lu,%8lu,%10lu,%5lu,",
                    m_tx_pkt_ok,
                    m_pkt_ok,
                    m_rx_check,
                    m_unsup_prot+m_no_magic+m_no_id+m_seq_error+m_length_error+m_no_ipv4_option+m_tx_pkt_err+m_l3_cs_err+m_l4_cs_err
            );

    fprintf(fd,"   %8.0f  ,%8.0f,%8d ",
                          m_hist.get_average_latency(),
                          m_hist.get_max_latency(),
                          get_jitter_usec()
                        );
    fprintf(fd,"     | ");
    m_hist.DumpWinMax(fd);

}

#define DPL_J(f)  json+=add_json(#f,f);
#define DPL_J_LAST(f)  json+=add_json(#f,f,true);

void CCPortLatency::dump_counters_json(std::string & json ){

    json+="\"stats\" : {";
    DPL_J(m_tx_pkt_ok);
    json+=add_json("tx_arp", m_ign_stats.get_tx_arp());
    json+=add_json("ipv6_n_solic", m_ign_stats.get_tx_n_solic());
    json+=add_json("ignore_bytes", m_ign_stats.get_tx_bytes());
    DPL_J(m_tx_pkt_err);
    DPL_J(m_pkt_ok);
    DPL_J(m_unsup_prot);
    DPL_J(m_no_magic);
    DPL_J(m_no_id);
    DPL_J(m_seq_error);
    DPL_J(m_l3_cs_err);
    DPL_J(m_l4_cs_err);
    DPL_J(m_length_error);
    DPL_J(m_no_ipv4_option);
    json+=add_json("m_jitter",get_jitter_usec());
    /* must be last */
    DPL_J_LAST(m_rx_check);
    json+="}";


}

void CCPortLatency::DumpCounters(FILE *fd){
#define DP_A1(f) if (f) fprintf(fd," %-40s : %llu \n",#f, (unsigned long long)f)
#define DP_A2(str, f) if (f) fprintf(fd," %-40s : %llu \n", str, (unsigned long long)f)

    fprintf(fd," counter  \n");
    fprintf(fd," -----------\n");

    DP_A1(m_tx_pkt_err);
    DP_A1(m_tx_pkt_ok);
    DP_A1(m_pkt_ok);
    DP_A1(m_unsup_prot);
    DP_A1(m_no_magic);
    DP_A1(m_no_id);
    DP_A1(m_seq_error);
    DP_A1(m_l3_cs_err); /* ip error */
    DP_A1(m_l4_cs_err); /* tcp/udp error */
    DP_A1(m_length_error);
    DP_A1(m_rx_check);
    DP_A1(m_no_ipv4_option);
    DP_A2("tx_arp", m_ign_stats.get_tx_arp());
    DP_A2("ipv6_n_solic", m_ign_stats.get_tx_n_solic());
    DP_A2("ignore_bytes", m_ign_stats.get_tx_bytes());

    fprintf(fd," -----------\n");
    m_hist.Dump(fd);
    fprintf(fd," %-40s : %lu \n","jitter", (ulong)get_jitter_usec());
}

bool CCPortLatency::dump_packet(rte_mbuf_t * m){
    fprintf(stdout," %f.03 dump packet ..\n",now_sec());
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
    utl_DumpBuffer(stdout,p,pkt_size,0);
    return (0);
#if 0
    if (pkt_size < ( sizeof(CRx_check_header)+14+20) ) {
        assert(0);
    }
    CRx_check_header * lp=(CRx_check_header *)(p+pkt_size-sizeof(CRx_check_header));

    lp->dump(stdout);

    return (0);
#endif

}

bool CCPortLatency::check_rx_check(rte_mbuf_t * m) {
    m_rx_check++;
    return (true);
}

bool CCPortLatency::do_learn(uint32_t external_ip) {
    m_nat_learn=true;
    m_nat_can_send=true;
    m_nat_external_ip=external_ip;
    return (true);
}

bool CCPortLatency::check_packet(rte_mbuf_t * m,CRx_check_header * & rx_p) {
    CSimplePacketParser parser(m);
    if ( !parser.Parse()  ) {
        m_unsup_prot++;  // Unsupported protocol
        return (false);
    }

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
        if ( (m->ol_flags & PKT_RX_IP_CKSUM_MASK) ==  PKT_RX_IP_CKSUM_BAD ){
            m_l3_cs_err++;
        }
    
        if ( (m->ol_flags & PKT_RX_L4_CKSUM_BAD) ==  PKT_RX_L4_CKSUM_BAD ){
            if ( parser.m_protocol != IPHeader::Protocol::SCTP ){
                /* in case of SCTP packets we don't fix checksum  for L4 */
                m_l4_cs_err++;
            }
        }
    }

    CLatencyPktMode *c_l_pkt_mode = m_parent->c_l_pkt_mode;
    uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
    uint16_t vlan_offset=parser.m_vlan_offset;
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);

    rx_p = (CRx_check_header *)0;

    bool is_lateancy_pkt =  c_l_pkt_mode->IsLatencyPkt(parser.m_ipv4) & IsLatencyPkt(parser.m_l4 + c_l_pkt_mode->l4_header_len());

    if ( ! is_lateancy_pkt) {

#ifdef NAT_TRACE_
        printf(" %.3f RX : got packet !!! \n",now_sec() );
#endif

        /* ipv6+rx-check */
        if ( parser.m_ipv6 ) {
            /* if we have ipv6 packet */
            if (parser.m_protocol == RX_CHECK_V6_OPT_TYPE) {
                if ( get_is_rx_check_mode() ){
                    m_rx_check++;
                    rx_p=(CRx_check_header *)((uint8_t*)parser.m_ipv6  +IPv6Header::DefaultSize);
                    return (true);
                }

            }
            m_seq_error++;
           return (false);
        }

        uint8_t opt_len = parser.m_ipv4->getOptionLen();
        uint8_t *opt_ptr = parser.m_ipv4->getOption();
        /* Process IP option header(s) */
        while ( opt_len != 0 ) {
            switch (*opt_ptr) {
            case RX_CHECK_V4_OPT_TYPE:
                    /* rx-check option header */
                    if ( ( !get_is_rx_check_mode() ) ||
                        (opt_len < RX_CHECK_LEN) ) {
                        m_seq_error++;
                        return (false);
                    }
                    m_rx_check++;
                    rx_p=(CRx_check_header *)opt_ptr;
                    opt_len -= RX_CHECK_LEN;
                    opt_ptr += RX_CHECK_LEN;
                    break;
            case CNatOption::noIPV4_OPTION:
                    /* NAT learn option header */
                    CNatOption *lp;
                    if ( ( !CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION) ) ||
                         (opt_len < CNatOption::noOPTION_LEN) ) {
                        m_seq_error++;
                        return (false);
                    }
                    lp = (CNatOption *)opt_ptr;
                    if ( !lp->is_valid_ipv4_magic() ) {
                        m_no_ipv4_option++;
                        return (false);
                    }
                    m_parent->get_nat_manager()->handle_packet_ipv4(lp, parser.m_ipv4, true);
                    opt_len -= CNatOption::noOPTION_LEN;
                    opt_ptr += CNatOption::noOPTION_LEN;
                    break;
            default:
                    m_seq_error++;
                    return (false);
            } // End of switch
        } // End of while

        bool first;
        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP) && parser.IsNatInfoPkt(first)) {
            m_parent->get_nat_manager()->handle_packet_ipv4(NULL, parser.m_ipv4, first);
        }

        return (true);
    } // End of check for non-latency packet
    // learn for latency packets. We only have one flow for latency, so translation is for it.
    if ( CGlobalInfo::is_learn_mode() && (m_nat_learn ==false) ) {
        do_learn(parser.m_ipv4->getSourceIp());
    }

    if ( (pkt_size-vlan_offset) != m_pkt_size ) {
        // patch for e1000 card. e1000 hands us the packet with Ethernet FCS, so it is 4 bytes longer
        if ((pkt_size - vlan_offset - 4) != m_pkt_size ) {
            m_length_error++;
            return (false);
        }
    }
    c_l_pkt_mode->update_recv(p + m_l4_offset + vlan_offset, &m_icmp_rx_seq, &m_icmp_tx_seq);
#ifdef LATENCY_DEBUG
    c_l_pkt_mode->rcv_debug_print(p + m_l4_offset + vlan_offset);
#endif
    latency_header * h=(latency_header *)(p+m_payload_offset + vlan_offset);
    if ( h->seq != m_rx_seq ){
        m_seq_error++;
        m_rx_seq =h->seq +1;
        return (false);
    }else{
        m_rx_seq++;
    }
    m_pkt_ok++;
    uint64_t d = (os_get_hr_tick_64() - h->time_stamp );
    dsec_t ctime=ptime_convert_hr_dsec(d);
    m_hist.Add(ctime);
    m_jitter.calc(ctime);
    return (true);
}

void CLatencyManager::Delete(){
    m_pkt_gen.Delete();

    if ( get_is_rx_check_mode() ) {
        m_rx_check_manager.Delete();
    }
    if ( CGlobalInfo::is_learn_mode() ){
        m_nat_check_manager.Delete();
    }
    m_cpu_cp_u.Delete();
    
    if (c_l_pkt_mode) {
        delete c_l_pkt_mode;
    }
}

/* 0->1
   1->0
   2->3
   3->2
*/
static uint8_t swap_port(uint8_t port_id){
    uint8_t offset= ((port_id>>1)<<1);
    uint8_t client_index = (port_id %2);
    return (offset + (client_index ^ 1));
}



bool CLatencyManager::Create(CLatencyManagerCfg *cfg){
    switch (CGlobalInfo::m_options.get_l_pkt_mode()) {
    default:
    case 0:
        c_l_pkt_mode = (CLatencyPktModeSCTP *) new CLatencyPktModeSCTP(CGlobalInfo::m_options.get_l_pkt_mode());
        break;
    case 1:
    case 2:
    case 3:
        c_l_pkt_mode =  (CLatencyPktModeICMP *) new CLatencyPktModeICMP(CGlobalInfo::m_options.get_l_pkt_mode());
        break;
    }

    m_max_ports=cfg->m_max_ports;
    assert (m_max_ports <= TREX_MAX_PORTS);
    assert ((m_max_ports%2)==0);
    m_port_mask =0xffffffff;
    m_do_stop =false;
    m_is_active =false;
    m_pkt_gen.Create(c_l_pkt_mode);
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        CCPortLatency * lpo=&m_ports[swap_port(i)].m_port;

        lp->m_io=cfg->m_ports[i];
        lp->m_port.Create(this,
                          i,
                          m_pkt_gen.get_payload_offset(),
                          m_pkt_gen.get_l4_offset(),
                          m_pkt_gen.get_pkt_size(),lpo );
    }
    m_cps= cfg->m_cps;
    if (m_cps != 0) {
        m_delta_sec =(1.0/m_cps);
    } else {
        m_delta_sec = 0.0;
    }

    if ( get_is_rx_check_mode() ) {
        assert(m_rx_check_manager.Create());
        m_rx_check_manager.m_cur_time= now_sec();
     }


    m_pkt_gen.set_ip(cfg->m_client_ip.v4,
                     cfg->m_server_ip.v4,
                     cfg->m_dual_port_mask);
    
    m_cpu_cp_u.Create(&m_cpu_dp_u);
    if ( CGlobalInfo::is_learn_mode() ){
        m_nat_check_manager.Create();
    }

    return (true);
}

void  CLatencyManager::send_pkt_all_ports(){
    m_start_time = os_get_hr_tick_64();
    int i;
    for (i=0; i<m_max_ports; i++) {
        if ( m_port_mask & (1<<i)  ){
            CLatencyManagerPerPort * lp=&m_ports[i];
            if (lp->m_port.can_send_packet(i%2) ){
                rte_mbuf_t * m=m_pkt_gen.generate_pkt(i,lp->m_port.external_nat_ip());
                lp->m_port.update_packet(m, i);

#ifdef LATENCY_DEBUG
                uint8_t *p = rte_pktmbuf_mtod(m, uint8_t*);
                c_l_pkt_mode->send_debug_print(p + 34);
#endif
                if ( lp->m_io->tx_latency(m) == 0 ){
                    lp->m_port.m_tx_pkt_ok++;
                }else{
                    lp->m_port.m_tx_pkt_err++;
                    rte_pktmbuf_free(m);
                }

            }
        }
    }
}

double CLatencyManager::grat_arp_timeout() {
    return (double)CGlobalInfo::m_options.m_arp_ref_per / m_arp_info.size();
}

void  CLatencyManager::add_grat_arp_src(COneIPv4Info &ip) {
    m_arp_info.insert(ip);
}

void CLatencyManager::send_one_grat_arp() {
    const COneIPInfo *ip_info;
    uint16_t port_id;
    CLatencyManagerPerPort * lp;
    rte_mbuf_t *m;
    uint8_t src_mac[ETHER_ADDR_LEN];
    uint16_t vlan;
    uint32_t sip;

    ip_info = m_arp_info.get_next();
    if (!ip_info)
        ip_info = m_arp_info.get_next();
    // Two times NULL means there are no addresses
    if (!ip_info)
        return;

    port_id = ip_info->get_port();
    lp = &m_ports[port_id];
    m = CGlobalInfo::pktmbuf_alloc_small(CGlobalInfo::m_socket.port_to_socket(port_id));
    assert(m);
    uint8_t *p = (uint8_t *)rte_pktmbuf_append(m, ip_info->get_grat_arp_len());
    ip_info->get_mac(src_mac);
    vlan = ip_info->get_vlan();
    switch(ip_info->ip_ver()) {
    case COneIPInfo::IP4_VER:
        sip = ((COneIPv4Info *)ip_info)->get_ip();
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, port_id, vlan);
        m->l2_len = 14 + (vlan ? 4 : 0);
        if (CGlobalInfo::m_options.preview.getVMode() >= 3) {
            printf("Sending gratuitous ARP on port %d vlan:%d, sip:%s\n", port_id, vlan
                   , ip_to_str(sip).c_str());
            utl_DumpBuffer(stdout, p, 60, 0);
        }
        if ( lp->m_io->tx(m) == 0 ) {
            lp->m_port.m_ign_stats.m_tx_arp++;
            lp->m_port.m_ign_stats.m_tot_bytes += 64; // mbuf size is smaller, but 64 bytes will be sent
        } else {
            lp->m_port.m_tx_pkt_err++;
            rte_pktmbuf_free(m);
        }
        break;
    case COneIPInfo::IP6_VER:
        //??? implement ipv6
        break;
    }
}

void  CLatencyManager::wait_for_rx_dump(){
    rte_mbuf_t * rx_pkts[64];
    int i;
    while ( true  ) {
        rte_pause();
        rte_pause();
        rte_pause();
        for (i=0; i<m_max_ports; i++) {
            CLatencyManagerPerPort * lp=&m_ports[i];
            rte_mbuf_t * m;
            uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
            if (cnt_p) {
                int j;
                for (j=0; j<cnt_p; j++) {
                    m=rx_pkts[j] ;
                    lp->m_port.dump_packet( m);
                    rte_pktmbuf_free(m);
                }
            } /*cnt_p*/
        }/* for*/
    }
}


void CLatencyManager::handle_rx_pkt(CLatencyManagerPerPort * lp,
                                    rte_mbuf_t * m){
    CRx_check_header *rxc = NULL;

#if 0
    /****************************************/
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
    utl_k12_pkt_format(stdout,p ,pkt_size) ;
    /****************************************/
#endif 

    lp->m_port.check_packet(m,rxc);
    if ( unlikely(rxc!=NULL) ){
        m_rx_check_manager.handle_packet(rxc);
    }

#if 0
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
    utl_DumpBuffer(stdout,p,pkt_size,0);
#endif

    rte_pktmbuf_free(m);
}

void  CLatencyManager::try_rx(){
    rte_mbuf_t * rx_pkts[64];
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        rte_mbuf_t * m;
        /* try to read 64 packets clean up the queue */
        uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
        if (cnt_p) {
            m_cpu_dp_u.start_work1();
            int j;
            for (j=0; j<cnt_p; j++) {
                m=rx_pkts[j] ;
                handle_rx_pkt(lp,m);
            }
            /* commit only if there was work to do ! */
            m_cpu_dp_u.commit1();
          }/* if work */
      }// all ports
}


void  CLatencyManager::reset(){

    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.reset();
    }

}


void CLatencyManager::tickle() {
    m_monitor.tickle();
}

#define RX_MSG_POOL_SEC (100.0/1000000.0)


void CLatencyManager::handle_pkt_msg(uint8_t thread_id, CGenNodeLatencyPktInfo * msg) {

    assert(msg->m_latency_offset==0xdead);

    uint8_t rx_port_index=(thread_id<<1)+(msg->m_dir&1);
    assert( rx_port_index <m_max_ports ) ;
    CLatencyManagerPerPort * lp=&m_ports[rx_port_index];
    handle_rx_pkt(lp,(rte_mbuf_t *)msg->m_pkt);
}


void CLatencyManager::handle_rx_one_queue(uint8_t ti,CNodeRing * r){

    while ( true ) {
         CGenNode * node;
         if ( r->Dequeue(node)!=0 ){
             break;
         }
         assert(node);
   
         CGenNodeMsgBase * msg=(CGenNodeMsgBase *)node;
   
         uint8_t   msg_type =  msg->m_msg_type;
         switch (msg_type ) {
         case CGenNodeMsgBase::LATENCY_PKT:
             handle_pkt_msg(ti,(CGenNodeLatencyPktInfo *) msg);
             break;
         default:
             printf("ERROR latency-thread message type is not valid %d \n",msg_type);
             assert(0);
         }
   
         CGlobalInfo::free_node(node);
    }
}

/* check for messages from DP cores */
void CLatencyManager::handle_rx_msgs(){
    /* dp->rx , rx->dp queues */
    CMessagingManager * rx_dp = CMsgIns::Ins()->getRxDp();
    uint8_t threads=CMsgIns::Ins()->get_num_threads();
    int ti;
    for (ti=0; ti<(int)threads; ti++) {
        CNodeRing * r = rx_dp->getRingDpToCp(ti);
        if ( !r->isEmpty() ){
            handle_rx_one_queue((uint8_t)ti,r);
        }
    }
}


void  CLatencyManager::start(int iter, bool activate_watchdog) {
    m_do_stop   = false;
    m_is_active = true;
    int cnt     = 0;

    double n_time;
    CGenNode * node = new CGenNode();
    node->m_type = CGenNode::FLOW_SYNC;   /* general stuff */
    node->m_time = now_sec()+0.007;
    m_p_queue.push(node);

    if (m_delta_sec > 0) {
        node = new CGenNode();
        node->m_type = CGenNode::FLOW_PKT; /* latency */
        node->m_time = now_sec(); /* 1/cps rate */
        m_p_queue.push(node);
    }

    if (CGlobalInfo::m_options.m_arp_ref_per > 0) {
        node = new CGenNode();
        node->m_type = CGenNode::GRAT_ARP; /* gratuitous ARP */
        node->m_time = now_sec() + (double) CGlobalInfo::m_options.m_arp_ref_per;
        m_p_queue.push(node);
    }

    node = new CGenNode();
    node->m_type = CGenNode::RX_MSG; /* gratuitous ARP */
    node->m_time = now_sec() + (double) RX_MSG_POOL_SEC;
    m_p_queue.push(node);

    if (activate_watchdog) {
        m_monitor.create("STF RX CORE", 1);
        TrexWatchDog::getInstance().register_monitor(&m_monitor);
    }

    while (  !m_p_queue.empty() ) {
        node = m_p_queue.top();
        n_time = node->m_time;

        /* wait for event */
        while ( true ) {
            double dt = now_sec() - n_time ;
            if (dt> (0.0)) {
                break;
            }
            try_rx();
            rte_pause_or_delay_lowend();
        }

        switch (node->m_type) {
        case CGenNode::FLOW_SYNC:

            tickle();

            if ( CGlobalInfo::is_learn_mode() ) {
                m_nat_check_manager.handle_aging();
            }

            m_p_queue.pop();
            node->m_time += SYNC_TIME_OUT;
            m_p_queue.push(node);

            break;
        case CGenNode::FLOW_PKT:
            m_cpu_dp_u.start_work1();
            send_pkt_all_ports();
            m_p_queue.pop();
            node->m_time += m_delta_sec;
            m_p_queue.push(node);
            m_cpu_dp_u.commit1();
            break;

        case CGenNode::GRAT_ARP:
            m_cpu_dp_u.start_work1();
            send_one_grat_arp();
            m_p_queue.pop();
            node->m_time += grat_arp_timeout();
            m_p_queue.push(node);
            m_cpu_dp_u.commit1();
            break;

        case CGenNode::RX_MSG:
            m_cpu_dp_u.start_work1();
            handle_rx_msgs();
            m_p_queue.pop();
            node->m_time += RX_MSG_POOL_SEC;
            m_p_queue.push(node);
            m_cpu_dp_u.commit1();
            break;

        }

        /* this will be called every sync which is 1msec */
        if ( m_do_stop ) {
             break;
        }
        if ( iter>0   ){
            if ( ( cnt>iter) ){
                printf("stop due iter %d\n",iter);
                break;
            }
        }
        cnt++;
    }

    /* free all nodes in the queue */
    while (!m_p_queue.empty()) {
        node = m_p_queue.top();
        m_p_queue.pop();
        delete node;
    }

    printf(" latency daemon has stopped\n");
    if ( get_is_rx_check_mode() ) {
        m_rx_check_manager.tw_drain();
    }

    /* disable the monitor */
    if (activate_watchdog) {
        m_monitor.disable();
    }
    
    m_is_active = false;

}

void  CLatencyManager::stop(){
    m_do_stop = true;
}

bool  CLatencyManager::is_active(){
    return (m_is_active);
}

// return the statistics we want to ignore for port port_id
// stat - hold values we return.
// if get_diff is true, return diff from last read. Else return total.
void CLatencyManager::get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff) {
    CLatencyManagerPerPort * lp = &m_ports[port_id];
    CRXCoreIgnoreStat temp;
    temp = lp->m_port.m_ign_stats;
    if (get_diff) {
        stat = temp - lp->m_port.m_ign_stats_prev;
        lp->m_port.m_ign_stats_prev = temp;
    } else {
        stat = lp->m_port.m_ign_stats;
    }
}

double CLatencyManager::get_max_latency(){
    double l=0.0;
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        if ( l <lp->m_port.m_hist.get_max_latency() ){
            l=lp->m_port.m_hist.get_max_latency();
        }
    }
    return (l);
}

double CLatencyManager::get_avr_latency(){
    double l=0.0;
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        if ( l <lp->m_port.m_hist.get_average_latency() ){
            l=lp->m_port.m_hist.get_average_latency();
        }
    }
    return (l);
}

uint64_t CLatencyManager::get_total_pkt(){
    int i;
    uint64_t t=0;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        t+=lp->m_port.m_tx_pkt_ok ;
    }
    return  t;
}

uint64_t CLatencyManager::get_total_bytes(){
    int i;
    uint64_t t=0;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        t+=lp->m_port.m_tx_pkt_ok* (m_pkt_gen.get_pkt_size()+4);
    }
    return  t;

}


bool   CLatencyManager::is_any_error(){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        if ( lp->m_port.is_any_err() ){
            return (true);
        }
    }
    return  (false);
}


void CLatencyManager::dump_json(std::string & json ){
    json="{\"name\":\"trex-latecny\",\"type\":0,\"data\":{";
    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.dump_json(json);
    }

    json+="\"unknown\":0}}"  ;

}

void CLatencyManager::dump_json_v2(std::string & json ){
    json="{\"name\":\"trex-latecny-v2\",\"type\":0,\"data\":{";
    json+=add_json("cpu_util",m_cpu_cp_u.GetVal());

    int i;
    for (i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.dump_json_v2(json);
    }

    json+="\"unknown\":0}}"  ;

}

void CLatencyManager::DumpRxCheck(FILE *fd){
    if ( get_is_rx_check_mode() ) {
        fprintf(fd," rx checker : \n");
        m_rx_check_manager.DumpShort(fd);
        m_rx_check_manager.Dump(fd);
    }
}

void CLatencyManager::DumpShortRxCheck(FILE *fd){
    if ( get_is_rx_check_mode() ) {
        m_rx_check_manager.DumpShort(fd);
    }
}

void CLatencyManager::dump_nat_flow_table(FILE *fd) {
    m_nat_check_manager.Dump(fd);
}

void CLatencyManager::rx_check_dump_json(std::string & json){
    if ( get_is_rx_check_mode() ) {
        m_rx_check_manager.dump_json(json );
    }
}


void CLatencyManager::update_cpu_util(){
    m_cpu_cp_u.Update() ;
}

double CLatencyManager::get_cpu_util() {
    return m_cpu_cp_u.GetVal();
}

void CLatencyManager::update(){
    for (int i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.m_hist.update();
    }
}

void CLatencyManager::DumpShort(FILE *fd){
    int i;
    fprintf(fd," Cpu Utilization : %2.1f %%  \n",m_cpu_cp_u.GetVal());
    CCPortLatency::DumpShortHeader(fd);
    for (i=0; i<m_max_ports; i++) {
        fprintf(fd," %d | ",i);
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.DumpShort(fd);
        fprintf(fd,"\n");
    }


}

void CLatencyManager::Dump(FILE *fd){
    int i;
    fprintf(fd," cpu : %2.1f  %% \n",m_cpu_cp_u.GetVal());
    for (i=0; i<m_max_ports; i++) {
        fprintf(fd," port   %d \n",i);
        fprintf(fd," -----------------\n");
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.DumpCounters(fd);
    }
}

void CLatencyManager::DumpRxCheckVerification(FILE *fd,
                                              uint64_t total_tx_rx_check){
    if ( !get_is_rx_check_mode() ) {
        fprintf(fd," rx_checker is disabled  \n");
        return;
    }
    fprintf(fd," rx_check Tx : %llu \n", (unsigned long long)total_tx_rx_check);
    fprintf(fd," rx_check Rx : %llu \n", (unsigned long long)m_rx_check_manager.getTotalRx() );
    fprintf(fd," rx_check verification :" );
    if (m_rx_check_manager.getTotalRx() == total_tx_rx_check) {
        fprintf(fd," OK \n" );
    }else{
        fprintf(fd," FAIL \n" );
    }
}

uint8_t CLatencyPktModeICMP::getPacketLen() {return sizeof(icmp_pkt);}
const uint8_t *CLatencyPktModeICMP::getPacketData() {return icmp_pkt;}
void CLatencyPktModeICMP::rcv_debug_print(uint8_t *pkt) {
    ICMPHeader *m_icmp = (ICMPHeader *)pkt;
    printf ("received latency ICMP packet code:%d seq:%x\n"
            , m_icmp->getType(), m_icmp->getSeqNum());
};

void CLatencyPktModeICMP::send_debug_print(uint8_t *pkt) {
    ICMPHeader *m_icmp = (ICMPHeader *)pkt;
    printf ("Sending latency ICMP packet code:%d seq:%x\n", m_icmp->getType(), m_icmp->getSeqNum());
}

void CLatencyPktModeICMP::update_pkt(uint8_t *pkt, bool is_client_to_server, uint16_t l4_len, uint16_t *tx_seq) {
    ICMPHeader * m_icmp =(ICMPHeader *)(pkt);

    if (m_submode == L_PKT_SUBMODE_0_SEQ) {
        m_icmp->setSeqNum(0);
    } else {
        m_icmp->setSeqNum(*tx_seq);
        (*tx_seq)++;
    }

    if ((!is_client_to_server) && (m_submode == L_PKT_SUBMODE_REPLY)) {
      m_icmp->setType(0); // echo reply
    } else {
      m_icmp->setType(8); // echo request
    }
    // ICMP checksum is calculated on payload + ICMP header
    m_icmp->updateCheckSum(l4_len);

}

bool CLatencyPktModeICMP::IsLatencyPkt(IPHeader *ip) {
    if (!ip)
        return false;
    if (ip->getProtocol() != 0x1)
        return false;
    return true;
};

void CLatencyPktModeICMP::update_recv(uint8_t *pkt, uint16_t *r_seq, uint16_t *t_seq) {
    ICMPHeader *m_icmp = (ICMPHeader *)(pkt);
    *r_seq = m_icmp->getSeqNum();
    // Previously, we assumed we can send for sequences smaller than r_seq.
    // Actually, if the DUT (firewall) dropped an ICMP request, we should not send response for the dropped packet.
    // We are only sure that we can send reqponse for the request we just got.
    // This should be OK, since we send requests and responses in the same rate.
    *t_seq = *r_seq;
}


uint8_t CLatencyPktModeSCTP::getPacketLen() {return sizeof(sctp_pkt);}
const uint8_t *CLatencyPktModeSCTP::getPacketData() {return sctp_pkt;}
void CLatencyPktModeSCTP::rcv_debug_print(uint8_t *pkt) {printf("Received latency SCTP packet\n");}
void CLatencyPktModeSCTP::send_debug_print(uint8_t *pkt) {printf("Sending latency SCTP packet\n");
    // utl_DumpBuffer(stdout,pkt-20,28,0);
}
void CLatencyPktModeSCTP::update_pkt(uint8_t *pkt, bool is_client_to_server, uint16_t l4_len, uint16_t *tx_seq) {}
bool CLatencyPktModeSCTP::IsLatencyPkt(IPHeader *ip) {
    if (!ip) {
        return false;
    }
    if (ip->getProtocol() != 0x84) {
        return false;
    }
    return true;
};
void CLatencyPktModeSCTP::update_recv(uint8_t *pkt, uint16_t *r_seq, uint16_t *t_seq) {}
