/*
 Hanoh Haim
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
#include "stt_cp.h"
#include "os_time.h"



bool CSTTCpPerTGIDPerDir::Create(uint32_t time_msec) {
    clear_aggregated_counters();
    create_clm_counters();
    m_tx_bw_l7.start(time_msec);
    m_tx_bw_l7_total.start(time_msec);
    m_rx_bw_l7.start(time_msec);
    m_tx_pps.start(time_msec);
    m_rx_pps.start(time_msec);
    return(true);
}

void CSTTCpPerTGIDPerDir::Delete() {
}

void CSTTCpPerTGIDPerDir::update_counters(bool is_sum, uint16_t tg_id) {
    tcpstat_int_t *lpt = &m_tcp.m_sts;
    udp_stat_int_t *lpt_udp = &m_udp.m_sts;
    CFlowTableIntStats *lpft = &m_ft.m_sts;

    clear_aggregated_counters();
    CGCountersUtl64 tcp((uint64_t *)lpt,sizeof(tcpstat_int_t)/sizeof(uint64_t));
    CGCountersUtl64 udp((uint64_t *)lpt_udp,sizeof(udp_stat_int_t)/sizeof(uint64_t));
    CGCountersUtl32 ft((uint32_t *)lpft,sizeof(CFlowTableIntStats)/sizeof(uint32_t));

    for (int i = 0; i < m_profile_ctx.size(); i++) {
        CPerProfileCtx* pctx = m_profile_ctx[i];
        uint64_t *base_tcp;
        uint64_t *base_udp;
        if (is_sum) {
            base_tcp = (uint64_t *)&pctx->m_tcpstat.m_sts;
            base_udp = (uint64_t *)&pctx->m_udpstat.m_sts;
        } else {
            base_tcp = (uint64_t *)&pctx->m_tcpstat.m_sts_tg_id[tg_id];
            base_udp = (uint64_t *)&pctx->m_udpstat.m_sts_tg_id[tg_id];
        }
        CGCountersUtl64 tcp_ctx(base_tcp,sizeof(tcpstat_int_t)/sizeof(uint64_t));
        CGCountersUtl64 udp_ctx(base_udp,sizeof(udp_stat_int_t)/sizeof(uint64_t));
        tcp += tcp_ctx;
        udp += udp_ctx;
    }

    for (int i = 0; i < m_tcp_ctx.size(); i++) {
        CTcpPerThreadCtx* lpctx = m_tcp_ctx[i];
        CGCountersUtl32 ft_ctx((uint32_t *)&lpctx->m_ft.m_sts,sizeof(CFlowTableIntStats)/sizeof(uint32_t));
        ft += ft_ctx;
    }

    uint64_t udp_active_flows=0;
    if ( (lpt_udp->udps_connects+
          lpt_udp->udps_accepts) > lpt_udp->udps_closed) {
        udp_active_flows =  lpt_udp->udps_connects+
                            lpt_udp->udps_accepts -
                            lpt_udp->udps_closed;
    }

    uint64_t tcp_active_flows=0;
    if ((lpt->tcps_connattempt + lpt->tcps_accepts) >= lpt->tcps_closed) {
        tcp_active_flows = lpt->tcps_connattempt + lpt->tcps_accepts - lpt->tcps_closed;

    }

    m_active_flows = tcp_active_flows + udp_active_flows;
    if (lpt->tcps_connects>lpt->tcps_closed) {
        m_est_flows = lpt->tcps_connects -
                      lpt->tcps_closed;
    }else{
        m_est_flows=0;
    }
    m_est_flows+=udp_active_flows;
    /* total bytes sent */
    uint64_t total_tx_bytes =
            lpt->tcps_sndbyte_ok +
            lpt->tcps_sndrexmitbyte +
            lpt->tcps_sndprobe +
            lpt_udp->udps_sndbyte;
    m_tx_bw_l7_r = m_tx_bw_l7.add(lpt->tcps_rcvackbyte + lpt_udp->udps_sndbyte)*_1Mb_DOUBLE; /* better to add the acked tx bytes than tcps_sndbyte */
    m_tx_bw_l7_total_r = m_tx_bw_l7_total.add(total_tx_bytes)*_1Mb_DOUBLE; /* how many L7 bytes sent */
    m_rx_bw_l7_r = m_rx_bw_l7.add(lpt->tcps_rcvbyte + lpt_udp->udps_rcvbyte)*_1Mb_DOUBLE;
    m_tx_pps_r = m_tx_pps.add(lpt->tcps_sndtotal + lpt_udp->udps_sndpkt);
    m_rx_pps_r = m_rx_pps.add(lpt->tcps_rcvpack + lpt->tcps_rcvackpack + lpt_udp->udps_rcvpkt);
    if ( (m_tx_pps_r+m_rx_pps_r) > 0.0){
        m_avg_size = (m_tx_bw_l7_r+m_rx_bw_l7_r)/(8.0*(m_tx_pps_r+m_rx_pps_r));
    } else {
        m_avg_size = 0.0;
    }

    m_tx_ratio = 0.0;
    if (m_tx_bw_l7_total_r > 0.0) {
        m_tx_ratio = m_tx_bw_l7_r*100.0/m_tx_bw_l7_total_r;
    }
}

