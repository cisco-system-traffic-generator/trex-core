#ifndef __TX_RX_CALLBACK_
#define __TX_RX_CALLBACK_

#include "mbuf.h"

class CTxRxCallback {
public:
    virtual int on_tx(int dir, rte_mbuf_t *m) = 0;
    virtual int on_rx(int dir, rte_mbuf_t *m) = 0;
    virtual ~CTxRxCallback(){}
};
#endif