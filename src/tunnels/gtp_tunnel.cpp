#include <rte_ip.h>
#include <rte_random.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>

#include "gtp_tunnel.h"

#include "bp_sim.h"
#include <iostream>


typedef std::map<uint32_t, GTPU *, std::less<uint32_t> > gtpu_flow_map_t;
typedef gtpu_flow_map_t::const_iterator gtpu_flow_map_iter_t;
typedef gtpu_flow_map_t::iterator gtpu_flow_map_iter_no_const_t;

/**
 *      Bits
 * Octets   8   7   6   5   4   3   2   1
 * 1                  Version   PT  (*) E   S   PN
 * 2        Message Type
 * 3        Length (1st Octet)
 * 4        Length (2nd Octet)
 * 5        Tunnel Endpoint Identifier (1st Octet)
 * 6        Tunnel Endpoint Identifier (2nd Octet)
 * 7        Tunnel `Endpoint Identifier (3rd Octet)
 * 8        Tunnel Endpoint Identifier (4th Octet)
 * 9        Sequence Number (1st Octet)1) 4)
 * 10       Sequence Number (2nd Octet)1) 4)
 * 11       N-PDU Number2) 4)
 * 12       Next Extension Header Type3) 4)
**/

#define GTPU_V1_HDR_LEN   8

#define GTPU_VER_MASK (7<<5)
#define GTPU_PT_BIT   (1<<4)
#define GTPU_E_BIT    (1<<2)
#define GTPU_S_BIT    (1<<1)
#define GTPU_PN_BIT   (1<<0)
#define GTPU_E_S_PN_BIT  (7<<0)

#define GTPU_V1_VER   (1<<5)

#define GTPU_PT_GTP    (1<<4)
#define GTPU_TYPE_GTPU  255


typedef struct __attribute__((__packed__))
{
    uint8_t ver_flags;
    uint8_t type;
    uint16_t length;           /* length in octets of the data following the fixed part of the header */
    uint32_t teid;
} gtpu_header_t;

struct Encapsulation {
    struct rte_ipv4_hdr ip;
    struct rte_udp_hdr udp;
    gtpu_header_t gtp;
};

struct Encapsulation6 {
    struct rte_ipv6_hdr ip6;
    struct rte_udp_hdr udp;
    gtpu_header_t gtp;
//}__attribute__((__packed__));
};

static const uint8_t IPV4_HDR_VERSION_IHL = 0x45;
static const uint8_t IPV4_HDR_TTL = 64;

static constexpr uint16_t kEtherTypeBeIPv4 = __builtin_bswap16(
    (RTE_ETHER_TYPE_IPV4));

static constexpr uint16_t kEtherTypeBeIPv6 = __builtin_bswap16(
    (RTE_ETHER_TYPE_IPV6));

static constexpr uint16_t kEtherTypeBeVLAN = __builtin_bswap16(
    (RTE_ETHER_TYPE_VLAN));

static constexpr uint16_t kEtherTypeBeARP = __builtin_bswap16(
    (RTE_ETHER_TYPE_ARP));

int GTPU::Prepend(rte_mbuf * pkt, u_int16_t queue) {

    if (rte_pktmbuf_headroom(pkt) < sizeof(struct Encapsulation)) {
        return -1;
    }
    
    struct rte_ether_hdr * eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_vlan_hdr *vh; 
    uint16_t l3_offset = 0;

    if (eth->ether_type == kEtherTypeBeARP) {
        return 0;
    }

    l3_offset += sizeof(struct rte_ether_hdr);

    if (eth->ether_type == kEtherTypeBeVLAN) {
        vh = (struct rte_vlan_hdr *)(eth + 1);
        if (vh->eth_proto == kEtherTypeBeARP)
            return 0;
        l3_offset += sizeof(struct rte_vlan_hdr);  
    }

    if(m_ipv6_set == 0) {
        Prepend_ipv4_tunnel(pkt, l3_offset, queue);
    }
    else
        Prepend_ipv6_tunnel(pkt, l3_offset, queue);

    return 0;
 }


