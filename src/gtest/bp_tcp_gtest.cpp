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
#include <common/n_uniform_prob.h>
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
#include "astf/astf_db.h"
#include "utl_sync_barrier.h"

#include <stdlib.h>
#include <common/c_common.h>
#include <common/captureFile.h>
#include <common/sim_event_driven.h>
#include "44bsd/sim_cs_tcp.h"


class gt_tcp_long  : public testing::Test {

protected:
      virtual void SetUp() {
      }

      virtual void TearDown() {
      }
    public:
};


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
    rte_mbuf_t   * o=m;
    //rte_pktmbuf_dump(m, 1024);

    off=8;
    m=utl_rte_pktmbuf_cpy(p,m,512,off);

    rte_pktmbuf_free(o);

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



void tcp_gen_test(std::string pcap_file,
                  bool debug_mode,
                  cs_sim_test_id_t test_id=tiTEST2,
                  int valn=0, 
                  cs_sim_mode_t sim_mode=csSIM_NONE,
                  bool is_ipv6=false,
                  uint16_t mss=0,
                  CClientServerTcpCfgExt * cfg=NULL){

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("generated",pcap_file);
    if (debug_mode){
        lpt1->set_debug_mode(true);
    }
    if (valn){
        lpt1->m_vlan=valn;
    }

    if (is_ipv6){
        lpt1->m_ipv6=true;
    }

    if (mss !=0){
        lpt1->m_mss=mss;
    }

    if (cfg){
        lpt1->set_cfg_ext(cfg);
    }

    if (sim_mode!=csSIM_NONE){
        lpt1->set_simulate_rst_error(sim_mode);
    }

    switch (test_id) {
    case tiTEST2:
        lpt1->test2();
        break;
    default:
        lpt1->simple_http();
        break;
    }
    

    lpt1->close_file();

    bool res=true;
    if (lpt1->m_skip_compare_file==false){
        res=lpt1->compare("exp");
    }

    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";

    lpt1->Delete();

    delete lpt1;
}


TEST_F(gt_tcp, tst30) {
    tcp_gen_test("tcp2",
                 true,
                 tiTEST2);

}

TEST_F(gt_tcp, tst30_vlan) {

    tcp_gen_test("tcp2_vlan",
                 true,
                 tiTEST2,
                 100);


}

TEST_F(gt_tcp, tst30_http_vlan) {

    tcp_gen_test("tcp2_http_vlan",
                 true,
                 tiHTTP,
                 100);

}


TEST_F(gt_tcp, tst30_http_simple) {
    tcp_gen_test("tcp2_http_simple",
                 true,
                 tiHTTP);

}

TEST_F(gt_tcp, tst30_http_simple_mss) {
    tcp_gen_test("tcp2_http_simple_mss",
                 true,
                 tiHTTP,
                 0,
                 csSIM_NONE,
                 false, /*IPv6*/
                 512 /* MSS*/
                 );
}


TEST_F(gt_tcp, tst30_http_rst) {

    tcp_gen_test("tcp2_http_rst",
                 true,
                 tiHTTP,
                 0,
                 csSIM_RST_SYN
                 );

}

TEST_F(gt_tcp, tst30_http_rst1) {

    tcp_gen_test("tcp2_http_rst1",
                 true,
                 tiHTTP,
                 0,
                 csSIM_RST_SYN1
                 );

}

TEST_F(gt_tcp, tst30_http_wrong_port) {

    tcp_gen_test("tcp2_http_wrong_port",
                 true,
                 tiHTTP,
                 0,
                 csSIM_WRONG_PORT
                 );

}


TEST_F(gt_tcp, tst30_http_drop) {
    CClientServerTcpCfgExt cfg;
    cfg.m_rate=0.1;
    cfg.m_check_counters=true;

    tcp_gen_test("tcp2_http_drop",
                 true,
                 tiHTTP,
                 0,
                 csSIM_DROP,
                 false, /* ipv6*/
                 0, /*mss*/
                 &cfg
                 );


}

