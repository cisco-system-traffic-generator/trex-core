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
    m_cpu_cp_u.Create(&m_cpu_dp_u);
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
    bool do_try_rx_queue =CGlobalInfo::m_options.preview.get_vm_one_queue_enable() ? true : false;

    while (true) {
        if (m_state == STATE_WORKING) {
            i++;
            if (i == 100) {
                i = 0;
                // if no packets in 100 cycles, sleep for a while to spare the cpu
                if (count == 0) {
                    delay(1);
                }
                count = 0;
                periodic_check_for_cp_messages(); // m_state might change in here
            }
        } else {
            if (m_state == STATE_QUIT)
                break;
            idle_state_loop();
        }
        if (do_try_rx_queue) {
            try_rx_queues();
        }
        count += try_rx();
    }
}

void CRxCoreStateless::handle_rx_pkt(CLatencyManagerPerPort *lp, rte_mbuf_t *m) {
    CFlowStatParser parser;

    if (parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len) == 0) {
        uint16_t ip_id;
        if (parser.get_ip_id(ip_id) == 0) {
            if (is_flow_stat_id(ip_id)) {
                uint16_t hw_id = get_hw_id(ip_id);
                lp->m_port.m_rx_pg_stat[hw_id].add_pkts(1);
                lp->m_port.m_rx_pg_stat[hw_id].add_bytes(m->pkt_len);
            }
        }
    }
}

// In VM setup, handle packets coming as messages from DP cores.
void CRxCoreStateless::handle_rx_queue_msgs(uint8_t thread_id, CNodeRing * r) {
    while ( true ) {
        CGenNode * node;
        if ( r->Dequeue(node) != 0 ) {
            break;
        }
        assert(node);

        CGenNodeMsgBase * msg = (CGenNodeMsgBase *)node;
        CGenNodeLatencyPktInfo * l_msg;
        uint8_t msg_type =  msg->m_msg_type;
        uint8_t rx_port_index;
        CLatencyManagerPerPort * lp;

        switch (msg_type) {
        case CGenNodeMsgBase::LATENCY_PKT:
            l_msg = (CGenNodeLatencyPktInfo *)msg;
            assert(l_msg->m_latency_offset == 0xdead);
            rx_port_index = (thread_id << 1) + (l_msg->m_dir & 1);
            assert( rx_port_index < m_max_ports );
            lp = &m_ports[rx_port_index];
            handle_rx_pkt(lp, (rte_mbuf_t *)l_msg->m_pkt);
            break;
        default:
            printf("ERROR latency-thread message type is not valid %d \n", msg_type);
            assert(0);
        }

        CGlobalInfo::free_node(node);
    }
}

// VM mode function. Handle messages from DP
void CRxCoreStateless::try_rx_queues() {

    CMessagingManager * rx_dp = CMsgIns::Ins()->getRxDp();
    uint8_t threads=CMsgIns::Ins()->get_num_threads();
    int ti;
    for (ti = 0; ti < (int)threads; ti++) {
        CNodeRing * r = rx_dp->getRingDpToCp(ti);
        if ( ! r->isEmpty() ) {
            handle_rx_queue_msgs((uint8_t)ti, r);
        }
    }
}

int CRxCoreStateless::try_rx() {
    rte_mbuf_t * rx_pkts[64];
    int i, total_pkts = 0;
    for (i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPort * lp = &m_ports[i];
        rte_mbuf_t * m;
        m_cpu_dp_u.start_work();
        /* try to read 64 packets clean up the queue */
        uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
        total_pkts += cnt_p;
        if (cnt_p) {
            int j;
            for (j = 0; j < cnt_p; j++) {
                m = rx_pkts[j];
                handle_rx_pkt(lp, m);
                rte_pktmbuf_free(m);
            }
            /* commit only if there was work to do ! */
            m_cpu_dp_u.commit();
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
        m_ports[port_id].m_port.m_rx_pg_stat[hw_id].clear();
    }
}

int CRxCoreStateless::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        rx_stats[hw_id - min] = m_ports[port_id].m_port.m_rx_pg_stat[hw_id];
        if (reset) {
            m_ports[port_id].m_port.m_rx_pg_stat[hw_id].clear();
        }
    }
    return 0;
}

double CRxCoreStateless::get_cpu_util() {
    m_cpu_cp_u.Update();
    return m_cpu_cp_u.GetVal();
}
