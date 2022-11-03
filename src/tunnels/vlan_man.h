#ifndef VLAN_MAN_H_
#define VLAN_MAN_H_

#include "mbuf.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "tunnel_handler.h"
#define ENCAPSULATION_LEN_VLAN 32

/*************** CVlanCtx ****************/
class CVlanCtx {

public:
    CVlanCtx(uint16_t vid, uint8_t pcp, bool dei = false);
    CVlanCtx(uint16_t tci);
    void update_vid_vlan_info(uint16_t vid);
    uint16_t get_vid();
    uint8_t get_pcp();
    bool get_dei();
    ~CVlanCtx();

// private:

private:
    uint8_t      m_pcp : 3;       // Priority Code point is 3 Bytes
    bool         m_dei : 1;       // Drop Elligible Indicator 1 Bytes
    uint16_t     m_vid : 12;      // VLAN ID is 12 bytes 
};

/*************** vlan_sts_t ****************/
typedef struct{
    uint64_t m_tunnel_encapsulated;
    uint64_t m_tunnel_decapsulated;
    uint64_t m_err_tunnel_drops;
    uint64_t m_err_tunnel_no_context;
    uint64_t m_err_tunnel_dir;
    uint64_t m_err_pkt_null;
    uint64_t m_err_buf_addr_null;
    uint64_t m_err_l3_hdr;
    uint64_t m_err_headroom;
    uint64_t m_err_l4_hdr;
    uint64_t m_err_buf_len;
    uint64_t m_err_dst_port;
    uint64_t m_err_vlan_type;
    uint64_t m_err_data_len;
    uint64_t m_encapsulated_hdr;
}vlan_sts_t;


/*************** CVlanCounters ****************/
class CVlanCounters {
public:
    void clear() {
        memset(&sts, 0, sizeof(vlan_sts_t));
    }

public:
    vlan_sts_t sts;
};

/*************** CVlanMan ****************/
class CVlanMan : public CTunnelHandler {
public:
    CVlanMan(uint8_t mode);
    int on_tx(uint8_t dir, rte_mbuf *pkt);
    int on_rx(uint8_t dir, rte_mbuf *pkt);
    void* get_tunnel_ctx(client_tunnel_data_t *data);
    void delete_tunnel_ctx(void *tunnel_context);
    tunnel_type get_tunnel_type();
    string get_tunnel_type_str();
    void update_tunnel_ctx(client_tunnel_data_t *data, void *tunnel_context);
    void parse_tunnel(const Json::Value &params, Json::Value &result, std::vector<client_tunnel_data_t> &all_msg_data);
    void* get_opposite_ctx();
    tunnel_ctx_del_cb_t get_tunnel_ctx_del_cb();
    bool is_tunnel_supported(std::string &error_msg);
    const meta_data_t* get_meta_data();
    dp_sts_t get_counters();

private:
    int prepend(rte_mbuf *pkt, uint8_t dir);
    int adjust(rte_mbuf *pkt, uint8_t dir);

private:
    uint8_t m_tunnel_context[ENCAPSULATION_LEN_VLAN];
    CVlanCounters m_total_sts[TCP_CS_NUM];
};

#endif