#ifndef TUNNEL_H_
#define TUNNEL_H_

#include <cstdint>
#include <vector>

#include <mbuf.h>
#include "utl_yaml.h"
#include "inet_pton.h"
#include <common/Network/Packet/CPktCmn.h>

class Tunnel {
public:
    virtual ~Tunnel() {
    }

    virtual int Prepend(rte_mbuf * m, u_int16_t ) = 0;
    virtual int Adjust(rte_mbuf * m, u_int8_t ) = 0;
    static uint16_t RxCallback(uint16_t port, uint16_t queue,
          struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
          void *user_param);

    static uint16_t TxCallback(uint16_t port, uint16_t queue,
         struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param);

    static int InstallRxCallback(uint16_t port, uint16_t queue);

    static int InstallTxCallback(uint16_t port, uint16_t queue);

private:
    bool m_ipv6_set;

};

#endif /* TUNNEL_H_*/