int GTPU::Prepend_ipv4_tunnel(rte_mbuf * pkt, u_int8_t l3_offset, u_int16_t queue ) {

    uint8_t l4_offset = 0;

    l4_offset = l3_offset + sizeof(struct rte_ipv4_hdr);

    struct rte_ether_hdr * eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_tcp_hdr *tch = rte_pktmbuf_mtod_offset (pkt, struct rte_tcp_hdr *, l4_offset);
    struct rte_vlan_hdr *vh; 

    struct Encapsulation * encap = (struct Encapsulation *) rte_pktmbuf_prepend(pkt, sizeof(struct
      Encapsulation));

    memset(encap, 0, sizeof(*encap));
    struct rte_ether_hdr * outer_eth = (struct rte_ether_hdr *)encap;
    rte_ether_addr_copy(&eth->s_addr, &outer_eth->s_addr);
    rte_ether_addr_copy(&eth->d_addr, &outer_eth->d_addr);

    struct rte_ipv4_hdr * outer_ipv4 = (struct rte_ipv4_hdr *)(outer_eth+1);

    if (eth->ether_type == kEtherTypeBeVLAN) {

        outer_eth->ether_type = kEtherTypeBeVLAN;
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)(outer_eth+1); 
        vh = (struct rte_vlan_hdr *)(eth + 1);
        vlan_hdr->eth_proto =  kEtherTypeBeIPv4;
        vlan_hdr->vlan_tci = vh->vlan_tci;
        outer_ipv4 = (struct rte_ipv4_hdr *)(vlan_hdr+1);
        /*Copy the pre-cooked packet*/
        rte_memcpy( (void*) outer_ipv4 , (void *)  m_outer_hdr , sizeof(struct Encapsulation));
        /*Fix outer header IPv4 length */
        outer_ipv4->total_length = rte_cpu_to_be_16(pkt->pkt_len - (RTE_ETHER_HDR_LEN  + 4));
    }
    else {
        outer_eth->ether_type = kEtherTypeBeIPv4;
        /*Copy the pre-cooked packet at the right place*/
        rte_memcpy( (void*) outer_ipv4 , (void *)  m_outer_hdr , sizeof(struct Encapsulation));
        /*Fix outer header IPv4 length */
        outer_ipv4->total_length = rte_cpu_to_be_16(pkt->pkt_len - RTE_ETHER_HDR_LEN);
    }
    outer_ipv4->hdr_checksum = rte_ipv4_cksum(outer_ipv4);

    /*Fix UDP header length */
    //static uint16_t port = 0;
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(outer_ipv4+1);
    udp->src_port = tch->src_port;
    udp->dgram_len  = rte_cpu_to_be_16(rte_be_to_cpu_16(outer_ipv4->total_length) - sizeof(struct
    rte_ipv4_hdr ));

    /*Fix GTPU length*/
    gtpu_header_t *gtpu = (gtpu_header_t *)(udp+1);
    gtpu->length = rte_cpu_to_be_16(rte_be_to_cpu_16(udp->dgram_len)  - sizeof(struct rte_udp_hdr)-sizeof(gtpu_header_t));

    /* to keep offload features for inner packet header */
    pkt->l2_len += sizeof(struct Encapsulation);

    return 0;
 }

