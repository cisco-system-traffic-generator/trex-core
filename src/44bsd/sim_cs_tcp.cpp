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

#include "sim_cs_tcp.h"
#include <astf/astf_db.h>
#include <astf/astf_template_db.h>
#include "trex_global.h"
#include "stt_cp.h"

#define CLIENT_SIDE_PORT        1025
#define DEFAULT_WIN 32768
#define DEFAULT_MSS 1460


bool CTcpSimShaper::Create(uint32_t queue_size,
                           double rate_bps,
                           CClientServerTcp *p,
                           int dir
                           ){
    m_state_on=false;
    m_max_queue_size=queue_size;
    m_rate = rate_bps;
    m_cc =0;
    m_dir = dir;
    m_p = p;
    return (true);
}

void CTcpSimShaper::Delete(){
}

void CTcpSimShaper::trigger_next(){
    bool trigger_next=false;


    if (m_q.size()) {
        trigger_next=true;
        if (!m_state_on) {
            m_state_on=true;
        }
     }else{
         if (m_state_on) {
             m_state_on=false;
         }
     }

    if (trigger_next) {
        uint32_t pktlen;
        double ct=m_p->m_sim.get_time();
        rte_mbuf_t * m_head=m_q[0];
        pktlen = m_head->pkt_len;

        m_cc-=pktlen;
        m_q.erase(m_q.begin());
        //printf(" [%d] queue_size : %dB (%d)\n",m_dir,m_cc,(int)m_q.size());

        ct += (double)(pktlen+20)/(m_rate/8); /* time to get it out */
        /* start next event */
        m_p->m_sim.add_event( new CTcpSimEventTxPkt(m_p,m_head,m_dir,ct) );
    }
}


void CTcpSimShaper::add_packet(rte_mbuf_t *m){

    uint32_t pktlen = m->pkt_len;
    if ( (m_cc + pktlen) > m_max_queue_size ){
        rte_pktmbuf_free(m); /* drop the packet */
        return;
    }

    m_cc+=pktlen;
    //printf(" queue add [%d] queue_size : %d \n",m_dir,m_cc);
    m_q.push_back(m);

    if (!m_state_on) {
        trigger_next();
    }
}

void CTcpSimShaper::update_config(uint32_t size,double rate){
    m_max_queue_size=size;
    m_rate = rate;
}


void CTcpSimShaper::next_packet(){
    trigger_next();
}




void  CTcpCtxPcapWrt::write_pcap_mbuf(rte_mbuf_t *m,
                                      double time){

    char *p;
    uint32_t pktlen=m->pkt_len;
    if (pktlen>MAX_PKT_SIZE) {
        printf("ERROR packet size is bigger than 9K \n");
    }

    p=utl_rte_pktmbuf_to_mem(m);
    memcpy(m_raw->raw,p,pktlen);
    m_raw->pkt_cnt++;
    m_raw->pkt_len =pktlen;
    m_raw->set_new_time(time);

    assert(m_writer);
    bool res=m_writer->write_packet(m_raw);
    if (res != true) {
        fprintf(stderr,"ERROR can't write to cap file");
    }
    free(p);
}


bool  CTcpCtxPcapWrt::open_pcap_file(std::string pcap){

    m_writer = CCapWriterFactory::CreateWriter(LIBPCAP,(char *)pcap.c_str());
    if (m_writer == NULL) {
        fprintf(stderr,"ERROR can't create cap file %s ",(char *)pcap.c_str());
        return (false);
    }
    m_raw = new CCapPktRaw();
    return(true);
}

void  CTcpCtxPcapWrt::close_pcap_file(){
    if (m_raw){
        delete m_raw;
        m_raw = NULL;
    }
    if (m_writer) {
        delete m_writer;
        m_writer = NULL;
    }
}



static rte_mbuf_t * utl_rte_convert_tx_rx_mbuf(rte_mbuf_t *m){

    rte_mbuf_t * mc;
    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);
    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    rte_pktmbuf_free(m); /* free original */
    return (mc);
}




int CTcpCtxDebug::on_flow_close(CTcpPerThreadCtx *ctx,
                                CFlowBase * flow){
    /* bothing to do */

    return(0);
}


int CTcpCtxDebug::on_redirect_rx(CTcpPerThreadCtx *ctx,
                                 rte_mbuf_t *m){
    assert(0);
    return(0);
}