TEST_F(gt_tcp, tst30_http_drop_high) {
    CClientServerTcpCfgExt cfg;
    cfg.m_rate=0.6;
    cfg.m_check_counters=true;
    cfg.m_seed = 29;

    tcp_gen_test("tcp2_http_drop_high",
                 true,
                 tiHTTP,
                 0,
                 csSIM_DROP,
                 false, /* ipv6*/
                 0, /*mss*/
                 &cfg
                 );
}


TEST_F(gt_tcp_long, tst30_http_drop_loop) {

    CClientServerTcpCfgExt cfg;
    cfg.m_rate=0.1;
    cfg.m_check_counters=true;
    cfg.m_skip_compare_file=true;

    int i;
    for (i=1; i<50; i++) {
        cfg.m_rate=(double)rand()/(double)(0xffffffff);
        if (cfg.m_rate>0.4) {
            cfg.m_rate=0.4;
        }
        if (cfg.m_rate<0.1) {
            cfg.m_rate=0.1;
        }
        cfg.m_seed=i+1;
        printf(" itr:%d rate %f seed %d \n",i,cfg.m_rate,i);
        tcp_gen_test("tcp2_http_drop_loop",
                     true,
                     tiHTTP,
                     0,
                     csSIM_DROP,
                     false, /* ipv6*/
                     0, /*mss*/
                     &cfg
                     );
    }
}

TEST_F(gt_tcp, tst30_http_reorder) {
    CClientServerTcpCfgExt cfg;
    cfg.m_rate=0.1;
    cfg.m_check_counters=true;
    cfg.m_skip_compare_file=false;

    tcp_gen_test("tcp2_http_reorder",
                 true,
                 tiHTTP,
                 0,
                 csSIM_REORDER,
                 false, /* ipv6*/
                 0, /*mss*/
                 &cfg
                 );
}


TEST_F(gt_tcp_long, tst30_http_reorder_loop) {
    CClientServerTcpCfgExt cfg;
    cfg.m_rate=0.1;
    cfg.m_check_counters=true;
    cfg.m_skip_compare_file=true;

    int i;
    for (i=1; i<50; i++) {
        cfg.m_rate=(double)rand()/(double)(0xffffffff);
        if (cfg.m_rate>0.4) {
            cfg.m_rate=0.4;
        }
        if (cfg.m_rate<0.1) {
            cfg.m_rate=0.1;
        }
        cfg.m_seed=i+1;
        tcp_gen_test("tcp2_http_reorder",
                     true,
                     tiHTTP,
                     0,
                     csSIM_REORDER,
                     false, /* ipv6*/
                     0, /*mss*/
                     &cfg
                     );
    }
}


TEST_F(gt_tcp, tst30_http_rst_middle) {

    tcp_gen_test("tcp2_http_rst_middle",
                 true,
                 tiHTTP,
                 0,
                 csSIM_RST_MIDDLE
                 );
}


TEST_F(gt_tcp, tst30_http_rst_middle1) {

    tcp_gen_test("tcp2_http_rst_middle2",
                 true,
                 tiHTTP,
                 0,
                 csSIM_RST_MIDDLE2
                 );

}

TEST_F(gt_tcp, tst30_http_ipv6) {

    tcp_gen_test("tcp2_http_ipv6",
                 true,
                 tiHTTP,
                 0,
                 csSIM_NONE,
                 true /* IPV6*/
                 );

}



