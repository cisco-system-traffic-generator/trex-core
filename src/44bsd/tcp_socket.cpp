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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"
#include "tcp_socket.h"
#include "timer_types.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>


uint32_t sbspace(struct sockbuf *sb){
    return(sb->sb_hiwat - sb->sb_cc);
}


void sbflush (struct sockbuf *sb){
    sb->sb_cc=0;
}

void    sbappend(struct tcp_socket *so,
                 struct sockbuf *sb, 
                 struct rte_mbuf *m,
                 uint32_t len){
    assert(len==m->pkt_len);
    sb->sb_cc+=len;
    if (len) {
        so->m_app->on_bh_rx_bytes(len,m);
    }else{
        if (m) {
            rte_pktmbuf_free(m);
        }
        so->m_app->on_bh_event(te_SOOTHERISDISCONNECTING);
        /* zero mean got FIN */
    }
}


void    sbappend_bytes(struct tcp_socket *so,
                       struct sockbuf *sb, 
                       uint32_t len){
    sb->sb_cc+=len;
    if (len) {
        so->m_app->on_bh_rx_bytes(len,(struct rte_mbuf *) 0);
    }else{
        /* zero mean got FIN */
        so->m_app->on_bh_event(te_SOOTHERISDISCONNECTING);
    }
}

std::string get_tcp_app_events_name(tcp_app_events_t event){

    switch (event) {
    case te_SOISCONNECTING :
        return("SOISCONNECTING");
        break;
    case te_SOISCONNECTED :
        return("SOISCONNECTED");
        break;
    case te_SOCANTRCVMORE :
        return("SOCANTRCVMORE");
        break;
    case te_SOABORT       :
        return("SOABORT");
        break;
    case te_SOWWAKEUP     :
        return("SOWWAKEUP");
        break;
    case te_SORWAKEUP     :
        return("SORWAKEUP");
        break;
    case te_SOISDISCONNECTING :
        return("SOISDISCONNECTING");
        break;
    case te_SOISDISCONNECTED :
        return("SOISDISCONNECTED");
        break;
    case te_SOOTHERISDISCONNECTING:
        return("SO_OTHER_IS_DISCONNECTING");
        break;
    default:
        return("UNKNOWN");
    }
}


void    soisconnecting(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOISCONNECTING);
}

void    soisconnected(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOISCONNECTED);
}

void    socantrcvmore(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOCANTRCVMORE);
}


/* delete a socket */
int soabort(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOABORT);
    return(0);
}

void sowwakeup(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOWWAKEUP);
}

void sorwakeup(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SORWAKEUP);
}

void    soisdisconnecting(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOISDISCONNECTING);
}

void    soisdisconnected(struct tcp_socket *so){
    so->m_app->on_bh_event(te_SOISDISCONNECTED);
}



void CBufMbufRef::Dump(FILE *fd){
    CMbufObject::Dump(fd);
    fprintf(fd," (offset:%lu) (rsize:%lu)  \n",(ulong)m_offset,(ulong)get_mbuf_size());
}


std::string get_mbuf_type_name(mbuf_types_t mbuf_type){
    switch (mbuf_type) {
    case MO_CONST:
        return ("CONST");
        break;
    case MO_RW:
        return ("RW");
        break;
    default:
        return ("Unknown");
    }
}


void CMbufObject::Dump(FILE *fd){
   fprintf(fd," %p (len:%lu) %s \n",m_mbuf->buf_addr,(ulong)m_mbuf->pkt_len,
           get_mbuf_type_name(m_type).c_str());
}


void CMbufBuffer::Reset(){
    m_num_pkt=0;
    m_blk_size=0;
    m_t_bytes=0;
    m_vec.clear();
}

void CMbufBuffer::Create(uint32_t blk_size){
    Reset();
    m_blk_size = blk_size;
}

void CMbufBuffer::Delete(){
    int i;
    for (i=0; i<m_vec.size(); i++) {
        m_vec[i].Free();
    }
    Reset();
}

void CMbufBuffer::get_by_offset(uint32_t offset,CBufMbufRef & res){
    if (offset>=m_t_bytes) {
        printf(" ERROR get_by_offset: %lu %lu\n",(ulong)offset,(ulong)m_t_bytes);
    }
    assert(offset<m_t_bytes);
    /* TBD might  worth to cache the calculation (%/) and save the next offset calculation ..,
       in case of MSS == m_blk_size it would be just ++ */
    uint32_t index=offset/m_blk_size;
    uint32_t mod=offset%m_blk_size;

    res.m_offset = mod;
    CMbufObject *lp= &m_vec[index];
    res.m_mbuf   = lp->m_mbuf;
    res.m_type   = lp->m_type; 
}


