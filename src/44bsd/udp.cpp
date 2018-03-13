#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include "tcp_var.h"
#include <stddef.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/captureFile.h>
#include "utl_mbuf.h"



void CUdpStats::Clear(){
    memset(&m_sts,0,sizeof(udp_stat_int_t));
}

void CUdpStats::Dump(FILE *fd){
}


void CUdpFlow::Create(CTcpPerThreadCtx *ctx,
                      bool client){
    CFlowBase::Create(ctx);
    m_keep_alive_timer.reset();
    m_keep_alive_timer.m_type = ttUDP_FLOW; 
    m_mbuf_socket = ctx->m_mbuf_socket;
    if (client){
        INC_UDP_STAT(m_ctx,udps_connects);
    }else{
        INC_UDP_STAT(m_ctx,udps_accepts);
    }
}

void CUdpFlow::Delete(){
    INC_UDP_STAT(m_ctx,udps_closed);
    disconnect();
}

void CUdpFlow::init(){
    CFlowBase::init();
    /* register the timer */

    m_closed =false;
    /* TBD need to fix this to tunebale */
    m_keepaive_ticks  =  tw_time_msec_to_ticks(1000);

    m_keepalive=1;
    m_ctx->m_timer_w.timer_start(&m_keep_alive_timer,m_keepaive_ticks);
}


void CUdpFlow::update_checksum_and_lenght(CFlowTemplate *ftp,
                                          rte_mbuf_t * m,
                                          uint16_t     udp_pyld_bytes,
                                          char *p){
    UDPHeader * udp =(UDPHeader*)(p+ftp->m_offset_l4);
    if (ftp->m_offload_flags & OFFLOAD_TX_CHKSUM){
        if (!ftp->m_is_ipv6){
            m->l2_len = ftp->m_offset_ip;
            m->l3_len = ftp->m_offset_l4-ftp->m_offset_ip;
            m->ol_flags |= (PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM);
            IPHeader * ipv4=(IPHeader *)(p+ftp->m_offset_ip);
            ipv4->ClearCheckSum();
            ipv4->setTotalLength(udp_pyld_bytes+UDP_HEADER_LEN+IPV4_HDR_LEN);
            udp->setChecksumRaw(pkt_AddInetChecksumRaw(ftp->m_l4_pseudo_checksum ,PKT_NTOHS(udp_pyld_bytes+UDP_HEADER_LEN)));
        }else{
            m->l2_len = ftp->m_offset_ip;
            m->l3_len = ftp->m_offset_l4-ftp->m_offset_ip;
            m->ol_flags |= ( PKT_TX_IPV6 | PKT_TX_UDP_CKSUM);
            IPv6Header * ipv6=(IPv6Header *)(p+ftp->m_offset_ip);
            ipv6->setPayloadLen(udp_pyld_bytes+UDP_HEADER_LEN);
            udp->setChecksumRaw(pkt_AddInetChecksumRaw(ftp->m_l4_pseudo_checksum ,PKT_NTOHS(udp_pyld_bytes+UDP_HEADER_LEN)));
        }
    }else{
        /* no optimization */
        udp->setChecksumRaw(0);
        if (!ftp->m_is_ipv6){
            IPHeader * lpv4=(IPHeader *)(p+ftp->m_offset_ip);
            lpv4->setTotalLength(udp_pyld_bytes+UDP_HEADER_LEN+IPV4_HDR_LEN);
            lpv4->updateCheckSumFast();
        }else{
            IPv6Header * Ipv6=(IPv6Header *)(p+ftp->m_offset_ip);
            Ipv6->setPayloadLen(udp_pyld_bytes+UDP_HEADER_LEN);
        }
    }
}


