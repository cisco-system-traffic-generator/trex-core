/*
 Hanoh Haim
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

#include "bp_sim.h"
#include <common/gtest.h>
#include <common/basic_utils.h>
#include "utl_cpuu.h"
#include "timer_wheel_pq.h"
#include "rx_check.h"
#include "time_histogram.h"
#include "utl_jitter.h"
#include "CRing.h"
#include "msg_manager.h"
#include <common/cgen_map.h>
#include "platform_cfg.h"
#include "latency.h"

int test_policer(){
    CPolicer policer;

    policer.set_cir( 100.0);
    policer.set_level(0.0);
    policer.set_bucket_size(100.0);

    double t;
    uint32_t c=0;
    for (t=0.001;t<10.0; t=t+0.001) {
        if ( policer.update(1.0,t) ){
            c++;
            printf(" %f \n",t);
        }
    }
    printf(" %u \n",c);
    if ( ( c > 970.0) &&  ( c < 1000.0) ) {
        printf(" ok \n");
        return (0);
    }else{
        printf("error \n");
        return (-1);
    }
}




int test_priorty_queue(void){
    CGenNode * node;
    std::priority_queue<CGenNode *, std::vector<CGenNode *>,CGenNodeCompare> p_queue;
    int i;
    for (i=0; i<10; i++) {
         node = new CGenNode();
         printf(" +%p \n",node);
         node->m_flow_id = 10-i; 
         node->m_pkt_info = (CFlowPktInfo *)(uintptr_t)i; 
         node->m_time = (double)i+0.1; 
         p_queue.push(node);
    }
    while (!p_queue.empty()) {
        node = p_queue.top();
        printf(" -->%p \n",node);
        //node->Dump(stdout);
        p_queue.pop();
        //delete node;
    }
    return (0);
}


#if 0
#ifdef WIN32

int test_rate(){
    int i;
    CBwMeasure m;
    uint64_t cnt=0;
    for (i=0; i<10; i++) {
        Sleep(100);
        cnt+=10000;
        printf (" %f \n",m.add(cnt));
    }
    return (0);
}
#endif
#endif




void histogram_test(){
    CTimeHistogram t;
    t.Create();
    t.Add(0.0001);
    t.Add(0.0002);
    t.Add(0.0003);
    int i;
    for (i=0; i<100;i++) {
        t.Add(1.0/1000000.0);
    }
    t.Dump(stdout);
    t.Delete();
}




int test_human_p(){
    printf ("%s \n",double_to_human_str(1024.0*1024.0,"Byte",KBYE_1024).c_str());
    printf ("%s \n",double_to_human_str(1.0,"Byte",KBYE_1024).c_str());
    printf ("%s \n",double_to_human_str(-3000.0,"Byte",KBYE_1024).c_str());
    printf ("%s \n",double_to_human_str(-3000000.0,"Byte",KBYE_1024).c_str());
    printf ("%s \n",double_to_human_str(-30000000.0,"Byte",KBYE_1024).c_str());
    return (0);
}






#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))


class CTestBasic {

public:
    CTestBasic(){
        m_threads=1;
        m_time_diff=0.001;
        m_req_ports=0;
        m_dump_json=false;
    }

    bool  init(void){

        uint16 * ports = NULL;
        CTupleBase tuple;

        CErfIF erf_vif;


        fl.Create();
        m_saved_packet_padd_offset=0;

        fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,m_threads);
        fl.generate_p_thread_info(m_threads);

        CFlowGenListPerThread   * lpt;

        fl.m_threads_info[0]->set_vif(&erf_vif);
        

        CErfCmp cmp;
        cmp.dump=1;

        bool res=true;


        int i;
        for (i=0; i<m_threads; i++) {
            lpt=fl.m_threads_info[i];

            CFlowPktInfo *  pkt=lpt->m_cap_gen[0]->m_flow_info->GetPacket(0);
            m_saved_packet_padd_offset =pkt->m_pkt_indication.m_packet_padding;

            char buf[100];
            char buf_ex[100];
            sprintf(buf,"%s-%d.erf",CGlobalInfo::m_options.out_file.c_str(),i);
            sprintf(buf_ex,"%s-%d-ex.erf",CGlobalInfo::m_options.out_file.c_str(),i);

            if ( m_req_ports ){
                /* generate from first template m_req_ports ports */
                int i;
                CTupleTemplateGeneratorSmart * lpg=&lpt->m_cap_gen[0]->tuple_gen;
                ports = new uint16_t[m_req_ports];
                lpg->GenerateTuple(tuple);
                for (i=0 ; i<m_req_ports;i++) {
                    ports[i]=lpg->GenerateOneSourcePort();
                }
            }

            lpt->start_generate_stateful(buf,CGlobalInfo::m_options.preview);
            lpt->m_node_gen.DumpHist(stdout);

            cmp.d_sec = m_time_diff;
            if ( cmp.compare(std::string(buf),std::string(buf_ex)) != true ) {
                res=false;
            }

        }
        if ( m_dump_json ){
            printf(" dump json ...........\n");
            std::string s;
            fl.m_threads_info[0]->m_node_gen.dump_json(s);
            printf(" %s \n",s.c_str());
        }

        if ( m_req_ports ){
            int i;
            fl.m_threads_info[0]->m_smart_gen.FreePort(0, tuple.getClientId(),tuple.getClientPort());

            for (i=0 ; i<m_req_ports;i++) {
                fl.m_threads_info[0]->m_smart_gen.FreePort(0,tuple.getClientId(),ports[i]);
            }
            delete []ports;
        }

        printf(" active %d \n", fl.m_threads_info[0]->m_smart_gen.ActiveSockets());
        EXPECT_EQ_UINT32(fl.m_threads_info[0]->m_smart_gen.ActiveSockets(),0);
        fl.Delete();
        return (res);
    }

    uint16_t  get_padd_offset_first_packet(){
        return (m_saved_packet_padd_offset);

    }



public:
    int           m_req_ports;
    int           m_threads;
    double        m_time_diff;
    bool          m_dump_json;
    uint16_t      m_saved_packet_padd_offset;
    CFlowGenList  fl;
};




class basic  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};

class cpu  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};



TEST_F(basic, limit_single_pkt) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/limit_single_pkt.yaml";
     po->out_file ="exp/limit_single_pkt";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, limit_multi_pkt) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/limit_multi_pkt.yaml";
     po->out_file ="exp/limit_multi_pkt";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, imix) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/imix.yaml";
     po->out_file ="exp/imix";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic, dns) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

/* test -p function */
TEST_F(basic, dns_flow_flip) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->preview.setClientServerFlowFlip(true);

     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns_p";
     bool res=t1.init();
     po->preview.setClientServerFlowFlip(false);

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, dns_flip) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->preview.setClientServerFlip(true);

     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns_flip";
     bool res=t1.init();
     po->preview.setClientServerFlip(false);

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, dns_e) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->preview.setClientServerFlowFlipAddr(true);

     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns_e";
     bool res=t1.init();
     po->preview.setClientServerFlowFlipAddr(false);

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



/* test the packet padding , must be valid for --rx-check to work */
TEST_F(basic, dns_packet_padding_test) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns";
     bool res=t1.init();
     EXPECT_EQ_UINT32(t1.get_padd_offset_first_packet(),0);
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic, dns_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->preview.set_ipv6_mode_enable(true);
     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns_ipv6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     EXPECT_EQ_UINT32(t1.get_padd_offset_first_packet(),0);
     po->preview.set_ipv6_mode_enable(false);
}

