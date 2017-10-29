/*
 Itay Marom
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
#include "stf/trex_stf.h"

static int cores = 1;

#ifdef LINUX



#include <pthread.h>

struct per_thread_t {
    pthread_t  tid;
};

#define MAX_THREADS 200
static per_thread_t  tr_info[MAX_THREADS];


//////////////

struct test_t_info1 {
    CPreviewMode *            preview_info;
    CFlowGenListPerThread   * thread_info;
    uint32_t                  thread_id;
};

void * thread_task(void *info){

    test_t_info1 * obj =(test_t_info1 *)info;

    CFlowGenListPerThread   * lpt=obj->thread_info;

    printf("start thread %d \n",obj->thread_id);
    //delay(obj->thread_id *3000);
    printf("-->start thread %d \n",obj->thread_id);
    if (1/*obj->thread_id ==3*/) {

        char buf[100];
        sprintf(buf,"my%d.erf",obj->thread_id);
        lpt->start_sim(buf,*obj->preview_info);
        lpt->m_node_gen.DumpHist(stdout);
        printf("end thread %d \n",obj->thread_id);
    }

    return (NULL);
}


void test_load_list_of_cap_files_linux(CParserOption * op){

    CFlowGenList fl;
    //CNullIF erf_vif;
    //CErfIF erf_vif;

    fl.Create();

    fl.load_from_yaml(op->cfg_file,cores);
    fl.DumpPktSize();

    
    fl.generate_p_thread_info(cores);
    CFlowGenListPerThread   * lpt;

    /* set the ERF file */
    //fl.set_vif_all(&erf_vif);

    int i;
    for (i=0; i<cores; i++) {
        lpt=fl.m_threads_info[i];
        test_t_info1 * obj = new test_t_info1();
        obj->preview_info =&op->preview;
        obj->thread_info  = fl.m_threads_info[i];
        obj->thread_id    = i;
        CNullIF * erf_vif = new CNullIF();
        //CErfIF  * erf_vif = new CErfIF();

        lpt->set_vif(erf_vif);

        assert(pthread_create( &tr_info[i].tid, NULL, thread_task, obj)==0);
    }

    for (i=0; i<cores; i++) {
        /* wait for all of them to stop */
       assert(pthread_join((pthread_t)tr_info[i].tid,NULL )==0);
    }

    printf("compare files \n");
    for (i=1; i<cores; i++) {

        CErfCmp cmp;
        char buf[100];
        sprintf(buf,"my%d.erf",i);
        char buf1[100];
        sprintf(buf1,"my%d.erf",0);
        if ( cmp.compare(std::string(buf),std::string(buf1)) != true ) {
            printf(" ERROR cap file is not ex !! \n");
            assert(0);
        }
        printf(" thread %d is ok \n",i);
    }

    fl.Delete();
}


#endif

/*************************************************************/
void test_load_list_of_cap_files(CParserOption * op){

    CFlowGenList fl;
    CNullIF erf_vif;

    fl.Create();

    #define NUM 1

    fl.load_from_yaml(op->cfg_file,NUM);
    fl.DumpPktSize();
    
    
    fl.generate_p_thread_info(NUM);
    CFlowGenListPerThread   * lpt;

    /* set the ERF file */
    //fl.set_vif_all(&erf_vif);

    int i;
    for (i=0; i<NUM; i++) {
        lpt=fl.m_threads_info[i];
        char buf[100];
        sprintf(buf,"my%d.erf",i);
        lpt->start_sim(buf,op->preview);
        lpt->m_node_gen.DumpHist(stdout);
    }
    //sprintf(buf,"my%d.erf",7);
    //lpt=fl.m_threads_info[7];

    //fl.Dump(stdout);
    fl.Delete();
}

