/*
  Itay Marom
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
#include "bp_sim.h"
#include "trex_stateless_rx_port_mngr.h"
#include "trex_stateless_rx_core.h"
#include "common/Network/Packet/Arp.h"
#include "common/Network/Packet/VLANHeader.h"
#include "pkt_gen.h"
#include "trex_stateless_capture.h"
#include "stateless/cp/trex_stateless.h"

/**************************************
 * latency RX feature
 * 
 *************************************/
RXLatency::RXLatency() {
    m_rcv_all    = false;
    m_rfc2544    = NULL;
    m_err_cntrs  = NULL;

    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        m_rx_pg_stat[i].clear();
    }
    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rx_pg_stat_payload[i].clear();
    }
}

void
RXLatency::create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs) {
    m_rfc2544   = rfc2544;
    m_err_cntrs = err_cntrs;
    if ((CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_ONE_QUEUE)
        || (CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_RSS)) {
        m_rcv_all    = true;
    } else {
        m_rcv_all    = false;
    }

    uint16_t num_counters, cap, ip_id_base;
    TrexStateless *tstateless = get_stateless_obj();
    assert(tstateless);

    const TrexPlatformApi *api = tstateless->get_platform_api();
    assert(api);
    api->get_port_stat_info(0, num_counters, cap, ip_id_base);
    m_ip_id_base = ip_id_base;
}

