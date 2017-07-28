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

#include <common/gtest.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <common/utl_gcc_diag.h>
#include <cmath>

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

#include <stdlib.h>
#include <common/c_common.h>
#include <common/captureFile.h>
#include <common/sim_event_driven.h>



class gt_tcp  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};







#if 0
TEST_F(gt_tcp, tst2) {
    CTcpPerThreadCtx tcp_ctx;
    CTcpFlow   flow;
    flow.Create(&tcp_ctx);

    tcp_ctx.Create(100,false);
    tcp_ctx.timer_w_start(&flow);
    int i;
    for (i=0; i<40; i++) {
        printf(" tick %d \n",i);
        tcp_ctx.timer_w_on_tick();
    }
    tcp_ctx.Delete();
}
#endif


TEST_F(gt_tcp, tst1) {
#if 0
    CTcpFlow   flow;
    flow.Create();
    flow.on_tick();
    flow.on_tick();
    flow.Delete();
#endif
}



TEST_F(gt_tcp, tst3) {
    printf(" MSS %d \n",(int)TCP_MSS);
    printf(" sizeof_tcpcb %d \n",(int)sizeof(tcpcb));
    tcpstat tcp_stats;
    tcp_stats.Clear();
    tcp_stats.m_sts.tcps_accepts++;
    tcp_stats.m_sts.tcps_connects++;
    tcp_stats.m_sts.tcps_connects++;
    tcp_stats.Dump(stdout);
}

TEST_F(gt_tcp, tst4) {
    tcp_socket socket;
    socket.so_options=0;
    printf(" %d \n", (int)sizeof(socket));
}




class CTcpReassTest  {

public:
    bool Create();
    void Delete();
    void add_pkts(vec_tcp_reas_t & list);
    void expect(vec_tcp_reas_t & list,FILE *fd);

public:
    bool                    m_verbose;

    CTcpPerThreadCtx        m_ctx;
    CTcpReass               m_tcp_res;
    CTcpFlow                m_flow;
};

bool CTcpReassTest::Create(){
    m_ctx.Create(100,false);
    m_flow.Create(&m_ctx);
    return(true);
}

void CTcpReassTest::Delete(){
    m_flow.Delete();
    m_ctx.Delete();
}

void CTcpReassTest::add_pkts(vec_tcp_reas_t & lpkt){
    int i;
    tcpiphdr ti;

    for (i=0; i<lpkt.size(); i++) {
        ti.ti_seq  = lpkt[i].m_seq;
        ti.ti_len  = (uint16_t)lpkt[i].m_len;
        ti.ti_flags =lpkt[i].m_flags;
        m_tcp_res.pre_tcp_reass(&m_ctx,&m_flow.m_tcp,&ti,(struct rte_mbuf *)0);
        if (m_verbose) {
            fprintf(stdout," inter %d \n",i);
            m_tcp_res.Dump(stdout);
        }
    }
}

void CTcpReassTest::expect(vec_tcp_reas_t & lpkt,FILE *fd){
    m_tcp_res.expect(lpkt,fd);
}


TEST_F(gt_tcp, tst5) {
    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0},
                              {.m_seq=2000,.m_len=20,.m_flags=0}
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);

    tst.Delete();
}





TEST_F(gt_tcp, tst6) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1000,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0}
                              
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvdupbyte,20);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);
    tst.Delete();

}

TEST_F(gt_tcp, tst7) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1001,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=21,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartdupbyte,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);

    tst.Delete();
}

TEST_F(gt_tcp, tst8) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=900,.m_len=3000,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=900,.m_len=3000,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvduppack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvdupbyte,40);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,3);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,3040);

    tst.Delete();
}

TEST_F(gt_tcp, tst9) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=TH_FIN},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=21,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=TH_FIN},
                              {.m_seq=2000,.m_len=21,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);


    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartdupbyte,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,3);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,61);
    tst.Delete();
}

TEST_F(gt_tcp, tst10) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=3000,.m_len=20,.m_flags=0},
                               {.m_seq=4000,.m_len=20,.m_flags=0},
                               {.m_seq=5000,.m_len=20,.m_flags=0},

                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=3000,.m_len=20,.m_flags=0},
                               {.m_seq=4000,.m_len=20,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,5);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,100);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopackdrop,1);
    tst.Delete();
}


TEST_F(gt_tcp, tst11) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1020,.m_len=20,.m_flags=0},
                               {.m_seq=1040,.m_len=20,.m_flags=0}

                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=60,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);
    tst.Delete();
}

#if 0

TEST_F(gt_tcp, tst12) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1020,.m_len=20,.m_flags=0},
                               {.m_seq=1040,.m_len=20,.m_flags=0},
                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=60,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_flow.m_tcp.rcv_nxt =500;
    tst.m_flow.m_tcp.t_state =TCPS_ESTABLISHED;

    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);

    tcpiphdr ti;

    ti.ti_seq  = 500;
    ti.ti_len  = 500;
    ti.ti_flags =0;

    tst.m_tcp_res.tcp_reass(&tst.m_ctx,&tst.m_flow.m_tcp,&ti,(struct rte_mbuf *)0);

    EXPECT_EQ(tst.m_tcp_res.get_active_blocks(),0);
    EXPECT_EQ(tst.m_flow.m_tcp.rcv_nxt,500+560);

    tst.m_ctx.m_tcpstat.Dump(stdout);
    tst.Delete();
}
#endif