int load_list_of_cap_files(CParserOption * op){
    CFlowGenList fl;
    fl.Create();
    fl.load_from_yaml(op->cfg_file,1);
    
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

    if ( op->preview.getVMode() >0 ) {
        fl.DumpCsv(stdout);
    }
    uint32_t start=    os_get_time_msec();

    CErfIF erf_vif;
    //CNullIF erf_vif;

    fl.generate_p_thread_info(1);
    CFlowGenListPerThread   * lpt;
    lpt=fl.m_threads_info[0];
    lpt->set_vif(&erf_vif);

    if ( (op->preview.getVMode() >1)  || op->preview.getFileWrite() ) {
        lpt->start_sim(op->out_file,op->preview);
    }

    lpt->m_node_gen.DumpHist(stdout);

    uint32_t stop=    os_get_time_msec();
    printf(" d time = %ul %ul \n",stop-start,os_get_time_freq());
    fl.Delete();
    return (0);
}


int test_dns(){

    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CParserOption po ;

    //po.cfg_file = "cap2/dns.yaml";
    //po.cfg_file = "cap2/sfr3.yaml";
    po.cfg_file = "cap2/sfr.yaml";

    po.preview.setVMode(0);
    po.preview.setFileWrite(true);
    #ifdef LINUX
      test_load_list_of_cap_files_linux(&po);
    #else
      test_load_list_of_cap_files(&po);
    #endif
    return (0);
}

void test_pkt_mbuf(void);

void test_compare_files(void);

#if 0
static int b=0;
static int c=0;
static int d=0;

int test_instructions(){
    int i;
    for (i=0; i<100000;i++) {
        b+=b+1;
        c+=+b+c+1;
        d+=+(b*2+1);
    }
    return (b+c+d);
}

#include <valgrind/callgrind.h>
#endif


void update_tcp_seq_num(CCapFileFlowInfo * obj,
                        int pkt_id,
                        int size_change){
    CFlowPktInfo * pkt=obj->GetPacket(pkt_id);
    if ( pkt->m_pkt_indication.m_desc.IsUdp() ){
        /* nothing to do */
        return;
    }

    bool o_init=pkt->m_pkt_indication.m_desc.IsInitSide();
    TCPHeader * tcp ;
    int s= (int)obj->Size();
    int i;

    for (i=pkt_id+1; i<s; i++) {

        pkt=obj->GetPacket(i);
        tcp=pkt->m_pkt_indication.l4.m_tcp;
        bool init=pkt->m_pkt_indication.m_desc.IsInitSide();
        if (init == o_init) {
            /* same dir update the seq number */
            tcp->setSeqNumber    (tcp->getSeqNumber    ()+size_change);

        }else{
            /* update the ack number */
            tcp->setAckNumber    (tcp->getAckNumber    ()+size_change);
        }
    }
}



void change_pkt_len(CCapFileFlowInfo * obj,int pkt_id, int size ){
    CFlowPktInfo * pkt=obj->GetPacket(pkt_id);

    /* enlarge the packet size by 9 */

    char * p=pkt->m_packet->append(size);
    /* set it to 0xaa*/
    memmove(p+size-4,p-4,4); /* CRCbytes */
    memset(p-4,0x0a,size);

    /* refresh the pointers */
    pkt->m_pkt_indication.RefreshPointers();

    IPHeader       * ipv4 = pkt->m_pkt_indication.l3.m_ipv4;
    ipv4->updateTotalLength	(ipv4->getTotalLength()+size );

    /* update seq numbers if needed */
    update_tcp_seq_num(obj,pkt_id,size);
}

void dump_tcp_seq_num_(CCapFileFlowInfo * obj){
    int s= (int)obj->Size();
    int i;
    uint32_t i_seq;
    uint32_t r_seq;

    CFlowPktInfo * pkt=obj->GetPacket(0);
    TCPHeader * tcp = pkt->m_pkt_indication.l4.m_tcp;
    i_seq=tcp->getSeqNumber    ();

    pkt=obj->GetPacket(1);
    tcp = pkt->m_pkt_indication.l4.m_tcp;
    r_seq=tcp->getSeqNumber    ();

    for (i=2; i<s; i++) {
        uint32_t seq;
        uint32_t ack;

        pkt=obj->GetPacket(i);
        tcp=pkt->m_pkt_indication.l4.m_tcp;
        bool init=pkt->m_pkt_indication.m_desc.IsInitSide();
        seq=tcp->getSeqNumber    ();
        ack=tcp->getAckNumber    ();
        if (init) {
            seq=seq-i_seq;
            ack=ack-r_seq;
        }else{
            seq=seq-r_seq;
            ack=ack-i_seq;
        }
        printf(" %4d ",i);
        if (!init) {
            printf("                             ");
        }
        printf("  %s   seq: %4d   ack : %4d   \n",init?"I":"R",seq,ack);
    }
}