void 
RXLatency::handle_pkt(const rte_mbuf_t *m) {
    uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
    CFlowStatParser parser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
    int ret = parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len);

    if (m_rcv_all ||  (ret == 0)) {
        uint32_t ip_id = 0;
        int ret2 = parser.get_ip_id(ip_id);
        if (m_rcv_all || ( ret2 == 0)) {
            if (m_rcv_all || is_flow_stat_id(ip_id)) {
                uint16_t hw_id;
                if (is_flow_stat_payload_id(ip_id) || ((! is_flow_stat_id(ip_id)) && m_rcv_all)) {
                    bool good_packet = true;
                    struct flow_stat_payload_header *fsp_head = (flow_stat_payload_header *)
                        utl_rte_pktmbuf_get_last_bytes(m, sizeof(struct flow_stat_payload_header), tmp_buf);
                    hw_id = fsp_head->hw_id;
                    CRFC2544Info *curr_rfc2544;

                    if (unlikely(fsp_head->magic != FLOW_STAT_PAYLOAD_MAGIC) || hw_id >= MAX_FLOW_STATS_PAYLOAD) {
                        good_packet = false;
                        if (!m_rcv_all)
                            m_err_cntrs->m_bad_header++;
                    } else {
                        curr_rfc2544 = &m_rfc2544[hw_id];

                        if (fsp_head->flow_seq != curr_rfc2544->get_exp_flow_seq()) {
                            // bad flow seq num
                            // Might be the first packet of a new flow, packet from an old flow, or garbage.

                            if (fsp_head->flow_seq == curr_rfc2544->get_prev_flow_seq()) {
                                // packet from previous flow using this hw_id that arrived late
                                good_packet = false;
                                m_err_cntrs->m_old_flow++;
                            } else {
                                if (curr_rfc2544->no_flow_seq()) {
                                    // first packet we see from this flow
                                    good_packet = true;
                                    curr_rfc2544->set_exp_flow_seq(fsp_head->flow_seq);
                                } else {
                                    // garbage packet
                                    good_packet = false;
                                    m_err_cntrs->m_bad_header++;
                                }
                            }
                        }
                    }

                    if (good_packet) {
                        uint32_t pkt_seq = fsp_head->seq;
                        uint32_t exp_seq = curr_rfc2544->get_seq();
                        if (unlikely(pkt_seq != exp_seq)) {
                            if (pkt_seq < exp_seq) {
                                if (exp_seq - pkt_seq > 100000) {
                                    // packet loss while we had wrap around
                                    curr_rfc2544->inc_seq_err(pkt_seq - exp_seq);
                                    curr_rfc2544->inc_seq_err_too_big();
                                    curr_rfc2544->set_seq(pkt_seq + 1);
#ifdef LAT_DEBUG
                                    printf("magic:%x flow_seq:%x hw_id:%x seq %x exp %x\n"
                                           , fsp_head->magic, fsp_head->flow_seq, fsp_head->hw_id, pkt_seq
                                           , exp_seq);
                                    utl_DumpBuffer(stdout, rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len, 0);
#endif
                                } else {
                                    if (pkt_seq == (exp_seq - 1)) {
                                        curr_rfc2544->inc_dup();
                                    } else {
                                        curr_rfc2544->inc_ooo();
                                        // We thought it was lost, but it was just out of order
                                        curr_rfc2544->dec_seq_err();
                                    }
                                    curr_rfc2544->inc_seq_err_too_low();
                                }
                            } else {
                                if (unlikely (pkt_seq - exp_seq > 100000)) {
                                    // packet reorder while we had wrap around
                                    if (pkt_seq == (exp_seq - 1)) {
                                        curr_rfc2544->inc_dup();
                                    } else {
                                        curr_rfc2544->inc_ooo();
                                        // We thought it was lost, but it was just out of order
                                        curr_rfc2544->dec_seq_err();
                                    }
                                    curr_rfc2544->inc_seq_err_too_low();
                                } else {
                                // seq > curr_rfc2544->seq. Assuming lost packets
                                    curr_rfc2544->inc_seq_err(pkt_seq - exp_seq);
                                    curr_rfc2544->inc_seq_err_too_big();
                                    curr_rfc2544->set_seq(pkt_seq + 1);
#ifdef LAT_DEBUG
                                    printf("2:seq %d exp %d\n", exp_seq, pkt_seq);
#endif
                                }
                            }
                        } else {
                            curr_rfc2544->set_seq(pkt_seq + 1);
                        }
                        m_rx_pg_stat_payload[hw_id].add_pkts(1);
                        m_rx_pg_stat_payload[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                        uint64_t d = (os_get_hr_tick_64() - fsp_head->time_stamp );
                        dsec_t ctime = ptime_convert_hr_dsec(d);
                        curr_rfc2544->add_sample(ctime);
                    }
                } else {
                    hw_id = get_hw_id((uint16_t)ip_id);
                    if (hw_id < MAX_FLOW_STATS) {
                        m_rx_pg_stat[hw_id].add_pkts(1);
                        m_rx_pg_stat[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                    }
                }
            }
        }
    }
}

void
RXLatency::reset_stats() {
    for (int hw_id = 0; hw_id < MAX_FLOW_STATS; hw_id++) {
        m_rx_pg_stat[hw_id].clear();
    }
}


void
RXLatency::get_stats(rx_per_flow_t *rx_stats,
                     int min,
                     int max,
                     bool reset,
                     TrexPlatformApi::driver_stat_cap_e type) {
    
    for (int hw_id = min; hw_id <= max; hw_id++) {
        if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
            rx_stats[hw_id - min] = m_rx_pg_stat_payload[hw_id];
        } else {
            rx_stats[hw_id - min] = m_rx_pg_stat[hw_id];
        }
        if (reset) {
            if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                m_rx_pg_stat_payload[hw_id].clear();
            } else {
                m_rx_pg_stat[hw_id].clear();
            }
        }
    }
}


Json::Value
RXLatency::to_json() const {
    return Json::objectValue;
}

/**************************************
 * RX feature queue 
 * 
 *************************************/

void
RXQueue::start(uint64_t size) {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
    }
    m_pkt_buffer = new TrexPktBuffer(size, TrexPktBuffer::MODE_DROP_HEAD);
}

void
RXQueue::stop() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
        m_pkt_buffer = NULL;
    }
}

