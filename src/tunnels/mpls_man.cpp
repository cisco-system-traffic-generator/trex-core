#include "mpls_man.h"
#include "common/Network/Packet/MPLSHeader.h"
#include "common/Network/Packet/EthernetHeader.h"
#include "common/Network/Packet/VLANHeader.h"
#include "common/Network/Packet/TcpHeader.h"
#include "common/Network/Packet/UdpHeader.h"
#include "rpc-server/trex_rpc_cmd_api.h"
#include "stx/common/trex_rx_rpc_tunnel.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "bp_sim.h"
#define DEST_PORT 2152
#define IPV4_HDR_TTL 64
#define HOP_LIMIT 255

static const meta_data_t mpls_meta_data ={"MPLS", 
                                         {CMetaDataPerCounter("m_tunnel_encapsulated", "num of encapsulated pkts"),
                                          CMetaDataPerCounter("m_tunnel_decapsulated", "num of decapsulated pkts"),
                                          CMetaDataPerCounter("m_err_tunnel_drops", "num of pkts drop", "", scERROR),
                                          CMetaDataPerCounter("m_err_tunnel_no_context", "num of pkts without ctx", "", scERROR),
                                          CMetaDataPerCounter("m_err_tunnel_dir", "wrong pkt direction", "", scERROR),
                                          CMetaDataPerCounter("m_err_pkt_null", "num of null pkts", "", scERROR),
                                          CMetaDataPerCounter("m_err_buf_addr_null", "num of pkts with null buf addr", "", scERROR),
                                          CMetaDataPerCounter("m_err_l3_hdr", "num of pkts with wrong l3 hdr", "", scERROR),
                                          CMetaDataPerCounter("m_err_headroom", "num of pkts with small headroom", "", scERROR),
                                          CMetaDataPerCounter("m_err_l4_hdr", "num of pkts with wrong l4 hdr", "", scERROR),
                                          CMetaDataPerCounter("m_err_buf_len", "num of pkts with small buf len", "", scERROR),
                                          CMetaDataPerCounter("m_err_dst_port", "num of pkts with wrong destination port", "", scERROR),
                                          CMetaDataPerCounter("m_err_data_len", "num of pkts with small data len", "", scERROR),
                                          CMetaDataPerCounter("m_encapsulated_hdr", "num of pkts with wrong encapsulated hdr", "", scERROR)
                                         }
                                        };

void del_mpls_ctx(void *tunnel_handler, void *tunnel_ctx) {
    if (tunnel_handler!=nullptr && tunnel_ctx != nullptr) {
        ((CMplsMan*)tunnel_handler)->delete_tunnel_ctx(tunnel_ctx);
    }
}

CMplsMan::CMplsMan(uint8_t mode) : CTunnelHandler(mode) {
    for(int i=0; i<TCP_CS_NUM; i++) {
        m_total_sts[i].clear();
    }
}

int CMplsMan::on_tx(uint8_t dir, rte_mbuf *pkt) {
    assert((m_mode & TUNNEL_MODE_TX) == TUNNEL_MODE_TX);
    if (dir != m_port_id && !(m_mode & TUNNEL_MODE_LOOPBACK)) {
        m_total_sts[dir].sts.m_err_tunnel_dir++;
        m_total_sts[dir].sts.m_err_tunnel_drops++;
        return -1;
    }
    tunnel_cntxt_t tunnel_context = pkt->dynfield_ptr;
    if (!tunnel_context){
        m_total_sts[dir].sts.m_err_tunnel_no_context++;
        m_total_sts[dir].sts.m_err_tunnel_drops++;
        return -1;
    }
    int res = prepend(pkt, dir);
    if (res) {
        m_total_sts[dir].sts.m_err_tunnel_drops++;
    }
    return res;
}

int CMplsMan::on_rx(uint8_t dir, rte_mbuf *pkt) {
    assert((m_mode & TUNNEL_MODE_RX) == TUNNEL_MODE_RX);
    if (dir != m_port_id && !(m_mode & TUNNEL_MODE_LOOPBACK)) {
        m_total_sts[dir].sts.m_err_tunnel_dir++;
        m_total_sts[dir].sts.m_err_tunnel_drops++;
        return -1;
    }
    int res = adjust(pkt, dir);
    if (res) {
        m_total_sts[dir].sts.m_err_tunnel_drops++;
    }
    return res;
}

void* CMplsMan::get_tunnel_ctx(client_tunnel_data_t *data) {
    assert((m_mode & TUNNEL_MODE_DP) == TUNNEL_MODE_DP);
    void* mpls = NULL;
    mpls = (void*) new CMplsCtx(data->label, data->tc, data->s, data->ttl);
    return mpls;
}