int CTcpCtxDebug::on_tx(CTcpPerThreadCtx *ctx,
                        struct tcpcb * tp,
                        rte_mbuf_t *m){
    int dir=1;
    assert(tp->m_flow);

    if (tp->m_flow->m_template.m_src_port==CLIENT_SIDE_PORT) {
        dir=0;
    }
    hw_checksum_sim(m);
    rte_mbuf_t *m_rx= utl_rte_convert_tx_rx_mbuf(m);

    m_p->on_tx(dir,m_rx);
    return(0);
}


void CClientServerTcp::set_cfg_ext(CClientServerTcpCfgExt * cfg){
    if (cfg->m_rate>0.0){
        set_drop_rate(cfg->m_rate);
        m_drop_rnd->setSeed(cfg->m_seed);
    }

    m_check_counters = cfg->m_check_counters;
    m_skip_compare_file = cfg->m_skip_compare_file;
}



void CClientServerTcp::set_drop_rate(double rate){
    assert(rate<0.99);
    assert(rate>0.0001);
    m_drop_ratio=rate;
    if (m_drop_rnd) {
        delete m_drop_rnd;
    }
    m_drop_rnd = new KxuNuBinRand(rate);
}


void CClientServerTcp::set_shaper(uint32_t kbps,uint32_t size){
    int i;
    if (kbps==0){
        m_shaper_enable=false;
        return;
    }
    m_shaper_enable=true;
    for (i=0; i<2; i++) {
        m_shapers[i].update_config(size,(double)(kbps*1000));
    }
}


void CClientServerTcp::set_rtt(uint32_t rtt_usec){
    m_rtt_sec =(double)rtt_usec/(1000000.0);
}



bool CClientServerTcp::is_drop(){
    assert(m_drop_rnd);
    return(m_drop_rnd->getRandom());
}



void CClientServerTcp::set_debug_mode(bool enable){
    m_debug=enable;
}

static bool compare_two_pcap_files(std::string c1,
                            std::string c2,
                            double dtime){
    CErfCmp cmp;
    cmp.dump = 1;
    cmp.d_sec = dtime;
    return (cmp.compare(c1, c2));
}

bool CClientServerTcp::compare(std::string exp_dir){
    bool res;
    std::string o_c= m_out_dir+m_pcap_file+"_c.pcap";
    std::string e_c= exp_dir+"/"+m_pcap_file+"_c.pcap";

    res=compare_two_pcap_files(o_c,e_c,0.001);
    if (!res) {
        return (res);
    }

    std::string o_s= m_out_dir+m_pcap_file+"_s.pcap" ;
    std::string e_s= exp_dir+"/"+m_pcap_file+"_s.pcap";

    res=compare_two_pcap_files(o_s,e_s,0.001);
    return (res);
}


bool CClientServerTcp::Create(std::string out_dir,
                              std::string pcap_file){

    m_shaper_enable =false;
    m_debug=false;
    m_sim_type=csSIM_NONE;
    m_sim_data=0;
    m_io_debug.m_p = this;
    m_tx_diff =0.0;
    m_vlan =0;
    m_ipv6=false;
    m_dump_json_counters=false;
    m_check_counters=false;
    m_skip_compare_file=false;
    m_mss=0;
    m_drop_rnd = nullptr;
    m_gen = nullptr;

    m_rtt_sec =0.05; /* 50msec */
    m_drop_ratio =0.0;

    if (!out_dir.empty()) {
        m_out_dir = out_dir + "/";
    } else {
        m_out_dir = "";
    }

    m_pcap_file = pcap_file;
    m_reorder_rnd  = new KxuLCRand();

    m_c_pcap.open_pcap_file(m_out_dir+pcap_file+"_c.pcap");

    m_s_pcap.open_pcap_file(m_out_dir+pcap_file+"_s.pcap");

    m_c_ctx.Create(100,true);
    m_c_ctx.tcp_iss=0x12121212; /* for testing start from the same value */
    m_c_ctx.set_cb(&m_io_debug);
    m_c_ctx.set_offload_dev_flags(OFFLOAD_TX_CHKSUM|OFFLOAD_RX_CHKSUM);

    m_s_ctx.Create(100,false);
    m_s_ctx.tcp_iss=0x21212121; /* for testing start from the same value */
    m_s_ctx.set_cb(&m_io_debug);
    m_s_ctx.set_offload_dev_flags(OFFLOAD_TX_CHKSUM|OFFLOAD_RX_CHKSUM);



    int i;
    for (i=0; i<2; i++) {
        m_shapers[i].Create(100*1024*1024,10*1000*1000,this,i);
    }
    return(true);
}