const TrexPktBuffer *
RXQueue::fetch() {

    /* if no buffer or the buffer is empty - give a NULL one */
    if ( (!m_pkt_buffer) || (m_pkt_buffer->get_element_count() == 0) ) {
        return nullptr;
    }
    
    /* hold a pointer to the old one */
    TrexPktBuffer *old_buffer = m_pkt_buffer;

    /* replace the old one with a new one and freeze the old */
    m_pkt_buffer = new TrexPktBuffer(old_buffer->get_capacity(), old_buffer->get_mode());

    return old_buffer;
}

void
RXQueue::handle_pkt(const rte_mbuf_t *m) {
    m_pkt_buffer->push(m);
}

Json::Value
RXQueue::to_json() const {
    assert(m_pkt_buffer != NULL);
    
    Json::Value output = Json::objectValue;
    
    output["size"]    = Json::UInt64(m_pkt_buffer->get_capacity());
    output["count"]   = Json::UInt64(m_pkt_buffer->get_element_count());
    
    return output;
}


/**************************************
 * RX feature server (ARP, ICMP) and etc.
 * 
 *************************************/

class RXPktParser {
public:
    RXPktParser(const rte_mbuf_t *m) {

        m_mbuf = m;
        
        /* start point */
        m_current   = rte_pktmbuf_mtod(m, uint8_t *);;
        m_size_left = rte_pktmbuf_pkt_len(m);
        
        m_ether    = NULL;
        m_arp      = NULL;
        m_ipv4     = NULL;
        m_icmp     = NULL;
        
        /* parse L2 (stripping VLANs) */
        uint16_t next_proto = parse_l2();
        
        /**
         * support only for ARP or IPv4 based protocols
         */
        switch (next_proto) {
        case EthernetHeader::Protocol::ARP:
            parse_arp();
            return;
            
        case EthernetHeader::Protocol::IP:
            parse_ipv4();
            return;
            
        default:
            return;
        }
        
    }
    
    const rte_mbuf_t         *m_mbuf;
    EthernetHeader           *m_ether;
    ArpHdr                   *m_arp;
    IPHeader                 *m_ipv4;
    ICMPHeader               *m_icmp;
    std::vector<uint16_t>     m_vlan_ids;
    
protected:
    
    /**
     * parse L2 header 
     * returns the payload protocol (VLANs stripped)
     */
    uint16_t parse_l2() {
        /* ethernet */
        m_ether = (EthernetHeader *)parse_bytes(14);
        
        uint16_t next_proto = m_ether->getNextProtocol();
     
        while (true) {
            switch (next_proto) {
            case EthernetHeader::Protocol::QINQ:
            case EthernetHeader::Protocol::VLAN:
                {
                    VLANHeader *vlan_hdr = (VLANHeader *)parse_bytes(4);
                    m_vlan_ids.push_back(vlan_hdr->getVlanTag());
    
                    next_proto = vlan_hdr->getNextProtocolHostOrder();
                    
                }
                break;
                
            default:
                /* break */
                return next_proto;
            }                            
        }
        
    }
    
    
    const uint8_t *parse_bytes(uint32_t size) {
        if (m_size_left < size) {
            parse_err();
        }
        
        const uint8_t *p = m_current;
        m_current    += size;
        m_size_left  -= size;
        
        return p;
    }
    
    void parse_arp() {
        m_arp = (ArpHdr *)parse_bytes(sizeof(ArpHdr));
    }
    
    void parse_ipv4() {
        m_ipv4 = (IPHeader *)parse_bytes(IPHeader::DefaultSize);
        
        /* advance over IP options if exists */
        parse_bytes(m_ipv4->getOptionLen());
        
        switch (m_ipv4->getNextProtocol()) {
        case IPHeader::Protocol::ICMP:
            parse_icmp();
            return;
            
        default:
            return;
        }
    }
    
    void parse_icmp() {
        m_icmp = (ICMPHeader *)parse_bytes(sizeof(ICMPHeader));
    }
    