void set_mbuf_test(struct rte_mbuf  *m,
                   uint32_t len,
                   uintptr_t addr){
    memset(m,0,sizeof(struct rte_mbuf));
    /* one segment */
    m->nb_segs=1;
    m->pkt_len  =len;
    m->data_len=len;
    m->buf_len=1024*2+128;
    m->data_off=128;
    m->refcnt_reserved=1;
    m->buf_addr=(void *)addr;
}



TEST_F(gt_tcp, tst13) {

    CMbufBuffer buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf.Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf.Add_mbuf(mbuf_obj);


    buf.Verify();

    CBufMbufRef res;

    buf.get_by_offset(0,res);
    EXPECT_EQ(res.m_offset,0);
    EXPECT_EQ(res.m_mbuf,&mbuf0);
    EXPECT_EQ(res.get_mbuf_size(),blk_size);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),false);

    buf.get_by_offset(1,res);
    EXPECT_EQ(res.m_offset,1);
    EXPECT_EQ(res.m_mbuf,&mbuf0);
    EXPECT_EQ(res.get_mbuf_size(),blk_size-1);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);

    buf.get_by_offset(2048+2,res);
    EXPECT_EQ(res.m_offset,2);
    EXPECT_EQ(res.m_mbuf,&mbuf2);
    EXPECT_EQ(res.get_mbuf_size(),blk_size-2-10);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);


    buf.get_by_offset(2048+1024-11,res);
    EXPECT_EQ(res.m_offset,1024-11);
    EXPECT_EQ(res.m_mbuf,&mbuf2);
    EXPECT_EQ(res.get_mbuf_size(),1);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);


    buf.Dump(stdout);
    //buf.Delete();
}

TEST_F(gt_tcp, tst14) {

    CMbufBuffer buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf.Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf.Add_mbuf(mbuf_obj);

    buf.Verify();

    buf.Dump(stdout);
    //buf.Delete();
}


#if 0
TEST_F(gt_tcp, tst15) {

    CTcpApp app;

    CTcpSockBuf  tx_sock;
    tx_sock.Create(8*1024);
    tx_sock.m_app=&app;

    CMbufBuffer * buf =&app.m_write_buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf->Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf->Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf->Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf->Add_mbuf(mbuf_obj);

    buf->Verify();

    buf->Dump(stdout);

    EXPECT_EQ(tx_sock.get_sbspace(),8*1024);

    
    //tx_sock.sb_start_new_buffer();

    /* append maximum */
    tx_sock.sbappend(min(buf->m_t_bytes,tx_sock.sb_hiwat));
    EXPECT_EQ(tx_sock.get_sbspace(),8*1024-(1024*3-10));

    int mss=120;
    CBufMbufRef res;
    int i;
    for (i=0; i<2; i++) {
        tx_sock.get_by_offset((CTcpPerThreadCtx *)0,mss*i,res);
        res.Dump(stdout);
    }

    tx_sock.sbdrop(1024);

    mss=120;
    for (i=0; i<2; i++) {
        tx_sock.get_by_offset((CTcpPerThreadCtx *)0,mss*i,res);
        res.Dump(stdout);
    }

    tx_sock.sbdrop(1024+1024-10);
    EXPECT_EQ(tx_sock.sb_cc,0);


    //buf.Delete();
}
#endif


#if 0
TEST_F(gt_tcp, tst16) {

    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


    //CTcpApp app;
    //utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,2048*5+10);
    //app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));

    CTcpPkt pkt;
    uint32_t offset=0;
    uint32_t diff=250;

    int i;
    for (i=0; i<10; i++) {
        printf(" %lu \n",(ulong)offset);
        tcp_build_dpkt(&m_ctx,
                       &m_flow.m_tcp,
                       offset, 
                       diff,
                       20, 
                       pkt);
        offset+=diff;

        rte_pktmbuf_dump(pkt.m_buf, diff+500);
        rte_pktmbuf_free(pkt.m_buf);
    }




    m_flow.Delete();
    m_ctx.Delete();

    app.m_write_buf.Delete();

}
#endif

#if 0

TEST_F(gt_tcp, tst17) {

    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,10);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));

    CTcpPkt pkt;
    uint32_t offset=0;
    uint32_t diff=10;

    int i;
    for (i=0; i<1; i++) {
        printf(" %lu \n",(ulong)offset);
        tcp_build_dpkt(&m_ctx,
                       &m_flow.m_tcp,
                       offset, 
                       diff,
                       20, 
                       pkt);
        offset+=diff;

        rte_pktmbuf_dump(pkt.m_buf, diff+500);
        rte_pktmbuf_free(pkt.m_buf);
    }


    m_ctx.m_tcpstat.Dump(stdout);


    m_flow.Delete();
    m_ctx.Delete();

    app.m_write_buf.Delete();

}
#endif