int manipolate_capfile() {
    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CCapFileFlowInfo flow_info;
    flow_info.Create();

    flow_info.load_cap_file("avl/delay_10_rtsp_0.pcap",0,0);

    change_pkt_len(&flow_info,4-1 ,6);
    change_pkt_len(&flow_info,5-1 ,6);
    change_pkt_len(&flow_info,6-1 ,6+2);
    change_pkt_len(&flow_info,7-1 ,4);
    change_pkt_len(&flow_info,8-1 ,6+2);
    change_pkt_len(&flow_info,9-1 ,4);
    change_pkt_len(&flow_info,10-1,6);
    change_pkt_len(&flow_info,13-1,6);
    change_pkt_len(&flow_info,16-1,6);
    change_pkt_len(&flow_info,19-1,6);

    flow_info.save_to_erf("exp/c.pcap",1);
    
    return (1);
}

int manipolate_capfile_sip() {
    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CCapFileFlowInfo flow_info;
    flow_info.Create();

    flow_info.load_cap_file("avl/delay_10_sip_0.pcap",0,0);

    change_pkt_len(&flow_info,1-1 ,6+6);
    change_pkt_len(&flow_info,2-1 ,6+6);

    flow_info.save_to_erf("exp/delay_10_sip_0_fixed.pcap",1);
    
    return (1);
}

int manipolate_capfile_sip1() {
    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CCapFileFlowInfo flow_info;
    flow_info.Create();

    flow_info.load_cap_file("avl/delay_sip_0.pcap",0,0);
    flow_info.GetPacket(1);

    change_pkt_len(&flow_info,1-1 ,6+6+10);

    change_pkt_len(&flow_info,2-1 ,6+6+10);

    flow_info.save_to_erf("exp/delay_sip_0_fixed_1.pcap",1);
    
    return (1);
}


class CMergeCapFileRec {
public:

    CCapFileFlowInfo m_cap;

    int              m_index;
    int              m_limit_number_of_packets; /* limit number of packets */
    bool             m_stop; /* Do we have more packets */

    double           m_offset; /* offset should be positive */
    double           m_start_time;

public:
    bool Create(std::string cap_file,double offset);
    void Delete();
    void IncPacket();
    bool GetCurPacket(double & time);
    CPacketIndication *  GetUpdatedPacket();

    void Dump(FILE *fd,int _id);
};


void CMergeCapFileRec::Dump(FILE *fd,int _id){
    double time = 0.0;
    bool stop=GetCurPacket(time);
    fprintf (fd," id:%2d  stop : %d index:%4d  %3.4f \n",_id,stop?1:0,m_index,time);
}


CPacketIndication *  CMergeCapFileRec::GetUpdatedPacket(){
    double t1;
    assert(GetCurPacket(t1)==false);
    CFlowPktInfo * pkt = m_cap.GetPacket(m_index);
    pkt->m_pkt_indication.m_packet->set_new_time(t1);
    return (&pkt->m_pkt_indication);
}


bool  CMergeCapFileRec::GetCurPacket(double & time){
    if (m_stop) {
        return(true);
    }
    CFlowPktInfo * pkt = m_cap.GetPacket(m_index);
    time= (pkt->m_packet->get_time() -m_start_time + m_offset);
    return (false);
}

void CMergeCapFileRec::IncPacket(){
    m_index++;
    if ( (m_limit_number_of_packets) && (m_index > m_limit_number_of_packets ) ) {
        m_stop=true;
        return;
    }

    if ( m_index == (int)m_cap.Size() ) {
        m_stop=true;
    }
}

void CMergeCapFileRec::Delete(){
    m_cap.Delete();
}