// Set fictive association table to be used by server side in simulation
void CClientServerTcp::set_assoc_table(uint16_t port, CEmulAppProgram *prog, CTcpTuneables *s_tune) {
    m_tcp_data_ro.set_test_assoc_table(port, prog, s_tune);
    m_s_ctx.set_template_ro(&m_tcp_data_ro);
    /* We work under the assumption that when you call this we don't need to resize. */
}

void CClientServerTcp::on_tx(int dir,
                             rte_mbuf_t *m){
    on_tx_shaper(dir,m); /* start shaping */
}


void CClientServerTcp::on_tx_transmit(int dir,
                             rte_mbuf_t *m){

    if (!m_shaper_enable){
        m_tx_diff+=1/1000000.0;  /* to make sure there is no out of order */
    }
    double t=m_sim.get_time()+m_tx_diff;

    if (!m_shaper_enable){
      write_pcap(dir,m,t);
    }


    bool drop=false;
    bool reorder=false;
    /* simulate drop/reorder/ corruption HERE !! */
    if (m_sim_type > 0) {
        char * p=rte_pktmbuf_mtod(m,char *);
        TCPHeader * tcp=(TCPHeader *)(p+14+20);
        //IPHeader * ip=(IPHeader *)(p+14);

        if ( m_sim_type == csSIM_REORDER_DROP ){
            if (is_drop()){
                drop=true;
            }
            if (is_drop()){
                reorder=true;
            }
        }

        if (m_sim_type == csSIM_PAD ){
            if (m->pkt_len<1000) {
                char *p=rte_pktmbuf_append(m, 12);
                assert(p!=NULL);
                memset(p,0xaa,12);
            }
        }

        if (m_sim_type == csSIM_DROP ){
            if (is_drop()){
                drop=true;
            }
        }

        if (m_sim_type == csSIM_REORDER ){
            if (is_drop()){
                reorder=true;
            }
        }

        if ( m_sim_type == csSIM_RST_SYN ){
            /* simulate RST */
            if (dir==1) {
                if (tcp->getSynFlag()){
                    tcp->setAckNumber(0x111111);
                    tcp->setSeqNumber(0x111111);
                }
            }
        }

        if ( m_sim_type == csSIM_WRONG_PORT ){
            if (dir==0) {
                /* c->s change port from 80 to 81 */
                if (tcp->getSynFlag()){
                    tcp->setDestPort(81);
                }
            }else{
                if (tcp->getResetFlag()){
                    tcp->setSourcePort(80);
                }
            }
        }

        /* send RST/ACK on first message */
        if ( m_sim_type == csSIM_RST_SYN1 ){
            if (dir==1) {
                if (m_sim_data==0){
                    if (tcp->getSynFlag()){
                        tcp->setFlag(TCPHeader::Flag::RST | TCPHeader::Flag::ACK);
                        m_sim_data=1;
                    }
                }else{
                    drop=true;
                }
            }
        }

        if ( m_sim_type == csSIM_RST_MIDDLE ){
            if (dir==1) {
                if ( (t>0.4) && (t<0.6)){
                    tcp->setAckNumber(0x111111);
                    tcp->setSeqNumber(0x111111);
                }
            }
        }
        if ( m_sim_type == csSIM_RST_MIDDLE2 ){
            if (dir==1) {
                if ( (m_sim_data==0) && (t> 0.4) && (t> 0.5)){
                    m_sim_data=1;
                    tcp->setResetFlag(true);
                }
            }
        }


        if ( m_sim_type == csSIM_RST_MIDDLE_KEEPALIVE ){
            if (dir==0) {
                if ( (t>0.4) && (m_sim_data==0) ) {
                    m_sim_data=1;
                    printf(" send RST to server \n");
                    tcp->setResetFlag(true);
                }
            }
            if (dir==1) { /* server to client */
                if ( (t>0.4)  && (!tcp->getResetFlag()) ) {
                  drop=true;
                }
            }

        }
    }

    if (drop==false){
        /* move the Tx packet to Rx side of server */
        double reorder_time=0.0;
        if (reorder) {
            reorder_time=m_reorder_rnd->getRandomInRange(0,m_rtt_sec*2);
        }
        m_sim.add_event( new CTcpSimEventRx(this,m,dir^1,(reorder_time+t+(m_rtt_sec/2.0) )) );
    }else{
        rte_pktmbuf_free(m);
    }
}


bool CTcpSimEventTxPkt::on_event(CSimEventDriven *sched,
                                 bool & reschedule){

    m_p->on_tx_transmit(m_dir,m_pkt);
    m_p->m_shapers[m_dir].next_packet();
    return(false);
}