#if 0
/* tcp_output simulation .. */
TEST_F(gt_tcp, tst18) {
#if 0
    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


#if 0

    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,10);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));
#endif


    tcp_connect(&m_ctx,&m_flow.m_tcp);
    int i;
    for (i=0; i<1000; i++) {
        //printf(" tick %lu \n",(ulong)i);
        m_ctx.timer_w_on_tick();
    }


    m_ctx.m_tcpstat.Dump(stdout);

    m_flow.Delete();
    m_ctx.Delete();

#endif
    //app.m_write_buf.Delete();

}

#endif



rte_mbuf_t   * test_build_packet_size(int pkt_len,
                                      int chunk_size,
                                      int rand){

    rte_mbuf_t   * m;
    rte_mbuf_t   * prev_m=NULL;;
    rte_mbuf_t   * mhead=NULL;
    uint8_t seg=0;
    uint32_t save_pkt_len=pkt_len;

    uint8_t cnt=1;
    while (pkt_len>0) {
        m=tcp_pktmbuf_alloc(0,chunk_size);
        seg++;

        assert(m);
        if (prev_m) {
            prev_m->next=m;
        }
        prev_m=m;
        if (mhead==NULL) {
            mhead=m;
        }

        uint16_t   csize=(uint16_t)std::min(chunk_size,pkt_len);
        utl_rte_pktmbuf_fill(m,
                       csize,
                       cnt,
                       rand);
        pkt_len-=csize;
    }
    mhead->pkt_len = save_pkt_len;
    mhead->nb_segs = seg;
    return(mhead);
}


TEST_F(gt_tcp, tst20) {
    uint16_t off=0;
    char buf[2048];
    char *p =buf;

    rte_mbuf_t   * m;
    m=test_build_packet_size(1024,
                             60,
                             0);
    //rte_pktmbuf_dump(m, 1024);

    off=8;
    m=utl_rte_pktmbuf_cpy(p,m,512,off);
//    utl_DumpBuffer(stdout,p,512,0);
}

/* deep copy of mbufs from tx to rx */
int test_mbuf_deepcpy(int pkt_size,int chunk_size){

    rte_mbuf_t   * m;
    rte_mbuf_t   * mc;

    m=test_build_packet_size(pkt_size,chunk_size,1);
    //rte_pktmbuf_dump(m, pkt_size);

    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);

    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    //rte_pktmbuf_dump(mc, pkt_size);

    int res=utl_rte_pktmbuf_deepcmp(m,mc);

    rte_pktmbuf_free(m);
    rte_pktmbuf_free(mc);
    return(res);
}

TEST_F(gt_tcp, tst21) {
   EXPECT_EQ(test_mbuf_deepcpy(128,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1024,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1025,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1026,67),0);
   EXPECT_EQ(test_mbuf_deepcpy(4025,129),0);
}



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



//typedef std::vector<rte_mbuf_t *> vec_queue_t;

class CTcpCtxDebug : public CTcpCtxCb {
public:

   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * tp,
             rte_mbuf_t *m);

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CTcpFlow * flow);


public:
    CClientServerTcp  * m_p ;

};

rte_mbuf_t * utl_rte_convert_tx_rx_mbuf(rte_mbuf_t *m){

    rte_mbuf_t * mc;
    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);
    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    rte_pktmbuf_free(m); /* free original */
    return (mc);
}

void utl_rte_pktmbuf_k12_format(FILE* fp,
                                rte_mbuf_t *m,
                                int time_sec) {

    char *p;
    uint32_t pktlen=m->pkt_len;
    p=utl_rte_pktmbuf_to_mem(m);
    utl_k12_pkt_format(stdout,p, pktlen, time_sec) ;    
    free(p);
}



#if 0
bool test_handle_queue(vec_queue_t * lpq,
                       CTcpPerThreadCtx *ctx,
                       struct tcpcb * flow){

    bool do_work=false;
    rte_mbuf_t *m;
    while (lpq->size()) {
        do_work=true;
        m=(*lpq)[0];
        lpq->erase(lpq->begin());

        char *p=rte_pktmbuf_mtod(m,char *);
        TCPHeader * tcp=(TCPHeader *)(p+14+20);
        IPHeader   *ipv4=(IPHeader *)(p+14);


        assert(tcp_flow_input(ctx,
                       flow,
                       m,
                       tcp,
                       14+20+tcp->getHeaderLength(),
                       ipv4->getTotalLength()-(20+tcp->getHeaderLength()) 
                       )==0);

    }
    return (do_work);
}
#endif


typedef enum {  csSIM_NONE    =0,
                csSIM_RST_SYN = 0x17,
                csSIM_RST_SYN1,
                csSIM_WRONG_PORT,
                csSIM_RST_MIDDLE , // corrupt the seq number
                csSIM_RST_MIDDLE2 ,// send RST flag
               } cs_sim_mode_t_;

typedef uint16_t cs_sim_mode_t ;


class CClientServerTcp {
public:
    bool Create(std::string pcap_file);
    void Delete();

