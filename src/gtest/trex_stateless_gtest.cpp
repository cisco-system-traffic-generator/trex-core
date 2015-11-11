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
#include <trex_stateless_dp_core.h>
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>
#include <trex_stream_node.h>
#include <trex_stream.h>
#include <trex_stateless_port.h>
#include <trex_rpc_server_api.h>


#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))



/* basic stateless test */
class basic_stl  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};



class CBasicStl {

public:
    CBasicStl(){
        m_time_diff=0.001;
        m_threads=1;
        m_dump_json=false;
    }

    bool  init(void){

        CErfIFStl erf_vif;
        fl.Create();
        fl.generate_p_thread_info(1);
        CFlowGenListPerThread   * lpt;

        fl.m_threads_info[0]->set_vif(&erf_vif);

        CErfCmp cmp;
        cmp.dump=1;

        CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

        m_ring_from_cp = cp_dp->getRingCpToDp(0);


        bool res=true;

        lpt=fl.m_threads_info[0];

        char buf[100];
        char buf_ex[100];
        sprintf(buf,"%s-%d.erf",CGlobalInfo::m_options.out_file.c_str(),0);
        sprintf(buf_ex,"%s-%d-ex.erf",CGlobalInfo::m_options.out_file.c_str(),0);

        lpt->start_stateless_simulation_file(buf,CGlobalInfo::m_options.preview);

        /* add stream to the queue */
        assert(m_msg);

        assert(m_ring_from_cp->Enqueue((CGenNode *)m_msg)==0);

        lpt->start_stateless_daemon_simulation();

        lpt->m_node_gen.DumpHist(stdout);


        cmp.d_sec = m_time_diff;
        if ( cmp.compare(std::string(buf),std::string(buf_ex)) != true ) {
            res=false;
        }

        if ( m_dump_json ){
            printf(" dump json ...........\n");
            std::string s;
            fl.m_threads_info[0]->m_node_gen.dump_json(s);
            printf(" %s \n",s.c_str());
        }

        fl.Delete();
        return (res);
    }


public:
    int           m_threads;
    double        m_time_diff;
    bool          m_dump_json;
    TrexStatelessCpToDpMsgBase * m_msg;
    CNodeRing           *m_ring_from_cp;
    CFlowGenList  fl;
};



const uint8_t my_test_pkt[]={ 

    0x00,0x04,0x96,0x08,0xe0,0x40,
    0x00,0x0e,0x2e,0x24,0x37,0x5f,
    0x08,0x00,

    0x45,0x02,0x00,0x30,
    0x00,0x00,0x40,0x00,
    0x40,0x84,0xbd,0x04,
    0x9b,0xe6,0x18,0x9b, //sIP
    0xcb,0xff,0xfc,0xc2, //DIP

    0x80,0x44,//SPORT
    0x00,0x50,//DPORT

    0x00,0x00,0x00,0x00, //checksum 

    0x11,0x22,0x33,0x44, // magic 
    0x00,0x00,0x00,0x00, //64 bit counter
    0x00,0x00,0x00,0x00,
    0x00,0x01,0xa0,0x00, //seq
    0x00,0x00,0x00,0x00,
};





TEST_F(basic_stl, limit_single_pkt) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_single_sctp_pkt";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStreamContinuous(0,0,1.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     uint8_t      *binary = new uint8_t[sizeof(my_test_pkt)];
     memcpy(binary,my_test_pkt,sizeof(my_test_pkt));

     stream1->m_pkt.binary = binary;
     stream1->m_pkt.len    = sizeof(my_test_pkt);


     streams.push_back(stream1);

     // stream - clean 

     TrexStreamsCompiledObj comp_obj(0,1.0);

     comp_obj.set_simulation_duration( 10.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}