const meta_data_t* CMplsMan::get_meta_data() {
    return &mpls_meta_data;
}

dp_sts_t CMplsMan::get_counters() {
    return std::make_pair((uint64_t*)&m_total_sts[TCP_CLIENT_SIDE].sts, (uint64_t*)&m_total_sts[TCP_SERVER_SIDE].sts);
}

void CMplsMan::delete_tunnel_ctx(void *tunnel_context) {
    if (tunnel_context) {
        delete ((CMplsCtx *)tunnel_context);
    }
}

tunnel_type CMplsMan::get_tunnel_type() {
    return TUNNEL_TYPE_MPLS;
}

string CMplsMan::get_tunnel_type_str() {
    return "MPLS";
}

void CMplsMan::update_tunnel_ctx(client_tunnel_data_t *data, void *tunnel_context) {
    assert((m_mode & TUNNEL_MODE_DP) == TUNNEL_MODE_DP);
    CMplsCtx* mpls = (CMplsCtx *)tunnel_context;
    mpls->update_label_mpls_info(data->label);
    mpls->update_tc_mpls_info(data->tc);
    mpls->update_s_mpls_info(data->s);
    mpls->update_ttl_mpls_info(data->ttl);
}

void CMplsMan::parse_tunnel(const Json::Value &params, Json::Value &result, std::vector<client_tunnel_data_t> &all_msg_data) {
    assert((m_mode & TUNNEL_MODE_CP) == TUNNEL_MODE_CP);
    CRpcTunnelBatch parser;
    const Json::Value &attr = parser.parse_array(params, "attr", result);

    for (auto each_client : attr) {
        client_tunnel_data_t msg_data;

        msg_data.label = parser.parse_int(each_client, "label", result);
        msg_data.tc = parser.parse_int(each_client, "tc", result);
        msg_data.s = parser.parse_bool(each_client, "s", result);
        msg_data.ttl = parser.parse_int(each_client, "ttl", result);
        all_msg_data.push_back(msg_data);
    }
}

bool CMplsMan::is_tunnel_supported(std::string &error_msg) {
    // bool is_multi_core = !CGlobalInfo::m_options.preview.getIsOneCore();
    // bool software_mode = CGlobalInfo::m_options.m_astf_best_effort_mode;
    // bool tso_disable = CGlobalInfo::m_options.preview.get_dev_tso_support();
    // bool res = true;
    // error_msg = "";
    // if (!software_mode && is_multi_core) {
    //     error_msg += "software mode is disable";
    //     res = false;
    // }
    // if (tso_disable) {
    //     if (!res) {
    //         error_msg += ", ";
    //     }
    //     error_msg += "tso is enable";
    //     res = false;
    // }
    return true;
}

void* CMplsMan::get_opposite_ctx() {
    if (!m_context_ready) {
        return nullptr;
    }
    CMplsCtx *opposite_context;
    MPLSHeader *mh = new MPLSHeader();
    mh->setLabel(12);
    opposite_context = new CMplsCtx(mh->getLabel(), mh->getTc(), mh->getS(), mh->getTtl());
    return (void *)opposite_context;
}

tunnel_ctx_del_cb_t CMplsMan::get_tunnel_ctx_del_cb() {
    return &del_mpls_ctx;
}