void CSTTCpPerTGIDPerDir::clear_aggregated_counters(void) {
    m_tcp.Clear();
    m_udp.Clear();
    m_ft.Clear();
}

void CSTTCpPerTGIDPerDir::clear_counters() {
    clear_aggregated_counters();

    m_active_flows = 0.0;
    m_est_flows = 0.0;

    m_tx_bw_l7.reset();
    m_tx_bw_l7_r = 0.0;
    m_tx_bw_l7_total.reset();
    m_tx_bw_l7_total_r = 0.0;
    m_rx_bw_l7.reset();
    m_rx_bw_l7_r = 0.0;
    m_tx_pps.reset();
    m_tx_pps_r = 0.0;
    m_rx_pps.reset();
    m_rx_pps_r = 0.0;

    m_avg_size = 0.0;
    m_tx_ratio = 0.0;

}

static void create_sc(CGTblClmCounters  * clm,
                                std::string name,
                                std::string help,
                                uint64_t * c,
                                bool keep_zero,
                                bool error,
                                bool is_abs = false){
    CGSimpleBase *lp;
    lp = new CGSimpleRefCnt64(c);
    lp->set_name(name);
    lp->set_help(help);

    lp->set_dump_zero(keep_zero);
    if (error){
        lp->set_info_level(scERROR);
    }
    lp->set_abs(is_abs);
    clm->add_count(lp);
}

static void create_sc_32(CGTblClmCounters  * clm,
                                std::string name,
                                std::string help,
                                uint32_t * c,
                                bool keep_zero,
                                bool error){
    CGSimpleBase *lp;
    lp = new CGSimpleRefCnt32(c);
    lp->set_name(name);
    lp->set_help(help);

    lp->set_dump_zero(keep_zero);
    if (error){
        lp->set_info_level(scERROR);
    }
    clm->add_count(lp);
}

static void create_sc_d(CGTblClmCounters  * clm,
                                std::string name,
                                std::string help,
                                double * c,
                                bool keep_zero,
                                std::string units,
                                bool error,
                                bool is_abs = false){
    CGSimpleBase *lp;
    lp = new CGSimpleRefCntDouble(c,units);
    lp->set_name(name);
    lp->set_help(help);
    lp->set_dump_zero(keep_zero);
    if (error){
        lp->set_info_level(scERROR);
    }
    lp->set_abs(is_abs);

    clm->add_count(lp);
}

static void create_bar(CGTblClmCounters  * clm,
                                std::string name){
    CGSimpleBase *lp;
    lp = new CGSimpleBar();
    lp->set_name(name);
    lp->set_abs(true);
    clm->add_count(lp);
}

#define TCP_S_ADD_CNT(f,help)  { create_sc(&m_clm,#f,help,&m_tcp.m_sts.f,false,false); }
// for errors
#define TCP_S_ADD_CNT_E(f,help)  { create_sc(&m_clm,#f,help,&m_tcp.m_sts.f,false,true); }

#define UDP_S_ADD_CNT(f,help)  { create_sc(&m_clm,#f,help,&m_udp.m_sts.f,false,false); }
#define UDP_S_ADD_CNT_E(f,help)  { create_sc(&m_clm,#f,help,&m_udp.m_sts.f,false,true); }