void CMbufBuffer::Dump(FILE *fd){
    fprintf(fd," total_size %lu, blk: %lu, pkts: %lu \n",(ulong)m_t_bytes,
            (ulong)m_blk_size,(ulong)m_num_pkt);

    int i;
    for (i=0; i<m_vec.size(); i++) {
        m_vec[i].Dump(fd);
    }
}


bool CMbufBuffer::Verify(){

    assert(m_vec.size()==m_num_pkt);
    if (m_num_pkt==0) {
        return(true);
    }
    int i=0;
    for (i=0; i<m_num_pkt-1; i++) {
        assert(m_vec[i].m_mbuf->pkt_len==m_blk_size);
    }
    assert(m_vec[i].m_mbuf->pkt_len<=m_blk_size);
    return(true);
}


void CMbufBuffer::Add_mbuf(CMbufObject & obj){
    /* should have one segment */
    assert(obj.m_mbuf);
    assert(obj.m_mbuf->nb_segs==1);

    m_num_pkt +=1;
    m_t_bytes +=obj.m_mbuf->pkt_len;
    m_vec.push_back(obj);
}


void CEmulApp::force_stop_timer(){
    if (get_timer_init_done()) {
        if (m_timer.is_running()){
            /* must stop timer before free the memory */
            m_pctx->m_ctx->m_timer_w.timer_stop(&m_timer);
        }
    }
}

void CEmulApp::tcp_udp_close(){
    if (is_udp_flow()) {
        m_api->disconnect(m_pctx,m_flow);
    }else{
        tcp_usrclosed(m_pctx,&m_flow->m_tcp);
        if (get_interrupt()==false) {
            m_api->tx_tcp_output(m_pctx,m_flow);
        }else{
            // in case we have somthing in the queue there is no need to mark it
            // the current function will take over 
            if (m_flow->m_tcp.m_socket.so_snd.sb_cc == 0) {
                set_do_close(true);
            }
        }
    }
}

void CEmulApp::tcp_udp_close_dpc(){
    if (is_udp_flow()) {
        m_api->disconnect(m_pctx,m_flow);
    } else{
        if (!m_flow->is_close_was_called()){
            tcp_usrclosed(m_pctx,&m_flow->m_tcp);
        }
        m_api->tx_tcp_output(m_pctx,m_flow);
    }
}


/* on tick */
void CEmulApp::on_tick(){
    next();
}

void CEmulApp::run_cmd_delay_rand(htw_ticks_t min_ticks,
                                 htw_ticks_t max_ticks){
    uint32_t rnd=m_pctx->m_ctx->m_rand->getRandomInRange(min_ticks,max_ticks);
    run_cmd_delay(rnd);
}

void CEmulApp::run_cmd_delay(htw_ticks_t ticks){
    m_state=te_NONE;
    if (!get_timer_init_done()) {
        /* lazey init */
        set_timer_init_done(true);
        m_timer.reset();
        if (is_udp_flow()){
            m_timer.m_type  = ttUDP_APP;
        }else{
            m_timer.m_type  = ttTCP_APP;
        }
    }
    /* make sure the timer is not running*/
    if (m_timer.is_running()) {
        m_pctx->m_ctx->m_timer_w.timer_stop(&m_timer);
    }
    /* to prevent unexpected loop at timer wheel */
    if (unlikely(ticks == 0)) {
        ticks = 1;
    }
    m_pctx->m_ctx->m_timer_w.timer_start(&m_timer,ticks);
}


void CEmulApp::check_rx_pkt_condition(){
    if (m_cmd_rx_bytes >= m_cmd_rx_bytes_wm) {
        m_cmd_rx_bytes -= m_cmd_rx_bytes_wm;
        if (get_rx_clear()){
            m_cmd_rx_bytes=0;
            set_rx_clear(false);
        }
        EMUL_LOG(0, "ON_RX_PKT [%d]- NEXT \n",m_debug_id);
        next();
    }
}