int GTPU::Prepend_ipv6_tunnel(rte_mbuf * pkt, u_int8_t l3_offset, u_int16_t queue) {

    uint8_t l4_offset = 0;

    /*FIX this - l4 offset should be based on the packet type.*/
    l4_offset = l3_offset + sizeof(struct rte_ipv4_hdr);
    struct rte_ether_hdr * eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_tcp_hdr *tch = rte_pktmbuf_mtod_offset (pkt, struct rte_tcp_hdr *, l4_offset);
    struct rte_vlan_hdr *vh; 

    struct Encapsulation6 * encap6 = (struct Encapsulation6 *) rte_pktmbuf_prepend(pkt, sizeof(struct
      Encapsulation6));

    memset(encap6, 0, sizeof(*encap6));
    struct rte_ether_hdr * outer_eth = (struct rte_ether_hdr *)encap6;
    rte_ether_addr_copy(&eth->s_addr, &outer_eth->s_addr);
    rte_ether_addr_copy(&eth->d_addr, &outer_eth->d_addr);
    struct rte_ipv6_hdr * outer_ipv6 = (struct rte_ipv6_hdr *)(outer_eth+1);
    outer_eth->ether_type = eth->ether_type;

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)(outer_eth+1);
        vh = (struct rte_vlan_hdr *)(eth + 1);
        /*outerether type to be Ipv6 , as its IPv6 tunnel */
        vlan_hdr->eth_proto =  kEtherTypeBeIPv6;
        vlan_hdr->vlan_tci = vh->vlan_tci;
        outer_ipv6 = (struct rte_ipv6_hdr *)(vlan_hdr+1);
        /*Copy the pre-cooked packet*/
        rte_memcpy( (void*) outer_ipv6 , (void *)  m_outer_hdr , sizeof(struct Encapsulation6));
        /*Fix outer header IPv4 length */
        outer_ipv6->payload_len= rte_cpu_to_be_16(pkt->pkt_len - ((RTE_ETHER_HDR_LEN  + 4) +
        sizeof(struct rte_ipv6_hdr)));
    }
    else {
        /*Override outerether type to be Ipv6 , as its IPv6 tunnel */
        outer_eth->ether_type = kEtherTypeBeIPv6;
        /*Copy the pre-cooked packet at the right place*/
        rte_memcpy( (void*) outer_ipv6 , (void *)  m_outer_hdr , sizeof(struct Encapsulation6));
        /*Fix outer header IPv4 length */
        outer_ipv6->payload_len= rte_cpu_to_be_16(pkt->pkt_len - (RTE_ETHER_HDR_LEN + sizeof(struct rte_ipv6_hdr)));
    }

    /*Fix UDP header length */
    //static uint16_t port = 0;
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(outer_ipv6+1);
    udp->src_port = tch->src_port;
    udp->dgram_len  = rte_cpu_to_be_16(rte_be_to_cpu_16(outer_ipv6->payload_len));

    /*Fix GTPU length*/
    gtpu_header_t *gtpu = (gtpu_header_t *)(udp+1);
    gtpu->length = rte_cpu_to_be_16(rte_be_to_cpu_16(udp->dgram_len)  - sizeof(struct rte_udp_hdr)-sizeof(gtpu_header_t));

    /* to keep offload features for inner packet header */
    pkt->l2_len += sizeof(struct Encapsulation6);

    return 0;
}

int GTPU::Adjust_ipv4_tunnel( rte_mbuf * pkt, struct rte_ether_hdr const *eth) {
  
    struct rte_ipv4_hdr *ipv4 = NULL;
    u_int16_t l2_len = 0;

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vh;
        vh = (struct rte_vlan_hdr *)(eth + 1);
        if (vh->eth_proto == kEtherTypeBeARP) {
            return 0;
        }
        ipv4 = (struct rte_ipv4_hdr *)(vh+1);
        if (ipv4->next_proto_id != IPPROTO_UDP)
            return 0;
    }
    else
        ipv4 = (struct rte_ipv4_hdr *)(eth+1);

    struct rte_udp_hdr * udp ;
    udp = (struct rte_udp_hdr *) (ipv4 + 1);
    u_int16_t port = rte_be_to_cpu_16(2152);
    if (udp->dst_port != port)  {
        return 0;
    }

    rte_pktmbuf_adj(pkt, sizeof(struct Encapsulation));

    struct rte_ether_hdr *new_eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    rte_ether_addr_copy(&eth->s_addr, &new_eth->s_addr);
    rte_ether_addr_copy(&eth->d_addr, &new_eth->d_addr);
    l2_len = sizeof(struct rte_ether_hdr);

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)(new_eth+1);
        struct rte_vlan_hdr *vh = (struct rte_vlan_hdr *)(eth + 1);
        new_eth->ether_type = eth->ether_type;
        l2_len += sizeof(struct rte_vlan_hdr);
        ipv4 = rte_pktmbuf_mtod_offset (pkt, struct rte_ipv4_hdr *, l2_len);
        vlan_hdr->vlan_tci = vh->vlan_tci;
        if ((ipv4->version_ihl >> 4) == 4)
            vlan_hdr->eth_proto = kEtherTypeBeIPv4 ;
        else
            vlan_hdr->eth_proto = kEtherTypeBeIPv6 ;
    }
    else {
        ipv4 = rte_pktmbuf_mtod_offset (pkt, struct rte_ipv4_hdr *, l2_len);
        if ((ipv4->version_ihl >> 4) == 4)
            new_eth->ether_type = kEtherTypeBeIPv4;
        else
            new_eth->ether_type = kEtherTypeBeIPv6;
     }

    return 0;

}