rte_mbuf_t   * CUdpFlow::alloc_and_build(CMbufBuffer *      buf){
    CFlowTemplate *ftp=&m_template;
    int hlen = ftp->m_offset_l4+UDP_HEADER_LEN;
    rte_mbuf_t * m;
    if ( (hlen+buf->m_t_bytes)>=MAX_PKT_SIZE){
        INC_UDP_STAT(m_ctx,udps_pkt_toobig);
        /* drop the packet */
        return(0);
    }

    m=pktmbuf_alloc(hlen);
    if (m==0) {
        INC_UDP_STAT(m_ctx,udps_nombuf);
        /* drop the packet */
        return(0);
    }
    rte_mbuf_set_as_core_local(m);
    char *p=rte_pktmbuf_append(m,hlen);
    memcpy(p,ftp->m_template_pkt,hlen);
    UDPHeader * udp =(UDPHeader*)(p+ftp->m_offset_l4);
    /* update length*/
    udp->setLength(buf->m_t_bytes+UDP_HEADER_LEN);
    m->pkt_len+=buf->m_t_bytes;
    update_checksum_and_lenght(ftp,m,buf->m_t_bytes,p);

    if (buf->m_num_pkt==1) {
        /* common case, only one mbuf*/
        rte_mbuf_t * mi=buf->m_vec[0].m_mbuf;
        assert(buf->m_vec[0].m_type==MO_CONST);
        rte_pktmbuf_refcnt_update(mi,1);
        m->next = mi;
        m->nb_segs += 1;
        return (m);
    }
    /* multi seg mbuf, could do better. last one does not need the indirect mbuf
      but it is not a common case */
    int i;
    rte_mbuf_t   * cur=m;
    for (i=0; i<buf->m_num_pkt; i++) {
        rte_mbuf_t   * mn=buf->m_vec[i].m_mbuf;

        rte_mbuf_t   * mi = pktmbuf_alloc_small();
        if (mi==0) {
            INC_UDP_STAT(m_ctx,udps_nombuf);
            rte_pktmbuf_free(m);
            return(0);
        }
        rte_mbuf_set_as_core_local(mi);
        /* inc mn ref count */
        rte_pktmbuf_attach(mi,mn);

        cur->next=mi;
        cur=mi;
        m->nb_segs += 1;
    }
    return (m);
}


void CUdpFlow::send_pkt(CMbufBuffer *      buf){
    m_keepalive=0;
    rte_mbuf_t   * m = alloc_and_build(buf);
    if (m==0) {
        return;
    }
    #ifdef _DEBUG
    assert(utl_rte_pktmbuf_verify(m)==0);
    #endif

    INC_UDP_STAT(m_ctx,udps_sndpkt);
    INC_UDP_STAT_CNT(m_ctx,udps_sndbyte,buf->m_t_bytes);

    /* send the packet */
    m_ctx->m_cb->on_tx(m_ctx,0,m);
}


void CUdpFlow::disconnect(){
    if (m_closed) {
        return;
    }
    /* stop the timers, apps etc*/
    if (m_keep_alive_timer.is_running()){
        m_ctx->m_timer_w.timer_stop(&m_keep_alive_timer);
    }
    m_closed=true;
}

void CUdpFlow::set_keepalive(uint64_t  msec){
    m_keepalive=1;
    m_keepaive_ticks  =  tw_time_msec_to_ticks(msec);
    /* stop and restart the timer with new time */
    m_ctx->m_timer_w.timer_stop(&m_keep_alive_timer);
    m_ctx->m_timer_w.timer_start(&m_keep_alive_timer,m_keepaive_ticks);
}


void CUdpFlow::on_tick(){
    if (m_keepalive==0) {
        m_keepalive=1;
        m_ctx->m_timer_w.timer_start(&m_keep_alive_timer,m_keepaive_ticks);
    }else{
        INC_UDP_STAT(m_ctx,udps_keepdrops);
        disconnect();
    }
}


static inline void udp_pktmbuf_adj(struct rte_mbuf * & m, uint16_t len){
    assert(m->pkt_len>=len);
    assert(utl_rte_pktmbuf_adj_ex(m, len)!=NULL);
}

static inline void udp_pktmbuf_trim(struct rte_mbuf *m, uint16_t len){
    assert(m->pkt_len>=len);
    assert(utl_rte_pktmbuf_trim_ex(m, len)==0);
}


/*
 * Drop UDP header, IP headers. go to L7 
   remove pad if exists
 */
static inline void udp_pktmbuf_fix_mbuf(struct rte_mbuf *m, 
                                        uint16_t adj_len,
                                        uint16_t l7_len){

    /*
     * Drop TCP, IP headers and TCP options. go to L7 
     */
    udp_pktmbuf_adj(m, adj_len);

    /*
     * remove padding if exists. 
     */
    if (unlikely(m->pkt_len > l7_len)) {
        uint32_t pad_size = m->pkt_len-(uint32_t)l7_len;
        assert(pad_size<0xffff);
        udp_pktmbuf_trim(m, (uint16_t)pad_size);
    }
}


void CUdpFlow::on_rx_packet(struct rte_mbuf *m,
                            UDPHeader *udp,
                            int offset_l7,
                            int total_l7_len){



    udp_pktmbuf_fix_mbuf(m,(uint16_t)offset_l7,(uint16_t)total_l7_len);
    INC_UDP_STAT(m_ctx,udps_rcvpkt);
    INC_UDP_STAT_CNT(m_ctx,udps_rcvbyte,total_l7_len);
    m_keepalive=0;
    m_app.on_bh_rx_pkts(0,m); /* count the packets */
}