/* state-machine without table for speed. might need to move to table in the future 
   Interrupt means that we are inside the Rx TCP function and need to make sure it is possible to call the function 
   It is possible to defer a function to do after we finish the interrupt see dpc functions
   Next () -- will jump to the next state

   SEND/RX could happen in parallel 

   te_SEND    - while sending a big buffer 
   te_WAIT_RX  - while waiting for buffer to be Rx
   te_NONE    - for general commands

*/
void CEmulApp::process_cmd(CEmulAppCmd * cmd){
    CEmulAppCmd temp_cmd;
    int process_cnt = 0;

    while(cmd) {
        // add implicit delay to avoid watchdog
        if (++process_cnt > 1000) {
            --m_cmd_index;  // rewind index for next()

            temp_cmd.m_cmd = tcDELAY;
            temp_cmd.u.m_delay_cmd.m_ticks = 1;
            cmd = &temp_cmd;
        }
        cmd = process_cmd_one(cmd);
    }
}

CEmulAppCmd* CEmulApp::process_cmd_one(CEmulAppCmd * cmd){

    EMUL_LOG(cmd, "CMD [%d] state : %d ,cmd_index [%d] -",m_debug_id,m_state,m_cmd_index);

    switch (cmd->m_cmd) {
    case  tcTX_BUFFER   :
        {
            CMbufBuffer *   b=cmd->u.m_tx_cmd.m_buf;
            assert(b);
            uint32_t len = b->len();
            m_q.add_buffer(b); /* add buffer */
            uint32_t add_to_queue = bsd_umin(m_api->get_tx_sbspace(m_flow),len);
            m_api->tx_sbappend(m_flow,add_to_queue);
            m_q.subtract_bytes(add_to_queue);

            m_state=te_SEND;
            if (get_interrupt()==false && m_flags&taCONNECTED) {
                m_api->tx_tcp_output(m_pctx,m_flow);
            }

            if (get_tx_mode_none_blocking() && m_api->get_tx_sbspace(m_flow)) {
                return next_cmd();
            }
        }
        break;
    case tcDELAY  :
        run_cmd_delay((htw_ticks_t) cmd->u.m_delay_cmd.m_ticks);
        break;
    case tcRX_BUFFER :
        {
            uint32_t  flags = cmd->u.m_rx_cmd.m_flags;
            /* clear rx counter */
            if (flags & CEmulAppCmdRxBuffer::rxcmd_CLEAR) {
                set_rx_clear(true);
            }
            /* disable rx thread if needed */
            set_rx_disabled((flags & CEmulAppCmdRxBuffer::rxcmd_DISABLE_RX)?true:false);

            if (flags & CEmulAppCmdRxBuffer::rxcmd_WAIT) {
                m_state=te_WAIT_RX;
                m_cmd_rx_bytes_wm = cmd->u.m_rx_cmd.m_rx_bytes_wm;
                check_rx_condition();
            }else{
                return next_cmd();
            }
        }
        break;
    case tcRESET :
        m_state=te_NONE;
        if (get_interrupt()==false) {
            do_disconnect();
        }else{
            set_disconnect(true); /* defect the close, NO next */
        }
        break;
    case tcDONT_CLOSE :
        m_state=te_NONE;
        m_flags|=taDO_WAIT_FOR_CLOSE;
        /* nothing, no explict close , no next , set defer close  */
        break;
    case tcCONNECT_WAIT :
        {
            m_state=te_NONE;
            /* if already connected defer next */
            if (m_flags&taCONNECTED) {
                if (get_interrupt()==false) {
                    return next_cmd();
                }else{
                    m_flags|=taDO_DPC_NEXT;
                }
            }else{
                m_flags|=taDO_WAIT_CONNECTED; /* wait to be connected */
            }
        }
        break;
    case tcDELAY_RAND : 
        run_cmd_delay_rand((htw_ticks_t) cmd->u.m_delay_rnd.m_min_ticks,
                          (htw_ticks_t) cmd->u.m_delay_rnd.m_max_ticks);
        break;
    case tcSET_VAR : 
        {
        assert(cmd->u.m_var.m_var_id<apVAR_NUM_SIZE);
        m_vars[cmd->u.m_var.m_var_id]=cmd->u.m_var.m_val;
        return next_cmd();
        }
        break;
    case tcSET_TICK_VAR: 
        {
            assert(cmd->u.m_tick_var.m_var_id < apVAR_NUM_SIZE);

            /* Save the current tick from ctx */
            m_tick_vars[cmd->u.m_tick_var.m_var_id] = m_pctx->m_ctx->m_tick_var->get_curr_tick();
            return next_cmd();
        }
        break;
    case tcJMPNZ : 
        {
            assert(cmd->u.m_jmpnz.m_var_id<apVAR_NUM_SIZE);
            if (--m_vars[cmd->u.m_jmpnz.m_var_id]>0){
                /* action jump  */
                m_cmd_index+=cmd->u.m_jmpnz.m_offset-1;
                /* make sure we are not in at the end */
                int end=m_program->get_size();
                if (m_cmd_index>end) {
                    m_cmd_index=end;
                }
            }
            return next_cmd();
        }
        break;
    case tcJMPDP : 
        {
            uint64_t duration_ticks = cmd->u.m_jmpdp.m_duration;
            uint64_t start_ticks = m_tick_vars[cmd->u.m_jmpdp.m_var_id];
            uint64_t curr_ticks = m_pctx->m_ctx->m_tick_var->get_curr_tick();

            if ( curr_ticks - start_ticks < duration_ticks ) {
                /* action jump  */
                m_cmd_index += cmd->u.m_jmpdp.m_offset - 1;
                /* make sure we are not in at the end */
                int end = m_program->get_size();
                if (m_cmd_index > end) {
                    m_cmd_index = end;
                }
            }
            return next_cmd();
        }
        break;
    case tcTX_PKT : 
        {
           m_state=te_NONE;
           m_api->send_pkt((CUdpFlow *)m_flow,cmd->u.m_tx_pkt.m_buf);
           return next_cmd();
        }
        break;

    case tcRX_PKT : 
        {
            uint32_t  flags = cmd->u.m_rx_pkt.m_flags;
            /* clear rx counter */
            if (flags & CEmulAppCmdRxPkt::rxcmd_CLEAR) {
                set_rx_clear(true);
            }
            /* disable rx thread if needed */
            set_rx_disabled((flags & CEmulAppCmdRxPkt::rxcmd_DISABLE_RX)?true:false);

            if (flags & CEmulAppCmdRxPkt::rxcmd_WAIT) {
                m_state=te_WAIT_RX;
                m_cmd_rx_bytes_wm = cmd->u.m_rx_pkt.m_rx_pkts;
                check_rx_pkt_condition();
            }else{
                return next_cmd();
            }
        }
        break;

    case tcKEEPALIVE : 
        {
            m_state=te_NONE;
            m_api->set_keepalive((CUdpFlow *)m_flow,cmd->u.m_keepalive.m_keepalive_msec,
                                 cmd->u.m_keepalive.m_rx_mode);
            return next_cmd();
        }
        break;
    case tcCLOSE_PKT:
        {
            m_state=te_NONE;
            m_api->disconnect(m_pctx,m_flow);
            return next_cmd();
        }
        break;
    case tcTX_MODE:
        {
            m_state=te_NONE;
            bool none_block=(cmd->u.m_tx_mode.m_flags&CEmulAppCmdTxMode::txcmd_BLOCK_MASK?true:false);
            set_tx_none_blocking(none_block);
            return next_cmd();
        }
        break;


    default:
        assert(0);
    }

    return nullptr;
}


