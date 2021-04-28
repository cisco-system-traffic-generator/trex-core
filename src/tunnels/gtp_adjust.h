#ifndef GTP_ADJUST_H_
#define GTP_ADJUST_H_

#include "pal/linux/mbuf.h"
class CGtpuAdjust {

public:
    int Adjust(rte_mbuf *pkt);

private:
    int Adjust_ipv4_tunnel(rte_mbuf *pkt, void *eth, uint8_t l3_offset);
    int Adjust_ipv6_tunnel(rte_mbuf *pkt, void *eth, uint8_t l3_offset);
    int validate_gtpu_udp(void *udp);
};
#endif