bool CTcpSimEventRx::on_event(CSimEventDriven *sched,
                              bool & reschedule){
    reschedule=false;
    m_p->on_rx(m_dir,m_pkt);
    return(false);
}

bool CTcpSimEventTimers::on_event(CSimEventDriven *sched,
                                  bool & reschedule){
     reschedule=true;
     m_time +=m_d_time;
     m_p->m_c_ctx.timer_w_on_tick();
     m_p->m_s_ctx.timer_w_on_tick();
     return(false);
 }




// goes to shaper first
void CClientServerTcp::on_tx_shaper(int dir, rte_mbuf_t *m){
    if (m_shaper_enable){
        write_pcap(dir,m,m_sim.get_time());
        m_shapers[dir].add_packet(m);
    }else{
        on_tx_transmit(dir,m);
    }
}


void CClientServerTcp::on_rx(int dir,
                             rte_mbuf_t *m){

    /* write RX side */
    double t = m_sim.get_time();
    CTcpPerThreadCtx * ctx;

    if (dir==1) {
        ctx =&m_s_ctx,
        m_s_pcap.write_pcap_mbuf(m,t);
    }else{
        ctx =&m_c_ctx;
        m_c_pcap.write_pcap_mbuf(m,t);
    }

    ctx->m_ft.rx_handle_packet(ctx,m,false,0);
}


void CClientServerTcp::Delete(){
    m_c_ctx.Delete();
    m_s_ctx.Delete();
    m_tcp_data_ro.Delete();
    if (m_drop_rnd){
        delete m_drop_rnd;
    }
    delete m_reorder_rnd;
    if ( CAstfDB::has_instance() ) {
        CAstfDB::instance()->clear_db_ro_rw(m_gen);
        CAstfDB::free_instance();
    }
    delete m_gen;
}


int CClientServerTcp::test2(){


    CMbufBuffer * buf;
    CEmulAppProgram * prog_c;
    CEmulAppProgram * prog_s;
    CTcpFlow          *  c_flow;
    CTcpTuneables *s_tune;

    CEmulApp * app_c;
    //CEmulApp * app_s;
    CEmulAppCmd cmd;

    uint32_t tx_num_bytes=100*1024;

    c_flow = m_c_ctx.m_ft.alloc_flow(DEFAULT_PROFILE_CTX(&m_c_ctx),0x10000001,0x30000001,1025,80,m_vlan,false);
    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(0x10000001);
    c_tuple.set_port(1025);
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

    assert(m_c_ctx.m_ft.insert_new_flow(c_flow,c_tuple)==true);


    printf("client  %p \n",c_flow);

    app_c = &c_flow->m_app;

        /* IW=1 */
    m_c_ctx.tcp_initwnd = m_c_ctx.tcp_mssdflt;
    m_s_ctx.tcp_initwnd = m_c_ctx.tcp_mssdflt;


    /* CONST */
    buf = new CMbufBuffer();
    prog_c = new CEmulAppProgram();
    prog_s = new CEmulAppProgram();
    s_tune = new CTcpTuneables();

    utl_mbuf_buffer_create_and_fill(0,buf,2048,tx_num_bytes);


    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf;

    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = tx_num_bytes;

    prog_s->add_cmd(cmd);


    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,c_flow);
    app_c->set_debug_id(0);
    c_flow->set_app(app_c);
    if (CGlobalInfo::m_options.preview.getEmulDebug() ){
        app_c->set_log_enable(true);
    }



    m_s_ctx.m_ft.set_tcp_api(&m_tcp_bh_api_impl_s);
    set_assoc_table(80, prog_s, s_tune);

    m_sim.add_event( new CTcpSimEventTimers(this, (((double)(TCP_TIMER_W_TICK)/((double)TCP_TIMER_W_DIV*1000.0)))));
    m_sim.add_event( new CTcpSimEventStop(1000.0) );

    /* start client */
    app_c->start(true);
    tcp_connect(&m_c_ctx,&c_flow->m_tcp);

    m_sim.run_sim();

    printf(" C counters \n");
    m_c_ctx.get_tcpstat()->Dump(stdout);
    m_c_ctx.m_ft.dump(stdout);
    printf(" S counters \n");
    m_s_ctx.get_tcpstat()->Dump(stdout);
    m_s_ctx.m_ft.dump(stdout);


    //EXPECT_EQ(m_c_ctx.get_tcpstat()->m_sts.tcps_sndbyte,4024);
    //EXPECT_EQ(m_c_ctx.get_tcpstat()->m_sts.tcps_rcvackbyte,4024);
    //EXPECT_EQ(m_c_ctx.get_tcpstat()->m_sts.tcps_connects,1);


    //EXPECT_EQ(m_s_ctx.get_tcpstat()->m_sts.tcps_rcvbyte,4024);
    //EXPECT_EQ(m_s_ctx.get_tcpstat()->m_sts.tcps_accepts,1);
    //EXPECT_EQ(m_s_ctx.get_tcpstat()->m_sts.tcps_preddat,m_s_ctx.get_tcpstat()->m_sts.tcps_rcvpack-1);


    //app.m_write_buf.Delete();
    //printf (" rx %d \n",m_s_flow.m_tcp.m_socket.so_rcv.sb_cc);
    //assert( m_s_flow.m_tcp.m_socket.so_rcv.sb_cc == 4024);


    //delete app_c;
    //delete app_s;

    delete prog_c;
    delete prog_s;
    delete s_tune;

    buf->Delete();
    delete buf;

    return(0);

}