    void set_debug_mode(bool enable);
    void set_simulate_rst_error(cs_sim_mode_t  sim_type){
        m_sim_type = sim_type;
    }

    /* dir ==0 , C->S 
       dir ==1   S->C */
    void on_tx(int dir,rte_mbuf_t *m);
    void on_rx(int dir,rte_mbuf_t *m);

public:
    int test2();
    int simple_http();


public:
    CTcpPerThreadCtx        m_c_ctx;  /* context */
    CTcpPerThreadCtx        m_s_ctx;


    CTcpAppApiImpl          m_tcp_bh_api_impl_c;
    CTcpAppApiImpl          m_tcp_bh_api_impl_s;

    CTcpCtxPcapWrt          m_c_pcap; /* capture to file */
    CTcpCtxPcapWrt          m_s_pcap;
    bool                    m_debug;
    cs_sim_mode_t           m_sim_type;
    uint16_t                m_sim_data;


    CTcpCtxDebug            m_io_debug;

    CSimEventDriven         m_sim;
    double                  m_rtt_sec;
    double                  m_tx_diff;
    uint16_t                m_vlan;
};



int CTcpCtxDebug::on_flow_close(CTcpPerThreadCtx *ctx,
                                CTcpFlow * flow){
    /* bothing to do */

    return(0);
}


int CTcpCtxDebug::on_tx(CTcpPerThreadCtx *ctx,
                        struct tcpcb * tp,
                        rte_mbuf_t *m){
    int dir=0;
    if (tp->src_port<1024) {
        dir=1;
    }
    rte_mbuf_t *m_rx= utl_rte_convert_tx_rx_mbuf(m);

    m_p->on_tx(dir,m_rx);
    return(0);
}


void CClientServerTcp::set_debug_mode(bool enable){
    m_debug=enable;
}

bool CClientServerTcp::Create(std::string pcap_file){

    m_debug=false;
    m_sim_type=csSIM_NONE;
    m_sim_data=0;
    m_io_debug.m_p = this;
    m_tx_diff =0.0;
    m_vlan =0;

    m_rtt_sec =0.05; /* 50msec */

    m_c_pcap.open_pcap_file(pcap_file+"_c.pcap");

    m_s_pcap.open_pcap_file(pcap_file+"_s.pcap");

    m_c_ctx.Create(100,true);
    m_c_ctx.set_cb(&m_io_debug);

    m_s_ctx.Create(100,false);
    m_s_ctx.set_cb(&m_io_debug);


    return(true);
}

void CClientServerTcp::on_tx(int dir,
                             rte_mbuf_t *m){

    m_tx_diff+=1/1000000.0;  /* to make sure there is no out of order */
    double t=m_sim.get_time()+m_tx_diff;


    /* write TX side */
    if (dir==0) {
        m_c_pcap.write_pcap_mbuf(m,t);
    }else{
        m_s_pcap.write_pcap_mbuf(m,t);
    }


    bool drop=false;
    /* simulate drop/reorder/ corruption HERE !! */
    if (m_sim_type > 0) {
        char * p=rte_pktmbuf_mtod(m,char *);
        TCPHeader * tcp=(TCPHeader *)(p+14+20);
        //IPHeader * ip=(IPHeader *)(p+14);

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
                if ( (t> 0.4) && (t> 0.5)){
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

        
    }

    if (drop==false){
        /* move the Tx packet to Rx side of server */
        m_sim.add_event( new CTcpSimEventRx(this,m,dir^1,(t+(m_rtt_sec/2.0) )) );
    }else{
        rte_pktmbuf_free(m);
    }
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

    ctx->m_ft.rx_handle_packet(ctx,m);
}


void CClientServerTcp::Delete(){
    m_c_ctx.Delete();
    m_s_ctx.Delete();
}


int CClientServerTcp::test2(){


    CMbufBuffer * buf;
    CTcpAppProgram * prog_c;
    CTcpAppProgram * prog_s;
    CTcpFlow          *  c_flow; 

    CTcpApp * app_c;
    //CTcpApp * app_s;
    CTcpAppCmd cmd;

    uint32_t tx_num_bytes=100*1024;

    c_flow = m_c_ctx.m_ft.alloc_flow(&m_c_ctx,0x10000001,0x30000001,1025,80,m_vlan,false);
    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(0x10000001);
    c_tuple.set_port(1025);
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

    assert(m_c_ctx.m_ft.insert_new_flow(c_flow,c_tuple)==true);


    printf("client  %p \n",c_flow);

    app_c = &c_flow->m_app;


    /* CONST */
    buf = new CMbufBuffer();
    prog_c = new CTcpAppProgram();
    prog_s = new CTcpAppProgram();
    utl_mbuf_buffer_create_and_fill(buf,2048,tx_num_bytes);


    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf;

    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = tx_num_bytes;

    prog_s->add_cmd(cmd);


    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,c_flow);
    app_c->set_debug_id(1);
    c_flow->set_app(app_c);


    m_s_ctx.m_ft.set_tcp_api(&m_tcp_bh_api_impl_s);
    m_s_ctx.m_ft.set_tcp_program(prog_s);


    m_rtt_sec = 0.05;

    m_sim.add_event( new CTcpSimEventTimers(this, (((double)(TCP_TIMER_W_TICK)/((double)TCP_TIMER_W_DIV*1000.0)))));
    m_sim.add_event( new CTcpSimEventStop(1000.0) );

    /* start client */
    app_c->start(true);
    tcp_connect(&m_c_ctx,&c_flow->m_tcp);

    m_sim.run_sim();

    printf(" C counters \n");
    m_c_ctx.m_tcpstat.Dump(stdout);
    m_c_ctx.m_ft.dump(stdout);
    printf(" S counters \n");
    m_s_ctx.m_tcpstat.Dump(stdout);
    m_s_ctx.m_ft.dump(stdout);


    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_sndbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_rcvackbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_connects,1);


    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_rcvbyte,4024);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_accepts,1);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_preddat,m_s_ctx.m_tcpstat.m_sts.tcps_rcvpack-1);


    //app.m_write_buf.Delete();
    //printf (" rx %d \n",m_s_flow.m_tcp.m_socket.so_rcv.sb_cc);
    //assert( m_s_flow.m_tcp.m_socket.so_rcv.sb_cc == 4024);


    //delete app_c;
    //delete app_s;

    delete prog_c;
    delete prog_s;

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


