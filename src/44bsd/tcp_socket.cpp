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


void CTcpApp::force_stop_timer(){
    if (get_timer_init_done()) {
        if (m_timer.is_running()){
            /* must stop timer before free the memory */
            m_ctx->m_timer_w.timer_stop(&m_timer);
        }
    }
}

void CTcpApp::tcp_close(){
    tcp_usrclosed(m_ctx,&m_flow->m_tcp);
    if (get_interrupt()==false) {
        m_api->tx_tcp_output(m_ctx,m_flow);
    }

}

/* on tick */
void CTcpApp::on_tick(){
    next();
}

void CTcpApp::run_cmd_delay_rand(htw_ticks_t min_ticks,
                                 htw_ticks_t max_ticks){
    uint32_t rnd=m_ctx->m_rand->getRandomInRange(min_ticks,max_ticks);
    run_cmd_delay(rnd);
}

void CTcpApp::run_cmd_delay(htw_ticks_t ticks){
    m_state=te_NONE;
    if (!get_timer_init_done()) {
        /* lazey init */
        set_timer_init_done(true);
        m_timer.reset();
        m_timer.m_type  = ttTCP_APP;
    }
    /* make sure the timer is not running*/
    if (m_timer.is_running()) {
        m_ctx->m_timer_w.timer_stop(&m_timer);
    }
    m_ctx->m_timer_w.timer_start(&m_timer,ticks);
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
void CTcpApp::process_cmd(CTcpAppCmd * cmd){

    switch (cmd->m_cmd) {
    case  tcTX_BUFFER   :
        {
            m_tx_offset =0;
            m_tx_active = cmd->u.m_tx_cmd.m_buf; /* tx is active */
            assert(m_tx_active);
            uint32_t add_to_queue = bsd_umin(m_api->get_tx_max_space(m_flow),m_tx_active->m_t_bytes);
            m_tx_residue = m_tx_active->m_t_bytes - add_to_queue;
            /* append to tx queue the maximum bytes */
            m_api->tx_sbappend(m_flow,add_to_queue);
            m_state=te_SEND;
            if (get_interrupt()==false) {
                m_api->tx_tcp_output(m_ctx,m_flow);
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
            if (flags & CTcpAppCmdRxBuffer::rxcmd_CLEAR) {
                m_cmd_rx_bytes =0;
            }
            /* disable rx thread if needed */
            set_rx_disabled((flags & CTcpAppCmdRxBuffer::rxcmd_DISABLE_RX)?true:false);

            if (flags & CTcpAppCmdRxBuffer::rxcmd_WAIT) {
                m_state=te_WAIT_RX;
                m_cmd_rx_bytes_wm = cmd->u.m_rx_cmd.m_rx_bytes_wm;
                check_rx_condition();
            }else{
                next();
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
                    next();
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
        next();
        }
        break;

    case tcJMPNZ : 
        {
            assert(cmd->u.m_jmpnz.m_var_id<apVAR_NUM_SIZE);
            if (--m_vars[cmd->u.m_jmpnz.m_var_id]>0){
                /* action jump  */
                m_cmd_index+=cmd->u.m_jmpnz.m_offset;
                /* make sure we are not in at the end */
                int end=m_program->get_size();
                if (m_cmd_index>end) {
                    m_cmd_index=end;
                }
            }
            next();
        }
        break;
    default:
        assert(0);
    }
}


void CTcpApp::next(){
    m_cmd_index++;
    if (m_cmd_index>m_program->get_size()) {
        /* could be in cases we get data after close of flow */
        assert(m_flow->is_close_was_called());
        return;
    }
    if ( m_cmd_index == m_program->get_size() ) {
        tcp_close();
        return;
    }
    CTcpAppCmd * lpcmd=m_program->get_index(m_cmd_index);
    process_cmd(lpcmd);
}


void CTcpApp::run_dpc_callbacks(){
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

void CTcpApp::start(bool interrupt){
    /* there is at least one command */
    set_interrupt(interrupt);
    CTcpAppCmd * lpcmd=m_program->get_index(m_cmd_index);
    process_cmd(lpcmd);

    set_interrupt(false);
}


void CTcpApp::do_disconnect(){
    if (!m_flow->is_close_was_called()){
        m_api->disconnect(m_ctx,m_flow);
    }
}

void CTcpApp::do_close(){
    if (!m_flow->is_close_was_called()){
        tcp_close();
    }
}


int CTcpApp::on_bh_tx_acked(uint32_t tx_bytes){
    assert(m_tx_active);
    assert(m_state==te_SEND);
    set_interrupt(true);
    m_tx_offset+=tx_bytes;
    if (m_tx_residue){
        uint32_t add_to_queue = bsd_umin(tx_bytes,m_tx_residue);
        m_api->tx_sbappend(m_flow,add_to_queue);
        m_tx_residue-=add_to_queue;
    }else{
        if ( m_tx_offset == m_tx_active->m_t_bytes){
            m_tx_active = (CMbufBuffer *)0;
            m_tx_offset=0;
            m_tx_residue=0;
            next();
        }
    }
    set_interrupt(false);
    return(0);
}


void CTcpApp::on_bh_event(tcp_app_events_t event){
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
    //printf(" event %d %s \n",(int)m_debug_id,get_tcp_app_events_name(event).c_str());
}

/* rx bytes */
int CTcpApp::on_bh_rx_bytes(uint32_t rx_bytes,
                            struct rte_mbuf * m){
    set_interrupt(true);
    /* for now do nothing with the mbuf */
    if (m) {
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


void CTcpAppProgram::add_cmd(CTcpAppCmd & cmd){
    m_cmds.push_back(cmd);
}


bool CTcpAppProgram::sanity_check(std::string & err){
    int i;
    for (i=0; i<m_cmds.size(); i++) {
        CTcpAppCmd * lp=&m_cmds[i];
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
            if (lp->u.m_var.m_var_id>=CTcpApp::apVAR_NUM_SIZE) {
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


void CTcpAppProgram::Dump(FILE *fd){
    int i;
    fprintf(fd," cmds : %lu \n",(ulong)get_size());
    fprintf(fd," --------------\n");

    for (i=0; i<m_cmds.size(); i++) {
        CTcpAppCmd * lp=&m_cmds[i];
        fprintf(fd," cmd : %d ",(int)i);
        lp->Dump(fd);
    }
}


void CTcpAppCmd::Dump(FILE *fd){
    switch (m_cmd) {
    case tcTX_BUFFER :
         fprintf(fd," tcTX_BUFFER %lu bytes\n",(ulong)u.m_tx_cmd.m_buf->m_t_bytes);
         break;
    case tcRX_BUFFER :
        if (u.m_rx_cmd.m_flags & CTcpAppCmdRxBuffer::rxcmd_CLEAR) {
            fprintf(fd," tcRX_BUFFER clear count,");
        }
        if (u.m_rx_cmd.m_flags & CTcpAppCmdRxBuffer::rxcmd_DISABLE_RX) {
            fprintf(fd," tcRX_BUFFER disable RX,");
        }else{
            fprintf(fd," tcRX_BUFFER enable RX ,");
        }

        if (u.m_rx_cmd.m_flags & CTcpAppCmdRxBuffer::rxcmd_WAIT) {
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
        if (mbuf_const){
            rte_mbuf_set_as_core_const(m);
        }
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
        if (mbuf_const){
            rte_mbuf_set_as_core_const(m);
        }

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