static char http_req[] = {
0x47, 0x45, 0x54, 0x20, 0x2f, 0x32, 0x31, 0x30,
0x30, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31,
0x2e, 0x31, 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74,
0x3a, 0x20, 0x32, 0x32, 0x2e, 0x30, 0x2e, 0x30,
0x2e, 0x33, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x6e,
0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20,
0x4b, 0x65, 0x65, 0x70, 0x2d, 0x41, 0x6c, 0x69,
0x76, 0x65, 0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72,
0x2d, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20,
0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0x2f,
0x34, 0x2e, 0x30, 0x20, 0x28, 0x63, 0x6f, 0x6d,
0x70, 0x61, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x3b,
0x20, 0x4d, 0x53, 0x49, 0x45, 0x20, 0x37, 0x2e,
0x30, 0x3b, 0x20, 0x57, 0x69, 0x6e, 0x64, 0x6f,
0x77, 0x73, 0x20, 0x4e, 0x54, 0x20, 0x35, 0x2e,
0x31, 0x3b, 0x20, 0x53, 0x56, 0x31, 0x3b, 0x20,
0x2e, 0x4e, 0x45, 0x54, 0x20, 0x43, 0x4c, 0x52,
0x20, 0x31, 0x2e, 0x31, 0x2e, 0x34, 0x33, 0x32,
0x32, 0x3b, 0x20, 0x2e, 0x4e, 0x45, 0x54, 0x20,
0x43, 0x4c, 0x52, 0x20, 0x32, 0x2e, 0x30, 0x2e,
0x35, 0x30, 0x37, 0x32, 0x37, 0x29, 0x0d, 0x0a,
0x41, 0x63, 0x63, 0x65, 0x70, 0x74, 0x3a, 0x20,
0x2a, 0x2f, 0x2a, 0x0d, 0x0a, 0x41, 0x63, 0x63,
0x65, 0x70, 0x74, 0x2d, 0x4c, 0x61, 0x6e, 0x67,
0x75, 0x61, 0x67, 0x65, 0x3a, 0x20, 0x65, 0x6e,
0x2d, 0x75, 0x73, 0x0d, 0x0a, 0x41, 0x63, 0x63,
0x65, 0x70, 0x74, 0x2d, 0x45, 0x6e, 0x63, 0x6f,
0x64, 0x69, 0x6e, 0x67, 0x3a, 0x20, 0x67, 0x7a,
0x69, 0x70, 0x2c, 0x20, 0x64, 0x65, 0x66, 0x6c,
0x61, 0x74, 0x65, 0x2c, 0x20, 0x63, 0x6f, 0x6d,
0x70, 0x72, 0x65, 0x73, 0x73, 0x0d, 0x0a, 0x0d,
0x0a };

static char http_res[] = {
0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31,
0x20, 0x32, 0x30, 0x30, 0x20, 0x4f, 0x4b, 0x0d,
0x0a, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3a,
0x20, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f,
0x66, 0x74, 0x2d, 0x49, 0x49, 0x53, 0x2f, 0x36,
0x2e, 0x30, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74,
0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65,
0x3a, 0x20, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x68,
0x74, 0x6d, 0x6c, 0x0d, 0x0a, 0x43, 0x6f, 0x6e,
0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65, 0x6e,
0x67, 0x74, 0x68, 0x3a, 0x20, 0x33, 0x32, 0x30,
0x30, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x3c, 0x68,
0x74, 0x6d, 0x6c, 0x3e, 0x3c, 0x70, 0x72, 0x65,
0x3e, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a
};