CEmulAppCmd* CEmulApp::next_cmd(){
    m_cmd_index++;
    if (m_cmd_index>m_program->get_size()) {
        /* could be in cases we get data after close of flow */
        if (is_udp_flow()) {
        }else{
            assert(m_flow->is_close_was_called());
        }
        return nullptr;
    }
    if ( m_cmd_index == m_program->get_size() ) {
        tcp_udp_close();
        return nullptr;
    }

    return m_program->get_index(m_cmd_index);
}

void CEmulApp::next(){
    CEmulAppCmd * lpcmd=next_cmd();
    if (lpcmd) {
        process_cmd(lpcmd);
    }
}


void CEmulApp::run_dpc_callbacks(){
    /* check all signals */
    if ( get_do_disconnect() ){
        set_disconnect(false);
        do_disconnect();
    }
    if (get_do_do_close()){
        set_do_close(false);
        do_close();
    }
    check_dpc_next();
}

void CEmulApp::start(bool interrupt){
    /* there is at least one command */
    set_interrupt(interrupt);
    assert(m_program->get_size()>0);
    if (!is_udp_flow()) {
        m_q.set_window_size(m_api->get_tx_max_space(m_flow));
    }
    CEmulAppCmd * lpcmd=m_program->get_index(m_cmd_index);
    /* inject implicit TCP accept to avoid issue #604 */
    CEmulAppCmd temp_cmd;
    if (!is_udp_flow() && !m_pctx->m_ctx->is_client_side() && (lpcmd->m_cmd == tcTX_BUFFER)) {
        temp_cmd.m_cmd = tcCONNECT_WAIT;
        lpcmd = &temp_cmd;
        --m_cmd_index;  // rewind index for next_cmd()
    }
    process_cmd(lpcmd);
    set_interrupt(false);
}


