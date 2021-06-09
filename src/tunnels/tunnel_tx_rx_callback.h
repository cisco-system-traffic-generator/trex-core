#ifndef __TUNNEL_TX_RX_CALLBACK_
#define __TUNNEL_TX_RX_CALLBACK_

#include "44bsd/tx_rx_callback.h"
#include "tunnel_handler.h"

class CTunnelTxRxCallback : public CTxRxCallback {

public:
    CTunnelTxRxCallback(CTunnelHandler *tunnel_handler) {
        m_tunnel_handler = tunnel_handler;
    }


    int on_tx(int dir, rte_mbuf *pkt) {
        return m_tunnel_handler->on_tx(dir, pkt);
    }


    int on_rx(int dir, rte_mbuf *pkt) {
        return m_tunnel_handler->on_rx(dir, pkt);
    }


    ~CTunnelTxRxCallback(){}


private:
    CTunnelHandler *m_tunnel_handler;
};
#endif