TEST_F(basic, dns_json) {
     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     t1.m_dump_json=true;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns.yaml";
     po->out_file ="exp/dns";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


#if 0

TEST_F(basic, dns_wlen) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns_wlen.yaml";
     po->out_file ="exp/dns_wlen";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, dns_wlen1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns_wlen1.yaml";
     po->out_file ="exp/dns_wlen1";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, dns_wlen2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns_wlen2.yaml";
     po->out_file ="exp/dns_wlen2";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



TEST_F(basic, dns_one_server_2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns_single_server.yaml";
     po->out_file ="exp/dns_single_server";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

#endif

TEST_F(basic, dns_one_server) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dns_one_server.yaml";
     po->out_file ="exp/dns_one_server";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic, sfr2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;

     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sfr2.yaml";
     po->out_file ="exp/sfr2";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic, sfr3) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sfr3.yaml";
     po->out_file ="exp/sfr3";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic, sfr4) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sfr4.yaml";
     po->out_file ="exp/sfr_4";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, per_template_gen1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/per_template_gen1.yaml";
     po->out_file ="exp/sfr_4";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}
TEST_F(basic, per_template_gen2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/per_template_gen2.yaml";
     po->out_file ="exp/sfr_4";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


/*
TEST_F(basic, sfr5) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(0);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sfr5.yaml";
     po->out_file ="exp/sfr_5";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}
*/


TEST_F(basic, ipv6_convert) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/imix.yaml";
     po->out_file ="exp/imix_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
}

TEST_F(basic, ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/ipv6.yaml";
     po->out_file ="exp/ipv6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
}

TEST_F(basic, ipv4_vlan) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/ipv4_vlan.yaml";
     po->out_file ="exp/ipv4_vlan";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic, ipv6_vlan) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/ipv6_vlan.yaml";
     po->out_file ="exp/ipv6_vlan";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
}


