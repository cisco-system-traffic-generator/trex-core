#include "gtp_adjust.h"
#include "common/Network/Packet/EthernetHeader.h"
#include "common/Network/Packet/GTPUHeader.h"
#include "common/Network/Packet/VLANHeader.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/UdpHeader.h"
#include "common/Network/Packet/IPv6Header.h"

#define ENCAPSULATION_LEN 36
#define ENCAPSULATION6_LEN 56
#define DEST_PORT 2152


/*
* Removes the encapsulation headers [ipv4/ipv6]/udp/gtpu from the packet,
* In case there is indeed encapsulation with those headers.
* For example :
* input: [ipv4/ipv6]-encp/udp-encp/gtpu-encp/[ipv4/ipv6]-inner/[udp/tcp]-inner/py
* output: [ipv4/ipv6]-inner/[udp/tcp]-inner/py
* return valuse: 0 in case of success otherwise -1.
*/
int CGtpuAdjust::Adjust(rte_mbuf * pkt) {
    EthernetHeader *eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    if (eth->getNextProtocol() == EthernetHeader::Protocol::ARP) {
        return -1;
    }
    uint8_t l3_offset = ETH_HDR_LEN;
    uint16_t l2_nxt_hdr = eth->getNextProtocol();
    IPHeader *iph = (IPHeader *)((uint8_t *)eth + ETH_HDR_LEN);
    if (l2_nxt_hdr == EthernetHeader::Protocol::VLAN) {
        VLANHeader *vh;
        vh = (VLANHeader *)((uint8_t *)eth + ETH_HDR_LEN);
        l2_nxt_hdr = vh->getNextProtocolHostOrder();
        iph = (IPHeader *)((uint8_t *)vh + VLAN_HDR_LEN);
        l3_offset += VLAN_HDR_LEN;
    }
    if (l2_nxt_hdr != EthernetHeader::Protocol::IP && l2_nxt_hdr != EthernetHeader::Protocol::IPv6) {
        return -1;
    }
    int res = 0;
    if (iph->getVersion() == IPHeader::Protocol::IP) {
        res = Adjust_ipv4_tunnel(pkt, eth, l3_offset);
    } else {
        res = Adjust_ipv6_tunnel(pkt,eth, l3_offset);
    }
    if(res != 0) {
        return -1;
    }
    uint16_t l2_len = 0;
    EthernetHeader *old_eth = (EthernetHeader *)eth;
    EthernetHeader *new_eth = rte_pktmbuf_mtod(pkt, EthernetHeader *);
    new_eth->mySource = old_eth->mySource;
    new_eth->myDestination = old_eth->myDestination;
    l2_len = ETH_HDR_LEN;
    if (old_eth->getNextProtocol() == EthernetHeader::Protocol::VLAN) {
        VLANHeader *vlan_hdr = (VLANHeader *)((uint8_t *)new_eth + ETH_HDR_LEN);
        VLANHeader *vh = (VLANHeader *)((uint8_t *)old_eth + ETH_HDR_LEN);
        new_eth->setNextProtocol(old_eth->getNextProtocol());
        l2_len += VLAN_HDR_LEN;
        iph = rte_pktmbuf_mtod_offset(pkt, IPHeader *, l2_len);
        vlan_hdr->myTag = vh->myTag;
        if (iph->getVersion() == IPHeader::Protocol::IP) {
            vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IP);
        } else {
            vlan_hdr->setNextProtocolFromHostOrder(EthernetHeader::Protocol::IPv6);
        }
    } else {
        iph = rte_pktmbuf_mtod_offset(pkt, IPHeader *, l2_len);
        if (iph->getVersion() == IPHeader::Protocol::IP) {
            new_eth->setNextProtocol(EthernetHeader::Protocol::IP);
        } else {
            new_eth->setNextProtocol(EthernetHeader::Protocol::IPv6);
        }
    }
    return 0;
}


int CGtpuAdjust::Adjust_ipv6_tunnel(rte_mbuf * pkt, void *eth,  uint8_t l3_offset) {
    IPv6Header *ipv6 = rte_pktmbuf_mtod_offset (pkt, IPv6Header *, l3_offset);
    if (ipv6->getNextHdr() != IPHeader::Protocol::UDP) {
        return -1;
    }
    UDPHeader *udp ;
    udp = (UDPHeader *) ((uint8_t *)ipv6 + IPV6_HDR_LEN);
    if (validate_gtpu_udp((void *)udp) == -1){
        return -1;
    }
    rte_pktmbuf_adj(pkt, ENCAPSULATION6_LEN);
    return 0;
}


int CGtpuAdjust::Adjust_ipv4_tunnel(rte_mbuf * pkt, void *eth, uint8_t l3_offset) {
    IPHeader *iph =  rte_pktmbuf_mtod_offset (pkt, IPHeader *,l3_offset);
    if (iph->getProtocol() != IPHeader::Protocol::UDP){
        return -1;
    }
    UDPHeader* udp ;
    udp = (UDPHeader*) ((uint8_t *)iph + IPV4_HDR_LEN);
    if (validate_gtpu_udp((void *)udp) == -1){
        return -1;
    }
    rte_pktmbuf_adj(pkt, ENCAPSULATION_LEN);
    return 0;
}


int CGtpuAdjust::validate_gtpu_udp(void *udp) {
    if (((UDPHeader *)udp)->getDestPort() != DEST_PORT) {
        return -1;
    }
    GTPUHeader *gtpu = (GTPUHeader*)((uint8_t *)udp + UDP_HEADER_LEN);
    if (gtpu->getType() != GTPU_TYPE_GTPU){
        return -1;
    }
    return 0;
}