int CClientServerTcp::simple_http(){


    CMbufBuffer * buf_req;
    CMbufBuffer * buf_res;
    CTcpAppProgram * prog_c;
    CTcpAppProgram * prog_s;
    CTcpFlow          *  c_flow; 

    CTcpApp * app_c;
    //CTcpApp * app_s;
    CTcpAppCmd cmd;
    uint32_t http_r_size=32*1024;

    c_flow = m_c_ctx.m_ft.alloc_flow(&m_c_ctx,0x10000001,0x30000001,1025,80,m_vlan,false);
    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(0x10000001);
    c_tuple.set_port(1025);
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

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

    prog_c = new CTcpAppProgram();
    prog_s = new CTcpAppProgram();

    uint8_t* http_r=(uint8_t*)allocate_http_res(http_r_size);

    utl_mbuf_buffer_create_and_copy(buf_req,2048,(uint8_t*)http_req,sizeof(http_req));
    utl_mbuf_buffer_create_and_copy(buf_res,2048,(uint8_t*)http_r,http_r_size);


    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_req;
    prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf_res;
    prog_s->add_cmd(cmd);


    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,c_flow);
    app_c->set_debug_id(1);
    c_flow->set_app(app_c);


    m_s_ctx.m_ft.set_tcp_api(&m_tcp_bh_api_impl_s);
    m_s_ctx.m_ft.set_tcp_program(prog_s);


    m_rtt_sec = 0.05;

    m_sim.add_event( new CTcpSimEventTimers(this, (((double)(TCP_TIMER_W_TICK)/((double)TCP_TIMER_W_DIV*1000.0)))));
    m_sim.add_event( new CTcpSimEventStop(1000.0) );

    /* start client */
    app_c->start(true);
    tcp_connect(&m_c_ctx,&c_flow->m_tcp);

    m_sim.run_sim();

    printf(" C counters \n");
    m_c_ctx.m_tcpstat.Dump(stdout);
    m_c_ctx.m_ft.dump(stdout);
    printf(" S counters \n");
    m_s_ctx.m_tcpstat.Dump(stdout);
    m_s_ctx.m_ft.dump(stdout);


    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_sndbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_rcvackbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_connects,1);


    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_rcvbyte,4024);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_accepts,1);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_preddat,m_s_ctx.m_tcpstat.m_sts.tcps_rcvpack-1);


    //app.m_write_buf.Delete();
    //printf (" rx %d \n",m_s_flow.m_tcp.m_socket.so_rcv.sb_cc);
    //assert( m_s_flow.m_tcp.m_socket.so_rcv.sb_cc == 4024);


    //delete app_c;
    //delete app_s;

    free_http_res((char *)http_r);
    delete prog_c;
    delete prog_s;

    buf_req->Delete();
    delete buf_req;

    buf_res->Delete();
    delete buf_res;

    return(0);

}



CClientServerTcp tcp_test1;




TEST_F(gt_tcp, tst30) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2");

    lpt1->test2();

    lpt1->Delete();

    delete lpt1;

}

TEST_F(gt_tcp, tst30_vlan) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_vlan");
    lpt1->m_vlan=100;

    lpt1->test2();

    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http");
    lpt1->m_vlan=100;

    lpt1->simple_http();

    lpt1->Delete();

    delete lpt1;
}



TEST_F(gt_tcp, tst30_http_simple) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http");
    lpt1->set_debug_mode(true);
    
    lpt1->simple_http();

    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http_rst) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http_rst");
    lpt1->set_debug_mode(true);
    lpt1->set_simulate_rst_error(csSIM_RST_SYN);
    
    lpt1->simple_http();

    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http_rst1) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http_rst1");
    lpt1->set_debug_mode(true);
    lpt1->set_simulate_rst_error(csSIM_RST_SYN1);
    
    lpt1->simple_http();

    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http_wrong_port) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http_wrong_port");
    lpt1->set_debug_mode(true);
    lpt1->set_simulate_rst_error(csSIM_WRONG_PORT);
    
    lpt1->simple_http();

    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http_rst_middle) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http_rst_middle");
    lpt1->set_debug_mode(true);
    lpt1->set_simulate_rst_error(csSIM_RST_MIDDLE);
    lpt1->simple_http();
    lpt1->Delete();

    delete lpt1;
}

