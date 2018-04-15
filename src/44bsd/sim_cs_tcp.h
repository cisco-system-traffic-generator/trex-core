#ifndef BSD44_SIM_CS_TCP
#define BSD44_SIM_CS_TCP

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


#include "44bsd/tcp.h"
#include "44bsd/tcp_var.h"
#include "44bsd/tcp.h"
#include "44bsd/tcp_fsm.h"
#include "44bsd/tcp_seq.h"
#include "44bsd/tcp_timer.h"
#include "44bsd/tcp_socket.h"
#include "44bsd/tcpip.h"
#include "44bsd/tcp_dpdk.h"
#include "44bsd/flow_table.h"

#include "mbuf.h"
#include "utl_mbuf.h"
#include "utl_counter.h"
#include "astf/astf_db.h"

#include <stdlib.h>
#include <functional>
#include <common/c_common.h>
#include <common/captureFile.h>
#include <common/sim_event_driven.h>
#include <common/n_uniform_prob.h>



class CTcpSimEventStop : public CSimEventBase {

public:
     CTcpSimEventStop(double time){
         m_time =time;
     }
     virtual bool on_event(CSimEventDriven *sched,
                           bool & reschedule){
         reschedule=false;
         return(true);
     }
};



class CClientServerTcp;

class CTcpSimEventTimers : public CSimEventBase {

public:
     CTcpSimEventTimers(CClientServerTcp *p,
                        double dtime){
         m_p = p;
         m_time = dtime;
         m_d_time =dtime;
     }

     virtual bool on_event(CSimEventDriven *sched,
                           bool & reschedule);

private:
    CClientServerTcp * m_p;
    double m_d_time;
};

class CTcpSimEventRx : public CSimEventBase {

public:
    CTcpSimEventRx(CClientServerTcp *p,
                   rte_mbuf_t *m,
                   int dir,
                   double time){
        m_p = p;
        m_pkt = m;
        m_dir = dir;
        m_time = time;
    }

    virtual bool on_event(CSimEventDriven *sched,
                          bool & reschedule);

private:
    CClientServerTcp * m_p;
    int                m_dir;
    rte_mbuf_t *       m_pkt;
};


class CTcpCtxPcapWrt  {
public:
    CTcpCtxPcapWrt(){
        m_writer=NULL;
        m_raw=NULL;
    }
    ~CTcpCtxPcapWrt(){
        close_pcap_file();
    }

    bool  open_pcap_file(std::string pcap);
    void  write_pcap_mbuf(rte_mbuf_t *m,double time);

    void  close_pcap_file();


public:

   CFileWriterBase         * m_writer;
   CCapPktRaw              * m_raw;
};



class CTcpCtxDebug : public CTcpCtxCb {
public:

   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * tp,
             rte_mbuf_t *m);

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CFlowBase * flow);

   int on_redirect_rx(CTcpPerThreadCtx *ctx,
                      rte_mbuf_t *m);


public:
    CClientServerTcp  * m_p ;

};


typedef enum {  csSIM_NONE    =0,
                csSIM_RST_SYN = 0x17,
                csSIM_RST_SYN1,
                csSIM_WRONG_PORT,
                csSIM_RST_MIDDLE , // corrupt the seq number
                csSIM_RST_MIDDLE2 ,// send RST flag
                csSIM_DROP,
                csSIM_REORDER,
                csSIM_REORDER_DROP,
                csSIM_PAD,
                csSIM_RST_MIDDLE_KEEPALIVE

               } cs_sim_mode_t_;

typedef uint16_t cs_sim_mode_t ;


typedef enum {  tiTEST2    =0,
                tiHTTP     =1,
                tiHTTP_RST =2, 
                tiHTTP_FIN_ACK =3, 
                tiHTTP_CONNECT =4, 
                tiHTTP_CONNECT_RST =5, 
                tiHTTP_CONNECT_RST2 =6, 
                tiHTTP_RST_KEEP_ALIVE =7, 


               } cs_sim_test_id_t_;

typedef uint16_t cs_sim_test_id_t;

/* extenstion cfg class */
class CClientServerTcpCfgExt {
public:
    CClientServerTcpCfgExt(){
        m_rate=0.0;
        m_check_counters=false;
        m_skip_compare_file=false;
        m_seed=0x5555;
    }
    double m_rate;
    bool   m_check_counters;
    bool   m_skip_compare_file;
    uint32_t m_seed;
};

