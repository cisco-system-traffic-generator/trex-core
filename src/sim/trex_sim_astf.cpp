/*
 Hanoch Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

#include "trex_sim.h"
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
#include "44bsd/sim_cs_tcp.h"
#include "stt_cp.h"


class CMbufQueue {
public:
    CMbufQueue(){
    }
    ~CMbufQueue();
    void add_head(rte_mbuf_t *m);
    bool is_empty();
    rte_mbuf_t * remove_tail();

private:
    std::vector<rte_mbuf_t *> m_que;
};


CMbufQueue::~CMbufQueue(){
    int i;
    for (i=0;i<m_que.size(); i++) {
        rte_pktmbuf_free(m_que[i]);
    }
    m_que.clear();
}

static rte_mbuf_t * utl_rte_convert_tx_rx_mbuf(rte_mbuf_t *m){

    rte_mbuf_t * mc;
    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);
    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    rte_pktmbuf_free(m); /* free original */
    return (mc);
}

void CMbufQueue::add_head(rte_mbuf_t *m){
    m_que.push_back(m);
}


bool CMbufQueue::is_empty(){
    return(m_que.size()==0?true:false);
}

rte_mbuf_t * CMbufQueue::remove_tail(){
    rte_mbuf_t * m=m_que[0];
    m_que.erase(m_que.begin());
    return(m);
}


class CCoreEthIFTcpSim : public CErfIFStl {
public:
    uint16_t     rx_burst(pkt_dir_t dir,
                          struct rte_mbuf **rx_pkts,
                          uint16_t nb_pkts);

    virtual int send_node(CGenNode *node);
    virtual void set_rx_burst_time(double time);

private:
    void   write_pkt_to_file(uint8_t dir,
                             struct rte_mbuf *m,
                             double time);

private:
    CMbufQueue         m_rx_que[2];
    double             m_rx_time;
};

void   CCoreEthIFTcpSim::write_pkt_to_file(uint8_t dir,
                                   struct rte_mbuf *m,
                                   double time){
    if (!m_preview_mode->getFileWrite()) {
        return;
    }
    fill_pkt(m_raw,m);
    CPktNsecTimeStamp t_c(time);
    m_raw->time_nsec = t_c.m_time_nsec;
    m_raw->time_sec  = t_c.m_time_sec;
    uint8_t p_id = (uint8_t)dir;
    m_raw->setInterface(p_id);
    int rc = write_pkt(m_raw);
    assert(rc == 0);
}



/* set the time for simulation */
void CCoreEthIFTcpSim::set_rx_burst_time(double time){
    m_rx_time=time;
}


uint16_t   CCoreEthIFTcpSim::rx_burst(pkt_dir_t dir,
                                      struct rte_mbuf **rx_pkts,
                                      uint16_t nb_pkts){

    uint16_t res=0;
    CMbufQueue *lp=&m_rx_que[dir];
    int i;
    for (i=0; i<nb_pkts; i++) {
        if (lp->is_empty()) {
            break;
        }
        res++;
        rx_pkts[i]=lp->remove_tail();
        if (dir==0) {
            write_pkt_to_file(dir,rx_pkts[i],m_rx_time);
        }
    }

    return(res);
}


int CCoreEthIFTcpSim::send_node(CGenNode *node){
    CNodeTcp * node_tcp = (CNodeTcp *) node;
    if (!m_preview_mode->getFileWrite()) {
        return (0);
    }
     rte_mbuf_t   *m  = node_tcp->mbuf;
     pkt_dir_t    dir = node_tcp->dir;
     double       time = node_tcp->sim_time;

     if (dir==0) {
         write_pkt_to_file(dir,m,time);
     }
     /* convert to rx mbuf */
     rte_mbuf_t *mrx=utl_rte_convert_tx_rx_mbuf(m);
     /* add to remote queue */
     m_rx_que[dir^1].add_head(mrx);
     return (0);
}


static void dump_tcp_counters(CTcpPerThreadCtx  *      c_ctx,
                              CTcpPerThreadCtx  *      s_ctx,
                              bool dump_json){

    CSTTCp stt_cp;
    stt_cp.Create();
    stt_cp.Init();
    stt_cp.m_init=true;
    stt_cp.Add(TCP_CLIENT_SIDE,c_ctx);
    stt_cp.Add(TCP_SERVER_SIDE,s_ctx);
    stt_cp.Update();
    stt_cp.DumpTable();
    std::string json;
    stt_cp.dump_json(json);
    if (dump_json){
      fprintf(stdout,"json-start \n");
      fprintf(stdout,"%s\n",json.c_str());
      fprintf(stdout,"json-end \n");
    }
    stt_cp.Delete();
}



