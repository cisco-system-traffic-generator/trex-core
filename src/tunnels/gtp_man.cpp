#include "gtp_man.h"
#include "common/Network/Packet/EthernetHeader.h"
#include "common/Network/Packet/GTPUHeader.h"
#include "common/Network/Packet/VLANHeader.h"
#include "common/Network/Packet/TcpHeader.h"
#include "common/Network/Packet/UdpHeader.h"

#define ENCAPSULATION_LEN 36
#define ENCAPSULATION6_LEN 56
#define DEST_PORT 2152
#define IPV4_HDR_TTL 64
#define HOP_LIMIT 255

/*
* Add encapsulation ([ipv4/ipv6]/udp/gtpu) to the packet with the ip.src/dst and teid that we set in the
* constructor.
* The Prepend method assert that the m_outer_hdr, that contains the encapsulation headers, is not null.
* In order for the Prepend method to work, the headroom in the rte_mbuf has to be large enough for the
* encapsulation we used (36 bytes for ipv4 and 56 bytes for ipv6).
* The Prepend method works only with [ipv4/ipv6]/[tcp/udp] packets.
* The IP header type in the encapsulation is determined by the constructor we used.
* In the udp header the checksum is being calculated by relying on the inner [udp/tcp] checksum.
* udp.checksum = ~inner.ips.checksum + outer.ips.checksum + checksum(udp_start - inner[tcp/udp]offset).
* For example:
* input: [ipv4/ipv6]-inner/[udp/tcp]-inner/py. and we chose the ipv4 constructor.
* output: ipv4-encp/udp-encp/gtpu-encp/[ipv4/ipv6]-inner/[udp/tcp]-inner/py
* return valuse: 0 in case of success otherwise -1.
*/
int CGtpuMan::Prepend(rte_mbuf * pkt) {
    assert(m_outer_hdr != NULL);
    EthernetHeader *eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    VLANHeader *vh;
    uint16_t l3_offset = 0;
    uint16_t l2_nxt_hdr = eth->getNextProtocol();
    l3_offset += ETH_HDR_LEN;
    if (l2_nxt_hdr  == EthernetHeader::Protocol::VLAN) {
        vh = (VLANHeader *)((uint8_t *)eth + ETH_HDR_LEN);
        l2_nxt_hdr = vh->getNextProtocolHostOrder();
        l3_offset += VLAN_HDR_LEN;
    }
    if (l2_nxt_hdr != EthernetHeader::Protocol::IP && l2_nxt_hdr != EthernetHeader::Protocol::IPv6) {
        return -1;
    }
    IPHeader *iph = rte_pktmbuf_mtod_offset (pkt, IPHeader *, l3_offset);
    uint8_t l4_offset = l3_offset + IPV4_HDR_LEN;
    uint16_t inner_cs = 0;
    uint8_t l3_nxt_hdr;
    if (iph->getVersion() == IPHeader::Protocol::IP) {
        IPPseudoHeader ips((IPHeader *)iph);
        inner_cs = ~ips.inetChecksum();
        l3_nxt_hdr = ((IPHeader *)iph)->getNextProtocol();
    } else {
        l4_offset = l3_offset + IPV6_HDR_LEN;
        IPv6PseudoHeader ips((IPv6Header *)iph);
        inner_cs = ~ips.inetChecksum();
        l3_nxt_hdr = ((IPv6Header *)iph)->getNextHdr();
    }
    if (l3_nxt_hdr != IPHeader::Protocol::UDP && l3_nxt_hdr != IPHeader::Protocol::TCP){
        return -1;
    }
    int res = 0;
    if(m_ipv6_set == 0) {
        res = Prepend_ipv4_tunnel(pkt, l4_offset, inner_cs);
    } else {
        res = Prepend_ipv6_tunnel(pkt, l4_offset, inner_cs);
    }
    return res;
}


