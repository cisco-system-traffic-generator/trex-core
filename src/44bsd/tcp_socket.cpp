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
    }
}


void    sbappend_bytes(struct tcp_socket *so,
                       struct sockbuf *sb, 
                       uint32_t len){
    sb->sb_cc+=len;
    if (len) {
        so->m_app->on_bh_rx_bytes(len,(struct rte_mbuf *) 0);
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

void CTcpApp::tcp_close(){

    tcp_usrclosed(m_ctx,&m_flow->m_tcp);
    if (get_interrupt()==false) {
        m_api->tx_tcp_output(m_ctx,m_flow);
    }

}


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
        m_state = te_DELAY;
        m_api->tcp_delay(cmd->u.m_delay_cmd.m_usec_delay); /* TBD */
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
        assert(0);
        break;
    }
}


void CTcpApp::next(){
    m_cmd_index++;
    if ( m_cmd_index == m_program->get_size() ) {
        /* nothing to do */
        /* TBD */
        tcp_close();
        return;
    }
    CTcpAppCmd * lpcmd=m_program->get_index(m_cmd_index);
    process_cmd(lpcmd);
}

void CTcpApp::start(bool interrupt){
    /* there is at least one command */
    set_interrupt(interrupt);
    CTcpAppCmd * lpcmd=m_program->get_index(m_cmd_index);
    process_cmd(lpcmd);

    set_interrupt(false);
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
        fprintf(fd," tcDELAY for %lu usec \n",u.m_delay_cmd.m_usec_delay);
        break;
    case tcRESET :
        fprintf(fd," reset connection \n");
        break;
    }
}


int utl_mbuf_buffer_create_and_copy(uint8_t socket,
                                    CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint8_t *d,
                                    uint32_t size){
    
    buf->Create(blk_size);
    while (size>0) {
        uint32_t alloc_size=bsd_umin(blk_size,size);
        rte_mbuf_t   * m=tcp_pktmbuf_alloc(socket,alloc_size);
        assert(m);
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
                                    uint32_t size){
    buf->Create(blk_size);
    uint8_t cnt=0; 
    while (size>0) {
        uint32_t alloc_size=bsd_umin(blk_size,size);
        rte_mbuf_t   * m=tcp_pktmbuf_alloc(socket,alloc_size);
        assert(m);
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