void CEmulApp::do_disconnect(){
    if (!m_flow->is_close_was_called()){
        m_api->disconnect(m_pctx,m_flow);
    }
}

void CEmulApp::do_close(){
    tcp_udp_close_dpc();
}


int CEmulApp::on_bh_tx_acked(uint32_t tx_bytes){
    set_interrupt(true);
    uint32_t  add_to_queue;
    
    bool is_next=m_q.on_bh_tx_acked(tx_bytes,add_to_queue,get_tx_mode_none_blocking()?false:true);

    if (add_to_queue) {
        m_api->tx_sbappend(m_flow,add_to_queue);
    }
    if ((m_state == te_SEND) && is_next) {
        EMUL_LOG(0, "ON_BH_TX [%d]-ACK \n",m_debug_id);
        next();
    }
    set_interrupt(false);
    return(0);
}


void CEmulApp::on_bh_event(tcp_app_events_t event){
    if ((m_flags&taDO_WAIT_FOR_CLOSE) && (event==te_SOOTHERISDISCONNECTING)){
        set_do_close(true);
    }
    if (event==te_SOISCONNECTED){
        m_flags|=taCONNECTED;
        if (m_flags&taDO_WAIT_CONNECTED) {
            m_flags|=taDO_DPC_NEXT;
            m_flags&=(~taDO_WAIT_CONNECTED);
        }
    }
    EMUL_LOG(0, "EVENT [%d]- %s \n",m_debug_id,get_tcp_app_events_name(event).c_str());
}


int CEmulApp::on_bh_rx_pkts(uint32_t rx_bytes,
                            struct rte_mbuf * m){
    set_interrupt(true);
    if (m) {
        rte_pktmbuf_free(m);
    }

    if ( get_rx_enabled() ) {
        m_cmd_rx_bytes+= 1;
        if (m_state==te_WAIT_RX) {
            check_rx_pkt_condition();
        }
    }

    set_interrupt(false);
    return(0);
}


/* rx bytes */
int CEmulApp::on_bh_rx_bytes(uint32_t rx_bytes,
                            struct rte_mbuf * m){
    set_interrupt(true);
    if (m) {
        check_l7_data(m);
        /* for now do nothing with the mbuf */
        rte_pktmbuf_free(m);
    }

    if ( get_rx_enabled() ) {

        /* drain the bytes from the queue */
        m_cmd_rx_bytes+= m_api->rx_drain(m_flow); 
        if (m_state==te_WAIT_RX) {
            check_rx_condition();
        }
    }

    set_interrupt(false);
    return(0);
}


void CEmulApp::check_l7_data(struct rte_mbuf * m) {
    if (unlikely(m_l7check_enable)) {
        uint8_t* l7_data = rte_pktmbuf_mtod(m, uint8_t*);
        uint16_t l7_len = rte_pktmbuf_data_len(m);

        m_flow->check_template_assoc_by_l7_data(l7_data, l7_len);
    }
}


#ifdef  TREX_SIM
void CEmulApp::set_flow_ctx(CTcpPerThreadCtx * ctx, CTcpFlow * flow) {
    m_pctx = DEFAULT_PROFILE_CTX(ctx);
    m_flow = flow;
}
#endif


void CEmulAppProgram::add_cmd(CEmulAppCmd & cmd){
    m_cmds.push_back(cmd);
}

bool CEmulAppProgram::is_common_commands(tcp_app_cmd_t cmd_id){
    if ( (cmd_id==tcDELAY) || 
         (cmd_id==tcDELAY_RAND) ||
         (cmd_id==tcSET_VAR) ||
         (cmd_id==tcJMPNZ) ||
         (cmd_id==tcSET_TICK_VAR) ||
         (cmd_id==tcJMPDP)
        ){
        return (true);
    }
    return(false);
}

