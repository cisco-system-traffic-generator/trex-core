
#include "tunnel_factory_creator.h"

void *TunnelFactoryCreator::get_tunnel_object(client_tunnel_data_t data, uint8_t tunnel_type)
{
    void *tunnel = NULL;
    if (tunnel_type == TUNNEL_TYPE_GTP){
        if (data.version == 4){
            tunnel = (void*) new GTPU(data.teid,
                                           data.u1.src_ipv4,
                                           data.u2.dst_ipv4);
         } else {
            tunnel = (void*) new GTPU(PKT_HTONL(data.teid),
                                           data.u1.src_ip,
                                           data.u2.dst_ip);
         }
    }
    return tunnel;
}

void TunnelFactoryCreator::update_tunnel_object(client_tunnel_data_t data, void *tunnel, uint8_t tunnel_type)
{
    if (tunnel_type == TUNNEL_TYPE_GTP){
        if (data.version == 4){
           ((GTPU *)tunnel)->update_ipv4_gtpu_info(data.teid, data.u1.src_ipv4, data.u2.dst_ipv4);
        } else {
           ((GTPU *)tunnel)->update_ipv6_gtpu_info(data.teid, data.u1.src_ip, data.u2.dst_ip);
        }
    }
}

string TunnelFactoryCreator::get_tunnel_type(uint8_t type) 
{
    switch(type){
        case TUNNEL_TYPE_GTP: 
             return "GTP";

        default:
             return "NA"; 
    }

    return "NA"; 
}
