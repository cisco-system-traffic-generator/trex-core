#ifndef __FLOW_TABLE_
#define __FLOW_TABLE_

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <common/closehash.h>
#include "flow_stat_parser.h"


typedef uint64_t flow_key_t; 

static inline uint32_t ft_hash_rot(uint32_t v,uint16_t r ){
    return ( (v<<r) | ( v>>(32-(r))) );
}


static inline uint32_t ft_hash1(uint64_t u ){
  uint64_t v = u * 3935559000370003845 + 2691343689449507681;

  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >>  4;

  v *= 4768777513237032717;

  v ^= v << 20;
  v ^= v >> 41;
  v ^= v <<  5;

  return (uint32_t)v;
}

static inline uint32_t ft_hash2(uint64_t in){
    uint64_t in1=in*2654435761;
    /* convert to 32bit */
    uint32_t x= (in1>>32) ^ (in1 & 0xffffffff);
    return (x);
}


class CFlowKeyTuple {
public:
    CFlowKeyTuple(){
        u.m_raw=0;
    }

    void set_ip(uint32_t ip){
        u.m_bf.m_ip = ip;
    }

    void set_port(uint16_t port){
        u.m_bf.m_port = port;
    }

    void set_proto(uint8_t proto){
        u.m_bf.m_proto = proto;
    }

    void set_ipv4(bool ipv4){
        u.m_bf.m_ipv4 = ipv4?1:0;
    }

    uint32_t get_ip(){
        return(u.m_bf.m_ip);
    }

    uint32_t get_port(){
        return(u.m_bf.m_port);
    }

    uint8_t get_proto(){
        return(u.m_bf.m_proto);
    }

    bool get_is_ipv4(){
        return(u.m_bf.m_ipv4?true:false);
    }

    uint64_t get_as_uint64(){
        return (u.m_raw);
    }

    uint32_t get_hash_worse(){
        uint16_t p = get_port();
        uint32_t res = ft_hash_rot(get_ip(),((p %16)+1)) ^ (p + get_proto()) ;
        return (res);
    }

    uint32_t get_hash(){
        return ( ft_hash2(get_as_uint64()) );
    }

    void dump(FILE *fd);
private:
    union {
        struct {
            uint64_t m_ip:32,
                     m_port:16,
                     m_proto:8,
                     m_ipv4:1,
                     m_spare:7;
        }        m_bf;
        uint64_t m_raw;
    } u;

};

/* full tuple -- for directional flow */

class CFlowKeyFullTuple {
public:
    uint8_t  m_proto;
    uint8_t  m_l3_offset;  /*IPv4/IPv6*/
    uint8_t  m_l4_offset; /* TCP/UDP*/
    uint8_t  m_l7_offset; 
    uint16_t m_l7_total_len;
    bool     m_ipv4;
};



typedef CHashEntry<flow_key_t> flow_hash_ent_t;
typedef CCloseHash<flow_key_t> flow_hash_t;


class CTcpPerThreadCtx ;
class CTcpFlow;
class CUdpFlow;
class CFlowBase;
class CEmulAppApi;
class CEmulAppProgram;

#define FT_INC_SCNT(p) {m_sts.m_sts.p += 1; }

struct  CFlowTableIntStats {

    uint32_t        m_err_client_pkt_without_flow;
    uint32_t        m_err_no_syn;
    uint32_t        m_err_len_err;
    uint32_t        m_err_fragments_ipv4_drop;
    uint32_t        m_err_no_tcp_udp;  
    uint32_t        m_err_no_template;  
    uint32_t        m_err_no_memory; 
    uint32_t        m_err_duplicate_client_tuple; 
    uint32_t        m_err_l3_cs;
    uint32_t        m_err_l4_cs;
    uint32_t        m_err_redirect_rx;
    uint32_t        m_redirect_rx_ok;
    uint32_t        m_err_rx_throttled;
    uint32_t        m_err_c_nf_throttled;
    uint32_t        m_err_s_nf_throttled;
    uint32_t        m_err_flow_overflow;
    uint32_t        m_err_c_tuple_err;
};

class  CSttFlowTableStats {

public:
    CFlowTableIntStats m_sts;
public:
    void Clear();
    void Dump(FILE *fd);
};


typedef enum { tPROCESS=0x12,
               tDROP , 
               tREDIRECT_RX_CORE,
               tEOP 
               } tcp_rx_pkt_action_t;


class CFlowTable {
public:
    bool Create(uint32_t size,
                bool client_side);

    void Delete();

    void set_tcp_api(CEmulAppApi    *   tcp_api){
         m_tcp_api = tcp_api;
    }

    void set_udp_api(CEmulAppApi    *   udp_api){
         m_udp_api = udp_api;
    }