bool CEmulAppProgram::is_udp_commands(tcp_app_cmd_t cmd_id){
    if ( (cmd_id==tcTX_PKT) || 
         (cmd_id==tcRX_PKT) ||
         (cmd_id==tcKEEPALIVE) ||
         (cmd_id==tcCLOSE_PKT) 
        ){
        return (true);
    }
    return(false);
}


bool CEmulAppProgram::sanity_check(std::string & err){
    int i;

    for (i=0; i<m_cmds.size(); i++) {
        CEmulAppCmd * lp=&m_cmds[i];

        if (m_stream) {  
            if ( is_udp_commands(lp->m_cmd) ){
                err="CMD is not valid with stream mode ";
                return false;
            }
        }else{
            if ( (!is_common_commands(lp->m_cmd)) &&
                  (!is_udp_commands(lp->m_cmd))
               ){
                err="CMD is not valid with none-stream mode ";
                return false;
            }
        }
    }

    for (i=0; i<m_cmds.size(); i++) {
        CEmulAppCmd * lp=&m_cmds[i];
        if (i>0) {
            /* not the first */
            if (lp->m_cmd==tcCONNECT_WAIT) {
                err="CMD connect() could only be first";
                return false;
            }
        }
        if (i<(m_cmds.size()-1)) {
            /* not the last */
            if ((lp->m_cmd==tcRESET) ||(lp->m_cmd==tcDONT_CLOSE)) {
                err="CMD tcRESET/tcDONT_CLOSE could only be the last command";
                return false;
            }
        }
        if (lp->m_cmd==tcSET_VAR||lp->m_cmd==tcJMPNZ){
            if (lp->u.m_var.m_var_id>=CEmulApp::apVAR_NUM_SIZE) {
                err="CMD tcSET_VAR/tcJMPNZ var_id is not valid";
                return false;
            }
        }
        if (lp->m_cmd==tcJMPNZ){
            int new_id=lp->u.m_jmpnz.m_offset+i;
            if ((new_id<0) || (new_id>=m_cmds.size())) {
                std::stringstream ss;
                ss << "CMD tcJMPNZ has not valid offset " << int(lp->u.m_jmpnz.m_offset) << " id: " << int(new_id) << " " ;
                err=ss.str();;
                return false;
            }
        }
    }
    return (true);
}


void CEmulAppProgram::Dump(FILE *fd){
    int i;
    fprintf(fd," cmds : %lu stream: %d\n",(ulong)get_size(),m_stream?1:0);
    fprintf(fd," --------------\n");

    for (i=0; i<m_cmds.size(); i++) {
        CEmulAppCmd * lp=&m_cmds[i];
        fprintf(fd," cmd : %d ",(int)i);
        lp->Dump(fd);
    }
}