static char http_res_post[] ={
0x3c, 0x2f, 0x70, 0x72, 0x65, 0x3e, 0x3c, 0x2f, 0x68, 0x74, 0x6d,
0x6c, 0x3e
};

static char * allocate_http_res(uint32_t &new_size){
    uint16_t min_size=sizeof(http_res)+sizeof(http_res_post);
    if (new_size<min_size) {
        new_size=min_size;
    }
    char *p=(char *)malloc(new_size);
    char *s=p;
    uint32_t fill=new_size-min_size;
    memcpy(p,http_res,sizeof(http_res));
    p+=sizeof(http_res);
    memset(p,'*',fill);
    p+=fill;
    memcpy(p,http_res_post,sizeof(http_res_post));
    return(s);
}

static void free_http_res(char *p){
    assert(p);
    free(p);
}





void CClientServerTcp::close_file(){
    m_c_pcap.close_pcap_file();
    m_s_pcap.close_pcap_file();
}


void CClientServerTcp::dump_counters(){

    CSTTCp stt_cp;
    stt_cp.Create();
    stt_cp.Init();
    stt_cp.m_init=true;
    stt_cp.Add(TCP_CLIENT_SIDE,&m_c_ctx);
    stt_cp.Add(TCP_SERVER_SIDE,&m_s_ctx);
    stt_cp.update_profile_ctx();
    stt_cp.Update();
    stt_cp.DumpTable();
    std::string json;
    stt_cp.dump_json(json);
    if (m_dump_json_counters ){
      fprintf(stdout,"json-start \n");
      fprintf(stdout,"%s\n",json.c_str());
      fprintf(stdout,"json-end \n");
    }
    stt_cp.Delete();
}


int CClientServerTcp::simple_http(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}

/* end with RST from server side */
int CClientServerTcp::simple_http_rst(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http_rst, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}

/* end with RST from server side */
int CClientServerTcp::simple_http_fin_ack(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http_fin_ack, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}

int CClientServerTcp::simple_http_connect(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http_connect, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}

int CClientServerTcp::simple_http_connect_rst(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http_connect_rst, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}

int CClientServerTcp::simple_http_connect_rst2(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http_connect_rst2, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}


int CClientServerTcp::simple_http_connect_rst_keepalive(){
    using namespace std::placeholders;
    method_program_cb_t cb = std::bind(&CClientServerTcp::build_http, this, _1, _2,_3,_4,_5);
    return(simple_http_generic(cb));
}


int CClientServerTcp::build_http(CMbufBuffer * buf_req,
                                  CMbufBuffer * buf_res,
                                  CEmulAppProgram * prog_c,
                                  CEmulAppProgram * prog_s,
                                  uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_res;
    prog_s->add_cmd(cmd);
    return(0);
}

int CClientServerTcp::build_http_rst(CMbufBuffer * buf_req,
                                     CMbufBuffer * buf_res,
                                     CEmulAppProgram * prog_c,
                                     CEmulAppProgram * prog_s,
                                     uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    prog_c->add_cmd(cmd);

    cmd.m_cmd = tcRESET;
    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_res;
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcDONT_CLOSE;
    prog_s->add_cmd(cmd);


    return(0);
}

int CClientServerTcp::build_http_fin_ack(CMbufBuffer * buf_req,
                                     CMbufBuffer * buf_res,
                                     CEmulAppProgram * prog_c,
                                     CEmulAppProgram * prog_s,
                                     uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    prog_c->add_cmd(cmd);



    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_res;
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcDONT_CLOSE;
    prog_s->add_cmd(cmd);


    return(0);
}

int CClientServerTcp::build_http_connect(CMbufBuffer * buf_req,
                                         CMbufBuffer * buf_res,
                                         CEmulAppProgram * prog_c,
                                         CEmulAppProgram * prog_s,
                                         uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcCONNECT_WAIT;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    prog_c->add_cmd(cmd);


    /* server program */
    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CEmulAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_res;
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcDONT_CLOSE;
    prog_s->add_cmd(cmd);


    return(0);
}

int CClientServerTcp::build_http_connect_rst(CMbufBuffer * buf_req,
                                         CMbufBuffer * buf_res,
                                         CEmulAppProgram * prog_c,
                                         CEmulAppProgram * prog_s,
                                         uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcCONNECT_WAIT;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRESET;
    prog_c->add_cmd(cmd);


    cmd.m_cmd = tcDONT_CLOSE;
    prog_s->add_cmd(cmd);

    return(0);
}