/* exacly like cap file */
TEST_F(basic, test_pcap_mode1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/test_pcap_mode1.yaml";
     po->out_file ="exp/pcap_mode1";
     t1.m_time_diff = 0.000005; // 5 nsec 
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

/* check override the low IPG */
TEST_F(basic, test_pcap_mode2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/test_pcap_mode2.yaml";
     po->out_file ="exp/pcap_mode2";
     t1.m_time_diff = 0.000005; // 5 nsec 
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

#define l_pkt_test_s_ip 0x01020304
#define l_pkt_test_d_ip 0x05060708

CLatencyPktMode *
latency_pkt_mod_create(uint8_t l_pkt_mode) {
    switch (l_pkt_mode) {
    default:
        return  new CLatencyPktModeSCTP(l_pkt_mode);
        break;
    case L_PKT_SUBMODE_NO_REPLY:
    case L_PKT_SUBMODE_REPLY:
    case L_PKT_SUBMODE_0_SEQ:
        return new CLatencyPktModeICMP(l_pkt_mode);
        break;
    }
}

rte_mbuf_t *
create_latency_test_pkt(uint8_t l_pkt_mode, uint16_t &pkt_size, uint8_t port_id, uint8_t pkt_num) {
    rte_mbuf_t * m;
    CLatencyPktMode *c_l_pkt_mode;
    CCPortLatency port0;
    CLatencyPktInfo info;
    CLatencyManager mgr;

    c_l_pkt_mode = latency_pkt_mod_create(l_pkt_mode);

    mgr.c_l_pkt_mode = c_l_pkt_mode;
    info.Create(c_l_pkt_mode);
    port0.Create(&mgr, 0, info.get_payload_offset(), info.get_l4_offset(), info.get_pkt_size(), 0);
    info.set_ip(l_pkt_test_s_ip ,l_pkt_test_d_ip, 0x01000000);
    m=info.generate_pkt(0);
    while (pkt_num > 0) {
        pkt_num--;
        port0.update_packet(m, port_id);
    }
    pkt_size = info.get_pkt_size();
    port0.Delete();
    info.Delete();
    delete c_l_pkt_mode;

    return m;
}

bool
verify_latency_pkt(uint8_t *p, uint8_t proto, uint16_t icmp_seq, uint8_t icmp_type) {
    EthernetHeader *eth = (EthernetHeader *)p;
    IPHeader *ip = (IPHeader *)(p + 14);
    uint8_t  srcmac[]={0,0,0,1,0,0};    
    uint8_t  dstmac[]={0,0,0,1,0,0};
    latency_header * h;
 
    // eth
    EXPECT_EQ_UINT32(eth->getNextProtocol(), 0x0800)<< "Failed ethernet next protocol check";
    EXPECT_EQ_UINT32(memcmp(p, srcmac, 6), 0)<<  "Failed ethernet source MAC check";
    EXPECT_EQ_UINT32(memcmp(p, dstmac, 6), 0)<<  "Failed ethernet dest MAC check";
    // IP
    EXPECT_EQ_UINT32(ip->getSourceIp(), l_pkt_test_s_ip)<<  "Failed IP src check";
    EXPECT_EQ_UINT32(ip->getDestIp(), l_pkt_test_d_ip)<<  "Failed IP dst check";
    EXPECT_EQ_UINT32(ip->getProtocol(), proto)<<  "Failed IP protocol check";
    EXPECT_EQ_UINT32(ip->isChecksumOK()?0:1, 0)<<  "Failed IP checksum check";
    EXPECT_EQ_UINT32(ip->getTimeToLive(), 0xff)<<  "Failed IP ttl check";
    EXPECT_EQ_UINT32(ip->getTotalLength(), 48)<<  "Failed IP total length check";
    
    // payload
    h=(latency_header *)(p+42);
    EXPECT_EQ_UINT32(h->magic, LATENCY_MAGIC)<<  "Failed latency magic check";

    if (proto == 0x84)
        return true;
    // ICMP
    ICMPHeader *icmp = (ICMPHeader *)(p + 34);
    EXPECT_EQ_UINT32(icmp->isCheckSumOk(28)?0:1, 0)<<  "Failed ICMP checksum verification";
    EXPECT_EQ_UINT32(icmp->getSeqNum(), icmp_seq)<< "Failed ICMP sequence number check";
    EXPECT_EQ_UINT32(icmp->getType(), icmp_type)<<  "Failed ICMP type check";
    EXPECT_EQ_UINT32(icmp->getCode(), 0)<<  "Failed ICMP code check";
    EXPECT_EQ_UINT32(icmp->getId(), 0xaabb)<<  "Failed ICMP ID check";

    return true;
}

bool
test_latency_pkt_rcv(rte_mbuf_t *m, uint8_t l_pkt_mode, uint8_t port_num, uint16_t num_pkt, bool exp_pkt_ok
                     , uint16_t exp_tx_seq, uint16_t exp_rx_seq) {
    CLatencyPktMode *c_l_pkt_mode;
    CCPortLatency port;
    CLatencyPktInfo info;
    CLatencyManager mgr;
    CRx_check_header *rxc;
    CGlobalInfo::m_options.m_l_pkt_mode = l_pkt_mode;

    c_l_pkt_mode = latency_pkt_mod_create(l_pkt_mode);
    mgr.c_l_pkt_mode = c_l_pkt_mode;
    info.Create(c_l_pkt_mode);
    port.Create(&mgr, 0, info.get_payload_offset(), info.get_l4_offset(), info.get_pkt_size(), 0);
    bool pkt_ok = port.check_packet(m, rxc);

    while (num_pkt > 0) {
        num_pkt--;
        if (port.can_send_packet(port_num)) {
            port.update_packet(m, 0);
        }
    }

    EXPECT_EQ_UINT32(pkt_ok ?0:1, exp_pkt_ok?0:1)<<  "Failed packet OK check";
    EXPECT_EQ_UINT32(port.get_icmp_tx_seq(), exp_tx_seq)<<  "Failed tx check";
    EXPECT_EQ_UINT32(port.get_icmp_rx_seq(), exp_rx_seq)<<  "Failed rx check";
    // if packet is bad, check_packet raise the error counter
    EXPECT_EQ_UINT32(port.m_unsup_prot, exp_pkt_ok?0:1)<<  "Failed unsupported packets count";

    return true;
}


// Testing latency packet generation
TEST_F(basic, latency1) {
    uint8_t *p;
    rte_mbuf_t * m;
    uint16_t pkt_size;

    // SCTP packet
    m = create_latency_test_pkt(0, pkt_size, 0, 1);
    p=rte_pktmbuf_mtod(m, uint8_t*);
    verify_latency_pkt(p, 0x84, 0, 0);
    rte_pktmbuf_free(m);

    m = create_latency_test_pkt(L_PKT_SUBMODE_NO_REPLY, pkt_size, 1, 2);
    p=rte_pktmbuf_mtod(m, uint8_t*);
    verify_latency_pkt(p, 0x01, 2, 8);
    rte_pktmbuf_free(m);

    // ICMP reply mode client side
    m = create_latency_test_pkt(L_PKT_SUBMODE_REPLY, pkt_size, 0, 3);
    p = rte_pktmbuf_mtod(m, uint8_t*);
    verify_latency_pkt(p, 0x01, 3, 8);
    rte_pktmbuf_free(m);

    // ICMP reply mode server side
    m = create_latency_test_pkt(L_PKT_SUBMODE_REPLY, pkt_size, 1, 4);
    p=rte_pktmbuf_mtod(m, uint8_t*);
    verify_latency_pkt(p, 0x01, 4, 0);
    rte_pktmbuf_free(m);

    m = create_latency_test_pkt(L_PKT_SUBMODE_0_SEQ, pkt_size, 1, 5);
    p=rte_pktmbuf_mtod(m, uint8_t*);
    verify_latency_pkt(p, 0x01, 0, 8);
    rte_pktmbuf_free(m);
}

// Testing latency packet receive path
TEST_F(basic, latency2) {
    rte_mbuf_t *m;
    uint16_t pkt_size;
    uint8_t port_id = 0;
    uint8_t pkt_num = 1;
    uint8_t l_pkt_mode;

    l_pkt_mode = 0;
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    test_latency_pkt_rcv(m, l_pkt_mode, 0, 2, true, 1, 0);

    l_pkt_mode = L_PKT_SUBMODE_NO_REPLY;
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    test_latency_pkt_rcv(m, l_pkt_mode, 1, 2, true, 3, 1);

    // reply mode server
    l_pkt_mode = L_PKT_SUBMODE_REPLY;
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    test_latency_pkt_rcv(m, l_pkt_mode, 1, 2, true, 2, 1);

    // reply mode client
    l_pkt_mode = L_PKT_SUBMODE_REPLY;
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    test_latency_pkt_rcv(m, l_pkt_mode, 0, 2, true, 3, 1);

    l_pkt_mode = L_PKT_SUBMODE_0_SEQ;
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    test_latency_pkt_rcv(m, l_pkt_mode, 0, 2, true, 0, 0);

    // bad packet
    m = create_latency_test_pkt(l_pkt_mode, pkt_size, port_id, pkt_num);
    EthernetHeader *eth = (EthernetHeader *)rte_pktmbuf_mtod(m, uint8_t*);
    eth->setNextProtocol(0x5);
    test_latency_pkt_rcv(m, l_pkt_mode, 0, 2, false, 1, 0);
}

class CDummyLatencyHWBase : public CPortLatencyHWBase {
public:
    CDummyLatencyHWBase(){
        m_queue=0;
        m_port_id=0;
    }

    virtual int tx(rte_mbuf_t * m){
        assert(m_queue==0);
        //printf(" tx on port %d \n",m_port_id);
      //  utl_DumpBuffer(stdout,rte_pktmbuf_mtod(m, uint8_t*),rte_pktmbuf_pkt_len(m),0);
        m_queue=m;
        return (0);
    }

    virtual rte_mbuf_t * rx(){
        //printf(" rx on port %d \n",m_port_id);
        rte_mbuf_t * m=0;
        if (m_queue ) {
            m=m_queue;
            m_queue=0;
        }

        /*if ( m ){
            utl_DumpBuffer(stdout,rte_pktmbuf_mtod(m , uint8_t*),rte_pktmbuf_pkt_len(m ),0);
        } */
        return ( m );
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, 
                               uint16_t nb_pkts){
        //printf(" rx on port %d \n",m_port_id);
        rte_mbuf_t * m=rx();
        if (m) {
            rx_pkts[0]=m;
            return (1);
        }else{
            return (0);
        }
    }


private:
    rte_mbuf_t * m_queue;
public:
    uint8_t      m_port_id;
};

// Testing latency statistics functions
TEST_F(basic, latency3) {
    CLatencyManager mg;
    CLatencyManagerCfg  cfg;
    CDummyLatencyHWBase dports[MAX_LATENCY_PORTS];
    cfg.m_cps =10;
    cfg.m_max_ports=4;
    int i;
    for (i=0; i<MAX_LATENCY_PORTS; i++) {
        dports[i].m_port_id=i;
        cfg.m_ports[i] = &dports[i];
    }

    mg.Create(&cfg);

    printf(" before sending \n");
    mg.Dump(stdout);
    std::string json;
    mg.dump_json_v2(json);
    printf(" %s \n",json.c_str());


    EXPECT_EQ_UINT32(mg.is_active()?1:0, (uint32_t)0)<< "pass";

    mg.start(8);
    mg.stop();
    mg.Dump(stdout);
    mg.DumpShort(stdout);
    mg.dump_json_v2(json);
    printf(" %s \n",json.c_str());

    mg.Delete();
}

TEST_F(basic, hist1) {

    CTimeHistogram  hist1;

    dsec_t dusec=1.0/1000000.0;

    hist1.Create();
    hist1.Add(dusec);
    hist1.Add(2.0*dusec);
    hist1.Add(11.0*dusec);
    hist1.Add(110.0*dusec);
    hist1.Add(1110.0*dusec);
    hist1.Add(10110.0*dusec);
    hist1.Add(40110.0*dusec);
    hist1.Add(400110.0*dusec);
    hist1.Dump(stdout);
    hist1.Delete();
}

TEST_F(basic, rtsp1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short1.yaml";
     po->out_file ="exp/rtsp_short1";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 

TEST_F(basic, rtsp2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short2.yaml";
     po->out_file ="exp/rtsp_short2";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 

TEST_F(basic, rtsp3) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short3.yaml";
     po->out_file ="exp/rtsp_short3";
     t1.m_req_ports = 32000;
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 


TEST_F(basic, rtsp1_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short1.yaml";
     po->out_file ="exp/rtsp_short1_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 

TEST_F(basic, rtsp2_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short2.yaml";
     po->out_file ="exp/rtsp_short2_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 

TEST_F(basic, rtsp3_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/rtsp_short3.yaml";
     po->out_file ="exp/rtsp_short3_v6";
     t1.m_req_ports = 32000;
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 


TEST_F(basic, sip1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short1.yaml";
     po->out_file ="exp/sip_short1";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 


TEST_F(basic, sip2) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short2.yaml";
     po->out_file ="exp/sip_short2";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 

TEST_F(basic, sip3) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short2.yaml";
     po->out_file ="exp/sip_short3";
     t1.m_req_ports = 32000;
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 


TEST_F(basic, sip1_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short1.yaml";
     po->out_file ="exp/sip_short1_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 


TEST_F(basic, sip2_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short2.yaml";
     po->out_file ="exp/sip_short2_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 

TEST_F(basic, sip3_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/sip_short2.yaml";
     po->out_file ="exp/sip_short3_v6";
     t1.m_req_ports = 32000;
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 


TEST_F(basic, dyn1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/dyn_pyld1.yaml";
     po->out_file ="exp/dyn_pyld1";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 

TEST_F(basic, http1) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/http_plugin.yaml";
     po->out_file ="exp/http_plugin";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
} 

TEST_F(basic, http1_ipv6) {

     CTestBasic t1;
     CParserOption * po =&CGlobalInfo::m_options;
     po->preview.setVMode(3);
     po->preview.set_ipv6_mode_enable(true);
     po->preview.setFileWrite(true);
     po->cfg_file ="cap2/http_plugin.yaml";
     po->out_file ="exp/http_plugin_v6";
     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
     po->preview.set_ipv6_mode_enable(false);
} 



void delay(int msec);


TEST_F(cpu, cpu1) {
    CCpuUtlDp cpu_dp;
    CCpuUtlCp cpu_cp;
    cpu_cp.Create(&cpu_dp);
    int i;
    for (i=0; i<5;i++) {
        cpu_cp.Update();
        double c=cpu_cp.GetVal();
        printf (" %f \n",c);
        EXPECT_EQ(c,(double)0.0);

        delay(100);
    }

    cpu_cp.Delete();
}

TEST_F(cpu, cpu2) {
    CCpuUtlDp cpu_dp;
    CCpuUtlCp cpu_cp;
    cpu_cp.Create(&cpu_dp);
    int i;
    for (i=0; i<100;i++) {
        cpu_dp.start_work();
        cpu_cp.Update();
        double c1 = cpu_cp.GetVal();
        printf(" cpu %2.0f \n",c1);
        if (i>50) {
            int s=( c1<80 && c1>30)?1:0;
            EXPECT_EQ(s,1);
        }
        delay(1);
        if ((i%2)==1) {
            cpu_dp.commit();
        }else{
            cpu_dp.revert();
        }
    }

    cpu_cp.Delete();
}

#if 0
TEST_F(cpu, cpu3) {
    CCpuUtlDp cpu_dp;
    CCpuUtlCp cpu_cp;
    cpu_cp.Create(&cpu_dp);
    int i;
    for (i=0; i<200;i++) {
        cpu_dp.start_work();
        if (i%10==0) {
            cpu_cp.Update();
        }
        double c1 = cpu_cp.GetVal();
        if (i>150) {
            printf(" cpu %2.0f \n",c1);
            int s=( c1<11 && c1>8)?1:0;
            EXPECT_EQ(s,1);
        } 
        delay(1);
        if ((i%10)==1) {
            cpu_dp.commit();
        }else{
            cpu_dp.revert();
        }
    }
    cpu_cp.Delete();
}
#endif


class timerwl  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};



void  flow_callback(CFlowTimerHandle * timer_handle);

class CTestFlow {
public:
	CTestFlow(){
		flow_id = 0;
        m_timer_handle.m_callback=flow_callback;
		m_timer_handle.m_object = (void *)this;
		m_timer_handle.m_id = 0x1234;
	}

	uint32_t		flow_id;
	CFlowTimerHandle m_timer_handle;
public:
	void OnTimeOut(){
        printf(" timeout %d \n",flow_id);
	}
};

void  flow_callback(CFlowTimerHandle * t){
    CTestFlow * lp=(CTestFlow *)t->m_object;
    assert(lp);
    assert(t->m_id==0x1234);
    lp->OnTimeOut();
}


TEST_F(timerwl, tw1) {
    CTimerWheel  my_tw;

    CTestFlow f1;
    CTestFlow f2;
    CTestFlow f3;
    CTestFlow f4;

    f1.flow_id=1;
    f2.flow_id=2;
    f3.flow_id=3;
    f4.flow_id=4;
    double time;
    EXPECT_EQ(my_tw.peek_top_time(time),false);
    my_tw.restart_timer(&f1.m_timer_handle,10.0);
    my_tw.restart_timer(&f2.m_timer_handle,5.0);
    my_tw.restart_timer(&f3.m_timer_handle,1.0);
    EXPECT_EQ(my_tw.peek_top_time(time),true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    EXPECT_EQ(my_tw.peek_top_time(time),true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    EXPECT_EQ(my_tw.peek_top_time(time),true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    EXPECT_EQ(my_tw.handle(),true);

    EXPECT_EQ(my_tw.peek_top_time(time),true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,5.0);

    EXPECT_EQ(my_tw.handle(),true);

    EXPECT_EQ(my_tw.peek_top_time(time),true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,10.0);

    EXPECT_EQ(my_tw.handle(),true);

}

TEST_F(timerwl, tw2) {
    CTimerWheel  my_tw;

    double mytime=0.1;
    int i;
    CTestFlow * af[100];

    for (i=0; i<100; i++) {
        CTestFlow * f=new CTestFlow();
        af[i]=f;
        f->flow_id=i;
        my_tw.restart_timer(&f->m_timer_handle,mytime+10.0);
    }
    EXPECT_EQ(my_tw.m_st_alloc-my_tw.m_st_free,100);

    my_tw.try_handle_events(mytime);

    EXPECT_EQ(my_tw.m_st_alloc-my_tw.m_st_free,100);

    for (i=0; i<100; i++) {
        CTestFlow * f=af[i];
        my_tw.stop_timer(&f->m_timer_handle);
    }
    EXPECT_EQ(my_tw.m_st_alloc-my_tw.m_st_free,100);

    my_tw.try_handle_events(mytime);

    /* expect to free all the object */
    EXPECT_EQ(my_tw.m_st_alloc-my_tw.m_st_free,0);

}

TEST_F(timerwl, check_stop_timer) {

    CTimerWheel  my_tw;


    CTestFlow f1;
    CTestFlow f2;
    CTestFlow f3;
    CTestFlow f4;

    f1.flow_id=1;
    f2.flow_id=2;
    f3.flow_id=3;
    f4.flow_id=4;
    double time;
    assert(my_tw.peek_top_time(time)==false);
    my_tw.restart_timer(&f1.m_timer_handle,10.0);
    my_tw.restart_timer(&f2.m_timer_handle,5.0);
    my_tw.restart_timer(&f3.m_timer_handle,1.0);
    my_tw.stop_timer(&f1.m_timer_handle);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,1.0);

    assert(my_tw.handle());

    assert(my_tw.peek_top_time(time)==true);
    printf(" time %f \n",time);
    EXPECT_EQ(time ,5.0);


    assert(my_tw.handle());

    EXPECT_EQ(my_tw.peek_top_time(time) ,false);
    my_tw.Dump(stdout);
}


static int many_timers_flow_id=0;

void  many_timers_flow_callback(CFlowTimerHandle * t){
    CTestFlow * lp=(CTestFlow *)t->m_object;
    assert(lp);
    assert(t->m_id==0x1234);
    assert(many_timers_flow_id==lp->flow_id);
    many_timers_flow_id--;
}


TEST_F(timerwl, many_timers) {

    CTimerWheel  my_tw;

	int i;
    for (i=0; i<100; i++) {
        CTestFlow * f= new CTestFlow();
        f->m_timer_handle.m_callback=many_timers_flow_callback;
        f->flow_id=(uint32_t)i;
        my_tw.restart_timer(&f->m_timer_handle,100.0-(double)i);
    }
    many_timers_flow_id=99;

	double time;
    double ex_time=1.0;
    while (true) {
        if ( my_tw.peek_top_time(time) ){
            assert(time==ex_time);
            ex_time+=1.0;
            assert(my_tw.handle());
		}
		else{
			break;
		}
    }

    my_tw.Dump(stdout);

    EXPECT_EQ(my_tw.m_st_handle ,100);
    EXPECT_EQ(my_tw.m_st_alloc ,100);
    EXPECT_EQ(my_tw.m_st_free ,100);
    EXPECT_EQ(my_tw.m_st_start ,100);
    
}

void  many_timers_stop_flow_callback(CFlowTimerHandle * t){
    CTestFlow * lp=(CTestFlow *)t->m_object;
    assert(lp);
    assert(t->m_id==0x1234);
    assert(0);
}

TEST_F(timerwl, many_timers_with_stop) {
    CTimerWheel  my_tw;

    int i;
    for (i=0; i<100; i++) {
        CTestFlow * f= new CTestFlow();
        f->m_timer_handle.m_callback=many_timers_stop_flow_callback;
        f->flow_id=(uint32_t)i;
        my_tw.restart_timer(&f->m_timer_handle, 500.0 - (double)i);
        my_tw.restart_timer(&f->m_timer_handle, 1000.0 - (double)i);
        my_tw.restart_timer(&f->m_timer_handle, 100.0 - (double)i);
        my_tw.stop_timer(&f->m_timer_handle);
    }

    double time;
    while (true) {
        if ( my_tw.peek_top_time(time) ){
            assert(0);
            assert(my_tw.handle());
        }
        else{
            break;
        }
    }

    my_tw.Dump(stdout);


    EXPECT_EQ(my_tw.m_st_handle ,0);
    EXPECT_EQ(my_tw.m_st_alloc-my_tw.m_st_free ,0);
    EXPECT_EQ(my_tw.m_st_start ,300);
}


//////////////////////////////////////////////
class rx_check  : public testing::Test {
 protected:
  virtual void SetUp() {
      m_rx_check.Create();

  }
  virtual void TearDown() {
      m_rx_check.Delete();
  }
public:
    RxCheckManager m_rx_check;
};


TEST_F(rx_check, rx_check_normal) {
    int i;

    for (i=0; i<10; i++) {
        CRx_check_header rxh;
        rxh.clean();

        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
        rxh.m_time_stamp=0;
        rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;
        rxh.m_pkt_id=i;
        rxh.m_flow_size=10;

        rxh.m_flow_id=7;
        rxh.set_dir(0);
        rxh.set_both_dir(0);

        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}


TEST_F(rx_check, rx_check_drop) {

    int i;

    for (i=0; i<10; i++) {
        CRx_check_header rxh;
        rxh.clean();

        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
        rxh.m_time_stamp=0;
        rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

        if (i==4) {
            /* drop packet 4 */
            continue;
        }
        rxh.m_pkt_id=i;

        rxh.m_flow_size=10;

        rxh.set_dir(0);
        rxh.set_both_dir(0);

        rxh.m_flow_id=7;

        rxh.m_flags=0;
        m_rx_check.handle_packet(&rxh);
    }
    m_rx_check.tw_drain();
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,1);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_ooo) {

	m_rx_check.Create();
	int i;

	for (i=0; i<10; i++) {
		CRx_check_header rxh;
        rxh.clean();

        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
		rxh.m_time_stamp=0;
		rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

        rxh.set_dir(0);
        rxh.set_both_dir(0);


		/* out of order */
		if (i==4) {
			rxh.m_pkt_id=5;
		}else{
			if (i==5) {
				rxh.m_pkt_id=4;
			}else{
				rxh.m_pkt_id=i;
			}
		}

	    rxh.m_flow_size=10;

	    rxh.m_flow_id=7;

	    rxh.m_flags=0;
        m_rx_check.handle_packet(&rxh);
	}
	m_rx_check.tw_drain();
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_early,1);
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,2);

	m_rx_check.Dump(stdout);
}


TEST_F(rx_check, rx_check_ooo_1) {
	int i;

	for (i=0; i<10; i++) {
		CRx_check_header rxh;
        rxh.clean();
        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
		rxh.m_time_stamp=0;
        rxh.set_dir(0);
        rxh.set_both_dir(0);

		rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

		/* out of order */
		if (i==4) {
			rxh.m_pkt_id=56565;
		}else{
			if (i==5) {
				rxh.m_pkt_id=4;
			}else{
				rxh.m_pkt_id=i;
			}
		}
	    rxh.m_flow_size=10;
	    rxh.m_flow_id=7;
	    rxh.m_flags=0;
		m_rx_check.handle_packet(&rxh);
	}
	m_rx_check.tw_drain();
    EXPECT_EQ(m_rx_check.m_stats.m_err_wrong_pkt_id,1);
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,1);

	m_rx_check.Dump(stdout);
}

// start without first packet ( not FIF */
TEST_F(rx_check, rx_check_ooo_2) {
	int i;

	for (i=0; i<10; i++) {
		CRx_check_header rxh;
        rxh.clean();
        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
		rxh.m_time_stamp=0;
		rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

		/* out of order */
        rxh.set_dir(0);
        rxh.set_both_dir(0);


        if (i==0) {
            rxh.m_pkt_id=7;
        }else{
            if (i==7) {
                rxh.m_pkt_id=0;
            }else{
                rxh.m_pkt_id=i;
            }
        }

	    rxh.m_flow_size=10;
	    rxh.m_flow_id=7;
	    rxh.m_flags=0;
		m_rx_check.handle_packet(&rxh);
	}
	m_rx_check.tw_drain();
    EXPECT_EQ(m_rx_check.m_stats.m_err_open_with_no_fif_pkt,1);
    EXPECT_EQ(m_rx_check.m_stats. m_err_oo_late,1);
	m_rx_check.Dump(stdout);
}


TEST_F(rx_check, rx_check_normal_two_dir) {
    int i;

    for (i=0; i<10; i++) {
        CRx_check_header rxh;
        rxh.clean();
        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
        rxh.m_time_stamp=0;
        rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

        rxh.m_pkt_id=i;
        rxh.m_flow_size=10;

        rxh.m_flow_id=7;
        rxh.set_dir(0);
        rxh.set_both_dir(0);

        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}



TEST_F(rx_check, rx_check_normal_two_dir_fails) {
    int i;

    for (i=0; i<10; i++) {
        CRx_check_header rxh;
        rxh.clean();
        rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
        rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
        rxh.m_time_stamp=0;
        rxh.m_magic=RX_CHECK_MAGIC;
        rxh.m_aging_sec=10;

        rxh.m_pkt_id=i;
        rxh.m_flow_size=10;

        rxh.m_flow_id=7;
        rxh.set_dir(0);
        rxh.set_both_dir(1);

        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,0);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_two_dir_ok) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(1);
    rxh.m_flow_id=7;
    rxh.m_flow_size=10;

    for (i=0; i<10; i++) {
        rxh.m_pkt_id=i;
        printf(" first : %d \n",i);
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }

    for (i=0; i<10; i++) {
        printf(" se : %d \n",i);
        rxh.m_pkt_id=i;
        rxh.set_dir(1);
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_one_pkt_one_dir) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(1);
    rxh.m_flow_id=7;
    rxh.m_flow_size=1;

    for (i=0; i<1; i++) {
        rxh.m_pkt_id=i;
        printf(" first : %d \n",i);
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }
    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,0);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_one_pkt_one_dir_0) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(0);
    rxh.m_flow_id=7;
    rxh.m_flow_size=1;

    for (i=0; i<1; i++) {
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }
    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_one_pkt_two_dir_0) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(1);
    rxh.m_flow_id=7;
    rxh.m_flow_size=1;

    for (i=0; i<1; i++) {
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }

    for (i=0; i<1; i++) {
        rxh.m_pkt_id=i;
        rxh.set_dir(1);
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,1);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_one_pkt_two_dir_err1) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(1);
    rxh.m_flow_id=7;
    rxh.m_flow_size=10;

    for (i=0; i<10; i++) {
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }

    for (i=0; i<10; i++) {
        if (i==0) {
            rxh.m_pkt_id=7;
        }else{
            if (i==7) {
                rxh.m_pkt_id=0;
            }else{
                rxh.m_pkt_id=i;
            }
        }

        rxh.set_dir(1);
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,1);
    EXPECT_EQ(m_rx_check.m_stats.m_err_fif_seen_twice,1);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,0);
    m_rx_check.Dump(stdout);
}


TEST_F(rx_check, rx_check_normal_two_dir_oo) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_time_stamp=0;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(1);
    rxh.m_flow_id=7;
    rxh.m_flow_size=10;

    for (i=0; i<10; i++) {
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        m_rx_check.handle_packet(&rxh);
    }

    for (i=0; i<10; i++) {

        if (i==4) {
            rxh.m_pkt_id=5;
        }else{
            if (i==5) {
                rxh.m_pkt_id=4;
            }else{
                rxh.m_pkt_id=i;
            }
        }


        rxh.set_dir(1);
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_early,1);
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,2);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,0);
    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_aging) {
    int i;

    CRx_check_header rxh;
    rxh.clean();

    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=2;

    rxh.set_both_dir(0);
    rxh.m_flow_id=7;
    rxh.m_flow_size=10;
    rxh.m_template_id=13;

    for (i=0; i<9; i++) {
        rxh.m_time_stamp=(uint32_t)now_sec();
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        rxh.m_pkt_id=i;
        if (i<5) {
            m_rx_check.m_cur_time = (now_sec()+10);
        }
        m_rx_check.tw_handle();
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_template_info[13].get_error_counter()>0?1:0,1);
    EXPECT_EQ(m_rx_check.m_stats.m_err_aged,4);
    EXPECT_EQ(m_rx_check.m_stats.m_err_open_with_no_fif_pkt,4 );
    EXPECT_EQ(m_rx_check.m_stats.m_err_oo_late,1);

    m_rx_check.Dump(stdout);
}

TEST_F(rx_check, rx_check_normal_no_aging) {
    int i;

    CRx_check_header rxh;
    rxh.clean();
    rxh.m_option_type=RX_CHECK_V4_OPT_TYPE;
    rxh.m_option_len=RX_CHECK_V4_OPT_LEN;
    rxh.m_magic=RX_CHECK_MAGIC;
    rxh.m_aging_sec=10;

    rxh.set_both_dir(0);
    rxh.m_flow_id=7;
    rxh.m_flow_size=10;

    for (i=0; i<9; i++) {
        rxh.m_time_stamp=(uint32_t)now_sec();
        rxh.m_pkt_id=i;
        rxh.set_dir(0);
        rxh.m_pkt_id=i;
        m_rx_check.tw_handle();
        m_rx_check.handle_packet(&rxh);
    }

    EXPECT_EQ(m_rx_check.m_stats.get_total_err(),0);
    EXPECT_EQ(m_rx_check.m_stats.m_add,1);
    EXPECT_EQ(m_rx_check.m_stats.m_remove,0);
}


///////////////////////////////////////////////////////////////
// check the generation of template and check sample of it 


class CRxCheckCallbackBase {
public:
    virtual void handle_packet(rte_mbuf  * m)=0;
    void * obj;
};

class CRxCheckIF : public CVirtualIF {

public:
    CRxCheckIF(){
        m_callback=NULL;
        m_raw=NULL;
        m_one_dir=true;
        m_store_pcfg=false;
    }
public:

    virtual int open_file(std::string file_name){
        m_raw = new CCapPktRaw();
        assert(m_raw);
        return (0);
    }

    virtual int close_file(void){
        assert(m_raw);
        m_raw->raw=0;
        delete m_raw;
        return (0);
    }


    /**
     * send one packet
     * 
     * @param node
     * 
     * @return 
     */
    virtual int send_node(CGenNode * node);


    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, rte_mbuf_t      *m){
        return (0);
    }


    /**
     * flush all pending packets into the stream 
     * 
     * @return 
     */
    virtual int flush_tx_queue(void){
        return (0);
    }


public:
    bool                      m_one_dir;
    bool                      m_store_pcfg;
    CErfIF                    erf_vif;
    CCapPktRaw              * m_raw;
    CRxCheckCallbackBase    * m_callback;
};