void CEmulAppCmd::Dump(FILE *fd){
    switch (m_cmd) {
    case tcTX_BUFFER :
         fprintf(fd," tcTX_BUFFER %lu bytes\n",(ulong)u.m_tx_cmd.m_buf->m_t_bytes);
         break;
    case tcRX_BUFFER :
        if (u.m_rx_cmd.m_flags & CEmulAppCmdRxBuffer::rxcmd_CLEAR) {
            fprintf(fd," tcRX_BUFFER clear count,");
        }
        if (u.m_rx_cmd.m_flags & CEmulAppCmdRxBuffer::rxcmd_DISABLE_RX) {
            fprintf(fd," tcRX_BUFFER disable RX,");
        }else{
            fprintf(fd," tcRX_BUFFER enable RX ,");
        }

        if (u.m_rx_cmd.m_flags & CEmulAppCmdRxBuffer::rxcmd_WAIT) {
            fprintf(fd," tcRX_BUFFER wait for %lu RX bytes",(ulong)u.m_rx_cmd.m_rx_bytes_wm);
        }
        fprintf(fd," \n");

         break;
    case tcDELAY :
        fprintf(fd," tcDELAY for %lu usec \n",(ulong)u.m_delay_cmd.m_ticks);
        break;
    case tcRESET :
        fprintf(fd," tcRESET : reset connection \n");
        break;
    case tcDONT_CLOSE :
        fprintf(fd," tcDONT_CLOSE : dont close, wait for RST \n");
        break;
    case tcCONNECT_WAIT :
        fprintf(fd," tcCONNECT_WAIT : wait for connection \n");
        break;
    case tcDELAY_RAND :
        fprintf(fd," tcDELAY_RAND : %lu-%lu\n",(ulong)u.m_delay_rnd.m_min_ticks,(ulong)u.m_delay_rnd.m_max_ticks);
        break;
    case tcSET_VAR :
        fprintf(fd," tcSET_VAR id:%lu, val:%lu\n",(ulong)u.m_var.m_var_id,(ulong)u.m_var.m_val);
        break;
    case tcJMPNZ :
        fprintf(fd," tcJMPNZ id:%lu, jmp:%d\n",(ulong)u.m_jmpnz.m_var_id,(int)u.m_jmpnz.m_offset);
        break;

    case tcTX_PKT :
         fprintf(fd," tcTX_PKT %lu bytes\n",(ulong)u.m_tx_pkt.m_buf->m_t_bytes);
         break;
    case tcRX_PKT :
        if (u.m_rx_pkt.m_flags & CEmulAppCmdRxPkt::rxcmd_CLEAR) {
            fprintf(fd," tcRX_PKT clear count,");
        }
        if (u.m_rx_pkt.m_flags & CEmulAppCmdRxPkt::rxcmd_DISABLE_RX) {
            fprintf(fd," tcRX_PKT disable RX,");
        }else{
            fprintf(fd," tcRX_PKT enable RX ,");
        }

        if (u.m_rx_pkt.m_flags & CEmulAppCmdRxPkt::rxcmd_WAIT) {
            fprintf(fd," tcRX_PKT wait for %lu RX pkts",(ulong)u.m_rx_pkt.m_rx_pkts);
        }
        fprintf(fd," \n");
        break;

    case tcKEEPALIVE:
        fprintf(fd," tcKEEPALIVE_PKT %lu msec\n",(ulong)u.m_keepalive.m_keepalive_msec);
        break;

    case tcCLOSE_PKT:
        fprintf(fd," tcCLOSE_PKT \n");
        break;

    case tcTX_MODE:
        fprintf(fd," tcTX_MODE : flags : %x  \n",u.m_tx_mode.m_flags);
        break;

    case tcSET_TICK_VAR :
        fprintf(fd," tcSET_TICK_VAR id:%lu\n",(ulong)u.m_var.m_var_id);
        break;
    case tcJMPDP :
        fprintf(fd," tcJMPDP id:%lu, jmp:%d, dur:%lu\n",(ulong)u.m_jmpdp.m_var_id,(int)u.m_jmpdp.m_offset,(ulong)u.m_jmpdp.m_duration);
        break;

    default:
        assert(0);
    }
}


int utl_mbuf_buffer_create_and_copy(uint8_t socket,
                                    CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint8_t *d,
                                    uint32_t size,
                                    bool mbuf_const){
    
    buf->Create(blk_size);
    while (size>0) {
        uint32_t alloc_size=bsd_umin(blk_size,size);
        rte_mbuf_t   * m=tcp_pktmbuf_alloc(socket,alloc_size);
        assert(m);
        /*  
        can't work with ASTF interactive see https://trex-tgn.cisco.com/youtrack/issue/trex-537
        if (mbuf_const){
            rte_mbuf_set_as_core_const(m);
        }*/
        char *p=(char *)rte_pktmbuf_append(m, alloc_size);
        memcpy(p,d,alloc_size);
        d+=alloc_size;
        CMbufObject obj;
        obj.m_type =MO_CONST;
        obj.m_mbuf=m;

        buf->Add_mbuf(obj);
        size-=alloc_size;
    }
    return(0);
}


int utl_mbuf_buffer_create_and_copy(uint8_t socket,
                                    CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint8_t *d,
                                    uint32_t d_size,
                                    uint32_t size,
                                    uint8_t *f,
                                    uint32_t f_size,
                                    bool mbuf_const){

    buf->Create(blk_size);
    uint8_t cnt = 0;
    rte_mbuf_t* mbuf_fill = nullptr;
    while (size>0) {
        uint32_t alloc_size = bsd_umin(blk_size,size);
        auto copy_size = bsd_umin(d_size, alloc_size);
        rte_mbuf_t* m;
        if (mbuf_fill && (alloc_size == blk_size)) {
            m = mbuf_fill;
            rte_mbuf_refcnt_update(m, 1);
        } else {
            m = tcp_pktmbuf_alloc(socket,alloc_size);
            assert(m);
            char *p = (char *)rte_pktmbuf_append(m, alloc_size);
            if (copy_size) {
                memcpy(p,d,copy_size);
                d += copy_size;
                p += copy_size;
                d_size -= copy_size;
            } else if (!mbuf_fill) {
                mbuf_fill = m; // filled data reference mbuf
            }
            for (auto i = 0; i < (alloc_size-copy_size); i++ ) {
                *p = (f_size>0) ? f[cnt%f_size]: cnt;
                cnt++;
                p++;
            }
        }
        CMbufObject obj;
        obj.m_type =MO_CONST;
        obj.m_mbuf=m;

        buf->Add_mbuf(obj);
        size-=alloc_size;
    }
    return(0);
}