#define FT_S_ADD_CNT(f,help)  { create_sc_32(&m_clm,#f,help,&m_ft.m_sts.m_##f,false,false); }
#define FT_S_ADD_CNT_Ex(s,f,help)  { create_sc_32(&m_clm,s,help,&m_ft.m_sts.m_##f,false,false); }
#define FT_S_ADD_CNT_SZ(f,help)  { create_sc_32(&m_clm,#f,help,&m_ft.m_sts.m_##f,true,false); }
#define FT_S_ADD_CNT_E(f,help)  { create_sc_32(&m_clm,#f,help,&m_ft.m_sts.m_##f,false,true); }
#define FT_S_ADD_CNT_OK(f,help)  { create_sc_32(&m_clm,#f,help,&m_ft.m_sts.m_##f,false,false); }

#define FT_S_ADD_CNT_Ex_E(s,f,help)  { create_sc_32(&m_clm,s,help,&m_ft.m_sts.m_##f,false,true); }
#define FT_S_ADD_CNT_SZ_E(f,help)  { create_sc_32(&m_clm,#f,help,&m_ft.m_sts.m_##f,true,true); }

#define CMN_S_ADD_CNT(f,help,z) { create_sc(&m_clm,#f,help,&f,z,false,true); }
#define CMN_S_ADD_CNT_d(f,help,z,u) { create_sc_d(&m_clm,#f,help,&f,z,u,false,true); }