int GTPU::Adjust_ipv6_tunnel( rte_mbuf * pkt, struct rte_ether_hdr const *eth)
{

    struct rte_ipv6_hdr *ipv6 = NULL;
    u_int16_t l2_len = 0;

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vh;
        vh = (struct rte_vlan_hdr *)(eth + 1);
        if (vh->eth_proto == kEtherTypeBeARP) {
            return 0;
        }
        ipv6 = (struct rte_ipv6_hdr *)(vh+1);
        if (ipv6->proto != IPPROTO_UDP)
            return 0;
    }
    else
        ipv6 = (struct rte_ipv6_hdr *)(eth+1);

    struct rte_udp_hdr * udp ;
    udp = (struct rte_udp_hdr *) (ipv6 + 1);
    u_int16_t port = rte_be_to_cpu_16(2152);
    if (udp->dst_port != port)  {
        return 0;
    }

    rte_pktmbuf_adj(pkt, sizeof(struct Encapsulation6));
    struct rte_ether_hdr *new_eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    rte_ether_addr_copy(&eth->s_addr, &new_eth->s_addr);
    rte_ether_addr_copy(&eth->d_addr, &new_eth->d_addr);
    l2_len = sizeof(struct rte_ether_hdr);

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)(new_eth+1);
        struct rte_vlan_hdr *vh = (struct rte_vlan_hdr *)(eth + 1);
        new_eth->ether_type = eth->ether_type;
        l2_len += sizeof(struct rte_vlan_hdr);
        ipv6 = rte_pktmbuf_mtod_offset (pkt, struct rte_ipv6_hdr *, l2_len);
        vlan_hdr->vlan_tci = vh->vlan_tci;
        if ((ipv6->vtc_flow >> 28) == 6)
            vlan_hdr->eth_proto = kEtherTypeBeIPv6 ;
        else
            vlan_hdr->eth_proto = kEtherTypeBeIPv4 ;
    }
    else {
        ipv6 = rte_pktmbuf_mtod_offset (pkt, struct rte_ipv6_hdr *, l2_len);
        if ((ipv6->vtc_flow >> 28) == 6)
            new_eth->ether_type = kEtherTypeBeIPv6;
        else
            new_eth->ether_type = kEtherTypeBeIPv4;
    }

    return 0;
}


int GTPU::Adjust(rte_mbuf * pkt, u_int8_t queue ) {

    struct rte_ether_hdr const * eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr const*);

    if (eth->ether_type == kEtherTypeBeARP) {
        return 0;
    }
    struct rte_ipv4_hdr *ipv4 = (struct rte_ipv4_hdr *) (eth + 1);

    if (eth->ether_type == kEtherTypeBeVLAN) {
        struct rte_vlan_hdr *vh;
        vh = (struct rte_vlan_hdr *)(eth + 1);
        if (vh->eth_proto == kEtherTypeBeARP)
            return 0;
        ipv4 = (struct rte_ipv4_hdr *)(vh+1);
    }

    if ((ipv4->version_ihl >> 4) == 4) {
        Adjust_ipv4_tunnel(pkt, eth);
    }
    else {
        Adjust_ipv6_tunnel(pkt,eth);
    }

    return 0;
}

