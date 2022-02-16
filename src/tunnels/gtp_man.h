#ifndef GTP_MAN_H_
#define GTP_MAN_H_

#include "mbuf.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "tunnel_handler.h"
#define ENCAPSULATION_LEN 36
#define ENCAPSULATION6_LEN 56


/*************** CGtpuCtx ****************/
class CGtpuCtx {

public:
    /** The ipv4 and ipv6 have to be in network order*/
    CGtpuCtx(uint32_t teid, ipv4_addr_t src_ip, ipv4_addr_t dst_ip, uint16_t src_port);
    CGtpuCtx(uint32_t teid, ipv6_addr_t* src_ipv6, ipv6_addr_t* dst_ipv6, uint16_t src_port);
    void update_ipv4_gtpu_info(uint32_t teid, ipv4_addr_t src_ip, ipv4_addr_t dst_ip, uint16_t src_port);
    void update_ipv6_gtpu_info(uint32_t teid, ipv6_addr_t* src_ipv6, ipv6_addr_t* dst_ipv6, uint16_t src_port);
    int32_t get_teid();
    ipv4_addr_t get_src_ipv4();
    ipv4_addr_t get_dst_ipv4();
    void get_src_ipv6(ipv6_addr_t* src_ipv6);
    void get_dst_ipv6(ipv6_addr_t* src_ipv6);
    void* get_outer_hdr();
    bool is_ipv6();
    ~CGtpuCtx();

private:
    void store_ipv4_members(uint32_t teid, ipv4_addr_t src_ip, ipv4_addr_t dst_ip, uint16_t src_port);
    void store_ipv6_members(uint32_t teid, ipv6_addr_t* src_ipv6, ipv6_addr_t* dst_ipv6, uint16_t src_port);
    void store_ipv4_gtpu_info();
    void store_ipv6_gtpu_info();
    void store_udp_gtpu(void *udp);

private:
    uint32_t     m_teid;
    uint16_t     m_src_port;
    uint8_t      *m_outer_hdr;
    bool         m_ipv6_set;
    ipv4_ipv6_t  m_src;
    ipv4_ipv6_t  m_dst;
};


/*************** CGtpuMan ****************/

class CGtpuMan : public CTunnelHandler {

public:
    CGtpuMan(uint8_t mode) : CTunnelHandler(mode){}
    int on_tx(uint8_t dir, rte_mbuf *pkt);
    int on_rx(uint8_t dir, rte_mbuf *pkt);
    void* get_tunnel_ctx(client_tunnel_data_t *data);
    void delete_tunnel_ctx(void *tunnel_context);
    tunnel_type get_tunnel_type();
    string get_tunnel_type_str();
    void update_tunnel_ctx(client_tunnel_data_t *data, void *tunnel_context);
    void parse_tunnel(const Json::Value &params, Json::Value &result, std::vector<client_tunnel_data_t> &all_msg_data);
    void parse_tunnel_delete(const Json::Value &params, Json::Value &result, std::vector<client_tunnel_delete_data_t> &all_msg_data);
    void* get_opposite_ctx();
    tunnel_ctx_del_cb_t get_tunnel_ctx_del_cb();
    bool is_tunnel_supported(std::string &error_msg);

private:
    int prepend(rte_mbuf *pkt);
    int adjust(rte_mbuf *pkt);
    int prepend_ipv4_tunnel(rte_mbuf *pkt, uint8_t l4_offset, uint16_t inner_cs, CGtpuCtx *gtp_context);
    int prepend_ipv6_tunnel(rte_mbuf *pkt, uint8_t l4_offset, uint16_t inner_cs, CGtpuCtx *gtp_context);
    int adjust_ipv4_tunnel(rte_mbuf *pkt, void *eth, uint8_t l3_offset);
    int adjust_ipv6_tunnel(rte_mbuf *pkt, void *eth, uint8_t l3_offset);
    int validate_gtpu_udp(void *udp);

private:
    uint8_t m_tunnel_context[ENCAPSULATION6_LEN];
};
#endif