int CRxCheckIF::send_node(CGenNode * node){

    CFlowPktInfo * lp=node->m_pkt_info;
    rte_mbuf_t * m=lp->generate_new_mbuf(node);

    /* update mac addr dest/src 12 bytes */
    uint8_t *p=(uint8_t *)m_raw->raw;
    uint8_t p_id = node->m_pkt_info->m_pkt_indication.m_desc.IsInitSide()?0:1;
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);

    if ( unlikely( node->is_rx_check_enabled() ) ) {
        lp->do_generate_new_mbuf_rxcheck(m,node,p_id,m_one_dir);
    }

    fill_pkt(m_raw,m);
    CPktNsecTimeStamp t_c(node->m_time);
    m_raw->time_nsec = t_c.m_time_nsec;
    m_raw->time_sec  = t_c.m_time_sec;
    m_raw->setInterface(node->m_pkt_info->m_pkt_indication.m_desc.IsInitSide());
    
    if (m_store_pcfg) {
        erf_vif.write_pkt(m_raw);
    }
        
    if ((m_callback) && (node->is_rx_check_enabled()) ) {
        m_callback->handle_packet(m);
    }

    // just free it 
    rte_pktmbuf_free(m);
    return (0);
}


class CRxCheckBasic {

public:
    CRxCheckBasic(){
        m_threads=1;
        lpVf=0;
    }