GTPU::GTPU( ):teid(0),dst_ip(0),src_ip(0)
{
    m_outer_hdr = nullptr;
}

GTPU::~GTPU()
{
    delete m_outer_hdr;
}

void GTPU::store_ipv4_gtpu_info(uint32_t teid,uint32_t src_ip, uint32_t dst_ip, void *pre_cooked_header) 
{

    Encapsulation *pre_cooked_hdr = (Encapsulation *) pre_cooked_header;
    struct rte_ipv4_hdr * ip = &pre_cooked_hdr->ip;

    ip->version_ihl = IPV4_HDR_VERSION_IHL;
    ip->type_of_service = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = IPV4_HDR_TTL;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->src_addr = src_ip;
    ip->dst_addr = dst_ip;

    struct rte_udp_hdr *udp = &pre_cooked_hdr->udp;

    udp->src_port = rte_cpu_to_be_16(2152);
    udp->dst_port = rte_cpu_to_be_16(2152);
    udp->dgram_cksum = 0;

    gtpu_header_t *gtpu = &pre_cooked_hdr->gtp;
    memset(gtpu, 0, sizeof(*gtpu));
    gtpu->ver_flags = GTPU_V1_VER | GTPU_PT_BIT;
    gtpu->type = GTPU_TYPE_GTPU;
    gtpu->teid = rte_cpu_to_be_32(teid);

    m_outer_hdr = (uint8_t *)pre_cooked_hdr;
    m_ipv6_set = 0;

}

GTPU::GTPU(uint32_t teid,uint32_t src_ip, uint32_t dst_ip)
{

  Encapsulation *pre_cooked_header = new Encapsulation[1];
  store_ipv4_gtpu_info(teid, src_ip, dst_ip, pre_cooked_header);
}


void GTPU::store_ipv6_gtpu_info(uint32_t teid, uint8_t src_ipv6[16], uint8_t dst_ipv6[16], void *pre_cooked_header)
{
    Encapsulation6 *pre_cooked_hdr = (Encapsulation6 *) pre_cooked_header;
    struct rte_ipv6_hdr * ip6 = &pre_cooked_hdr->ip6;

    ip6->vtc_flow=  rte_cpu_to_be_32(6 << 28);
    ip6->hop_limits = 255;
    ip6->proto = IPPROTO_UDP;

    rte_memcpy((void *) ip6->src_addr, (void*)src_ipv6, 16);
    rte_memcpy((void *) ip6->dst_addr,(void*)dst_ipv6, 16);

    struct rte_udp_hdr *udp = &pre_cooked_hdr->udp;

    udp->src_port = rte_cpu_to_be_16(2152);
    udp->dst_port = rte_cpu_to_be_16(2152);
    udp->dgram_cksum = 0;

    gtpu_header_t *gtpu = &pre_cooked_hdr->gtp;
    memset(gtpu, 0, sizeof(*gtpu));
    gtpu->ver_flags = GTPU_V1_VER | GTPU_PT_BIT;
    gtpu->type = GTPU_TYPE_GTPU;
    gtpu->teid = rte_cpu_to_be_32(teid);

    m_outer_hdr = (uint8_t *)pre_cooked_hdr;
    m_ipv6_set = 1;

}

GTPU::GTPU(uint32_t teid, uint8_t src_ipv6[16], uint8_t dst_ipv6[16])
{
  Encapsulation6 *pre_cooked_header = new Encapsulation6[1];
  store_ipv6_gtpu_info(teid, src_ipv6, dst_ipv6, pre_cooked_header);
}

void GTPU::update_ipv6_gtpu_info(uint32_t teid, uint8_t src_ipv6[16], uint8_t dst_ipv6[16])
{
  store_ipv6_gtpu_info(teid, src_ipv6, dst_ipv6, (void *)m_outer_hdr);
}

void GTPU::update_ipv4_gtpu_info(uint32_t teid, uint32_t src_ip, uint32_t dst_ip)
{
  store_ipv4_gtpu_info(teid, src_ip, dst_ip, (void *)m_outer_hdr);
}