/*
* Add encapsulation (mpls) to the packet with the vid, pcp, dei that
* we set in the constructor.
* In order for the Prepend method to work, the headroom in the rte_mbuf has to be large enough for the
* encapsulation we used (32 bytes).
* input: [ipv4/ipv6]-inner/[udp/tcp]-inner/py. and we chose the ipv4 constructor.
* output: ipv4-encp/udp-encp/gtpu-encp/[ipv4/ipv6]-inner/[udp/tcp]-inner/py
* return valuse: 0 in case of success otherwise -1.
*/
int CMplsMan::prepend(rte_mbuf *pkt, uint8_t dir) {
    // RTE_MBUF_F_RX_VLAN
    int res = 0;
    if (pkt == NULL) {
        m_total_sts[dir].sts.m_err_pkt_null++;
        return -1;
    }
    if (pkt->buf_addr == NULL) {
        m_total_sts[dir].sts.m_err_buf_addr_null++;
        return -1;
    }
    if (pkt->buf_len == 0) {
         m_total_sts[dir].sts.m_err_buf_len++;
        return -1;
    }
    CMplsCtx *vlan_context = (CMplsCtx *)pkt->dynfield_ptr;


    if (rte_pktmbuf_headroom(pkt) < ENCAPSULATION_LEN_MPLS) {
        m_total_sts[dir].sts.m_err_headroom++;
        return -1;
    }

    EthernetHeader *eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    uint16_t l2_nxt_hdr = eth->getNextProtocol();
    uint8_t l3_nxt_hdr;
    // uint16_t l3_header = 0;

    if (l2_nxt_hdr == EthernetHeader::Protocol::VLAN) {
        std::cout<<"Already VLAN traffic!\n";
        return -1;
    }

    if (l2_nxt_hdr != EthernetHeader::Protocol::IP && l2_nxt_hdr != EthernetHeader::Protocol::IPv6) {
        m_total_sts[dir].sts.m_err_l3_hdr++;
        std::cout<<"\nError parsing packets. Exiting..";
        std::cout<<"Protocol: "<<l2_nxt_hdr<<"\n";
        return -1;
    }


    uint8_t *encap = (uint8_t *) rte_pktmbuf_prepend(pkt, ENCAPSULATION_LEN_MPLS);
    memset(encap, 0, ENCAPSULATION_LEN_MPLS);
    EthernetHeader *outer_eth = (EthernetHeader *)encap;

    outer_eth->mySource=eth->mySource;
    outer_eth->myDestination=eth->myDestination;
    outer_eth->setNextProtocol(EthernetHeader::Protocol::VLAN);
    VLANHeader *vlan_hdr = (VLANHeader *)((uint8_t *)outer_eth + ETH_HDR_LEN);

    // vlan_hdr->setTagID(vlan_context->get_vid());
    vlan_hdr->setTagUserPriorty(vlan_context->get_pcp());
    vlan_hdr->setTagCFI(vlan_context->get_dei());
    IPHeader* iph;
    IPHeader * new_iph;


    if (l2_nxt_hdr == EthernetHeader::Protocol::IP) {
        iph = rte_pktmbuf_mtod_offset(pkt, IPHeader *, ETH_HDR_LEN);

        l3_nxt_hdr = ((IPHeader *)iph)->getNextProtocol();
        vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IP);
        new_iph = (IPHeader *)((uint8_t*)vlan_hdr + VLAN_HDR_LEN);

        memcpy((void*)new_iph, (void*)iph, IPV4_HDR_LEN+TCP_HEADER_LEN);
        // new_iph->setTotalLength((uint16_t)pkt->pkt_len-(ETH_HDR_LEN + VLAN_HDR_LEN));
        // new_iph->setVersion(iph->getVersion());
        // new_iph->setDestIp(iph->getDestIp());
        // new_iph->setSourceIp(iph->getSourceIp());
        // new_iph->setProtocol(iph->getProtocol());
        // new_iph->setHeaderLength(iph->getHeaderLength());
        // new_iph->setTimeToLive(iph->getTimeToLive());
        // new_iph->setFirstWord(iph->getFirstWord());
        // new_iph->SetCheckSumRaw(iph->getChecksum());
        new_iph->setHeaderLength(IPV4_HDR_LEN);

        // l3_header = IPV4_HDR_LEN;
    } else {
        std::cout<<"IPV6 error\n";
        return -1;
        // IPv6Header* iph = rte_pktmbuf_mtod_offset(pkt, IPv6Header *, ETH_HDR_LEN);
        // l3_nxt_hdr = ((IPv6Header *)iph)->getNextHdr();
        // vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IPv6);
        // IPv6Header * new_iph = (IPv6Header *)((uint8_t*)vlan_hdr + VLAN_HDR_LEN);

        // // memcpy((void*)new_iph, (void*)iph, IPV4_HDR_LEN);
        // // new_iph->set((uint16_t)pkt->pkt_len-(ETH_HDR_LEN + VLAN_HDR_LEN));
        // new_iph->setVersion(iph->getVersion());
        // new_iph->setDestIpv6(iph->getDestIpv6());
        // new_iph->setSourceIpv6(iph->getSourceIpv6());
        // new_iph->setHopLimit(iph->getHopLimit());
        // new_iph->setNextHdr(iph->getNextHdr());
        // new_iph->setPayloadLen(iph->getPayloadLen());
        // new_iph->setTrafficClass(iph->getTrafficClass());
        // l3_header = IPV6_HDR_LEN;
    }

    if (l3_nxt_hdr != IPHeader::Protocol::ICMP && (l3_nxt_hdr != IPHeader::Protocol::UDP && l3_nxt_hdr != IPHeader::Protocol::TCP)){
        m_total_sts[dir].sts.m_err_l4_hdr++;
        return -1;
    }



    if (new_iph->getProtocol() != IPHeader::Protocol::TCP) {
        std::cout<<"Not TCP"<<std::endl;
        m_total_sts[dir].sts.m_err_l4_hdr++;
    }

    // if(new_iph->getProtocol() == IPHeader::Protocol::TCP) {
    //     new_iph->setProtocol(IPHeader::Protocol::TCP);
    //     TCPHeader *tch = rte_pktmbuf_mtod_offset (pkt, TCPHeader *, ETH_HDR_LEN + IPV4_HDR_LEN);

    //     TCPHeader *new_tch =  (TCPHeader *)((uint8_t*)new_iph + IPV4_ADDR_LEN);
    //     new_tch->setAckFlag(tch->getAckFlag());
    //     new_tch->setAckNumber(tch->getAckNumber());
    //     new_tch->setChecksum(tch->getChecksum());
    //     new_tch->setDestPort(tch->getDestPort());
    //     new_tch->setFinFlag(tch->getFinFlag());
    //     new_tch->setFlag(tch->getFlags());
    //     new_tch->setHeaderLength(tch->getHeaderLength());
    //     // new_tch->setNextProtocol(tch->getNextProtocol());
    //     new_tch->setPushFlag(tch->getPushFlag());
    //     new_tch->setResetFlag(tch->getResetFlag());
    //     new_tch->setSeqNumber(tch->getSeqNumber());
    //     new_tch->setSourcePort(tch->getSourcePort());
    //     new_tch->setSynFlag(tch->getSynFlag());
    //     new_tch->setUrgentFlag(tch->getUrgentFlag());
    //     new_tch->setUrgentOffset(tch->getUrgentFlag());
    //     new_tch->setWindowSize(tch->getWindowSize());
    // //     //memcpy((void*)new_tch, (void*)tch, TCP_HEADER_LEN);
    // //     new_tch->setHeaderLength((uint16_t)pkt->pkt_len-(ETH_HDR_LEN + VLAN_HDR_LEN + IPV4_HDR_LEN));

    // }
    m_total_sts[dir].sts.m_tunnel_encapsulated+=1;
    return res;
}