    void parse_err() {
        throw TrexException(TrexException::T_RX_PKT_PARSE_ERR);
    }
    
    const uint8_t *m_current;
    uint16_t       m_size_left;
};

RXServer::RXServer() {
    m_io        = NULL;
    m_vlan_cfg  = NULL;
    m_src_addr  = NULL;
    m_port_id   = 255;
}

void
RXServer::create(uint8_t port_id,
                 CPortLatencyHWBase *io,
                 const CManyIPInfo *src_addr,
                 const VLANConfig *vlan_cfg) {
    
    m_port_id  = port_id;
    m_io       = io;
    m_vlan_cfg = vlan_cfg;
    m_src_addr = src_addr;
}


void
RXServer::handle_pkt(const rte_mbuf_t *m) {
    try {
        
        RXPktParser parser(m);
        
        /* verify that packet matches the port VLAN config */
        if (!m_vlan_cfg->in_vlan(parser.m_vlan_ids)) {
            return;
        }
        
        if (parser.m_icmp) {
            handle_icmp(parser);
        } else if (parser.m_arp) {
            handle_arp(parser);
        } else {
            return;
        }
        
    } catch (const TrexException &e) {
        return;
    }

}

void
RXServer::handle_icmp(RXPktParser &parser) {
    
    /* maybe not for us... */
    const COneIPv4Info *ipv4_info = (const COneIPv4Info *)m_src_addr->get_first();
    if (!ipv4_info) {
        return;
    }
    
    uint32_t sip = ipv4_info->get_ip();
    if (parser.m_ipv4->getDestIp() != sip) {
        return;
    }
    
    /* we handle only echo request */
    if (parser.m_icmp->getType() != ICMPHeader::TYPE_ECHO_REQUEST) {
        return;
    }
    
    /* duplicate the MBUF */
    rte_mbuf_t *response = duplicate_mbuf(parser.m_mbuf);
    if (!response) {
        return;
    }
    
    /* reparse the cloned packet */
    RXPktParser response_parser(response);
    
    /* swap MAC */
    MacAddress tmp = response_parser.m_ether->mySource;
    response_parser.m_ether->mySource = response_parser.m_ether->myDestination;
    response_parser.m_ether->myDestination = tmp;
    
    /* swap IP */
    std::swap(response_parser.m_ipv4->mySource, response_parser.m_ipv4->myDestination);
    
    /* new packet - new TTL */
    response_parser.m_ipv4->setTimeToLive(128);
    response_parser.m_ipv4->updateCheckSum();
    
    /* update type and fix checksum */
    response_parser.m_icmp->setType(ICMPHeader::TYPE_ECHO_REPLY);
    response_parser.m_icmp->updateCheckSum(response_parser.m_ipv4->getTotalLength() - response_parser.m_ipv4->getHeaderLength());
    
    /* send */
    TrexStatelessCaptureMngr::getInstance().handle_pkt_tx(response, m_port_id);
    m_io->tx_raw(response);
}