TEST_F(gt_tcp, tst30_http_rst_middle1) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2_http_rst_middle");
    lpt1->set_debug_mode(true);
    lpt1->set_simulate_rst_error(csSIM_RST_MIDDLE2);
    lpt1->simple_http();
    lpt1->Delete();

    delete lpt1;
}



TEST_F(gt_tcp, tst31) {

    CMbufBuffer * buf;
    CTcpAppProgram * prog;
    CTcpApp * app;

    app = new CTcpApp();
    buf = new CMbufBuffer();

    utl_mbuf_buffer_create_and_fill(buf,2048,100*1024);

    prog = new CTcpAppProgram();

    CTcpAppCmd cmd;
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf;


    prog->add_cmd(cmd);

    cmd.m_cmd = tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags = CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = 1000;

    prog->add_cmd(cmd);

    cmd.m_cmd = tcDELAY;
    cmd.u.m_delay_cmd.m_usec_delay  = 1000;

    prog->add_cmd(cmd);

    prog->Dump(stdout);

    app->set_program(prog);


    delete app;
    delete prog;
    buf->Delete();
    delete buf;
}



#include <common/dlist.h>




#if 0
class CRxCheckFlowTableHash   {
public:
    bool Create(int max_size){
		return ( m_hash.Create(max_size,0,false,false,true) );
	}
    void Delete(){
		m_hash.Delete();
	}
    bool remove(uint64_t fid ) {
		return(m_hash.Remove(fid)==hsOK?true:false);
	}
    CRxCheckFlow * lookup(uint64_t fid ){
		rx_c_hash_ent_t *lp=m_hash.Find(fid);
		if (lp) {
			return (&lp->value);
		}else{
			return ((CRxCheckFlow *)NULL);
		}
	}
    CRxCheckFlow * add(uint64_t fid ){
		rx_c_hash_ent_t *lp;
		assert(m_hash.Insert(fid,lp)==hsOK);
		return (&lp->value);
	}

    void remove_all(void){
		
	}
    void dump_all(FILE *fd){
		m_hash.Dump(fd);
	}
    uint64_t count(void){
		return ( m_hash.GetSize());

	}
public:

	rx_c_hash_t                  m_hash;
};

#endif




struct CMyNode {
    TCDListNode m_node;
    uint32_t    m_id;
};
      

TEST_F(gt_tcp, tst32) {
    TCGenDList list;
    printf(" dlist tests  \n");
    list.Create();
#if 1
    int i;
    for (i=0; i<10; i++) {
        CMyNode * lp=new CMyNode();
        lp->m_id=i;
        lp->m_node.reset();
        list.insert_head(&lp->m_node);
    }
#endif
    TCGenDListIterator it(list);

    CMyNode * lp=0;
    for ( ;(lp=(CMyNode *)it.node()); it++){
        printf(" id: %lu \n",(ulong)lp->m_id);
    }

    while (!list.is_empty()) {
        delete list.remove_head();
    }
}


#include <common/closehash.h>


TEST_F(gt_tcp, tst33) {
    CFlowKeyTuple key;
    key.dump(stdout);
    key.set_ip(0x12345678);
    key.set_port(1025);
    key.set_proto(06);
    key.set_ipv4(true);
    key.dump(stdout);
    printf(" %lx \n",(ulong)key.get_as_uint64());
}

#include <algorithm>
#include <map>

typedef std::map<uint64_t, uint32_t, std::less<uint64_t> > hash_test_map_t;
typedef hash_test_map_t::iterator hash_test_map_iter_t;

struct chash_result {
    uint32_t max_lookup;
    double   table_util;
    double   lookup_hit;
};

/* to check hash function */
int hash_func_test(uint32_t mask,int clients,int ports,chash_result & res){
    int i;
    hash_test_map_t map_tbl;
    int c;
    int hit=0;
    int lookup=0;
    int max_lookup=0;

    for (c=0; c<clients; c++) {
        for (i=1025; i<(ports+1025); i++) {
            lookup++;

            CFlowKeyTuple key;
            key.set_ip(0x16000001+c);
            key.set_port(i);
            key.set_proto(06);
            key.set_ipv4(true);
            uint32_t mkey = key.get_hash()&mask;
            uint32_t val=0;

            hash_test_map_iter_t iter;
            iter = map_tbl.find(mkey);
            if (iter != map_tbl.end() ) {
                val=((*iter).second);
                hit++;
                (*iter).second=val+1;
                if (val+1>max_lookup){
                    max_lookup=val+1;
                }

            }else{
                map_tbl.insert(hash_test_map_t::value_type(mkey,1));
            }
        }
    }

    res.max_lookup = max_lookup;
    res.table_util = (ulong)100.0*(lookup)/(double)mask;
    res.lookup_hit = ((double)(hit+lookup)/(double)lookup);


    printf(" lookup        : %lu \n",(ulong)lookup);
    printf(" hit           : %lu \n",(ulong)hit);
    printf(" table size    : %lu \n",(ulong)mask);
    printf(" max_lookup    : %lu \n",(ulong)max_lookup);
    printf(" table_util    : %f  \n",res.table_util);
    printf(" lookup factor %.1f %% \n",res.lookup_hit);
    return (0);
}

