#ifndef TUNNEL_FACTORY_H_
#define TUNNEL_FACTORY_H_
#include "gtp_man.h"
#include "tunnel_tx_rx_callback.h"

inline CTunnelHandler *create_tunnel_handler(uint8_t tunnel_type, uint8_t mode) {
    CTunnelHandler *tunnel = nullptr;
    switch (tunnel_type) {
        case TUNNEL_TYPE_GTP: {
            tunnel = new CGtpuMan(mode);
            break;
        }
    }
    return tunnel;
}
#endif