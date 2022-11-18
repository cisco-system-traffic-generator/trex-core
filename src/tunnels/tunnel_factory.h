#ifndef TUNNEL_FACTORY_H_
#define TUNNEL_FACTORY_H_
#include "gtp_man.h"
#include "mpls_man.h"
#include "tunnel_tx_rx_callback.h"

inline CTunnelHandler *create_tunnel_handler(uint8_t tunnel_type, uint8_t mode) {
    CTunnelHandler *tunnel = nullptr;
    switch (tunnel_type) {
        case TUNNEL_TYPE_GTP: {
            tunnel = new CGtpuMan(mode);
            break;
        }
        case TUNNEL_TYPE_MPLS: {
            tunnel = new CMplsMan(mode);
            break;
        }
    }
    return tunnel;
}


inline tunnel_cntxt_t get_tunnel_ctx(client_tunnel_data_t* data) {
    tunnel_cntxt_t tunnel_ctx = nullptr;
    uint8_t tunnel_type = data->type;
    switch (tunnel_type) {
        case TUNNEL_TYPE_GTP: {
            if (data->version == 4) {
                tunnel_ctx = (void*) new CGtpuCtx(data->teid,
                                            data->src.ipv4,
                                            data->dst.ipv4,
                                            data->src_port);
            } else {
                tunnel_ctx = (void*) new CGtpuCtx(data->teid,
                                            &(data->src.ipv6),
                                            &(data->dst.ipv6),
                                            data->src_port);
            }
            break;
        }
        case TUNNEL_TYPE_MPLS: {
            tunnel_ctx = (void *) new CMplsCtx(data->label,
                                               data->tc,
                                               data->s,
                                               data->ttl);
            break;
        }
    }
    return tunnel_ctx;
}
#endif