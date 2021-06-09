#ifndef TUNNEL_HANDLER_
#define TUNNEL_HANDLER_

#include "mbuf.h"
#include "44bsd/tx_rx_callback.h"
#include "common/Network/Packet/IPv6Header.h"
#include "stx/astf/trex_astf_topo.h"
#include "trex_global.h"

using namespace std;

#define TUNNEL_MODE_TX 1
#define TUNNEL_MODE_RX 2
#define TUNNEL_MODE_CP 4
#define TUNNEL_MODE_DP 8

typedef void* tunnel_cntxt_t;

enum tunnel_type: uint8_t {
    TUNNEL_TYPE_NONE,
    TUNNEL_TYPE_GTP
};

typedef struct client_tunnel_data {
    uint32_t client_ip;
    uint32_t teid;
    uint32_t version;
    uint8_t  type;
    ipv4_ipv6_t src;
    ipv4_ipv6_t dst;
}client_tunnel_data_t;


class CTunnelHandler {

public:
    CTunnelHandler(uint8_t mode) {
        m_mode = mode;
        m_port_id = 0;
    }
    virtual int on_tx(uint8_t dir, rte_mbuf *pkt) = 0;
    virtual int on_rx(uint8_t dir, rte_mbuf *pkt) = 0;
    virtual void* get_tunnel_context(client_tunnel_data_t *data) = 0;
    virtual void delete_tunnel_context(void *tunnel_context) = 0;
    virtual tunnel_type get_tunnel_type() = 0;
    virtual string get_tunnel_type_str() = 0;
    virtual void update_tunnel_context(client_tunnel_data_t *data, void *tunnel_context) = 0;
    virtual void parse_tunnel(const Json::Value &params, Json::Value &result, std::vector<client_tunnel_data_t> &all_msg_data) = 0;
    virtual void parse_options(){};

    void set_port(uint8_t port) {
        m_port_id = port;
    }
    virtual ~CTunnelHandler(){}
protected:
    uint8_t m_mode;
    uint8_t m_port_id;
};
#endif