int utl_mbuf_buffer_create_and_fill(uint8_t socket,
                                    CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint32_t size,
                                    bool mbuf_const){
    buf->Create(blk_size);
    uint8_t cnt=0; 
    while (size>0) {
        uint32_t alloc_size=bsd_umin(blk_size,size);
        rte_mbuf_t   * m=tcp_pktmbuf_alloc(socket,alloc_size);
        assert(m);
        /*  
        can't work with ASTF interactive see https://trex-tgn.cisco.com/youtrack/issue/trex-537
        if (mbuf_const){
            rte_mbuf_set_as_core_const(m);
        }
        */

        char *p=(char *)rte_pktmbuf_append(m, alloc_size);
        int i;
        for (i=0;i<alloc_size; i++) {
            *p=cnt;
            cnt++;
            p++;
        }
        CMbufObject obj;
        obj.m_type =MO_CONST;
        obj.m_mbuf=m;
        
        buf->Add_mbuf(obj);
        size-=alloc_size;
    }
    return(0);
}


void CEmulTxQueue::get_by_offset(uint32_t offset,CBufMbufRef & res){
    uint32_t z=m_q.size();
    assert(z>0);
    if (likely(z==1)) {
        /* most of the time there are *one* buffer in the queue */ 
        CMbufBuffer * b=m_q[0];
        b->get_by_offset(m_tx_offset+offset,res);
        return;
    }
    /* sequential search. The common case there are very few buffers, so it is not a performance  problem*/
    uint32_t calc_of = m_tx_offset+offset; /*m_tx_offset should be in the range of the first buffer */
    int i;
    for (i=0;i<(int)z; i++ ) {
        CMbufBuffer * b=m_q[i];
        uint32_t c_len =m_q[i]->len();
        if ( calc_of < c_len ){
            b->get_by_offset(calc_of,res);
            return;
        }
        calc_of -=c_len;
    }
    /* can't happen */
    assert(0);
}


/* ack number tx_bytes, 
   tx_residue : how many to fill the virtual queue of TCP Tx
   is_zero    : do we want to get event if the queue is zero or almost zero ?

*/   
bool CEmulTxQueue::on_bh_tx_acked(uint32_t tx_bytes,
                              uint32_t & add_to_tcp_queue,
                              bool is_zero){
    uint32_t z=m_q.size();
    assert(z>0);

    m_tx_offset += tx_bytes;
    if (unlikely(z>1)) {
        /* update the vector, remove buffer if needed */
        for (int i=0; i<(int)z; i++ ) {
            CMbufBuffer * b=m_q[0];
            uint32_t c_len =b->len();
            if (m_tx_offset < c_len){
                break;
            }
            m_tx_offset -= c_len;

            m_q_tot_bytes -= c_len;
            m_q.erase(m_q.begin());
        }
    }

    uint32_t residue = m_q_tot_bytes - m_tx_offset; /* how much we have to send */

    add_to_tcp_queue = bsd_umin(tx_bytes,m_v_cc);
    m_v_cc -= add_to_tcp_queue;

    if (residue==0) {
        reset();
    }
    if (is_zero) {
        return(residue?false:true);
    }else{
        if ( (residue==0) || ( (m_v_cc>0)&& (m_v_cc<m_wnd_div_2) )) {
            /*
            There is no need to go next (add another buffer) if  v_cc==0 (byte in outer queue), in any case pipeline won't happen and BDP would be larger. 
            For  example, in case that window> buffer_size. Adding next buffer cost CPU% and better to do it only when there is a need for that.
            */
            return true;
        }
        return(false);
    }
}


void CEmulTxQueue::reset(){
    m_tx_offset=0;
    m_q.clear();
    m_q_tot_bytes=0;
}