void CSTTCpPerTGIDPerDir::create_clm_counters(){

    CMN_S_ADD_CNT(m_active_flows,"active open flows",true);
    CMN_S_ADD_CNT(m_est_flows,"active established flows",true);
    CMN_S_ADD_CNT_d(m_tx_bw_l7_r,"tx L7 bw acked",true,"bps");
    CMN_S_ADD_CNT_d(m_tx_bw_l7_total_r,"tx L7 bw total",true,"bps");
    CMN_S_ADD_CNT_d(m_rx_bw_l7_r,"rx L7 bw acked",true,"bps");
    CMN_S_ADD_CNT_d(m_tx_pps_r,"tx pps",true,"pps");
    CMN_S_ADD_CNT_d(m_rx_pps_r,"rx pps",true,"pps");
    CMN_S_ADD_CNT_d(m_avg_size,"average pkt size",true,"B");
    CMN_S_ADD_CNT_d(m_tx_ratio,"Tx acked/sent ratio",true,"%");
    create_bar(&m_clm,"-");
    create_bar(&m_clm,"TCP");
    create_bar(&m_clm,"-");


    TCP_S_ADD_CNT(tcps_connattempt,"connections initiated");
    TCP_S_ADD_CNT(tcps_accepts,"connections accepted");
    TCP_S_ADD_CNT(tcps_connects,"connections established");
    TCP_S_ADD_CNT(tcps_closed,"conn. closed (includes drops)");
    TCP_S_ADD_CNT(tcps_segstimed,"segs where we tried to get rtt");
    TCP_S_ADD_CNT(tcps_rttupdated,"times we succeeded");
    TCP_S_ADD_CNT(tcps_delack,"delayed acks sent");
    TCP_S_ADD_CNT(tcps_sndtotal,"total packets sent");
    TCP_S_ADD_CNT(tcps_sndpack,"data packets sent");
    TCP_S_ADD_CNT(tcps_sndbyte,"data bytes sent by application");
    TCP_S_ADD_CNT(tcps_sndbyte_ok,"data bytes sent by tcp");
    TCP_S_ADD_CNT(tcps_sndctrl,"control (SYN|FIN|RST) packets sent");
    TCP_S_ADD_CNT(tcps_sndacks,"ack-only packets sent ");
    TCP_S_ADD_CNT(tcps_rcvtotal,"total packets received ");
    TCP_S_ADD_CNT(tcps_rcvpack,"packets received in sequence");
    TCP_S_ADD_CNT(tcps_rcvbyte,"bytes received in sequence");
    TCP_S_ADD_CNT(tcps_rcvackpack,"rcvd ack packets");
    TCP_S_ADD_CNT(tcps_rcvackbyte,"tx bytes acked by rcvd acks ");
    TCP_S_ADD_CNT(tcps_rcvackbyte_of,"tx bytes acked by rcvd acks - overflow acked");

    TCP_S_ADD_CNT(tcps_preddat,"times hdr predict ok for data pkts ");

    TCP_S_ADD_CNT(tcps_drops,"connections dropped");
    TCP_S_ADD_CNT_E(tcps_conndrops,"embryonic connections dropped");
    TCP_S_ADD_CNT_E(tcps_timeoutdrop,"conn. dropped in rxmt timeout");
    TCP_S_ADD_CNT_E(tcps_rexmttimeo,"retransmit timeouts");
    TCP_S_ADD_CNT_E(tcps_rexmttimeo_syn,"retransmit SYN timeouts");
    TCP_S_ADD_CNT_E(tcps_persisttimeo,"persist timeouts");
    TCP_S_ADD_CNT_E(tcps_keeptimeo,"keepalive timeouts");
    TCP_S_ADD_CNT_E(tcps_keepprobe,"keepalive probes sent");
    TCP_S_ADD_CNT_E(tcps_keepdrops,"connections dropped in keepalive");
    TCP_S_ADD_CNT(tcps_testdrops,"connections dropped by user at timeout (no-close flag --nc)"); // due to test timeout --nc 
    
    
    TCP_S_ADD_CNT_E(tcps_sndrexmitpack,"data packets retransmitted");
    TCP_S_ADD_CNT_E(tcps_sndrexmitbyte,"data bytes retransmitted");
    TCP_S_ADD_CNT_E(tcps_sndprobe,"window probes sent");
    TCP_S_ADD_CNT_E(tcps_sndurg,"packets sent with URG only");
    TCP_S_ADD_CNT(tcps_sndwinup,"window update-only packets sent");
    
    TCP_S_ADD_CNT_E(tcps_rcvbadsum,"packets received with ccksum errs");
    TCP_S_ADD_CNT_E(tcps_rcvbadoff,"packets received with bad offset");
    TCP_S_ADD_CNT_E(tcps_rcvshort,"packets received too short");
    TCP_S_ADD_CNT_E(tcps_rcvduppack,"duplicate-only packets received");
    TCP_S_ADD_CNT_E(tcps_rcvdupbyte,"duplicate-only bytes received");
    TCP_S_ADD_CNT_E(tcps_rcvpartduppack,"packets with some duplicate data");
    TCP_S_ADD_CNT_E(tcps_rcvpartdupbyte,"dup. bytes in part-dup. packets");
    TCP_S_ADD_CNT_E(tcps_rcvoopackdrop,"OOO packet drop due to queue len");
    TCP_S_ADD_CNT_E(tcps_rcvoobytesdrop,"OOO bytes drop due to queue len");
    
    
    TCP_S_ADD_CNT_E(tcps_rcvoopack,"out-of-order packets received");
    TCP_S_ADD_CNT_E(tcps_rcvoobyte,"out-of-order bytes received");
    TCP_S_ADD_CNT_E(tcps_rcvpackafterwin,"packets with data after window");

    TCP_S_ADD_CNT_E(tcps_rcvbyteafterwin,"bytes rcvd after window");
    TCP_S_ADD_CNT_E(tcps_rcvafterclose,"packets rcvd after close");
    TCP_S_ADD_CNT_E(tcps_rcvwinprobe,"rcvd window probe packets");
    TCP_S_ADD_CNT_E(tcps_rcvdupack,"rcvd duplicate acks");
    TCP_S_ADD_CNT_E(tcps_rcvacktoomuch,"rcvd acks for unsent data");
    TCP_S_ADD_CNT_E(tcps_rcvwinupd,"rcvd window update packets");
    TCP_S_ADD_CNT_E(tcps_pawsdrop,"segments dropped due to PAWS");
    TCP_S_ADD_CNT(tcps_predack,"times hdr predict ok for acks");
    TCP_S_ADD_CNT_E(tcps_persistdrop,"timeout in persist state");
    TCP_S_ADD_CNT_E(tcps_badsyn,"bogus SYN, e.g. premature ACK");
    
    TCP_S_ADD_CNT_E(tcps_reasalloc,"allocate tcp reasembly ctx");
    TCP_S_ADD_CNT_E(tcps_reasfree,"free tcp reasembly ctx");
    TCP_S_ADD_CNT_E(tcps_nombuf,"no mbuf for tcp - drop the packets");

    create_bar(&m_clm,"-");
    create_bar(&m_clm,"UDP");
    create_bar(&m_clm,"-");

    UDP_S_ADD_CNT(udps_accepts,"connections accepted");
    UDP_S_ADD_CNT(udps_connects,"connections established");
    UDP_S_ADD_CNT(udps_closed,"conn. closed (includes drops)");
    UDP_S_ADD_CNT(udps_sndbyte,"data bytes transmitted");
    UDP_S_ADD_CNT(udps_sndpkt,"data packets transmitted");
    UDP_S_ADD_CNT(udps_rcvbyte,"data bytes received");
    UDP_S_ADD_CNT(udps_rcvpkt,"data packets received");
    UDP_S_ADD_CNT_E(udps_keepdrops,"keepalive drop");
    UDP_S_ADD_CNT_E(udps_nombuf,"no mbuf");
    UDP_S_ADD_CNT_E(udps_pkt_toobig,"packets transmitted too big");


    create_bar(&m_clm,"-");
    create_bar(&m_clm,"Flow Table");
    create_bar(&m_clm,"-");

    FT_S_ADD_CNT_Ex_E("err_cwf",err_client_pkt_without_flow,"client pkt without flow");
    FT_S_ADD_CNT_E(err_no_syn,"server first flow packet with no SYN");
    FT_S_ADD_CNT_E(err_len_err,"pkt with length error"); 
    FT_S_ADD_CNT_E(err_fragments_ipv4_drop,"fragments_ipv4_drop"); /* frag is not supported */
    FT_S_ADD_CNT_OK(err_no_tcp_udp,"no tcp/udp packet");
    FT_S_ADD_CNT_E(err_no_template,"server can't match L7 template");
    FT_S_ADD_CNT_E(err_no_memory,"No heap memory for allocating flows");
    FT_S_ADD_CNT_Ex_E("err_dct",err_duplicate_client_tuple,"duplicate flow - more clients require ");
    FT_S_ADD_CNT_E(err_l3_cs,"ip checksum error");
    FT_S_ADD_CNT_E(err_l4_cs,"tcp/udp checksum error");

    FT_S_ADD_CNT_E(err_redirect_rx,"redirect to rx error");
    FT_S_ADD_CNT_OK(redirect_rx_ok,"redirect to rx OK");
    FT_S_ADD_CNT_OK(err_rx_throttled,"rx thread was throttled");
    FT_S_ADD_CNT_E(err_c_nf_throttled,"client new flow throttled");
    FT_S_ADD_CNT_E(err_c_tuple_err,"client new flow, not enough clients");
    FT_S_ADD_CNT_E(err_s_nf_throttled,"server new flow throttled");
    FT_S_ADD_CNT_E(err_flow_overflow,"too many flows errors");
}