    bool  init(void){
        CFlowGenList fl;
        fl.Create();
        fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,m_threads);
        CGlobalInfo::m_options.set_rxcheck_const_ts();

        fl.generate_p_thread_info(m_threads);
        CFlowGenListPerThread   * lpt;
        fl.m_threads_info[0]->set_vif(lpVf);
        int i;
        for (i=0; i<m_threads; i++) {
            lpt=fl.m_threads_info[i];
            lpt->start_generate_stateful("t1",CGlobalInfo::m_options.preview);
        }
        fl.Delete();
        return (true);
    }

public:
    int           m_threads;
    CRxCheckIF   * lpVf;
};


class CRxCheck1 : public CRxCheckCallbackBase {
public:

    virtual void handle_packet(rte_mbuf_t  * m){
        rte_pktmbuf_mtod(m, char*);
        CRx_check_header * rx_p;
        rte_mbuf_t  * m2 = m->next;
        rx_p=(CRx_check_header *)rte_pktmbuf_mtod(m2, char*);
        mg->handle_packet(rx_p);
        //pkt->Dump(stdout,1);
    }
    RxCheckManager * mg;
};


class rx_check_system  : public testing::Test {
 protected:
  virtual void SetUp() {

      m_rx_check.m_callback=&m_callback;
      m_callback.mg   =&m_mg;
      m_mg.Create();
      CParserOption * po =&CGlobalInfo::m_options;
      po->preview.setVMode(0);
      po->preview.setFileWrite(true);
      po->preview.set_rx_check_enable(true);

  }