#if 0

/* check hash */
TEST_F(gt_tcp, tst34) {

    uint32_t mask=((1<<20)-1);
    int i; 
    chash_result res;
    for (i=1000;i<6000; i+=100) {
        hash_func_test(mask,255,i,res);
    }
}

TEST_F(gt_tcp, tst35) {

    int c; int i;
    int p=0;

    uint32_t mask=((1<<20)-1);

    for (c=0; c<1000; c++) {
        for (i=1025; i<60000; i++) {
            CFlowKeyTuple key;
            key.set_ip(0x16000001+c);
            key.set_port(i);
            key.set_proto(06);
            key.set_ipv4(true);
            uint32_t mkey = key.get_hash()&mask;
            p+=mkey;
        }
    }
}

#endif

typedef CHashEntry<flow_key_t> test_hash_ent_t;
typedef CCloseHash<flow_key_t> test_hash_t;


class CMyFlowTest {
public:
    test_hash_ent_t m_hash_key;
    uint32_t        m_id;
};


TEST_F(gt_tcp, tst36) {

    test_hash_t   ht;
    test_hash_ent_t * lp;

    CFlowKeyTuple tuple;

    tuple.set_ip(0x16000001);
    tuple.set_port(1025);
    tuple.set_proto(6);
    tuple.set_ipv4(true);

    ht.Create(32);

    flow_key_t key = tuple.get_as_uint64();
    uint32_t   hash =tuple.get_hash(); 

    lp=ht.find(key,hash);
    assert(lp==0);

    CMyFlowTest * lp_flow = new CMyFlowTest();

    lp_flow->m_hash_key.key =key;
    lp_flow->m_id =17;

    ht.insert_nc(&lp_flow->m_hash_key,hash);

    lp=ht.find(key,hash);
    assert(lp==&lp_flow->m_hash_key);

    ht.remove(&lp_flow->m_hash_key);
    delete lp_flow;

    ht.Delete();
}


void my_close_hash_on_detach_cb(void *userdata,
                             void  *obj){

    CMyFlowTest * lpobj=(CMyFlowTest *)obj;
    printf (" free id  %lu \n",(ulong)lpobj->m_id);
    delete lpobj;
}

TEST_F(gt_tcp, tst37) {

    test_hash_t   ht;
    test_hash_ent_t * lp;

    CFlowKeyTuple tuple;

    tuple.set_ip(0x16000001);
    tuple.set_port(1025);
    tuple.set_proto(6);
    tuple.set_ipv4(true);

    ht.Create(32);

    flow_key_t key = tuple.get_as_uint64();
    uint32_t   hash =tuple.get_hash(); 

    lp=ht.find(key,hash);
    assert(lp==0);

    CMyFlowTest * lp_flow = new CMyFlowTest();

    lp_flow->m_hash_key.key =key;
    lp_flow->m_id =17;

    ht.insert_nc(&lp_flow->m_hash_key,hash);

    lp=ht.find(key,hash);
    assert(lp==&lp_flow->m_hash_key);

    ht.detach_all(0,my_close_hash_on_detach_cb);

    ht.Delete();
}

class MyTestVirt {
public:
    virtual int a1(){
        return(17);
    }
    virtual ~MyTestVirt(){
    }
    int a;
    int b;
};

struct MyTestVirt2 {
    int a;
    int b;
};


TEST_F(gt_tcp, tst38) {
    MyTestVirt * lp=new MyTestVirt();
    MyTestVirt2 *lp1=new MyTestVirt2();

    printf(" %p %p\n",lp,&lp->a);

    void *p=&lp->a;

    UNSAFE_CONTAINER_OF_PUSH
    MyTestVirt * lp2 =(MyTestVirt *)((uint8_t*)p-offsetof (MyTestVirt,a));
    UNSAFE_CONTAINER_OF_POP

    printf(" %p %p \n",lp2,&lp2->a);

    delete lp;
    delete lp1;

}