TEST_F(gt_tcp, tst31) {

    CMbufBuffer * buf;
    CTcpAppProgram * prog;
    CTcpApp * app;

    app = new CTcpApp();
    buf = new CMbufBuffer();

    utl_mbuf_buffer_create_and_fill(0,buf,2048,100*1024);

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
    aa.c=1;

    bb.a=17;
    bb.b=18;
    bb.c=1;

    CGCountersUtl64 au((uint64_t*)&aa,sizeof(aa)/sizeof(uint64_t));
    CGCountersUtl64 bu((uint64_t*)&bb,sizeof(bb)/sizeof(uint64_t));

    clm_aa.set_name("aa");
    clm_bb.set_name("bb");
    clm_aa.set_free_objects_own(true);
    clm_bb.set_free_objects_own(true);

    CGSimpleBase *lp;

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
    lp->set_info_level(scERROR);
    clm_aa.add_count(lp);

    lp = new CGSimpleRefCnt64(&bb.c);
    lp->set_name("cc");
    lp->set_help("cc help1");
    lp->set_info_level(scERROR);
    clm_bb.add_count(lp);

    CTblGCounters tbl;

    tbl.add(&clm_aa);
    tbl.add(&clm_bb);

    tbl.dump_table(stdout,true,true);

    tbl.dump_table(stdout,false,true);

    std::string json;
    tbl.dump_as_json("my",json);
    printf("%s \n",json.c_str());
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




int test_buffer(int pkt_size,
                int blk_size,
                int trim,
                int verbose){

    char * buf=utl_rte_pktmbuf_mem_fill(pkt_size,1); 
    struct rte_mbuf * m;
    assert(buf);
    m=utl_rte_pktmbuf_mem_to_pkt(buf,pkt_size,
                                 blk_size,
                                 tcp_pktmbuf_get_pool(0,blk_size));
    assert(m);
    assert(utl_rte_pktmbuf_verify(m)==0);


    if ( verbose ){
        utl_rte_pktmbuf_dump_k12(stdout,m);
    }

    if (trim>0){
        assert(utl_rte_pktmbuf_trim_ex(m,trim)==0);
    }
    assert(utl_rte_pktmbuf_verify(m)==0);
    char  * p2= utl_rte_pktmbuf_to_mem(m);
    assert(p2);

    assert(m->pkt_len==(pkt_size-trim));
    int res=memcmp(buf,p2,(pkt_size-trim));
    rte_pktmbuf_free(m);
    free(p2);
    free(buf);
    return(res);
}

TEST_F(gt_tcp_long, tst50) {
    int i;
    for (i=0; i<2047; i++) {
        printf(" test : %d \n",(int)i);
        assert(test_buffer(2048,512,i,0)==0);
    } 
}


int test_buffer_adj(int pkt_size,
                    int blk_size,
                    int adj,
                    int verbose){

    char * buf=utl_rte_pktmbuf_mem_fill(pkt_size,1); 
    struct rte_mbuf * m;
    assert(buf);
    m=utl_rte_pktmbuf_mem_to_pkt(buf,pkt_size,
                                 blk_size,
                                 tcp_pktmbuf_get_pool(0,blk_size));
    assert(m);
    assert(utl_rte_pktmbuf_verify(m)==0);


    if ( verbose ){
        utl_rte_pktmbuf_dump_k12(stdout,m);
    }

    if (adj>0){
        assert(utl_rte_pktmbuf_adj_ex(m,adj)!=0);
    }
    assert(utl_rte_pktmbuf_verify(m)==0);

    char  * p2= utl_rte_pktmbuf_to_mem(m);
    assert(p2);

    assert(m->pkt_len==(pkt_size-adj));
    int res=memcmp(buf+adj,p2,(pkt_size-adj));
    rte_pktmbuf_free(m);
    free(p2);
    free(buf);
    return(res);
}

TEST_F(gt_tcp_long, tst51) {
    //assert(test_buffer(2048,512,1,0)==0);
    assert(test_buffer(2048,512,512,0)==0);

    int i;
    for (i=0; i<2047; i++) {
        printf(" test : %d \n",(int)i);
        assert(test_buffer(2048,512,i,0)==0);
    } 
}


TEST_F(gt_tcp_long, tst_adj) {
    //assert(test_buffer(2048,512,1,0)==0);
    assert(test_buffer_adj(2048,512,512,0)==0);

    int i;
    for (i=0; i<2047; i++) {
        printf(" test : %d \n",(int)i);
        assert(test_buffer_adj(2048,512,i,0)==0);
    } 
}


#include "astf/astf_template_db.h"

TEST_F(gt_tcp, tst52) {
    CTupleGeneratorSmart  g_gen;
    ClientCfgDB g_dummy;

    g_gen.Create(0,0);
    g_gen.add_client_pool(cdSEQ_DIST,0x10000001,0x1000000f,64000, g_dummy, 0, 0);
    g_gen.add_server_pool(cdSEQ_DIST,0x30000001,0x40000001,64000,false);

    CAstfPerTemplateRO template_ro;
    CAstfPerTemplateRO *lp=&template_ro;

    template_ro.m_dual_mask=0x01000000;

    lp->m_client_pool_idx=0;
    lp->m_server_pool_idx=0;
    lp->m_one_app_server =false;
    lp->m_server_addr =0;
    lp->m_dual_mask=0x01000000;
    lp->m_w=1;
    lp->m_k_cps=1;
    lp->m_destination_port=80;

    CAstfPerTemplateRW t;
    t.Create(&g_gen,0,0,lp,0);
    CTupleBase tuple;
    t.m_tuple_gen.GenerateTuple(tuple);
    tuple.setServerPort(t.get_dest_port()) ;


    printf(" %x %x %x %x \n",tuple.getClient(),
                            tuple.getServer(),
                            tuple.getClientPort(),
                            tuple.getServerPort());


    g_gen.Delete();
}


int test_prob(int intr,std::vector<double> & dist){
    std::vector<int>     cnt;   
    cnt.resize(dist.size());

    KxuLCRand rnd;
    KxuNuRand * ru=new KxuNuRand(dist,&rnd);
    int i;

    for (i=0; i<intr; i++) {
        uint32_t index=ru->getRandom();
        cnt[index]++;
    }
    for (i=0; i<cnt.size(); i++) {
        double p= ((double)cnt[i]/(double)intr);
        double pp=fabs((100.0*p/dist[i])-100.0);
        printf(" %f %f\n",p,pp);
    }
    delete ru;
    return(0);
}

#if 0
TEST_F(gt_tcp, tst53) {
    std::vector<double>  dist {0.1,0.2,0.3,0.4 };
    test_prob(10000,dist);
}

/* example of SFR prob */
TEST_F(gt_tcp, tst54) {
    std::vector<double>  dist {
        102,
        102,
        33.0,
        179.0,
        64.0,
        1.2,
        1.2,
        1.2,

        20,
        0.7,
        0.5,
        1.85,
        1.85,
        1.85,

        3.0,
        7.4,
        11.0,
        498.0 ,

        102,
        102,
        33.0,
        179.0,
        64.0,
        1.2,
        1.2,
        1.2,

        20,
        0.7,
        0.5,
        1.85,
        1.85,
        1.85,

        3.0,
        7.4,
        11.0,
        498.0 ,

        102,
        102,
        33.0,
        179.0,
        64.0,
        1.2,
        1.2,
        1.2,

        20,
        0.7,
        0.5,
        1.85,
        1.85,
        1.85,

        3.0,
        7.4,
        11.0,
        498.0 ,
    };
    std::vector<double>  ndist;
    Kx_norm_prob(dist,ndist);
    Kx_dump_prob(ndist);
    #ifdef _DEBUG
        test_prob(1000,ndist);
    #else
      test_prob(100000000,ndist);
    #endif
}

class CPerProbInfo {
public:
    CPolicer        m_policer;
};
/* policer random */
 class CPolicerNuRand : public  KxuRand {
 public:
    virtual ~CPolicerNuRand();
    CPolicerNuRand(const std::vector<double> & prob);
    virtual uint32_t    getRandom();

 private:
     bool  getTick(uint32_t & tmp);
 private:
     std::vector<CPerProbInfo *> m_vec;
     uint32_t                    m_index;
     double                      m_cur_time_sec;
 };


 CPolicerNuRand::CPolicerNuRand(const std::vector<double> & prob){
     int i;
     for (i=0; i<prob.size(); i++) {
         CPerProbInfo * lp= new CPerProbInfo();
         lp->m_policer.set_cir(prob[i]);
         lp->m_policer.set_level(0.0);
         lp->m_policer.set_bucket_size(100.0);
         m_vec.push_back(lp);
     }
     m_index=0;
     m_cur_time_sec=1.0;
 }


 CPolicerNuRand::~CPolicerNuRand(){
     int i;
     for (i=0; i<m_vec.size(); i++) {
         delete m_vec[i];
     }
     m_vec.clear();
 }

 bool  CPolicerNuRand::getTick(uint32_t & tmp){
     CPerProbInfo * cur;
     int i;
     for (i=0;i<(int)m_vec.size();i++ ) {
         cur=m_vec[m_index];
         if ( cur->m_policer.update(1.0,m_cur_time_sec) ){
             tmp=m_index;
             m_index++;
             if (m_index == m_vec.size()) {
                 m_index=0;
             }
             return(true);
         }
         m_index++;
         if (m_index == m_vec.size()) {
             m_index=0;
         }
     }
     return(false);
 }



 uint32_t  CPolicerNuRand::getRandom(){

     uint32_t tmp;
     while (true) {
         if ( getTick(tmp) ) {
             return (tmp);
         }
         m_cur_time_sec+=1.0;
     }

  }


 int test_prob2(int intr,std::vector<double> & dist){
     std::vector<int>     cnt;   
     cnt.resize(dist.size());

     CPolicerNuRand * ru=new CPolicerNuRand(dist);
     int i;

     for (i=0; i<intr; i++) {
         uint32_t index=ru->getRandom();
         cnt[index]++;
     }
     for (i=0; i<cnt.size(); i++) {
         double p= ((double)cnt[i]/(double)intr);
         double pp=fabs((100.0*p/dist[i])-100.0);
         printf(" %f %f\n",p,pp);
     }
     delete ru;
     return(0);
 }

 TEST_F(gt_tcp, tst55) {
     std::vector<double>  dist {
         102,
         102,
         33.0,
         179.0,
         64.0,
         1.2,
         1.2,
         1.2,

         20,
         0.7,
         0.5,
         1.85,
         1.85,
         1.85,

         3.0,
         7.4,
         11.0,
         498.0 ,

         102,
         102,
         33.0,
         179.0,
         64.0,
         1.2,
         1.2,
         1.2,

         20,
         0.7,
         0.5,
         1.85,
         1.85,
         1.85,

         3.0,
         7.4,
         11.0,
         498.0 ,

         102,
         102,
         33.0,
         179.0,
         64.0,
         1.2,
         1.2,
         1.2,

         20,
         0.7,
         0.5,
         1.85,
         1.85,
         1.85,

         3.0,
         7.4,
         11.0,
         498.0 ,
     };
     std::vector<double>  ndist;
     Kx_norm_prob(dist,ndist);
     Kx_dump_prob(ndist);
     test_prob2(100000000,ndist);
}

TEST_F(gt_tcp, tst60) {
    KxuNuBinRand bin_rand(0.2);

    int i;
    for (i=0; i<100; i++) {
        bool index=bin_rand.getRandom();
        printf(" %d \n",index?1:0);
    }
}

TEST_F(gt_tcp, tst61) {
    KxuLCRand ur;
    int i;
    for (i=0; i<100; i++) {
       printf(" %f \n", ur.getRandomInRange(10.0,100.0));
    }
}
#endif




TEST_F(gt_tcp, tst70) {
    CSyncBarrier * a= new CSyncBarrier(2,0.01);
    assert(a->sync_barrier(0)==-1);
    assert(a->sync_barrier(1)==0);
    delete a;
}


