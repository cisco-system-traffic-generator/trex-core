#include <stdio.h>
#include "bp_sim.h"
#include "flow_stat_parser.h"
#include "latency.h"
#include "trex_stateless_messaging.h"
#include "trex_stateless_rx_core.h"

void CRxCoreStateless::create(const CRxSlCfg &cfg) {
    m_max_ports = cfg.m_max_ports;

    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_ring_to_cp   = cp_rx->getRingDpToCp(0);
    m_state = STATE_IDLE;

    for (int i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPort * lp = &m_ports[i];
        lp->m_io = cfg.m_ports[i];
    }
}

void CRxCoreStateless::handle_cp_msg(TrexStatelessCpToRxMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

bool CRxCoreStateless::periodic_check_for_cp_messages() {
    /* fast path */
    if ( likely ( m_ring_from_cp->isEmpty() ) ) {
        return false;
    }

    while ( true ) {
        CGenNode * node = NULL;

        if (m_ring_from_cp->Dequeue(node) != 0) {
            break;
        }
        assert(node);
        TrexStatelessCpToRxMsgBase * msg = (TrexStatelessCpToRxMsgBase *)node;
        handle_cp_msg(msg);
    }

    return true;

}

void CRxCoreStateless::idle_state_loop() {
    const int SHORT_DELAY_MS    = 2;
    const int LONG_DELAY_MS     = 50;
    const int DEEP_SLEEP_LIMIT  = 2000;

    int counter = 0;

    while (m_state == STATE_IDLE) {
        bool had_msg = periodic_check_for_cp_messages();
        if (had_msg) {
            counter = 0;
            continue;
        }

        /* enter deep sleep only if enough time had passed */
        if (counter < DEEP_SLEEP_LIMIT) {
            delay(SHORT_DELAY_MS);
            counter++;
        } else {
            delay(LONG_DELAY_MS);
        }
    }
}

void CRxCoreStateless::start() {
      static int count = 0;
      static int i = 0;

      while (true) {
          if (m_state == STATE_WORKING) {
              count += try_rx();
              i++;
              if (i == 100) {
                  i = 0;
                  // if no packets in 100 cycles, sleep for a while to spare the cpu
                  if (count == 0) {
                      delay(1);
                  }
                  count = 0;
                  periodic_check_for_cp_messages();
              }
          } else {
              idle_state_loop();
          }
#if 0
          ??? do we need this?
          if ( m_core->is_terminated_by_master() ) {
              break;
          }
#endif
      }
}

int CRxCoreStateless::try_rx() {
    rte_mbuf_t * rx_pkts[64];
    int i, total_pkts = 0;
    for (i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPort * lp = &m_ports[i];
        rte_mbuf_t * m;
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