int CClientServerTcp::build_http_connect_rst2(CMbufBuffer * buf_req,
                                         CMbufBuffer * buf_res,
                                         CEmulAppProgram * prog_c,
                                         CEmulAppProgram * prog_s,
                                         uint32_t http_r_size){
    CEmulAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcRESET;
    prog_c->add_cmd(cmd);

    cmd.m_cmd = tcDONT_CLOSE;
    prog_s->add_cmd(cmd);

    return(0);
}


void CClientServerTcp::write_pcap(int dir, rte_mbuf_t *m,double t){
     /* write TX side */
    if (dir==0) {
        m_c_pcap.write_pcap_mbuf(m,t);
    }else{
        m_s_pcap.write_pcap_mbuf(m,t);
    }
}


int CClientServerTcp::simple_http_generic(method_program_cb_t cb){


    CMbufBuffer * buf_req;
    CMbufBuffer * buf_res;
    CEmulAppProgram * prog_c;
    CEmulAppProgram * prog_s;
    CTcpFlow          *  c_flow;
    CTcpTuneables *s_tune;

    CEmulApp * app_c;
    uint32_t http_r_size=32*1024;

    if (m_mss) {
        /* change context MSS */
        m_c_ctx.tcp_mssdflt =m_mss;
        m_s_ctx.tcp_mssdflt =m_mss;
    }

    /* IW=1 */
    m_c_ctx.tcp_initwnd = m_c_ctx.tcp_mssdflt;
    m_s_ctx.tcp_initwnd = m_c_ctx.tcp_mssdflt;

    c_flow = m_c_ctx.m_ft.alloc_flow(DEFAULT_PROFILE_CTX(&m_c_ctx),0x10000001,0x30000001,1025,80,m_vlan,m_ipv6);
    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(0x10000001);
    c_tuple.set_port(1025);
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(m_ipv6?false:true);

    if (m_debug) {
        /* enable client debug */
        c_flow->m_tcp.m_socket.so_options |= US_SO_DEBUG;
        tcp_set_debug_flow(&c_flow->m_tcp);
    }

    assert(m_c_ctx.m_ft.insert_new_flow(c_flow,c_tuple)==true);


    printf("client  %p \n",c_flow);

    app_c = &c_flow->m_app;


    /* CONST */
    buf_req = new CMbufBuffer();
    buf_res = new CMbufBuffer();

    prog_c = new CEmulAppProgram();
    prog_s = new CEmulAppProgram();
    s_tune = new CTcpTuneables();


    uint8_t* http_r=(uint8_t*)allocate_http_res(http_r_size);

    utl_mbuf_buffer_create_and_copy(0,buf_req,2048,(uint8_t*)http_req,sizeof(http_req));
    utl_mbuf_buffer_create_and_copy(0,buf_res,2048,(uint8_t*)http_r,http_r_size);

    cb(buf_req,buf_res,prog_c,prog_s,http_r_size);



    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,c_flow);
    app_c->set_debug_id(1);
    c_flow->set_app(app_c);


    m_s_ctx.m_ft.set_tcp_api(&m_tcp_bh_api_impl_s);

    set_assoc_table(80, prog_s, s_tune);


    m_sim.add_event( new CTcpSimEventTimers(this, (((double)(TCP_TIMER_W_TICK)/((double)TCP_TIMER_W_DIV*1000.0)))));
    m_sim.add_event( new CTcpSimEventStop(10000.0) );

    /* start client */
    app_c->start(true);
    tcp_connect(&m_c_ctx,&c_flow->m_tcp);

    m_sim.run_sim();

    dump_counters();

    #define TX_BYTES 249
    #define RX_BYTES 32768

    printf(" [%d %d] [%d %d] [%d %d] \n",(int)m_c_ctx.get_tcpstat()->m_sts.tcps_sndbyte,
                                         (int)m_c_ctx.get_tcpstat()->m_sts.tcps_rcvbyte,
                                         (int)m_s_ctx.get_tcpstat()->m_sts.tcps_rcvbyte,
                                         (int)m_s_ctx.get_tcpstat()->m_sts.tcps_sndbyte,
                                         (int)TX_BYTES,
                                         (int)RX_BYTES );

    if (m_check_counters){
        if (m_s_ctx.get_tcpstat()->m_sts.tcps_sndbyte>0 &&
            m_s_ctx.get_tcpstat()->m_sts.tcps_rcvbyte>0) {
            if ( (m_c_ctx.get_tcpstat()->m_sts.tcps_drops ==0) &&
                 (m_s_ctx.get_tcpstat()->m_sts.tcps_drops ==0) ){
                            /* flow wasn't initiated due to drop of SYN too many times */
            assert(m_c_ctx.get_tcpstat()->m_sts.tcps_sndbyte==TX_BYTES);
            assert(m_c_ctx.get_tcpstat()->m_sts.tcps_rcvbyte==RX_BYTES);
            assert(m_c_ctx.get_tcpstat()->m_sts.tcps_rcvackbyte==TX_BYTES);

            assert(m_s_ctx.get_tcpstat()->m_sts.tcps_rcvackbyte==RX_BYTES);
            assert(m_s_ctx.get_tcpstat()->m_sts.tcps_sndbyte==RX_BYTES);

            assert(m_s_ctx.get_tcpstat()->m_sts.tcps_rcvbyte==TX_BYTES);
            }
        }
    }


    free_http_res((char *)http_r);
    delete prog_c;
    delete prog_s;
    delete s_tune;

    buf_req->Delete();
    delete buf_req;

    buf_res->Delete();
    delete buf_res;

    return(0);
}