int CMplsMan::adjust(rte_mbuf *pkt, uint8_t dir) {
    if (pkt == NULL) {
        m_total_sts[dir].sts.m_err_pkt_null++;
        return -1;
    }
    if (pkt->buf_addr == NULL) {
        m_total_sts[dir].sts.m_err_buf_addr_null++;
        return -1;
    }
    if (pkt->data_len < ENCAPSULATION_LEN_MPLS) {
        m_total_sts[dir].sts.m_err_data_len++;
        return -1;
    }
    EthernetHeader *eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    rte_pktmbuf_adj(pkt, ENCAPSULATION_LEN_MPLS);

    EthernetHeader *old_eth = (EthernetHeader *)eth;

    EthernetHeader *new_eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    new_eth->mySource = old_eth->mySource;
    new_eth->myDestination = old_eth->myDestination;
    if (old_eth->getNextProtocol() != EthernetHeader::Protocol::VLAN) {
        return -1;
    }

    VLANHeader *vh = (VLANHeader *)((uint8_t *)old_eth + ETH_HDR_LEN);
    uint16_t l2_nxt_hdr = vh->getNextProtocolHostOrder();
    if (l2_nxt_hdr != EthernetHeader::Protocol::IP && l2_nxt_hdr != EthernetHeader::Protocol::IPv6) {
        m_total_sts[dir].sts.m_err_l3_hdr++;
        return -1;
    }
    IPHeader* iph = (IPHeader *)((uint8_t *)vh + VLAN_HDR_LEN);
    if (iph->getVersion() == IPHeader::Protocol::IP) {
        new_eth->setNextProtocol(EthernetHeader::Protocol::IP);
    } else {
        new_eth->setNextProtocol(EthernetHeader::Protocol::IPv6);
    }
    return 0;
}



/************** CMplsCtx class ******************/

CMplsCtx::CMplsCtx(uint32_t label, uint8_t tc, bool s, uint8_t ttl); {
    m_label = label;
    m_tc = tc;
    m_s = s;
    m_ttl = ttl;
}

// CMplsCtx::CMplsCtx(uint16_t tci) {
//     m_vid = tci & 0xfff;
//     tci >>= 12;
//     m_dei = tci & 0x1;
//     tci >>=1;
//     m_pcp = tci & 0x7;
// }

CMplsCtx::~CMplsCtx() {
}

void CMplsCtx::update_label_mpls_info(uint32_t label) {
    m_label = label;
}

void CMplsCtx::update_tc_mpls_info(uint8_t tc) {
    m_tc = tc;
}

void CMplsCtx::update_s_mpls_info(bool s) {
    m_s = s;
}

void CMplsCtx::update_ttl_mpls_info(uint8_t ttl) {
    m_ttl = ttl;
}

uint16_t CMplsCtx::get_label() {
    return m_label;
}

uint8_t CMplsCtx::get_tc() {
    return m_tc;
}

bool CMplsCtx::get_s() {
    return m_s;
}

uint8_t CMplsCtx::get_ttl() {
    return m_ttl;
}