bool CMergeCapFileRec::Create(std::string cap_file,
                              double offset){
   m_cap.Create();
   m_cap.load_cap_file(cap_file,0,0);
   CFlowPktInfo * pkt = m_cap.GetPacket(0);

   m_index=0;
   m_stop=false;
   m_limit_number_of_packets =0;
   m_start_time =     pkt->m_packet->get_time() ;
   m_offset = offset;

   return (true);
}



#define MERGE_CAP_FILES (2)

class CMergeCapFile {
public:
    bool Create();
    void Delete();
    bool run_merge(std::string to_cap_file);
private:
    void append(int _cap_id);

public:
    CMergeCapFileRec m[MERGE_CAP_FILES];
    CCapFileFlowInfo m_results;
};

bool CMergeCapFile::Create(){
    m_results.Create();
    return(true);
}

void CMergeCapFile::Delete(){
    m_results.Delete();
}

void CMergeCapFile::append(int _cap_id){
    CPacketIndication * lp=m[_cap_id].GetUpdatedPacket();
    lp->m_packet->Dump(stdout,0);
    m_results.Append(lp);
}


bool CMergeCapFile::run_merge(std::string to_cap_file){

    int i=0;
    int cnt=0;
    while ( true ) {
        int    min_index=0;
        double min_time;

        fprintf(stdout," --------------\n");
        fprintf(stdout," pkt : %d \n",cnt);
        for (i=0; i<MERGE_CAP_FILES; i++) {
            m[i].Dump(stdout,i);
        }
        fprintf(stdout," --------------\n");

        bool valid = false;
        for (i=0; i<MERGE_CAP_FILES; i++) {
            double t1;
            if ( m[i].GetCurPacket(t1) == false ){
                /* not in stop */
                if (!valid) {
                    min_time  = t1;
                    min_index = i;
                    valid=true;
                }else{
                    if (t1 < min_time) {
                        min_time=t1;
                        min_index = i;
                    }
                }

            }
        }

        /* nothing to do */
        if (valid==false) {
            fprintf(stdout,"nothing to do \n");
            break;
        }

        cnt++;
        fprintf(stdout," choose id %d \n",min_index);
        append(min_index);
        m[min_index].IncPacket();
    };

    m_results.save_to_erf(to_cap_file,1);

    return (true);
}



int merge_3_cap_files() {
    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CMergeCapFile merger;
    merger.Create();
    merger.m[0].Create("exp/c.pcap",0.001);
    merger.m[1].Create("avl/delay_10_rtp_160k_0.pcap",0.31);
    merger.m[2].Create("avl/delay_10_rtp_160k_1.pcap",0.311);

    //merger.m[1].Create("avl/delay_10_rtp_250k_0_0.pcap",0.31);
    //merger.m[1].m_limit_number_of_packets =6;
    //merger.m[2].Create("avl/delay_10_rtp_250k_1_0.pcap",0.311);
    //merger.m[2].m_limit_number_of_packets =6;

    merger.run_merge("exp/delay_10_rtp_160k_full.pcap");

    return (0);
}

int merge_2_cap_files_sip() {
    time_init();
    CGlobalInfo::init_pools(1000, MBUF_2048);

    CMergeCapFile merger;
    merger.Create();
    merger.m[0].Create("exp/delay_sip_0_fixed_1.pcap",0.001);
    merger.m[1].Create("avl/delay_video_call_rtp_0.pcap",0.51);
    //merger.m[1].m_limit_number_of_packets=7;

    //merger.m[1].Create("avl/delay_10_rtp_250k_0_0.pcap",0.31);
    //merger.m[1].m_limit_number_of_packets =6;
    //merger.m[2].Create("avl/delay_10_rtp_250k_1_0.pcap",0.311);
    //merger.m[2].m_limit_number_of_packets =6;

    merger.run_merge("avl/delay_10_sip_video_call_full.pcap");

    return (0);
}

int
SimStateful::run() {
    TrexSTXCfg cfg;
    
    assert( CMsgIns::Ins()->Create(4) );
    STXSimGuard guard(new TrexStateful(cfg, nullptr));
    
    try {
        return load_list_of_cap_files(&CGlobalInfo::m_options);
    } catch (const std::runtime_error &e) {
        std::cout << "\n*** " << e.what() << "\n\n";
        exit(-1);
    }
     return (0);
}