void
RXServer::handle_arp(RXPktParser &parser) {
    MacAddress src_mac;
    
    /* only ethernet format supported */
    if (parser.m_arp->getHrdType() != ArpHdr::ARP_HDR_HRD_ETHER) {
        return;
    }
    
    /* IPV4 only */    
    if (parser.m_arp->getProtocolType() != ArpHdr::ARP_HDR_PROTO_IPV4) {
        return;
    }

    /* support only for ARP request */
    if (parser.m_arp->getOp() != ArpHdr::ARP_HDR_OP_REQUEST) {
        return;
    }
    
    /* are we the target ? if not - go home */
    if (!m_src_addr->lookup(parser.m_arp->getTip(), 0, src_mac)) {
        return;
    }
    
    /* duplicate the MBUF */
    rte_mbuf_t *response = duplicate_mbuf(parser.m_mbuf);
    if (!response) {
        return;
    }
 
    /* reparse the cloned packet */
    RXPktParser response_parser(response);
    
    /* reply */
    response_parser.m_arp->setOp(ArpHdr::ARP_HDR_OP_REPLY);
    
    /* fix the MAC addresses */
    response_parser.m_ether->mySource = src_mac;
    response_parser.m_ether->myDestination = parser.m_ether->mySource;
    
    /* fill up the fields */
    
    /* src */
    response_parser.m_arp->m_arp_sha = src_mac;
    response_parser.m_arp->setSip(parser.m_arp->getTip());
    
    /* dst */
    response_parser.m_arp->m_arp_tha = parser.m_arp->m_arp_sha;
    response_parser.m_arp->m_arp_tip = parser.m_arp->m_arp_sip;
    
    /* send */
    TrexStatelessCaptureMngr::getInstance().handle_pkt_tx(response, m_port_id);
    m_io->tx_raw(response);
    
}

rte_mbuf_t *
RXServer::duplicate_mbuf(const rte_mbuf_t *m) {
    
    /* allocate */
    rte_mbuf_t *clone_mbuf = CGlobalInfo::pktmbuf_alloc_by_port(m_port_id, rte_pktmbuf_pkt_len(m));
    if (!clone_mbuf) {
        return NULL;
    }
    
    /* append data */
    uint8_t *dest = (uint8_t *)rte_pktmbuf_append(clone_mbuf, rte_pktmbuf_pkt_len(m));
    if (!dest) {
        return NULL;
    }
    
    /* copy data */
    mbuf_to_buffer(dest, m);
    
    return clone_mbuf;
}

/**************************************
 * Gratidious ARP
 * 
 *************************************/
void
RXGratARP::create(uint8_t port_id,
                  CPortLatencyHWBase *io,
                  CManyIPInfo *src_addr,
                  CRXCoreIgnoreStat *ign_stats,
                  const VLANConfig *vlan_cfg) {
    
    m_port_id     = port_id;
    m_io          = io;
    m_src_addr    = src_addr;
    m_ign_stats   = ign_stats;
    m_vlan_cfg    = vlan_cfg;
}

void
RXGratARP::send_next_grat_arp() {
    uint8_t src_mac[ETHER_ADDR_LEN];
    
    const COneIPInfo *ip_info = m_src_addr->get_next_loop();
    if (!ip_info) {
        return;
    }
    
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_small(CGlobalInfo::m_socket.port_to_socket(m_port_id));
    assert(m);
    
    uint8_t *p = (uint8_t *)rte_pktmbuf_append(m, ip_info->get_grat_arp_len());
    ip_info->get_mac(src_mac);
    
    /* for now only IPv4 */
    assert(ip_info->ip_ver() == COneIPInfo::IP4_VER);
    uint32_t sip = ((COneIPv4Info *)ip_info)->get_ip();
    
    /* generate ARP request according to the VLAN configuration */
    switch (m_vlan_cfg->count()) {
    case 0:
        /* no VLAN */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, m_port_id);
        break;
        
    case 1:
        /* single VLAN */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, m_port_id, m_vlan_cfg->m_tags[0]);
        break;
        
    case 2:
        /* QinQ */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, m_port_id, m_vlan_cfg->m_tags[0], m_vlan_cfg->m_tags[1]);
        break;
    }

    TrexStatelessCaptureMngr::getInstance().handle_pkt_tx(m, m_port_id);
    if (m_io->tx_raw(m) == 0) {
        m_ign_stats->m_tx_arp    += 1;
        m_ign_stats->m_tot_bytes += 64;
    }
    
}

Json::Value
RXGratARP::to_json() const {
    Json::Value output = Json::objectValue;
    output["interval_sec"] = (double)CGlobalInfo::m_options.m_arp_ref_per;
    
    return output;
}

/**************************************
 * Port manager 
 * 
 *************************************/

