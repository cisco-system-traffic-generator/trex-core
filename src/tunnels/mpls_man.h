#ifndef MPLS_MAN_H_
#define MPLS_MAN_H_

#include "mbuf.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "tunnel_handler.h"
#define ENCAPSULATION_LEN_MPLS 32

/*************** CMplsCtx ****************/
class CMplsCtx {

public:
    CMplsCtx(uint32_t label, uint8_t tc, bool s, uint8_t ttl);
    //CMplsCtx(uint16_t tci);
    void update_label_mpls_info(uint32_t label);
    void update_tc_mpls_info(uint8_t tc);
    void update_s_mpls_info(bool s);
    void update_ttl_mpls_info(uint8_t ttl);
    uint16_t get_label();
    uint8_t get_tc();
    bool get_s();
    uint8_t get_ttl();
    ~CMplsCtx();

// private:

private:
    uint32_t     m_label : 20;      // MPLS Label is 20 bits 
    uint8_t      m_tc : 3;      // Priority Code point is 3 Bites
    bool         m_s : 1;       // Bottom of Stack Indicator 1 Bites
    uint8_t      m_ttl : 8;     //
};

/*************** mpls_sts_t ****************/
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
}mpls_sts_t;


/*************** CMplsCounters ****************/
class CMplsCounters {
public:
    void clear() {
        memset(&sts, 0, sizeof(mpls_sts_t));
    }

public:
    mpls_sts_t sts;
};

/*************** CMplsMan ****************/
class CMplsMan : public CTunnelHandler {
public:
    CMplsMan(uint8_t mode);
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
    uint8_t m_tunnel_context[ENCAPSULATION_LEN_MPLS];
    CMplsCounters m_total_sts[TCP_CS_NUM];
};

#endif