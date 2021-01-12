#ifndef TUNNEL_FACTORY_CREATER_H_
#define TUNNEL_FACTORY_CREATER_H_

#include "gtp_tunnel.h"
#include "tunnel.h"
#include "stx/astf/trex_astf_dp_core.h"

enum Tunnel_type: uint8_t {
    TUNNEL_TYPE_NONE,
    TUNNEL_TYPE_GTP,
    TUNNEL_TYPE_MAX
};

class TunnelFactoryCreator {
public:
   void *get_tunnel_object(client_tunnel_data_t data, uint8_t tunnel_type);
   void  update_tunnel_object(client_tunnel_data_t data, void *tunnel, uint8_t tunnel_type);
   string get_tunnel_type(uint8_t tunnel_type);
};

#endif /*TUNNEL_FACTORY_CREATER_H_*/ 
