#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t (*tx_burst)(uint16_t port, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
} TrexCallbackFuncs;

extern TrexCallbackFuncs trex_callback_funcs;

inline void trex_callback_funcs_set(TrexCallbackFuncs*  cbs) {
  trex_callback_funcs = *cbs;
}

#ifdef __cplusplus
}
#endif

#endif