int CGtpuMan::Prepend_ipv4_tunnel(rte_mbuf * pkt, uint8_t l4_offset, uint16_t inner_cs) {
    if (rte_pktmbuf_headroom(pkt) < ENCAPSULATION_LEN) {
        return -1;
    }
    EthernetHeader *eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    TCPHeader *tch = rte_pktmbuf_mtod_offset (pkt, TCPHeader *, l4_offset);
    VLANHeader *vh;
    uint8_t *encap = (uint8_t *) rte_pktmbuf_prepend(pkt, ENCAPSULATION_LEN);
    memset(encap, 0, ENCAPSULATION_LEN);
    EthernetHeader *outer_eth = (EthernetHeader *)encap;
    outer_eth->mySource=eth->mySource;
    outer_eth->myDestination=eth->myDestination;
    IPHeader *outer_ipv4 = (IPHeader *)((uint8_t *)outer_eth + ETH_HDR_LEN);
    if (eth->getNextProtocol() == EthernetHeader::Protocol::VLAN) {
        outer_eth->setNextProtocol(EthernetHeader::Protocol::VLAN);
        VLANHeader *vlan_hdr = (VLANHeader *)((uint8_t *)outer_eth + ETH_HDR_LEN); 
        vh = (VLANHeader *)((uint8_t *)eth + ETH_HDR_LEN);
        vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IP);
        vlan_hdr->myTag = vh->myTag;
        outer_ipv4 = (IPHeader *)((uint8_t *)vlan_hdr + VLAN_HDR_LEN);
        /*Copy the pre-cooked packet*/
        memcpy((void*)outer_ipv4, (void *)m_outer_hdr, ENCAPSULATION_LEN);
        /*Fix outer header IPv4 length */
        outer_ipv4->setTotalLength((uint16_t)(pkt->pkt_len - (ETH_HDR_LEN  + VLAN_HDR_LEN)));
    } else {
        outer_eth->setNextProtocol(EthernetHeader::Protocol::IP);
        /*Copy the pre-cooked packet at the right place*/
        memcpy((void*)outer_ipv4, (void *)m_outer_hdr, ENCAPSULATION_LEN);
        /*Fix outer header IPv4 length */
        outer_ipv4->setTotalLength((uint16_t)(pkt->pkt_len - ETH_HDR_LEN));
    }
    /*Fix UDP header length */
    UDPHeader* udp = (UDPHeader *)((uint8_t *)outer_ipv4 + IPV4_HDR_LEN);
    udp->setSourcePort(tch->getSourcePort());
    udp->setLength((uint16_t)(outer_ipv4->getTotalLength() - IPV4_HDR_LEN));
    /*Fix GTPU length*/
    GTPUHeader *gtpu = (GTPUHeader *)(udp+1);
    gtpu->setLength(udp->getLength() - UDP_HEADER_LEN - GTPU_V1_HDR_LEN);
    udp->setChecksumRaw(udp->calcCheckSum(outer_ipv4, (uint8_t *)tch - (uint8_t*)udp, inner_cs));
    return 0;
}


int CGtpuMan::Prepend_ipv6_tunnel(rte_mbuf * pkt, uint8_t l4_offset, uint16_t inner_cs) {
    if (rte_pktmbuf_headroom(pkt) < ENCAPSULATION6_LEN) {
        return -1;
    }
    EthernetHeader * eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    TCPHeader *tch = rte_pktmbuf_mtod_offset (pkt, TCPHeader *, l4_offset);
    VLANHeader *vh; 
    uint8_t * encap6 = (uint8_t *) rte_pktmbuf_prepend(pkt, ENCAPSULATION6_LEN);
    memset(encap6, 0, ENCAPSULATION6_LEN);
    EthernetHeader * outer_eth = (EthernetHeader *)encap6;
    outer_eth->mySource=eth->mySource;
    outer_eth->myDestination=eth->myDestination;
    IPv6Header * outer_ipv6 = (IPv6Header *)((uint8_t *)outer_eth + ETH_HDR_LEN);
    if (eth->getNextProtocol() == EthernetHeader::Protocol::VLAN) {
        outer_eth->setNextProtocol(EthernetHeader::Protocol::VLAN);
        VLANHeader *vlan_hdr = (VLANHeader *)((uint8_t *)outer_eth+ETH_HDR_LEN); 
        vh = (VLANHeader *)((uint8_t *)eth + ETH_HDR_LEN);
        vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IPv6);
        vlan_hdr->myTag = vh->myTag;
        /*outerether type to be Ipv6 , as its IPv6 tunnel */
        outer_ipv6 = (IPv6Header *)((uint8_t *)vlan_hdr + VLAN_HDR_LEN);
        /*Copy the pre-cooked packet*/
        memcpy((void*)outer_ipv6, (void *)m_outer_hdr, ENCAPSULATION6_LEN);
        /*Fix outer header IPv6 length */
        outer_ipv6->setPayloadLen(pkt->pkt_len - (ETH_HDR_LEN  + VLAN_HDR_LEN + IPV6_HDR_LEN));
    } else {
        /*Override outerether type to be Ipv6 , as its IPv6 tunnel */
        outer_eth->setNextProtocol(EthernetHeader::Protocol::IPv6);
        /*Copy the pre-cooked packet at the right place*/
        memcpy((void*)outer_ipv6, (void *)m_outer_hdr, ENCAPSULATION6_LEN);
        /*Fix outer header IPv6 length */
        outer_ipv6->setPayloadLen(pkt->pkt_len - (ETH_HDR_LEN + IPV6_HDR_LEN));
    }
    /*Fix UDP length*/
    UDPHeader* udp = (UDPHeader *)((uint8_t *)outer_ipv6 + IPV6_HDR_LEN);
    udp->setSourcePort(tch->getSourcePort());
    udp->setLength((uint16_t)(outer_ipv6->getPayloadLen()));
    /*Fix GTPU length*/
    GTPUHeader *gtpu = (GTPUHeader *)(udp+1);
    gtpu->setLength(udp->getLength() - UDP_HEADER_LEN - GTPU_V1_HDR_LEN);
    udp->setChecksumRaw(udp->calcCheckSum(outer_ipv6, (uint8_t *)tch - (uint8_t*)udp, inner_cs));
    return 0;
}