    bool is_client_side(){
        return (m_client_side);
    }

public:
      void parse_packet(struct rte_mbuf *   mbuf,
                        CSimplePacketParser & parser,
                        CFlowKeyTuple      & tuple,
                        CFlowKeyFullTuple  & ftuple,
                        bool rx_l4_check,
                        tcp_rx_pkt_action_t & action);

      bool rx_handle_packet_tcp(CTcpPerThreadCtx * ctx,
                                struct rte_mbuf * mbuf,
                                flow_hash_ent_t * lpflow,
                                CSimplePacketParser & parser,
                                CFlowKeyTuple & tuple,
                                CFlowKeyFullTuple & ftuple,
                                uint32_t  hash
                                );

      void process_udp_packet(CTcpPerThreadCtx * ctx,
                              CUdpFlow *  flow,
                              struct rte_mbuf * mbuf,
                              UDPHeader    * lpUDP,
                              CFlowKeyFullTuple &ftuple);


      bool rx_handle_packet_udp(CTcpPerThreadCtx * ctx,
                                struct rte_mbuf * mbuf,
                                flow_hash_ent_t * lpflow,
                                CSimplePacketParser & parser,
                                CFlowKeyTuple & tuple,
                                CFlowKeyFullTuple & ftuple,
                                uint32_t  hash
                                );

      bool rx_handle_packet(CTcpPerThreadCtx * ctx,
                            struct rte_mbuf * mbuf,
                            bool is_idle);

      /* insert new flow - usualy client */
      bool insert_new_flow(CFlowBase *  flow,
                           CFlowKeyTuple  & tuple);

      void handle_close(CTcpPerThreadCtx * ctx,
                        CFlowBase  * flow,
                        bool remove_from_ft);

      void process_tcp_packet(CTcpPerThreadCtx * ctx,
                              CTcpFlow *  flow,
                              struct rte_mbuf * mbuf,
                              TCPHeader    * lpTcp,
                              CFlowKeyFullTuple &ftuple);

      void dump(FILE *fd);

      void inc_rx_throttled_cnt(){
          m_sts.m_sts.m_err_rx_throttled++;
      }


      void inc_err_c_new_tuple_err_cnt(){
          m_sts.m_sts.m_err_c_tuple_err++;
      }

      void inc_err_c_new_flow_throttled_cnt(){
          m_sts.m_sts.m_err_c_nf_throttled++;
      }

      void inc_err_s_new_flow_throttled_cnt(){
          m_sts.m_sts.m_err_s_nf_throttled++;
      }

      void inc_flow_overflow_cnt(){
          m_sts.m_sts.m_err_flow_overflow++;
      }

      bool flow_table_resource_ok(){
          return(m_ft.get_count() < m_ft.get_hash_size()?true:false);
      }
public:

void generate_rst_pkt(CTcpPerThreadCtx * ctx,
                      uint32_t src,
                      uint32_t dst,
                      uint16_t src_port,
                      uint16_t dst_port,
                      uint16_t vlan,
                      bool is_ipv6,
                      TCPHeader    * lpTcp,
                      uint8_t *   pkt,
                      IPv6Header *    ipv6,
                      CFlowKeyFullTuple &ftuple);


    CTcpFlow * alloc_flow(CTcpPerThreadCtx * ctx,
                          uint32_t src,
                          uint32_t dst,
                          uint16_t src_port,
                          uint16_t dst_port,
                          uint16_t vlan,
                          bool is_ipv6);

    CUdpFlow * alloc_flow_udp(CTcpPerThreadCtx * ctx,
                              uint32_t src,
                              uint32_t dst,
                              uint16_t src_port,
                              uint16_t dst_port,
                              uint16_t vlan,
                              bool is_ipv6,
                              bool client);


    void free_flow(CFlowBase * flow);

    void set_debug(bool enable){
        m_verbose = enable;
    }
public:
    void terminate_all_flows();
    void terminate_flow(CTcpPerThreadCtx * ctx,
                        CFlowBase * flow);


private:
    void redirect_to_rx_core(CTcpPerThreadCtx * ctx,
                             struct rte_mbuf * mbuf);

    __attribute__ ((noinline)) void rx_non_process_packet(tcp_rx_pkt_action_t action,
                                                          CTcpPerThreadCtx * ctx,
                                                          struct rte_mbuf * mbuf);


private: 
    void reset_stats();
public:
      CSttFlowTableStats m_sts;

private:
    bool            m_verbose;
    bool            m_client_side;
    flow_hash_t     m_ft;

    CEmulAppApi    *   m_tcp_api;
    CEmulAppApi    *   m_udp_api;

};




#endif