/**
 * Control Plane Statistics
 **/

void CSTTCp::Add(tcp_dir_t dir, CTcpPerThreadCtx* ctx){
    m_sts[dir].m_tcp_ctx.push_back(ctx);
}

void CSTTCp::Init(bool first_time){
    uint32_t time_msec = os_get_time_msec();
    const char * names[]={"client","server"};
    for (int i = 0; i < TCP_CS_NUM; i++) {
        for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
            m_sts_per_tg_id[i][tg_id]->Create(time_msec);
            m_sts_per_tg_id[i][tg_id]->m_clm.set_name(names[i]);
        }
        if (first_time) {
            m_sts[i].Create(time_msec);
            m_sts[i].m_clm.set_name(names[i]);
        }
    }
}

void CSTTCp::Create(uint32_t stt_id, uint16_t num_of_tg_ids, bool first_time){
    m_stt_id = stt_id;
    m_num_of_tg_ids = num_of_tg_ids;
    if (first_time) {
        m_init = false;
        m_update = true;
        m_profile_ctx_updated = false;
        m_epoch = 0;
        m_dtbl.set_epoch(m_epoch);
        for (int i = 0; i < TCP_CS_NUM; i++) {
            m_dtbl.add(&m_sts[i].m_clm); // Adding the counters for the sum, only the first time Create is called.
        }
    }
    for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
        CTblGCounters* cnt = new CTblGCounters();
        cnt->set_epoch(m_epoch);
        for (int i = 0; i < TCP_CS_NUM; i++) {
            CSTTCpPerTGIDPerDir* stt = new CSTTCpPerTGIDPerDir;
            stt->clear_counters();
            m_sts_per_tg_id[i].push_back(stt);
            cnt->add(&(stt->m_clm));
        }
        m_dtbl_per_tg_id.push_back(cnt);
    }
    if (!first_time && m_init) {
        Init(false);
    }
}