  virtual void TearDown() {
      m_mg.Delete();
  }
public:
    CRxCheckBasic  m_rxcs;
    CRxCheckIF     m_rx_check;
    CRxCheck1      m_callback;
    RxCheckManager m_mg;

};


// check DNS yaml with sample of 1/2 check that there is no errors 
TEST_F(rx_check_system, rx_system1) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/dns.yaml";

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}

// check DNS with rxcheck and write results out to capture file 
TEST_F(rx_check_system, rx_system1_dns) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=1; /* sample rate */
    po->m_duration=1;
    po->cfg_file ="cap2/dns.yaml";
    m_rx_check.m_store_pcfg=true;

    m_rx_check.erf_vif.set_review_mode(&CGlobalInfo::m_options.preview);
    m_rx_check.erf_vif.open_file("exp/dns_rxcheck.erf");

    m_rxcs.init();
    m_mg.tw_drain();
    m_rx_check.erf_vif.close_file();

    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/dns_rxcheck.erf","exp/dns_rxcheck-ex.erf"),true);
}

// check DNS yaml with sample of 1/4 using IPv6 packets 
TEST_F(rx_check_system, rx_system1_ipv6) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.set_ipv6_mode_enable(true);

    po->m_rx_check_sampe=4; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/dns.yaml";

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
    po->preview.set_ipv6_mode_enable(false);
}