int32_t CGtpuMan::get_teid() {
    assert(m_outer_hdr != NULL);
    return m_teid;
}


ipv4_addr_t CGtpuMan::get_src_ipv4() {
    assert(!m_ipv6_set);
    return m_src.ipv4;
}


ipv4_addr_t CGtpuMan::get_dst_ipv4() {
    assert(!m_ipv6_set);
    return m_dst.ipv4;
}


void CGtpuMan::get_src_ipv6(ipv6_addr_t* src) {
    assert(m_ipv6_set);
    *src = m_src.ipv6;
}


void CGtpuMan::get_dst_ipv6(ipv6_addr_t* dst) {
    assert(m_ipv6_set);
    *dst = m_dst.ipv6;
}


CGtpuMan::CGtpuMan( uint32_t teid, ipv4_addr_t src_ip, ipv4_addr_t dst_ip) {
    m_outer_hdr = (uint8_t *)malloc(ENCAPSULATION_LEN);
    m_teid = teid;
    memset(m_outer_hdr, 0, ENCAPSULATION_LEN);
    m_src.ipv4 = src_ip;
    m_dst.ipv4 = dst_ip;
    store_ipv4_gtpu_info();
    m_ipv6_set = false;
}


CGtpuMan::CGtpuMan(uint32_t teid, ipv6_addr_t* src_ipv6, ipv6_addr_t* dst_ipv6) {
    m_outer_hdr = (uint8_t *)malloc(ENCAPSULATION6_LEN);
    m_teid = teid;
    memset(m_outer_hdr, 0, ENCAPSULATION6_LEN);
    m_src.ipv6 = *src_ipv6;
    m_dst.ipv6 = *dst_ipv6;
    store_ipv6_gtpu_info();
    m_ipv6_set = true;
}


CGtpuMan::~CGtpuMan() {
    free(m_outer_hdr);
}


void CGtpuMan::store_ipv4_gtpu_info() {
    IPHeader *ip = (IPHeader *)(m_outer_hdr);
    ip->setVersion(IPHeader::Protocol::IP);
    ip->setHeaderLength(IPV4_HDR_LEN);
    ip->setTimeToLive(IPV4_HDR_TTL);
    ip->setProtocol(IPHeader::Protocol::UDP);
    ip->setSourceIp(m_src.ipv4);
    ip->setDestIp(m_dst.ipv4);
    UDPHeader *udp = (UDPHeader *)(m_outer_hdr + IPV4_HDR_LEN);
    store_udp_gtpu((void *)udp);
}


void CGtpuMan::store_ipv6_gtpu_info() {
    IPv6Header *ip6 = (IPv6Header *)(m_outer_hdr);
    ip6->setVersion(6);
    ip6->setHopLimit(HOP_LIMIT);
    ip6->setNextHdr(IPHeader::Protocol::UDP);
    memcpy((void*)ip6->mySource ,(void *)m_src.ipv6.addr, IPV6_ADDR_LEN);
    memcpy((void*)ip6->myDestination, (void *)m_dst.ipv6.addr ,IPV6_ADDR_LEN);
    UDPHeader *udp = (UDPHeader *)(m_outer_hdr + IPV6_HDR_LEN);
    store_udp_gtpu((void *)udp);
}


void CGtpuMan::store_udp_gtpu(void *udp){
    ((UDPHeader *)udp)->setDestPort((uint16_t)(DEST_PORT));
    ((UDPHeader *)udp)->setChecksum(0);
    GTPUHeader *gtpu = (GTPUHeader *)((uint8_t *)udp + UDP_HEADER_LEN);
    memset(gtpu, 0, GTPU_V1_HDR_LEN);
    gtpu->setVerFlags(GTPU_V1_VER | GTPU_PT_BIT);
    gtpu->setType(GTPU_TYPE_GTPU);
    gtpu->setTeid(m_teid);
}