int CClientServerTcp::fill_from_file() {
    CEmulAppProgram *prog_c;
    CTcpFlow *c_flow;
    CEmulApp *app_c;

    m_gen = new CTupleGeneratorSmart();


    CAstfDbRO * ro_db=CAstfDB::instance()->get_db_ro(0);
    CAstfTemplatesRW * rw_db=CAstfDB::instance()->get_db_template_rw(0, m_gen,0,1,0);
    CAstfDB::instance()->clear_db_ro_rw(m_gen);
    ro_db=CAstfDB::instance()->get_db_ro(0);
    rw_db=CAstfDB::instance()->get_db_template_rw(0, m_gen,0,1,0);



    m_c_ctx.update_tuneables(rw_db->get_c_tuneables());
    m_s_ctx.update_tuneables(rw_db->get_s_tuneables());

    uint16_t dst_port = ro_db->get_dport(0);
    uint16_t src_port = CLIENT_SIDE_PORT;
    if (src_port == dst_port) {
        printf("WARNING DEST port changed \n");
        dst_port+=1;
    }

    uint16_t temp_index = 0;
    uint16_t tg_id = ro_db->get_template_tg_id(temp_index);

    c_flow = m_c_ctx.m_ft.alloc_flow(DEFAULT_PROFILE_CTX(&m_c_ctx),0x10000001,0x30000001,src_port,dst_port,m_vlan,false, tg_id);

    CFlowKeyTuple c_tuple;
    c_tuple.set_ip(0x10000001);
    c_tuple.set_port(src_port);
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

    if (m_debug) {
        /* enable client debug */
        c_flow->m_tcp.m_socket.so_options |= US_SO_DEBUG;
        tcp_set_debug_flow(&c_flow->m_tcp);
    }

    assert(m_c_ctx.m_ft.insert_new_flow(c_flow,c_tuple)==true);
    app_c = &c_flow->m_app;

    prog_c = ro_db->get_client_prog(temp_index);
    if (prog_c->is_stream() == false) {
        printf(" \n");
        printf(" ERROR try using simulator with --full, none-stream program is not supported in simple mode\n");
        printf(" \n");
        exit(-1);
    }
    // tunables setting currently does not work with this simulation.
    // need to do something like
    // c_flow->set_c_tcp_info(rw_db, temp_index);
    // s_flow->set_c_tcp_info(rw_db, temp_index);
    m_s_ctx.set_template_ro(ro_db);
    m_s_ctx.resize_stats();

    if (m_debug) {
        CTcpServreInfo * s_info = ro_db->get_server_info_by_port(dst_port,true);
        CEmulAppProgram *prog_s = s_info->get_prog();
        prog_c->Dump(stdout);
        prog_s->Dump(stdout);
    }

    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,c_flow);
    app_c->set_debug_id(0);
    c_flow->set_app(app_c);
    if (CGlobalInfo::m_options.preview.getEmulDebug() ){
        app_c->set_log_enable(true);
    }


    m_s_ctx.m_ft.set_tcp_api(&m_tcp_bh_api_impl_s);

    m_sim.add_event( new CTcpSimEventTimers(this, (((double)(TCP_TIMER_W_TICK)/((double)TCP_TIMER_W_DIV*1000.0)))));
    m_sim.add_event( new CTcpSimEventStop(1000.0) );

    /* start client */
    app_c->start(true);
    tcp_connect(&m_c_ctx,&c_flow->m_tcp);

    m_sim.run_sim();

    dump_counters();

    return(0);
}
