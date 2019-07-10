/*
 Hanoh Haim
 Ido Barnea
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#ifndef __STATEFUL_RX_CORE_H__
#define __STATEFUL_RX_CORE_H__

#include "bp_sim.h"
#include "utl_ip.h"

#define L_PKT_SUBMODE_NO_REPLY 1
#define L_PKT_SUBMODE_REPLY 2
#define L_PKT_SUBMODE_0_SEQ 3

class TrexWatchDog;

class CLatencyPktInfo {
public:
    
    CLatencyPktInfo() {
        m_client_cfg = NULL;
        m_packet     = NULL;
    }
    
    
    void Create(class CLatencyPktMode *m_l_pkt_info);
    void Delete();

    /**
     * regular set IP
     */
    void set_ip(uint32_t                src,
                uint32_t                dst,
                uint32_t                dual_port_mask);
    
    /**
     * set IP with client clustering configuration
     */
    void set_ip(uint32_t                src,
                uint32_t                dst,
                uint32_t                dual_port_mask,
                uint8_t                 port_cnt,
                ClientCfgDB            &db);
    
    
    rte_mbuf_t * generate_pkt(int port_id,uint32_t extern_ip=0,uint32_t extern_dest_ip=0);

    CGenNode   *    getNode(){
        return (&m_dummy_node);
    }

    uint16_t get_payload_offset(void){
        return ( m_pkt_indication.getFastPayloadOffset());
    }

    uint16_t get_l4_offset(void){
        return ( m_pkt_indication.getFastTcpOffset());
    }

    uint16_t get_pkt_size(void){
        return ( m_packet->pkt_len );
    }

private:
    ipaddr_t                m_client_ip;
    ipaddr_t                m_server_ip;
    uint32_t                m_dual_port_mask;
    CGenNode                m_dummy_node;
    CFlowPktInfo            m_pkt_info;
    CPacketIndication       m_pkt_indication;
    CCapPktRaw *            m_packet;
    ClientCfgBase          *m_client_cfg;
};

#define LATENCY_MAGIC 0x12345600

struct  latency_header {
    uint64_t time_stamp;
    uint32_t magic;
    uint32_t seq;

    uint8_t get_id(){
        return( magic & 0xff);
    }
};

class CLatencyManager ;

// per port
class CCPortLatency {
public:
    bool Create(uint8_t id,
                uint16_t offset,
                uint16_t l4_offset,
                uint16_t pkt_size,
                CCPortLatency * rx_port,
                CLatencyPktMode * pkt_mode,
                CNatRxManager *   nat_manager
                );
    void Delete();
    void reset();
    void reset_seq();
    void set_epoc(uint8_t epoc){
        m_epoc = epoc;
    }

    void set_enable_none_latency_processing(bool enable){
        m_handle_none_latency =enable;
    }
    
    bool can_send_packet(int direction){
        // in icmp_reply mode, can send response from server, only after we got the relevant request
        // if we got request, we are sure to have NAT translation in place already.
        if ((CGlobalInfo::m_options.m_l_pkt_mode == L_PKT_SUBMODE_REPLY) && (direction == 1)) {
            if (m_icmp_tx_seq <= m_icmp_rx_seq)
                return(true);
            else
                return(false);
        }

        if ( !CGlobalInfo::is_learn_mode() ) {
            return(true);
        }
        return ( m_nat_can_send );
    }
    uint32_t external_nat_ip(){
        return (m_nat_external_ip);
    }

    uint32_t external_dest_ip(){
        return (m_nat_external_dest_ip);
    }

    void update_packet(rte_mbuf_t * m, int port_id);
    bool do_learn(uint32_t external_ip,
                  uint32_t external_dest_ip);
    bool check_packet(rte_mbuf_t * m, CRx_check_header * & rx_p);
    bool check_rx_check(rte_mbuf_t * m);
    bool dump_packet(rte_mbuf_t * m);
    void DumpCounters(FILE *fd);
    void dump_counters_json(std::string & json );
    void DumpShort(FILE *fd);
    void dump_json(std::string & json );
    void dump_json_v2(std::string & json );
    uint32_t get_jitter_usec(void){
        return ((uint32_t)(m_jitter.get_jitter()*1000000.0));
    }
    static void DumpShortHeader(FILE *fd);
    bool has_dummy_port_in_pair(void) { return m_dummy_port_in_pair; }
    void set_dummy_port_in_pair(void) { m_dummy_port_in_pair = true; }

    bool is_any_err(){
        if ( (m_dummy_port_in_pair || (m_tx_pkt_ok == m_rx_port->m_pkt_ok)) &&

              ((m_unsup_prot+
                m_no_magic+
                m_no_id+
                m_seq_error+
                m_l3_cs_err+
                m_l4_cs_err+
                m_length_error+
                m_no_ipv4_option+
                m_tx_pkt_err)==0) ) {
            return (false);
        }
        return (true);
    }