RXPortManager::RXPortManager() {
    clear_all_features();
    m_io          = NULL;
    m_cpu_dp_u    = NULL;
    m_port_id     = UINT8_MAX;
}


void
RXPortManager::create(const TRexPortAttr *port_attr,
                      CPortLatencyHWBase *io,
                      CRFC2544Info *rfc2544,
                      CRxCoreErrCntrs *err_cntrs,
                      CCpuUtlDp *cpu_util) {
    
    m_port_id = port_attr->get_port_id();
    m_io = io;
    m_cpu_dp_u = cpu_util;
    
    /* init features */
    m_latency.create(rfc2544, err_cntrs);
    m_server.create(m_port_id, io, &m_src_addr, &m_vlan_cfg);
    m_grat_arp.create(m_port_id, io, &m_src_addr, &m_ign_stats, &m_vlan_cfg);
    
    /* by default, server is always on */
    set_feature(SERVER);
}
    
void RXPortManager::handle_pkt(const rte_mbuf_t *m) {

    /* handle features */

    if (is_feature_set(LATENCY)) {
        m_latency.handle_pkt(m);
    }

    if (is_feature_set(QUEUE)) {
        m_queue.handle_pkt(m);
    }
    
    if (is_feature_set(SERVER)) {
        m_server.handle_pkt(m);
    }
    
    /* capture */
    TrexStatelessCaptureMngr::getInstance().handle_pkt_rx(m, m_port_id);
}

int RXPortManager::process_all_pending_pkts(bool flush_rx) {

    rte_mbuf_t *rx_pkts[64];

    /* try to read 64 packets clean up the queue */
    uint16_t cnt_p = m_io->rx_burst(rx_pkts, 64);
    if (cnt_p == 0) {
        return cnt_p;
    }


    m_cpu_dp_u->start_work1();

    for (int j = 0; j < cnt_p; j++) {
        rte_mbuf_t *m = rx_pkts[j];

        if (!flush_rx) {
            handle_pkt(m);
        }

        rte_pktmbuf_free(m);
    }

    /* commit only if there was work to do ! */
    m_cpu_dp_u->commit1();


    return cnt_p;
}

void
RXPortManager::send_next_grat_arp() {
    if (is_feature_set(GRAT_ARP)) {
        m_grat_arp.send_next_grat_arp();
    }
}

  
void
RXPortManager::set_l2_mode() {
        
    /* no IPv4 addresses */
    m_src_addr.clear();
        
    /* stop grat arp */
    stop_grat_arp();     
}

void
RXPortManager::set_l3_mode(const CManyIPInfo &ip_info, bool is_grat_arp_needed) {
        
    /* copy L3 address */
    m_src_addr = ip_info;
    
    if (is_grat_arp_needed) {
        start_grat_arp();
    }
    else {
        stop_grat_arp();
    }
    
}


    
Json::Value
RXPortManager::to_json() const {
    Json::Value output = Json::objectValue;
    
    if (is_feature_set(LATENCY)) {
        output["latency"] = m_latency.to_json();
        output["latency"]["is_active"] = true;
    } else {
        output["latency"]["is_active"] = false;
    }

    if (is_feature_set(QUEUE)) {
        output["queue"] = m_queue.to_json();
        output["queue"]["is_active"] = true;
    } else {
        output["queue"]["is_active"] = false;
    }
 
    if (is_feature_set(GRAT_ARP)) {
        output["grat_arp"] = m_grat_arp.to_json();
        output["grat_arp"]["is_active"] = true;
    } else {
        output["grat_arp"]["is_active"] = false;
    }
    
    return output;
}


void RXPortManager::get_ignore_stats(CRXCoreIgnoreStat &stat, bool get_diff) {
    if (get_diff) {
        stat = m_ign_stats - m_ign_stats_prev;
        m_ign_stats_prev = m_ign_stats;
    } else {
        stat = m_ign_stats;
    }
}