// check DNS with rxcheck using IPv6 packets
// and write results out to capture file 
TEST_F(rx_check_system, rx_system1_dns_ipv6) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->preview.set_ipv6_mode_enable(true);
    po->m_rx_check_sampe=1; /* sample rate */
    po->m_duration=1;
    po->cfg_file ="cap2/dns.yaml";
    m_rx_check.m_store_pcfg=true;

    m_rx_check.erf_vif.set_review_mode(&CGlobalInfo::m_options.preview);
    m_rx_check.erf_vif.open_file("exp/dns_ipv6_rxcheck.erf");

    m_rxcs.init();
    m_mg.tw_drain();
    m_rx_check.erf_vif.close_file();

    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/dns_ipv6_rxcheck.erf","exp/dns_ipv6_rxcheck-ex.erf"),true);
    po->preview.set_ipv6_mode_enable(false);
}

TEST_F(rx_check_system, rx_system2_plugin_one_dir) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/rtsp_short1.yaml";

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}

// check HTTP with rxcheck and write results out to capture file 
TEST_F(rx_check_system, rx_system2_plugin) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=1; /* sample rate */
    po->m_duration=1;
    po->cfg_file ="cap2/rtsp_short1.yaml";
    m_rx_check.m_store_pcfg=true;

    m_rx_check.erf_vif.set_review_mode(&CGlobalInfo::m_options.preview);
    m_rx_check.erf_vif.open_file("exp/rtsp_short1_rxcheck.erf");

    m_rxcs.init();
    m_mg.tw_drain();
    m_rx_check.erf_vif.close_file();

    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/rtsp_short1_rxcheck.erf","exp/rtsp_short1_rxcheck-ex.erf"),true);
}

// check DNS with rxcheck using IPv6 packets
// and write results out to capture file 
TEST_F(rx_check_system, rx_system2_plugin_ipv6) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->preview.set_ipv6_mode_enable(true);
    po->m_rx_check_sampe=1; /* sample rate */
    po->m_duration=1;
    po->cfg_file ="cap2/rtsp_short1.yaml";
    m_rx_check.m_store_pcfg=true;

    m_rx_check.erf_vif.set_review_mode(&CGlobalInfo::m_options.preview);
    m_rx_check.erf_vif.open_file("exp/rtsp_short1_ipv6_rxcheck.erf");

    m_rxcs.init();
    m_mg.tw_drain();
    m_rx_check.erf_vif.close_file();

    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/rtsp_short1_ipv6_rxcheck.erf","exp/rtsp_short1_ipv6_rxcheck-ex.erf"),true);
    po->preview.set_ipv6_mode_enable(false);
}

TEST_F(rx_check_system, rx_system2_plugin_two_dir) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/rtsp_short1_slow.yaml";
    m_rx_check.m_one_dir=false;

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}

TEST_F(rx_check_system, rx_system2_plugin_two_dir_2) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/rtsp_short1.yaml";
    m_rx_check.m_one_dir=false;

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}

TEST_F(rx_check_system, rx_system_two_dir) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/dns.yaml";
    m_rx_check.m_one_dir=false;

    m_rxcs.init();
    m_mg.tw_drain();

    m_mg.Dump(stdout);

    EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}


TEST_F(rx_check_system, rx_json) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;

    po->m_rx_check_sampe=2; /* sample rate */
    po->m_duration=100;
    po->cfg_file ="cap2/dns.yaml";

    m_rxcs.init();
    m_mg.tw_drain();

    std::string json;
    m_mg.dump_json(json);
    printf(" %s \n",json.c_str());
}



//////////////////////////////////////////////////////////////


class CNatCheck1 : public CRxCheckCallbackBase {
public:

    virtual void handle_packet(rte_mbuf_t  * m){
            char *mp=rte_pktmbuf_mtod(m, char*);
            CNatOption * option=(CNatOption *)(mp+14+20);
            IPHeader * ipv4=(IPHeader *)(mp+14);
            if ( ipv4->getHeaderLength()>20 ) {
                assert(ipv4->getTimeToLive()==255);
                /* ip option packet */
                printf(" rx got ip option packet ! \n");
                mg->handle_packet_ipv4(option,ipv4);
                delay(10);          // delay for queue flush 
                mg->handle_aging(); // flush the RxRing 
            }
    }
    CNatRxManager * mg;
};



class nat_check_system  : public testing::Test {
 protected:
  virtual void SetUp() {
      m_rx_check.m_callback=&m_callback;
      m_callback.mg   =&m_mg;
      m_mg.Create();
      CParserOption * po =&CGlobalInfo::m_options;
      po->preview.setVMode(0);
      po->preview.setFileWrite(true);
      po->preview.set_lean_mode_enable(true);
  }

  virtual void TearDown() {
      CParserOption * po =&CGlobalInfo::m_options;
      po->preview.set_lean_mode_enable(false);
      m_mg.Delete();
  }
public:
    CRxCheckBasic   m_rxcs;
    CRxCheckIF      m_rx_check;
    CNatCheck1      m_callback;
    CNatRxManager   m_mg;

};

#if 0

TEST_F(nat_check_system, nat_system1) {

    m_rxcs.lpVf=&m_rx_check;
    CParserOption * po =&CGlobalInfo::m_options;
    po->m_duration=2;
    po->cfg_file ="cap2/dns.yaml";

    m_rxcs.init();
    m_mg.Dump(stdout);

    //EXPECT_EQ(m_mg.m_stats.get_total_err(),0);
}

#endif

//////////////////////////////////////////////////////////////

class file_flow_info  : public testing::Test {

protected:
  virtual void SetUp() {
      assert(m_flow_info.Create());
  }

  virtual void TearDown() {
      m_flow_info.Delete();
  }
public:
    CCapFileFlowInfo m_flow_info;

};

TEST_F(file_flow_info, f1) {
    m_flow_info.load_cap_file("cap2/delay_10_rtp_250k_short.pcap",1,7) ;
    m_flow_info.update_info();
    //m_flow_info.Dump(stdout);

    int i;
    for (i=0; i<m_flow_info.Size(); i++) {
            CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)i);
            uint16_t flow_id=lp->m_pkt_indication.m_desc.getFlowId();
            switch (flow_id) {
            case 0:
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxPktsPerFlow(),23);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxFlowTimeout(),64);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.IsBiDirectionalFlow(),1);
                break;
            case 1:
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxPktsPerFlow(),7);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxFlowTimeout(),10);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.IsBiDirectionalFlow(),0);

                break;
            case 2:
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxPktsPerFlow(),7);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxFlowTimeout(),5);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.IsBiDirectionalFlow(),0);

                break;
            default:
                assert(0);
            }
    }

}

TEST_F(file_flow_info, f2) {
    m_flow_info.load_cap_file("cap2/citrix.pcap",1,0) ;
    m_flow_info.update_info();


    int i;
    for (i=0; i<m_flow_info.Size(); i++) {
            CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)i);
            uint16_t flow_id=lp->m_pkt_indication.m_desc.getFlowId();
            switch (flow_id) {
            case 0:
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxPktsPerFlow(),271);
                EXPECT_EQ(lp->m_pkt_indication.m_desc.GetMaxFlowTimeout(),5);
                break;
            default:
                assert(0);
            }
    }
}

TEST_F(file_flow_info, http_two_dir) {
    m_flow_info.load_cap_file("avl/delay_10_http_browsing_0.pcap",1,0) ;
    m_flow_info.update_info();
    CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)0);
    EXPECT_EQ(lp->m_pkt_indication.m_desc.IsOneDirectionalFlow(),0);
}