void CSTTCp::Update(){
    if (!m_update) return;

    // Updates the counters only for the sum.
    for (int i = 0; i < TCP_CS_NUM; i++) {
        m_sts[i].update_counters(true);
    }
}

void CSTTCp::DumpTable(){
    m_dtbl.dump_table(stdout,false,true);
}

bool CSTTCp::dump_json(std::string &json){
    if (m_init) {
        m_dtbl.dump_as_json("tcp-v1",json);
        return(true);
    }else{
        return(false);
    }
}

void CSTTCp::DumpTGNames(Json::Value &result) {
    result["tg_names"] = Json::arrayValue;
    for (int i = 0; i < m_tg_names.size(); i++) {
        result["tg_names"][i] = m_tg_names[i];
    }
}

void CSTTCp::UpdateTGNames(const std::vector<std::string>& tg_names) {
    m_tg_names = tg_names;
}

void CSTTCp::DumpTGStats(Json::Value &result, const std::vector<uint16_t>& tg_ids) {
    for (auto &tg_id :tg_ids) {
        result[std::to_string(tg_id)] = Json::objectValue;
        m_dtbl_per_tg_id[tg_id]->dump_values("counters per TGID: " + std::to_string(tg_id), false, result[std::to_string(tg_id)]);
    }
}

void CSTTCp::UpdateTGStats(const std::vector<uint16_t>& tg_ids) {
    if (!m_update) return;

    for (int i = 0; i < TCP_CS_NUM; i++) {
        for (uint16_t tg_id : tg_ids) {
            m_sts_per_tg_id[i][tg_id]->update_counters(false, tg_id);
        }
    }
}

void CSTTCp::Delete(bool last_time){

    for (int i = 0; i < TCP_CS_NUM; i++) {
        for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
            m_sts_per_tg_id[i][tg_id]->m_clm.set_free_objects_own(true);
            m_sts_per_tg_id[i][tg_id]->Delete();
            delete m_sts_per_tg_id[i][tg_id];
        }
        m_sts_per_tg_id[i].clear();
    }

    for (auto &cnt: m_dtbl_per_tg_id) {
        delete cnt;
    }
    m_dtbl_per_tg_id.clear();
    if (last_time) {
        for (int i = 0; i < TCP_CS_NUM; i++) {
        m_sts[i].m_clm.set_free_objects_own(true);
        m_sts[i].Delete();
        }
    }
    
}

void CSTTCp::clear_counters(bool epoch_increment) {
    for (auto &sts : m_sts) {
        sts.clear_counters();
    }
    if (epoch_increment) m_epoch++;
}

void CSTTCp::Resize(uint16_t new_num_of_tg_ids) {

    clear_counters();

    Delete(false);

    Create(m_stt_id, new_num_of_tg_ids, false);

    for (int i = 0 ; i < TCP_CS_NUM; i++) {
        for (auto &ctx : m_sts[i].m_tcp_ctx) {
            for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
                m_sts_per_tg_id[i][tg_id]->m_tcp_ctx.push_back(ctx);
            }
        }
    }

    m_profile_ctx_updated = false;
}

void CSTTCp::update_profile_ctx() {
    /* clear profile_ctx */
    for (int i = 0 ; i < TCP_CS_NUM; i++) {
        m_sts[i].m_profile_ctx.clear();
        for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
            m_sts_per_tg_id[i][tg_id]->m_profile_ctx.clear();
        }
    }

    /* update profile_ctx */
    for (int i = 0 ; i < TCP_CS_NUM; i++) {
        for (auto &ctx : m_sts[i].m_tcp_ctx) {
            if (!ctx->is_profile_ctx(m_stt_id)) {
                m_profile_ctx_updated = false;
                return;
            }
            CPerProfileCtx* pctx = ctx->get_profile_ctx(m_stt_id);
            m_sts[i].m_profile_ctx.push_back(pctx);
            for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++) {
                m_sts_per_tg_id[i][tg_id]->m_profile_ctx.push_back(pctx);
            }
        }
    }

    m_profile_ctx_updated = true;
    m_update = true;
}

