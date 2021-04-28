#ifndef GTP_MAN_H_
#define GTP_MAN_H_

#include "pal/linux/mbuf.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"

class CGtpuMan {

public:
    CGtpuMan(uint32_t teid, ipv4_addr_t src_ip, ipv4_addr_t dst_ip);
    CGtpuMan(uint32_t teid, ipv6_addr_t* src_ipv6, ipv6_addr_t* dst_ipv6);
    ~CGtpuMan();
    int Prepend(rte_mbuf *pkt);
    int32_t get_teid();
    ipv4_addr_t get_src_ipv4();
    ipv4_addr_t get_dst_ipv4();
    void get_src_ipv6(ipv6_addr_t* src_ipv6);
    void get_dst_ipv6(ipv6_addr_t* src_ipv6);

private:
    int Prepend_ipv4_tunnel(rte_mbuf *pkt, uint8_t l4_offset, uint16_t inner_cs);
    int Prepend_ipv6_tunnel(rte_mbuf *pkt, uint8_t l4_offset, uint16_t inner_cs);
    void store_ipv4_gtpu_info();
    void store_ipv6_gtpu_info();
    void store_udp_gtpu(void *udp);

private:
    uint32_t     m_teid;
    uint8_t     *m_outer_hdr;
    bool         m_ipv6_set;
    ipv4_ipv6_t  m_src;
    ipv4_ipv6_t  m_dst;
};
#endif