TEST_F(file_flow_info, one_dir) {

    m_flow_info.load_cap_file("avl/delay_rtp_160k_1_1_0.pcap",1,0) ;
    m_flow_info.update_info();
    CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)0);
    EXPECT_EQ(lp->m_pkt_indication.m_desc.IsOneDirectionalFlow(),1);
}



TEST_F(file_flow_info, nat_option_check) {
    uint8_t buffer[8];
    CNatOption *lp=(CNatOption *)&buffer[0];
    lp->set_init_ipv4_header();
    lp->set_fid(0x12345678);
    lp->set_thread_id(7);
    lp->dump(stdout);
    EXPECT_EQ(lp->is_valid_ipv4_magic(),true);

    lp->set_init_ipv6_header();
    lp->dump(stdout);
    EXPECT_EQ(lp->is_valid_ipv6_magic(),true);
}

TEST_F(file_flow_info, http_add_ipv4_option) {
    m_flow_info.load_cap_file("avl/delay_10_http_browsing_0.pcap",1,0) ;
    m_flow_info.update_info();
    CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)0);
    printf(" before the change \n");
    //lp->Dump(stdout);
    //lp->m_packet->Dump(stdout,1);
    CNatOption *lpNat =(CNatOption *)lp->push_ipv4_option_offline(8);
    lpNat->set_init_ipv4_header();
    lpNat->set_fid(0x12345678);
    lpNat->set_thread_id(7);
    lp->m_pkt_indication.l3.m_ipv4->updateCheckSum();
    m_flow_info.save_to_erf("exp/http1_with_option.pcap",true);

    m_flow_info.Delete();
    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/http1_with_option.pcap","exp/http1_with_option-ex.pcap"),true);
}

TEST_F(file_flow_info, http_add_ipv6_option) {
    /* convert it to ipv6 */
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.set_ipv6_mode_enable(true);

    m_flow_info.load_cap_file("avl/delay_10_http_browsing_0.pcap",1,0) ;
    m_flow_info.update_info();
    CFlowPktInfo * lp=m_flow_info.GetPacket((uint32_t)0);
    //lp->Dump(stdout);
    //lp->m_packet->Dump(stdout,1);
    CNatOption *lpNat =(CNatOption *)lp->push_ipv6_option_offline(8);
    lpNat->set_init_ipv6_header();
    lpNat->set_fid(0x12345678);
    lpNat->set_thread_id(7);
    m_flow_info.save_to_erf("exp/http1_with_option_ipv6.pcap",true);
    m_flow_info.Delete();
    CErfCmp cmp;
    cmp.dump=1;
    EXPECT_EQ(cmp.compare("exp/http1_with_option_ipv6.pcap","exp/http1_with_option_ipv6-ex.pcap"),true);
    po->preview.set_ipv6_mode_enable(false);
}





//////////////////////////////////////////////////////////////

class time_histogram  : public testing::Test {

protected:
  virtual void SetUp() {
      m_hist.Create();
  }

  virtual void TearDown() {
      m_hist.Delete();
  }
public:
    CTimeHistogram m_hist;
};

TEST_F(time_histogram, test_average) {
    int i;
    int j;
    for (j=0; j<10; j++) {
        for (i=0; i<100; i++) {
            m_hist.Add(10e-6);
        }
        for (i=0; i<100; i++) {
            m_hist.Add(10e-3);
        }
        m_hist.update();
    }

    EXPECT_GT(m_hist.get_average_latency(),7400.0);
    EXPECT_LT(m_hist.get_average_latency(),7600.0);
    
    m_hist.Dump(stdout);
}

TEST_F(time_histogram, test_json) {
    int i;
    int j;
    for (j=0; j<10; j++) {
        for (i=0; i<100; i++) {
            m_hist.Add(10e-6);
        }
        for (i=0; i<100; i++) {
            m_hist.Add(10e-3);
        }
        m_hist.update();
    }

    m_hist.Dump(stdout);
    std::string  json ;
    m_hist.dump_json("myHis",json );
    printf(" %s \n",json.c_str());
}



class gt_jitter  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
    CJitter m_jitter;
};


class gt_jitter_uint  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
    CJitterUint m_jitter;
};



TEST_F(gt_jitter, jitter1) {
    int i;
    double a=0.000030;
    for (i=0; i<100; i++) {
        if (i%2) {
            a+=0.000100;
        }else{
            a-=0.000100;
        }
        m_jitter.calc(a);
    }
    EXPECT_EQ((uint32_t)(m_jitter.get_jitter()*1000000.0), 99);
}

TEST_F(gt_jitter_uint, jitter2) {
    int i;
    int32_t a=30;
    for (i=0; i<100; i++) {
        if (i%2) {
            a+=20;
        }else{
            a-=20;
        }
        m_jitter.calc(a);
    }
    EXPECT_EQ((uint32_t)(m_jitter.get_jitter()), 19);
}


class gt_ring  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};

TEST_F(gt_ring, ring1) {

    CTRingSp<uint32_t> my;
    bool res=my.Create("a",1024,0);
    assert(res);

    int i;
    for (i=0; i<10; i++) {
        uint32_t *p=new uint32_t();
        *p=i;
        assert(my.Enqueue(p)==0);
    }
    for (i=0; i<10; i++) {
        uint32_t *p;
        assert(my.Dequeue(p)==0);
        EXPECT_EQ_UINT32(*p, i);
    }
    uint32_t *p;
    assert(my.Dequeue(p)!=0);

    EXPECT_EQ(my.isEmpty(), true);
    EXPECT_EQ(my.isFull(), false);

    my.Delete();
}


TEST_F(gt_ring, ring2) {
    CMessagingManager ringmg;
    ringmg.Create(8, "test");

    int i;
    for (i=0; i<8; i++) {
        CNodeRing * ln=ringmg.getRingDpToCp(i);
        assert(ln);
        CGenNode * node=new CGenNode();
        node->m_flow_id=i;
        assert(ln->Enqueue(node)==0);
    }

    for (i=0; i<8; i++) {
        CNodeRing * ln=ringmg.getRingDpToCp(i);
        assert(ln);
        CGenNode * node;
        assert(ln->Dequeue(node)==0);
        EXPECT_EQ(node->m_flow_id, i);
        delete node;
    }

    ringmg.Delete();
}







void my_free_map_uint32_t(uint32_t *p){
    printf("before free %d \n",*p);
    delete p;
}


TEST_F(gt_ring, ring3) {

    typedef  CGenericMap<uint32_t,uint32_t> my_test_map;
    my_test_map my_map;

    my_map.Create();
    int i;

    uint32_t *p;
    for (i=0; i<10;i++) {
        p=new uint32_t(i);
        my_map.add((uint32_t)i,p);
    }

    for (i=0; i<10;i++) {
        p=my_map.lookup((uint32_t)i);
        printf(" %d \n",*p);
    }

    my_map.remove_all(my_free_map_uint32_t);
    #if 0
    for (i=0; i<10;i++) {
        p=my_map.remove((uint32_t)i);
        assert(p);
        delete p;
    }
    #endif
    my_map.Delete();
}


class gt_conf  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};


#if 0
TEST_F(gt_conf, t1) {
    CPlatformYamlInfo info;
    info.load_from_yaml_file("cfg/ex1.yaml");
    info.Dump(stdout);
    CPlatformSocketInfoConfig cfg;
    cfg.Create(&info.m_platform);

    cfg.set_latency_thread_is_enabled(true);
    cfg.set_number_of_dual_ports(1);
    cfg.set_number_of_threads_per_ports(1);


    cfg.sanity_check();
    cfg.dump(stdout);
} 

#endif