    uint16_t get_icmp_tx_seq() {return m_icmp_tx_seq;}
    uint16_t get_icmp_rx_seq() {return m_icmp_rx_seq;}

    // Check if packet contains latency data
    static inline bool IsLatencyPkt(uint8_t *p,uint8_t epoc) {
        if (! p)
            return false;

        latency_header * h=(latency_header *)(p);
        if ( (h->magic & 0xffffff00) != LATENCY_MAGIC+(epoc<<8) ){
            return false;
        }

        return true;
    }

private:
    std::string get_field(std::string name,float f);


private:
     CLatencyPktMode * m_pkt_mode;
     CNatRxManager *   m_nat_manager;
     CCPortLatency *   m_rx_port; /* corespond rx port  */
     bool              m_nat_learn;
     bool              m_nat_can_send;
     uint32_t          m_nat_external_ip;
     uint32_t          m_nat_external_dest_ip;
     bool     m_dummy_port_in_pair; /* one of ports on this core is dummy */
     uint32_t m_tx_seq;
     uint32_t m_rx_seq;
     uint8_t  m_pad;
     uint8_t  m_id;
     uint16_t m_payload_offset;
     uint16_t m_l4_offset;
     uint16_t m_pkt_size;
     // following two variables are for the latency ICMP reply mode.
     // if we want to pass through firewall, we want to send reply only after we got request with same seq num
     // ICMP seq num of next packet we will transmit
     uint16_t m_icmp_tx_seq;
     // ICMP seq num of last request we got
     uint16_t m_icmp_rx_seq;
     uint16_t pad1[1];
     bool     m_handle_none_latency;
     uint8_t  m_epoc;

public:
     uint64_t m_tx_pkt_ok;
     uint64_t m_tx_pkt_err;
     uint64_t m_pkt_ok;
     uint64_t m_unsup_prot;
     uint64_t m_l3_cs_err; /* ip error */
     uint64_t m_l4_cs_err; /* tcp/udp error */
     uint64_t m_no_magic;
     uint64_t m_no_id;
     uint64_t m_seq_error;
     uint64_t m_rx_check;
     uint64_t m_no_ipv4_option;
     uint64_t m_length_error;
     CRXCoreIgnoreStat m_ign_stats;
     CRXCoreIgnoreStat m_ign_stats_prev;
     CJitter         m_jitter;
	 CTimeHistogram  m_hist; /* all window */
};


class CPortLatencyHWBase {
public:
    
    /**
     * sends a packet
     *  
     * in case of a failure the user should free the mbuf 
     */
    virtual int tx(rte_mbuf_t *m) = 0;
    
    /**
     * send a packet as raw (no vlan tagging or any other 
     * modification) 
     * used mainly by stateless RX core 
     *  
     * in case of a failure the user should free the mbuf 
     */
    virtual int tx_raw(rte_mbuf_t *m) = 0;
    
    /**
     * sends a latency packet 
     * if needed, timestamp will be updated 
     *  
     * in case of a failure the user should free the mbuf
     */
    virtual int tx_latency(rte_mbuf_t *m) = 0;
    
    virtual rte_mbuf_t * rx() = 0;
    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, uint16_t nb_pkts) = 0;
    
protected:
    
    /**
     * apply HW VLAN
     */
    void apply_hw_vlan(rte_mbuf_t *m, uint8_t port_id);
    
};


class CLatencyManagerCfg {
 public:
    CLatencyManagerCfg (){
        m_max_ports=0;
        m_cps=0.0;
        memset(m_ports, 0, sizeof(m_ports));
        m_client_ip.v4=0x10000000;
        m_server_ip.v4=0x20000000;
        m_dual_port_mask=0x01000000;
    }

 public:
    uint32_t             m_max_ports;
    double               m_cps;// CPS
    CPortLatencyHWBase * m_ports[TREX_MAX_PORTS];
    ipaddr_t             m_client_ip;
    ipaddr_t             m_server_ip;
    uint32_t             m_dual_port_mask;
};

class CLatencyManagerPerPort {
public:
     CCPortLatency          m_port;
     CPortLatencyHWBase  *  m_io;
     uint32_t               m_flag;
};

class CLatencyPktMode {
 public:
    uint8_t m_submode;
    CLatencyPktMode(uint8_t submode) {m_submode = submode;}
    virtual uint8_t getPacketLen() = 0;
    virtual const uint8_t *getPacketData() = 0;
    virtual void rcv_debug_print(uint8_t *pkt) = 0;
    virtual void send_debug_print(uint8_t *pkt) = 0;
    virtual void update_pkt(uint8_t *pkt, bool is_client_to_server, uint16_t l4_len, uint16_t *tx_seq) = 0;
    virtual bool IsLatencyPkt(IPHeader *ip) = 0;
    uint8_t l4_header_len() {return 8;}
    virtual void update_recv(uint8_t *pkt, uint16_t *r_seq, uint16_t *t_seq) = 0;
    virtual uint8_t getProtocol() = 0;
    virtual ~CLatencyPktMode() {}
};