class CTestStats64 {
public:
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

TEST_F(gt_tcp, tst39) {
    CTestStats64 aa;
    CTestStats64 bb;

    memset(&aa,0,sizeof(CTestStats64));
    memset(&bb,0,sizeof(CTestStats64));
    aa.a=1;
    aa.b=2;
    aa.c=0;

    bb.a=17;
    bb.b=18;
    bb.c=0;

    CGCountersUtl64 au((uint64_t*)&aa,sizeof(aa)/sizeof(uint64_t));
    CGCountersUtl64 bu((uint64_t*)&bb,sizeof(bb)/sizeof(uint64_t));

    au+=bu;
    printf(" %lu %lu \n",(ulong)aa.a,(ulong)aa.b);
}

TEST_F(gt_tcp, tst40) {

    CGTblClmCounters clm_aa;
    CGTblClmCounters clm_bb;

    CTestStats64 aa;
    CTestStats64 bb;

    memset(&aa,0,sizeof(CTestStats64));
    memset(&bb,0,sizeof(CTestStats64));
    aa.a=1;
    aa.b=2;
    aa.c=0;

    bb.a=17;
    bb.b=18;
    bb.c=0;

    CGCountersUtl64 au((uint64_t*)&aa,sizeof(aa)/sizeof(uint64_t));
    CGCountersUtl64 bu((uint64_t*)&bb,sizeof(bb)/sizeof(uint64_t));


    CGSimpleBase *lp;
    clm_aa.set_name("aa");
    clm_bb.set_name("bb");

    lp = new CGSimpleRefCnt64(&aa.a);
    lp->set_name("aa");
    lp->set_help("aa help");
    clm_aa.add_count(lp);

    lp = new CGSimpleRefCnt64(&aa.b);
    lp->set_name("bb");
    lp->set_help("bb help");
    clm_aa.add_count(lp);

    lp = new CGSimpleRefCnt64(&bb.a);
    lp->set_name("aa");
    lp->set_help("aa help");
    clm_bb.add_count(lp);

    lp = new CGSimpleRefCnt64(&bb.b);
    lp->set_name("bb");
    lp->set_help("bb help");
    clm_bb.add_count(lp);

    lp = new CGSimpleBar();
    lp->set_name("ERRORS");
    clm_aa.add_count(lp);

    lp = new CGSimpleBar();
    lp->set_name("ERRORS");
    clm_bb.add_count(lp);



    lp = new CGSimpleRefCnt64(&aa.c);
    lp->set_name("cc");
    lp->set_help("cc help");
    clm_aa.add_count(lp);

    lp = new CGSimpleRefCnt64(&bb.c);
    lp->set_name("cc");
    lp->set_help("cc help");
    clm_bb.add_count(lp);

    CTblGCounters tbl;

    tbl.add(&clm_aa);
    tbl.add(&clm_bb);

    tbl.dump_table(stdout,true,true);

    tbl.dump_table(stdout,false,true);
}


/* reass test1  flow is not enabled */
TEST_F(gt_tcp, tst42) {
    CTcpPerThreadCtx       ctx;  
    CTcpFlow               flow;
    ctx.Create(100,true);
    flow.Create(&ctx);

    tcpcb * tp=&flow.m_tcp;

    tp->rcv_nxt = 0x1000; /* expect this seq in rcv */

    struct tcpiphdr ti;
    memset(&ti,0,sizeof(struct tcpiphdr));
    ti.ih_len = 50;
    ti.ti_seq = 0x1000+100;

    int tiflags=0;

    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ti.ih_len = 100;
    ti.ti_seq +=100 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 


    ti.ih_len = 100;
    ti.ti_seq =0x1000+1000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000+2000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000+3000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ti.ih_len = 100;
    ti.ti_seq =0x1000+4000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ctx.m_tcpstat.Dump(stdout);
    printf(" tiflags:%x \n",tiflags);

    flow.Delete();
    ctx.Delete();
}

/* reass test1  flow is enabled by application*/

TEST_F(gt_tcp, tst43) {
    CTcpPerThreadCtx       ctx;  
    CTcpFlow               flow;
    CTcpAppApiImpl         tcp_bh_api_impl_c;
    ctx.Create(100,true);
    flow.Create(&ctx);

    CTcpAppProgram * prog_s;
    prog_s = new CTcpAppProgram();
    CTcpAppCmd cmd;

    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = 1000;

    prog_s->add_cmd(cmd);


    CTcpApp * app_c = &flow.m_app;

    app_c->set_program(prog_s);
    app_c->set_bh_api(&tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&ctx,&flow);
    flow.set_app(app_c);


    tcpcb * tp=&flow.m_tcp;

    tp->rcv_nxt = 0x1000; /* expect this seq in rcv */

    tp->t_state = TCPS_SYN_RECEIVED;

    struct tcpiphdr ti;
    memset(&ti,0,sizeof(struct tcpiphdr));
    ti.ih_len = 50;
    ti.ti_seq = 0x1000+100;

    int tiflags=0;

    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ti.ih_len = 100;
    ti.ti_seq +=100 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 


    ti.ih_len = 100;
    ti.ti_seq =0x1000+1000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000+2000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    tp->m_tpc_reass->Dump(stdout);

    ti.ih_len = 100;
    ti.ti_seq =0x1000+3000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ti.ih_len = 100;
    ti.ti_seq =0x1000+4000 ;
    tiflags = tcp_reass(&ctx,tp, &ti, (struct rte_mbuf *)0); 

    ctx.m_tcpstat.Dump(stdout);
    printf(" tiflags:%x \n",tiflags);

    delete prog_s;
    flow.Delete();
    ctx.Delete();
}



