#include <stdio.h>
#include "latency.h"
#include "flow_stat_parser.h"
#include "stateless/rx/trex_stateless_rx_core.h"


void CRxCoreStateless::create(const CRxSlCfg &cfg) {
    m_max_ports = cfg.m_max_ports;

    for (int i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPort * lp = &m_ports[i];
        //        CCPortLatency * lpo = &m_ports[swap_port(i)].m_port;
        
        lp->m_io = cfg.m_ports[i];
        /*        lp->m_port.Create(this,
                          i,
                          m_pkt_gen.get_payload_offset(),
                          m_pkt_gen.get_l4_offset(),
                          m_pkt_gen.get_pkt_size(),lpo );???*/
    }

}

void CRxCoreStateless::start() {
    static int count = 0;
    static int i = 0;
    while (1) {
        count += try_rx();
        i++;
        if (i == 100000000) {
            i = 0;
            //??? remove
            printf("counter:%d port0:[%u], port1:[%u]\n", count, m_ports[0].m_port.m_rx_pg_pkts[0], m_ports[1].m_port.m_rx_pg_pkts[1]);
        }
    }
}

// ??? temp try
int CRxCoreStateless::try_rx() {
    rte_mbuf_t * rx_pkts[64];
    int i, total_pkts = 0;
    for (i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPort * lp = &m_ports[i];
        rte_mbuf_t * m;
        //m_cpu_dp_u.start_work();
        /* try to read 64 packets clean up the queue */
        uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
        total_pkts += cnt_p;
        if (cnt_p) {
            int j;
            for (j = 0; j < cnt_p; j++) {
                Cxl710Parser parser;
                m = rx_pkts[j];
                if (parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len) == 0) {
                    uint16_t ip_id;
                    if (parser.get_ip_id(ip_id) == 0) {
                        if (is_flow_stat_id(ip_id)) {
                            uint16_t hw_id = get_hw_id(ip_id);
                            m_ports[i].m_port.m_rx_pg_bytes[hw_id] += m->pkt_len;
                            m_ports[i].m_port.m_rx_pg_pkts[hw_id]++;
                        }
                    }
                }
                rte_pktmbuf_free(m);
            }
            /* commit only if there was work to do ! */
            //m_cpu_dp_u.commit(); //??? what's this?
          }/* if work */
      }// all ports
    return total_pkts;
}

bool CRxCoreStateless::is_flow_stat_id(uint16_t id) {
    if ((id & 0xff00) == IP_ID_RESERVE_BASE) return true;
    return false;
}

uint16_t CRxCoreStateless::get_hw_id(uint16_t id) {
    return (0x00ff & id);
}

void CRxCoreStateless::reset_rx_stats(uint8_t port_id) {
    for (int hw_id = 0; hw_id < MAX_FLOW_STATS; hw_id++) {
        m_ports[port_id].m_port.m_rx_pg_bytes[hw_id] = 0;
        m_ports[port_id].m_port.m_rx_pg_pkts[hw_id] = 0;
    }
}

int CRxCoreStateless::get_rx_stats(uint8_t port_id, uint32_t *pkts, uint32_t *prev_pkts
                                   , uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        pkts[hw_id] = m_ports[port_id].m_port.m_rx_pg_pkts[hw_id] - prev_pkts[hw_id];
        prev_pkts[hw_id] = m_ports[port_id].m_port.m_rx_pg_pkts[hw_id];
        bytes[hw_id] = m_ports[port_id].m_port.m_rx_pg_bytes[hw_id] - prev_bytes[hw_id];
        prev_bytes[hw_id] = m_ports[port_id].m_port.m_rx_pg_bytes[hw_id];
    }

    return 0;
}