class CLatencyPktModeICMP: public CLatencyPktMode {
 public:
    CLatencyPktModeICMP(uint8_t submode) : CLatencyPktMode(submode) {}
    uint8_t getPacketLen();
    const uint8_t *getPacketData();
    void rcv_debug_print(uint8_t *);
    void send_debug_print(uint8_t *);
    void update_pkt(uint8_t *pkt, bool is_client_to_server, uint16_t l4_len, uint16_t *tx_seq);
    bool IsLatencyPkt(IPHeader *ip);
    void update_recv(uint8_t *pkt, uint16_t *r_seq, uint16_t *t_seq);
    uint8_t getProtocol() {return 0x1;}
};

class CLatencyPktModeSCTP: public CLatencyPktMode {
 public:
    CLatencyPktModeSCTP(uint8_t submode) : CLatencyPktMode(submode) {}
    uint8_t getPacketLen();
    const uint8_t *getPacketData();
    void rcv_debug_print(uint8_t *);
    void send_debug_print(uint8_t *);
    void update_pkt(uint8_t *pkt, bool is_client_to_server, uint16_t l4_len, uint16_t *tx_seq);
    bool IsLatencyPkt(IPHeader *ip);
    void update_recv(uint8_t *pkt, uint16_t *r_seq, uint16_t *t_seq);
    uint8_t getProtocol() {return 0x84;}
};

class CLatencyManager : public TrexRxCore {
public:
    bool Create(CLatencyManagerCfg * cfg);
    void Delete();
    void  reset();
    void  start(int iter, bool activate_watchdog);

    void  stop();
    bool  is_active();

    
    void set_ip(uint32_t client_ip,
                uint32_t server_ip,
                uint32_t mask_dual_port) {
        
        m_pkt_gen.set_ip(client_ip, server_ip, mask_dual_port);
    }
    
    
    void set_ip(uint32_t        client_ip,
                uint32_t        server_ip,
                uint32_t        mask_dual_port,
                ClientCfgDB    &client_cfg_db) {
        
        m_pkt_gen.set_ip(client_ip, server_ip, mask_dual_port, m_max_ports, client_cfg_db);
    }
    
    
    void Dump(FILE *fd); // dump all
    void DumpShort(FILE *fd); // dump short histogram of latency

    void DumpRxCheck(FILE *fd); // dump all
    void DumpShortRxCheck(FILE *fd); // dump short histogram of latency
    void dump_nat_flow_table(FILE *fd);
    void rx_check_dump_json(std::string & json);
    uint16_t get_latency_header_offset(){
        return ( m_pkt_gen.get_payload_offset() );
    }
    void update();
    void update_cpu_util();
    double get_cpu_util();

    void dump_json(std::string & json ); // dump to json
    void dump_json_v2(std::string & json );
    void DumpRxCheckVerification(FILE *fd,uint64_t total_tx_rx_check);
    void set_mask(uint32_t mask){
        m_port_mask=mask;
    }
    void get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff);
    double get_max_latency(void);
    double get_avr_latency(void);
    bool   is_any_error();
    uint64_t get_total_pkt();
    uint64_t get_total_bytes();
    CNatRxManager * get_nat_manager(){
        return ( &m_nat_check_manager );
    }
    CLatencyPktMode *c_l_pkt_mode;
    void add_grat_arp_src(COneIPv4Info &ip);

    
    /* TrexRxCore interface */

    void start() {
        start(0, true);
    }

    float get_pps_rate() {
        /* not supported */
        return -1;
    }
    
    
private:
    void handle_rx_msgs();
    void handle_rx_one_queue(uint8_t ti,CNodeRing * r);
    void handle_pkt_msg(uint8_t thread_id, 
                        CGenNodeLatencyPktInfo * msg);

    void  tickle();
    void  send_pkt_all_ports();
    double grat_arp_timeout();
    void  send_one_grat_arp();
    bool  try_rx();
    void  wait_for_rx_dump();
    void  handle_rx_pkt(CLatencyManagerPerPort * lp, rte_mbuf_t * m);

 private:
     pqueue_t                m_p_queue; /* priorty queue */
     volatile bool           m_is_active;
     CLatencyPktInfo         m_pkt_gen;
     CLatencyManagerPerPort  m_ports[TREX_MAX_PORTS];
     double                  m_cps;
     double                  m_delta_sec;
     uint64_t                m_start_time; // calc tick between sending
     uint32_t                m_port_mask;
     uint32_t                m_max_ports;
     std::vector<uint8_t>    m_port_ids; // (non dummy) port IDs
     RxCheckManager          m_rx_check_manager;
     CNatRxManager           m_nat_check_manager;
     CCpuUtlDp               m_cpu_dp_u;
     CCpuUtlCp               m_cpu_cp_u;
     TrexMonitor             m_monitor;
     CManyIPInfo             m_arp_info; // for grat ARP
     volatile bool           m_do_stop; 
     char                    m_false_sharing[128];
};

#endif
