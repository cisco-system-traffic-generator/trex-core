#include <rte_ip.h>
#include <rte_random.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>

#include "bp_sim.h"
#include <iostream>
#include "tunnel.h"
#include "gtp_tunnel.h"

uint16_t Tunnel::RxCallback(uint16_t port, uint16_t queue,
                struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
                void *user_param) {

    GTPU t;
    u_int8_t i = 0;

    if (0 == nb_pkts)
        return nb_pkts;

    rte_mbuf *m = NULL;

    /*Process the packets*/
    for (i=0; i < nb_pkts ; i++) {
        m = pkts[i];
        t.Adjust(m,queue);
    }

    return nb_pkts;
}

uint16_t Tunnel::TxCallback(uint16_t port, uint16_t queue,
            struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param) {

    if (!CGlobalInfo::m_options.m_ip_cfg[port].is_gtp_enabled()) {
        return nb_pkts;
    }

    u_int8_t i = 0 ;
    struct rte_ipv4_hdr *iph __attribute__((unused))  = NULL;
    Tunnel *tunnel;


    /*Process the packets*/
    for (i=0; i < nb_pkts; i++) {
        rte_mbuf * m = pkts[i];
        iph = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, m->l2_len);
        tunnel = (Tunnel *)m->dynfield_ptr;
        if(tunnel) {
          tunnel->Prepend(m, queue);
        }
    }
    return nb_pkts;
 }

int Tunnel::InstallRxCallback(uint16_t port, uint16_t queue) {

    if (NULL == rte_eth_add_rx_callback(port, queue, Tunnel::RxCallback, NULL)) {
        printf("failed to install RX callback at %u:%u.", port, queue);
        return -1;
    }
    else
        printf("installed RX callback at %u:%u.", port, queue);

    return 0;
}

int Tunnel::InstallTxCallback(uint16_t port, uint16_t queue) {

    if (NULL == rte_eth_add_tx_callback(port, queue, Tunnel::TxCallback, NULL)) {
        printf("failed to install TX callback at %u:%u.", port, 0);
        return -1;
    }
    else
        printf("installed TX callback at %u:%u.", port, queue);

    return 0;
}