static int load_list_of_cap_files(CParserOption * op,
                                  asrtf_args_t * args){
    /* set TCP mode */
    op->m_op_mode = CParserOption::OP_MODE_ASTF_BATCH;

    CFlowGenList fl;
    fl.Create();
    fl.load_astf();
    
    if (op->client_cfg_file != "") {
        try {
            fl.load_client_config_file(op->client_cfg_file);
            // The simulator only test MAC address configs, so this parameter is not used
            CManyIPInfo pretest_result;
            fl.set_client_config_resolved_macs(pretest_result);
        } catch (const std::runtime_error &e) {
            std::cout << "\n*** " << e.what() << "\n\n";
            exit(-1);
        }
        CGlobalInfo::m_options.preview.set_client_cfg_enable(true);
    }

    try {
        CGlobalInfo::m_options.verify();
    } catch (const std::runtime_error &e) {
        std::cout << "\n*** " << e.what() << "\n\n";
        exit(-1);
    }

    std::string json_file_name = CGlobalInfo::m_options.astf_cfg_file;
    fprintf(stdout, "Using json file %s\n", json_file_name.c_str());

    /* load json */
    if (! CAstfDB::instance()->parse_file(json_file_name) ) {
       exit(-1);
    }

    CAstfDB::instance()->set_client_cfg_db(&fl.m_client_config_info);

    uint32_t start=    os_get_time_msec();

    CCoreEthIFTcpSim tcp_erf_vif;

    fl.generate_p_thread_info(1);
    CFlowGenListPerThread   * lpt;
    lpt=fl.m_threads_info[0];
    lpt->set_vif(&tcp_erf_vif);

    if ( (op->preview.getVMode() >1)  || op->preview.getFileWrite() ) {
        lpt->start_sim(op->out_file,op->preview);
    }

    lpt->m_node_gen.DumpHist(stdout);


    dump_tcp_counters(lpt->m_c_tcp,
                      lpt->m_s_tcp,
                      args->dump_json);
                                      
    uint32_t stop=    os_get_time_msec();
    printf(" took time: %umsec (freq: %u) \n", stop-start, os_get_time_freq());

    lpt->Delete();
    fl.Delete();
    CAstfDB::free_instance();
    return (0);
}


int SimAstf::run() {
     
    assert( CMsgIns::Ins()->Create(4) );
    
    STXSimGuard guard(new TrexAstfBatch(TrexSTXCfg(), nullptr));
    
    try {
        return load_list_of_cap_files(&CGlobalInfo::m_options, args);
    } catch (const std::runtime_error &e) {
        std::cout << "\n*** " << e.what() << "\n\n";
        exit(-1);
    }
}



int SimAstfSimple::run() {
    CParserOption * po=&CGlobalInfo::m_options;
    bool rc = CAstfDB::instance()->parse_file(CGlobalInfo::m_options.astf_cfg_file);
    assert(rc);
    CClientServerTcp *lpt = new CClientServerTcp;
    // run
    lpt->Create("", CGlobalInfo::m_options.out_file);
    lpt->set_debug_mode(true);
    lpt->m_dump_json_counters = args->dump_json;
    lpt->m_ipv6 = po->preview.get_ipv6_mode_enable();

    if (args->sim_mode){
        lpt->set_simulate_rst_error(args->sim_mode);
    }

    /* shaper */
    if (args->m_shaper_kbps) {
        if (args->m_shaper_size==0){
            args->m_shaper_size= 100*1024*1024; /* default */
        }
        lpt->set_shaper(args->m_shaper_kbps,args->m_shaper_size);
    }

    /* rtt */
    if (args->m_rtt_usec) {
        lpt->set_rtt(args->m_rtt_usec);
    }

    /* drop */
    if (args->m_drop_prob_precent>0.0) {
        lpt->set_simulate_rst_error(csSIM_DROP);
        lpt->set_drop_rate((args->m_drop_prob_precent/100.0));
    }

    if (args->sim_arg>0.0){
        CClientServerTcpCfgExt cfg;
        cfg.m_rate=args->sim_arg;
        cfg.m_check_counters=true;
        lpt->set_cfg_ext(&cfg);
    }

    lpt->fill_from_file();

    // free stuff
    lpt->close_file();
    lpt->Delete();
    delete lpt;
    CAstfDB::free_instance();
    return 0;
}