typedef std::function<int(CMbufBuffer * buf_req,
                          CMbufBuffer * buf_res,
                          CEmulAppProgram * prog_c,
                          CEmulAppProgram * prog_s,
                          uint32_t http_r_size)> method_program_cb_t;




class CClientServerTcp {
public:
    bool Create(std::string out_dir,
                std::string pcap_file);
    void Delete();

    void set_debug_mode(bool enable);
    void set_simulate_rst_error(cs_sim_mode_t  sim_type){
        m_sim_type = sim_type;
    }
    void set_drop_rate(double rate);

    /* dir ==0 , C->S 
       dir ==1   S->C */
    void on_tx(int dir,rte_mbuf_t *m);
    void on_rx(int dir,rte_mbuf_t *m);
    void set_cfg_ext(CClientServerTcpCfgExt * cfg);

public:
    int test2();
    int simple_http();
    int simple_http_rst();
    int simple_http_fin_ack();
    int simple_http_connect();
    int simple_http_connect_rst();
    int simple_http_connect_rst2();
    int simple_http_connect_rst_keepalive();



    int fill_from_file();
    bool compare(std::string exp_dir);
    void close_file();
    void set_assoc_table(uint16_t port, CEmulAppProgram *prog, CTcpTuneables *s_tune);
private: 
    int simple_http_generic(method_program_cb_t bc);
    int build_http(CMbufBuffer * buf_req,
                  CMbufBuffer * buf_res,
                  CEmulAppProgram * prog_c,
                  CEmulAppProgram * prog_s,
                  uint32_t http_r_size);
    int build_http_rst(CMbufBuffer * buf_req,
                       CMbufBuffer * buf_res,
                       CEmulAppProgram * prog_c,
                       CEmulAppProgram * prog_s,
                       uint32_t http_r_size);
    int build_http_fin_ack(CMbufBuffer * buf_req,
                           CMbufBuffer * buf_res,
                           CEmulAppProgram * prog_c,
                           CEmulAppProgram * prog_s,
                           uint32_t http_r_size);
    int build_http_connect(CMbufBuffer * buf_req,
                         CMbufBuffer * buf_res,
                         CEmulAppProgram * prog_c,
                         CEmulAppProgram * prog_s,
                         uint32_t http_r_size);


    int build_http_connect_rst(CMbufBuffer * buf_req,
                         CMbufBuffer * buf_res,
                         CEmulAppProgram * prog_c,
                         CEmulAppProgram * prog_s,
                         uint32_t http_r_size);

    int build_http_connect_rst2(CMbufBuffer * buf_req,
                         CMbufBuffer * buf_res,
                         CEmulAppProgram * prog_c,
                         CEmulAppProgram * prog_s,
                         uint32_t http_r_size);


    void dump_counters();
    bool is_drop();
public:
    std::string             m_out_dir;
    std::string             m_pcap_file;

    CTcpPerThreadCtx        m_c_ctx;  /* context */
    CTcpPerThreadCtx        m_s_ctx;
    CAstfDbRO                m_tcp_data_ro;

    CEmulAppApiImpl          m_tcp_bh_api_impl_c;
    CEmulAppApiImpl          m_tcp_bh_api_impl_s;

    CTcpCtxPcapWrt          m_c_pcap; /* capture to file */
    CTcpCtxPcapWrt          m_s_pcap;
    bool                    m_debug;
    cs_sim_mode_t           m_sim_type;
    uint16_t                m_sim_data;


    CTcpCtxDebug            m_io_debug;

    CSimEventDriven         m_sim;
    double                  m_rtt_sec;
    double                  m_tx_diff;
    double                  m_drop_ratio; /* 1 drop all, 0.0 no drop, valid in case of csSIM_DROP */
    KxuNuBinRand *          m_drop_rnd;
    KxuLCRand*              m_reorder_rnd;
    uint16_t                m_vlan;
    bool                    m_ipv6;
    bool                    m_dump_json_counters;
    bool                    m_check_counters;
    bool                    m_skip_compare_file;
    uint16_t                m_mss;

};




#endif
