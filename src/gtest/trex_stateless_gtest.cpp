/*
 Hanoch Haim
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

#include <algorithm>
#include <iostream>
#include <vector>
#include <inttypes.h>
#include <tuple>

#include <common/gtest.h>
#include <common/basic_utils.h>

#include "bp_sim.h"
#include "bp_gtest.h"

#include "flow_stat_parser.h"
#include "trex_rpc_server_api.h"


#include "stl/trex_stl.h"
#include "stl/trex_stl_dp_core.h"
#include "stl/trex_stl_messaging.h"
#include "stl/trex_stl_streams_compiler.h"
#include "stl/trex_stl_stream_node.h"
#include "stl/trex_stl_stream.h"
#include "stl/trex_stl_port.h"

 

class CPcapLoader {
public:
    CPcapLoader();
    ~CPcapLoader();


public:
    bool load_pcap_file(std::string file,int pkt_id=0);
    void update_ip_src(uint32_t ip_addr);
    void clone_packet_into_stream(TrexStream * stream);
    void dump_packet();

public:
    bool                    m_valid;
    CCapPktRaw              m_raw;
    CPacketIndication       m_pkt_indication;
};

CPcapLoader::~CPcapLoader(){
}

bool CPcapLoader::load_pcap_file(std::string cap_file,int pkt_id){
    m_valid=false;
    CPacketParser parser;

    CCapReaderBase * lp=CCapReaderFactory::CreateReader((char *)cap_file.c_str(),0);

    if (lp == 0) {
        printf(" ERROR file %s does not exist or not supported \n",(char *)cap_file.c_str());
        return false;
    }

    int cnt=0;
    bool found =false;


    while ( true ) {
        /* read packet */
        if ( lp->ReadPacket(&m_raw) ==false ){
            break;
        }
        if (cnt==pkt_id) {
            found = true;
            break;
        }
        cnt++;
    }
    if ( found ){
        if ( parser.ProcessPacket(&m_pkt_indication, &m_raw) ){
            m_valid = true;
        }
    }

    delete lp;
    return (m_valid);
}

void CPcapLoader::update_ip_src(uint32_t ip_addr){

    if ( m_pkt_indication.l3.m_ipv4 ) {
        m_pkt_indication.l3.m_ipv4->setSourceIp(ip_addr);
        m_pkt_indication.l3.m_ipv4->updateCheckSum();
    }
}

void CPcapLoader::clone_packet_into_stream(TrexStream * stream){

    uint16_t pkt_size=m_raw.getTotalLen();

    uint8_t      *binary = new uint8_t[pkt_size];
    memcpy(binary,m_raw.raw,pkt_size);
    stream->m_pkt.binary = binary;
    stream->m_pkt.len    = pkt_size;
}




CPcapLoader::CPcapLoader(){

}

void CPcapLoader::dump_packet(){
    if (m_valid ) {
        m_pkt_indication.Dump(stdout,1);
    }else{
        fprintf(stdout," no packets were found \n");
    }
}




class basic_vm  : public trexStlTest {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};


TEST_F(basic_vm, cache_basic) {

    CGenNodeStateless *node = new CGenNodeStateless();

    node->cache_mbuf_array_init();
    int i;
    node->cache_mbuf_array_alloc(10);
    for (i=0; i<10; i++) {
        rte_mbuf_t * m =CGlobalInfo::pktmbuf_alloc_small(0);
        m->data_off=i;
        node->cache_mbuf_array_set(i,(rte_mbuf_t *) m);
    }

    for (i=0; i<10; i++) {
      rte_mbuf_t * m =node->cache_mbuf_array_get_cur();
      printf(" %d \n",m->data_off);
    }

    node->cache_mbuf_array_free();

    delete node;
}


TEST_F(basic_vm, pkt_size) {

    EXPECT_EQ(calc_writable_mbuf_size(36,62),62);
    EXPECT_EQ(calc_writable_mbuf_size(63,62),62);
    EXPECT_EQ(calc_writable_mbuf_size(45,65),65);
    EXPECT_EQ(calc_writable_mbuf_size(66,65),65);
    EXPECT_EQ(calc_writable_mbuf_size(62,128),128);
    EXPECT_EQ(calc_writable_mbuf_size(62,252),63);
    EXPECT_EQ(calc_writable_mbuf_size(121,252),122);
    EXPECT_EQ(calc_writable_mbuf_size(253,252),254);
    EXPECT_EQ(calc_writable_mbuf_size(250,252),252);
    EXPECT_EQ(calc_writable_mbuf_size(184,252),185);
}


TEST_F(basic_vm, vm_rand_limit0) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",1,100,0,200,0x1234) );

    vm.Dump(stdout);
}

TEST_F(basic_vm, vm_rand_limit1) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",1,100,0,200,0x1234) );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );
    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

}

TEST_F(basic_vm, vm_rand_limit2) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",1,100,0,100,0x1234) );
    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var2",2,100,0,100,0x1234) );
    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var3",4,100,0,100,0x1234) );
    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var4",8,100,0,100,0x1234) );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );
    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

}




/* start/stop/stop back to back */
TEST_F(basic_vm, vm0) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(20) );
    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",8,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,7 )
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",14, 0,true)
                        );

    vm.Dump(stdout);
}

TEST_F(basic_vm, vm1) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,7 )
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );
    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

}

TEST_F(basic_vm, vm2) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,5,1,7 )
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );
    //vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    uint8_t test_udp_pkt[14+20+4+4]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

    uint8_t ex[]={5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3};

    uint32_t random_per_thread=0;
    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);
        EXPECT_EQ(test_udp_pkt[26],ex[i]);
    }

}


TEST_F(basic_vm, vm3) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,5,1,7 )
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );
    //vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

    uint8_t ex[]={5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3};

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d \n",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* big */
        EXPECT_EQ(test_udp_pkt[29],ex[i]);
        EXPECT_EQ(test_udp_pkt[28],0);
        EXPECT_EQ(test_udp_pkt[27],0);
        EXPECT_EQ(test_udp_pkt[26],0);
    }

}

TEST_F(basic_vm, vm4) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1", 8 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,5,1,7 )
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,false)
                        );
    //vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

    uint8_t ex[]={5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3};

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d \n",i);
        utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        EXPECT_EQ(test_udp_pkt[33],0);
        EXPECT_EQ(test_udp_pkt[32],0);
        EXPECT_EQ(test_udp_pkt[31],0);
        EXPECT_EQ(test_udp_pkt[30],0);
        EXPECT_EQ(test_udp_pkt[29],0);
        EXPECT_EQ(test_udp_pkt[28],0);
        EXPECT_EQ(test_udp_pkt[27],0);
        EXPECT_EQ(test_udp_pkt[26],ex[i]);
    }

}


/* two fields */
TEST_F(basic_vm, vm5) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,5,1,7 )
                        );

    vm.add_instruction( new StreamVmInstructionFlowMan( "var2",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC,24,23,27 ) );

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );

    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var2",15, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

    uint8_t ex[]={5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3,
                 4,
                 5,
                 6,
                 7,
                 1,
                 2,
                 3};

         uint8_t ex_tos[]={0x18,
                      0x17,
                      0x1b,
                      0x1a,
                      0x19,
                      0x18,
                      0x17,
                      0x1b,
                     0x1a,
                     0x19,
                     0x18,
                     0x17,
             0x1b,
            0x1a,
            0x19,
            0x18,
            0x17,
             0x1b,
            0x1a,
            0x19,
            0x18,
            0x17,

             0x1b,
            0x1a,
            0x19,
            0x18,
            0x17,

             0x1b,
            0x1a,
            0x19,
            0x18,
            0x17,

             0x1b,
            0x1a,
            0x19,
            0x18,
            0x17,
         };

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d \n",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        EXPECT_EQ(test_udp_pkt[29],ex[i]);
        EXPECT_EQ(test_udp_pkt[28],0);
        EXPECT_EQ(test_udp_pkt[27],0);
        EXPECT_EQ(test_udp_pkt[26],0);

        /* check tos */
        EXPECT_EQ(test_udp_pkt[15],ex_tos[i]);
    }
}

/* two fields */
TEST_F(basic_vm, vm5_list) {

    StreamVm vm;
    std::vector<uint64_t> value_list_inc;
    std::vector<uint64_t> value_list_dec;

    int n;
    for (n=5; n<=7; n++) {
        value_list_inc.push_back(n);
    }
    for (n=1; n<=4; n++) {
        value_list_inc.push_back(n);
    }

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,value_list_inc ));

    for (n=25; n<=27; n++) {
        value_list_dec.push_back(n);
    }
    for (n=23; n<=24; n++) {
        value_list_dec.push_back(n);
    }

    vm.add_instruction( new StreamVmInstructionFlowMan( "var2",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC,value_list_dec ));

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );

    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var2",15, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);

    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

    uint8_t ex[]={5,
                  6,
                  7,
                  1,
                  2,
                  3,
                  4,
                  5,
                  6,
                  7,
                  1,
                  2,
                  3,
                  4,
                  5,
                  6,
                  7,
                  1,
                  2,
                  3};

         uint8_t ex_tos[]={0x18,
                           0x17,
                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,
                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,
                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,
                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,

                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,

                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,

                           0x1b,
                           0x1a,
                           0x19,
                           0x18,
                           0x17,
         };

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d \n",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        EXPECT_EQ(test_udp_pkt[29],ex[i]);
        EXPECT_EQ(test_udp_pkt[28],0);
        EXPECT_EQ(test_udp_pkt[27],0);
        EXPECT_EQ(test_udp_pkt[26],0);

        /* check tos */
        EXPECT_EQ(test_udp_pkt[15],ex_tos[i]);
    }
}

TEST_F(basic_vm, vm_rand_len) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",1,10,0,255,0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

         uint8_t ex_tos[]={
             0x98,          
             0xbd,          
             0xb5,          
             0x7 ,          
             0x14,          
             0xd6,          
             0x3 ,          
             0x90,          
             0x5d,          
             0x88,          
              0x98,         
              0xbd,         
              0xb5,         
              0x7 ,         
              0x14,         
              0xd6,         
              0x3 ,         
              0x90,         
              0x5d,         
              0x88
         };


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %x \n",test_udp_pkt[15]);

        /* check tos */
        EXPECT_EQ(test_udp_pkt[15],ex_tos[i]);
    }
}

TEST_F(basic_vm, vm_rand_len_l0) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",1,10,1,10,0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

         uint8_t ex_tos[]={
         0x7,    
         0xa,    
         0xa,    
         0x4,    
         0x7,    
         0x5,    
         0x2,    
         0x3,    
         0x8,    
         0x9,    
          0x7,   
          0xa,   
          0xa,   
          0x4,   
          0x7,   
          0x5,   
          0x2,   
          0x3,   
          0x8,   
          0x9,
             
         };


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %x \n",test_udp_pkt[15]);

        /* check tos */
        EXPECT_EQ(test_udp_pkt[15],ex_tos[i]);
    }
}


TEST_F(basic_vm, vm_rand_len1) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",2,10,0,((1<<16)-1),0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;


         uint16_t ex_tos[]={
             0x983b,         
             0xbd64,         
             0xb512,         
             0x715 ,         
             0x1410,         
             0xd664,         
             0x371 ,         
             0x9058,         
             0x5d3b,         
             0x884d,         
             0x983b,         
             0xbd64,         
             0xb512,         
             0x715 ,         
             0x1410,         
             0xd664,         
             0x371 ,         
             0x9058,         
             0x5d3b,         
             0x884d         
         };


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %x \n",*((uint16_t*)&test_udp_pkt[15]));

        /* check tos */
        EXPECT_EQ(*((uint16_t*)&test_udp_pkt[15]),ex_tos[i]);
    }
}

TEST_F(basic_vm, vm_rand_len2) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",4,10,0,((1UL<<32)-1),0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

         uint32_t ex_tos[]={
             0xbd64983b,        
             0x715b512 ,        
             0xd6641410,        
             0x90580371,        
             0x884d5d3b,        
             0x8e0f8212,        
             0xf00b2f39,        
             0xa015ee4e,        
             0x540b390e,        
             0xdb778538,        
             0xbd64983b,       
             0x715b512 ,       
             0xd6641410,       
             0x90580371,       
             0x884d5d3b,       
             0x8e0f8212,       
             0xf00b2f39,       
             0xa015ee4e,       
             0x540b390e,       
             0xdb778538       
         };


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %x \n",*((uint32_t*)&test_udp_pkt[15]));

        /* check tos */
        EXPECT_EQ(*((uint32_t*)&test_udp_pkt[15]),ex_tos[i]);
    }
}

TEST_F(basic_vm, vm_rand_len3) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",8,10,0,UINT64_MAX,0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

#if 0
         uint32_t ex_tos[]={
             0xbd64983b,        
             0x715b512 ,        
             0xd6641410,        
             0x90580371,        
             0x884d5d3b,        
             0x8e0f8212,        
             0xf00b2f39,        
             0xa015ee4e,        
             0x540b390e,        
             0xdb778538,        
             0xbd64983b,       
             0x715b512 ,       
             0xd6641410,       
             0x90580371,       
             0x884d5d3b,       
             0x8e0f8212,       
             0xf00b2f39,       
             0xa015ee4e,       
             0x540b390e,       
             0xdb778538       
         };
#endif


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %" PRIx64 " \n",*((uint64_t*)&test_udp_pkt[15]));

        /* check tos */
        //EXPECT_EQ(*((uint64_t*)&test_udp_pkt[15]),ex_tos[i]);
    }
}


TEST_F(basic_vm, vm_rand_len3_l0) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",8,10,0x1234567,0x2234567,0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

#if 0
         uint32_t ex_tos[]={
             0xbd64983b,        
             0x715b512 ,        
             0xd6641410,        
             0x90580371,        
             0x884d5d3b,        
             0x8e0f8212,        
             0xf00b2f39,        
             0xa015ee4e,        
             0x540b390e,        
             0xdb778538,        
             0xbd64983b,       
             0x715b512 ,       
             0xd6641410,       
             0x90580371,       
             0x884d5d3b,       
             0x8e0f8212,       
             0xf00b2f39,       
             0xa015ee4e,       
             0x540b390e,       
             0xdb778538       
         };
#endif


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %" PRIx64 " \n",pal_ntohl64(*((uint64_t*)&test_udp_pkt[15])));

        /* check tos */
        //EXPECT_EQ(*((uint64_t*)&test_udp_pkt[15]),ex_tos[i]);
    }
}


/* -load file, write to file  */
TEST_F(basic_vm, vm6) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000001,0x10000001,0x100000fe)
                        );

    vm.add_instruction( new StreamVmInstructionFlowMan( "var2",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC,24,23,27 ) );

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",26, 0,true)
                        );

    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var2",15, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm6.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        //utl_DumpBuffer(stdout,pcap.m_raw.raw,pcap.m_raw.pkt_len,0);
        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm6.pcap","exp/udp_64B_vm6-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_rand_len3_l1) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowRandLimit( "var1",4,10,0x01234567,0x02234567,0x1234) );


    /* change TOS */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",15, 0,true));


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    #define PKT_TEST_SIZE (14+20+4+4)
    uint8_t test_udp_pkt[PKT_TEST_SIZE]={
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x01,0x00,0x00,
        0x08,0x00,

        0x45,0x00,0x00,0x81, /*14 */
        0xaf,0x7e,0x00,0x00, /*18 */
        0x12,0x11,0xd9,0x23, /*22 */
        0x01,0x01,0x01,0x01, /*26 */
        0x3d,0xad,0x72,0x1b, /*30 */

        0x11,0x11,           /*34 */
        0x11,0x11,

        0x00,0x6d,
        0x00,0x00,
    };



    StreamDPVmInstructionsRunner runner;

#if 0
         uint32_t ex_tos[]={
             0xbd64983b,        
             0x715b512 ,        
             0xd6641410,        
             0x90580371,        
             0x884d5d3b,        
             0x8e0f8212,        
             0xf00b2f39,        
             0xa015ee4e,        
             0x540b390e,        
             0xdb778538,        
             0xbd64983b,       
             0x715b512 ,       
             0xd6641410,       
             0x90580371,       
             0x884d5d3b,       
             0x8e0f8212,       
             0xf00b2f39,       
             0xa015ee4e,       
             0x540b390e,       
             0xdb778538       
         };
#endif


    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_udp_pkt);

        fprintf(stdout," %d :",i);
        //utl_DumpBuffer(stdout,test_udp_pkt,PKT_TEST_SIZE,0);
        /* not big */
        printf(" %x \n",PAL_NTOHL(*((uint64_t*)&test_udp_pkt[15])));

        /* check tos */
        //EXPECT_EQ(*((uint64_t*)&test_udp_pkt[15]),ex_tos[i]);
    }
}


/* test client command */
TEST_F(basic_vm, vm7) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowClient( "cl1",
                                                           0x10000001,
                                                           0x10000004,
                                                           1025,
                                                           1027,
                                                           100,
                                                           0) );

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "cl1.ip",26, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* src port */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "cl1.port",34, 0,true)
                        );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm7.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm7.pcap","exp/udp_64B_vm7-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


////////////////////////////////////////////////////////

TEST_F(basic_vm, vm_no_flow_var) {

    bool fail=false;
    /* should not be failed */

    try {
        StreamVm vm;
        vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );
        vm.compile(128);
        uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();
        printf(" program_size : %lu \n",(ulong)program_size);
     } catch (const TrexException &ex) {
        fail=true;
    }

    EXPECT_EQ(fail,false);
}

TEST_F(basic_vm, vm_mask_err) {

    bool fail=false;
    /* should fail */

    try {
        StreamVm vm;
        vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00ff,0,0,1) );
        vm.compile(128);
        uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();
        printf(" program_size : %lu \n",(ulong)program_size);
     } catch (const TrexException &ex) {
        fail=true;
    }

     EXPECT_EQ(true, fail);
}


TEST_F(basic_vm, vm_mask1) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000007,0x10000007,0x100000fe)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00ff,0,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask1.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask1.pcap","exp/udp_64B_vm_mask1-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_mask2) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000007,0x10000007,0x100000fe)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0xff00,8,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask2.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask2.pcap","exp/udp_64B_vm_mask2-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask3) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000007,0x10000007,0x100000fe)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,1,0x2,1,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask3.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask3.pcap","exp/udp_64B_vm_mask3-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask4) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,10)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0xFF00,8,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask4.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask4.pcap","exp/udp_64B_vm_mask4-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask5) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,10)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,4,0x00FF0000,16,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask5.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask5.pcap","exp/udp_64B_vm_mask5-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_mask6) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,20)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00FF,-1,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask6.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask6.pcap","exp/udp_64B_vm_mask6-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask6_list) {

    StreamVm vm;
    std::vector<uint64_t> value_list;

    int n;
    for (n=1; n<=20; n++) {
        value_list.push_back(n);
    }
    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC, value_list));

    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00FF,-1,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm_mask6.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm_mask6.pcap","exp/udp_64B_vm_mask6-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

////////////////////////////////////////////////////////


TEST_F(basic_vm, vm8) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowClient( "cl1",
                                                           0x10000001,
                                                           0x10000006,
                                                           1025,
                                                           1027,
                                                           4,
                                                           0) );

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "cl1.ip",26, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* src port */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "cl1.port",34, 0,true)
                        );


    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm8.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<20; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm8.pcap","exp/udp_64B_vm8-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

static void vm_build_program_seq(StreamVm & vm,
                                 uint16_t packet_size,
                                 bool should_compile){

    vm.add_instruction( new StreamVmInstructionFlowClient( "tuple_gen",
                                                           0x10000001,
                                                           0x10000006,
                                                           1025,
                                                           1027,
                                                           20,
                                                           0) );

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "tuple_gen.ip",26, 0,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* src port */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "tuple_gen.port",34, 0,true)
                        );

    if (should_compile) {
        vm.compile(packet_size);
    }
}


TEST_F(basic_vm, vm9) {


    StreamVm vm;

    vm_build_program_seq(vm,128, true);

    printf(" max packet update %lu \n",(ulong)vm.get_max_packet_update_offset());

    EXPECT_EQ(36,vm.get_max_packet_update_offset());

    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm9.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<30; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;

    bool res1=cmp.compare("generated/udp_64B_vm9.pcap","exp/udp_64B_vm9-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


/* test vmDP object */
TEST_F(basic_vm, vm10) {

    StreamVm vm;

    vm_build_program_seq(vm,128, true);

    printf(" max packet update %lu \n",(ulong)vm.get_max_packet_update_offset());

    EXPECT_EQ(36,vm.get_max_packet_update_offset());

    StreamVmDp * lpDpVm =vm.generate_dp_object();

    EXPECT_EQ(lpDpVm->get_bss_size(),vm.get_bss_size());

    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/udp_64B_vm9.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<30; i++) {

        runner.run(&random_per_thread,
                   lpDpVm->get_program_size(),
                   lpDpVm->get_program(),
                   lpDpVm->get_bss(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;
    delete  lpDpVm;

    bool res1=cmp.compare("generated/udp_64B_vm9.pcap","exp/udp_64B_vm9-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


/* test vmDP object */
TEST_F(basic_vm, vm_syn_attack) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "ip_src",
                                                        4,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        0,
                                                        0,
                                                        1000000));

    vm.add_instruction( new StreamVmInstructionFlowMan( "ip_dst",
                                                        4,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        0,
                                                        0,
                                                        1000000));

    vm.add_instruction( new StreamVmInstructionFlowMan( "src_port",
                                                        2,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        1025,
                                                        1025,
                                                        65000));

    vm.add_instruction( new StreamVmInstructionFlowMan( "dst_port",
                                                        2,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        1025,
                                                        1025,
                                                        65000));

    /* src ip */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "ip_src",26, 0x10000000,true)
                        );

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "ip_dst",26+4, 0x40000000,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* src port */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "src_port",34, 0,true)
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "dst_port",34+2, 0,true)
                        );

    vm.compile(128);

    printf(" max packet update %lu \n",(ulong)vm.get_max_packet_update_offset());

    EXPECT_EQ(38,vm.get_max_packet_update_offset());

    StreamVmDp * lpDpVm =vm.generate_dp_object();

    EXPECT_EQ(lpDpVm->get_bss_size(),vm.get_bss_size());

    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("stl/syn_packet.pcap",0);



    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"generated/stl_syn_attack.pcap");
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<30; i++) {

        runner.run(&random_per_thread,
                   lpDpVm->get_program_size(),
                   lpDpVm->get_program(),
                   lpDpVm->get_bss(),
                   (uint8_t*)pcap.m_raw.raw);

        assert(lpWriter->write_packet(&pcap.m_raw));
    }

    delete lpWriter;

    CErfCmp cmp;
    delete  lpDpVm;

    bool res1=cmp.compare("generated/stl_syn_attack.pcap","exp/stl_syn_attack-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


void run_vm_program( StreamVm & vm,
                    std::string input_pcap_file,
                    std::string out_file_name,
                    int num_pkts
                    ){

    CPcapLoader pcap;
    pcap.load_pcap_file(input_pcap_file,0);

    printf(" packet size : %lu \n",(ulong)pcap.m_raw.pkt_len);
    vm.compile(pcap.m_raw.pkt_len);

    StreamVmDp * lpDpVm =vm.generate_dp_object();

    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);

    vm.Dump(stdout);

    std::string out_file_full ="generated/"+out_file_name +".pcap";
    std::string out_file_ex_full ="exp/"+out_file_name +"-ex.pcap";

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)out_file_full.c_str());
    assert(lpWriter);


    StreamDPVmInstructionsRunner runner;

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<num_pkts; i++) {

        runner.run(&random_per_thread,
                   lpDpVm->get_program_size(),
                   lpDpVm->get_program(),
                   lpDpVm->get_bss(),
                   (uint8_t*)pcap.m_raw.raw);
        uint16_t new_pkt_size=runner.get_new_pkt_size();
        assert(new_pkt_size>0);
        if (new_pkt_size ==0) {
            assert(lpWriter->write_packet(&pcap.m_raw));
        }else{
            /* we can only reduce */
            if (new_pkt_size>pcap.m_raw.pkt_len) {
                new_pkt_size=pcap.m_raw.pkt_len;
            }
            CCapPktRaw np(new_pkt_size);
             np.time_sec  = pcap.m_raw.time_sec;
             np.time_nsec = pcap.m_raw.time_nsec;
             np.pkt_cnt   = pcap.m_raw.pkt_cnt;
             memcpy(np.raw,pcap.m_raw.raw,new_pkt_size);
             assert(lpWriter->write_packet(&np));
        }
    }

    delete lpWriter;

    CErfCmp cmp;
    delete  lpDpVm;

    bool res1=cmp.compare(out_file_full.c_str() ,out_file_ex_full.c_str());
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_inc_size_64_128) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,
                                                        128,
                                                        128,
                                                        256));

    vm.add_instruction( new StreamVmInstructionChangePktSize( "rand_pkt_size_var"));

    /* src ip */
    /*14+ 2 , remove the */

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",16, -14,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* update UDP length */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",32+6, -(14+20),true)
                        );

    run_vm_program(vm,"stl/udp_1518B_no_crc.pcap","stl_vm_inc_size_64_128",20);
}

TEST_F(basic_vm, vm_random_size_64_128) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        128,
                                                        128,
                                                        256));

    vm.add_instruction( new StreamVmInstructionChangePktSize( "rand_pkt_size_var"));

    /* src ip */
    /*14+ 2 , remove the */

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",16, -14,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* update UDP length */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",32+6, -(14+20),true)
                        );


    run_vm_program(vm,"stl/udp_1518B_no_crc.pcap","stl_vm_rand_size_64_128",20);

}



/* should have exception packet size is smaller than range */
TEST_F(basic_vm, vm_random_size_64_127_128) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        128,
                                                        128,
                                                        256));

    vm.add_instruction( new StreamVmInstructionChangePktSize( "rand_pkt_size_var"));

    /* src ip */
    /*14+ 2 , remove the */

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",16, -14,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* update UDP length */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",32+6, -(14+20),true)
                        );

    bool fail=false;

    try {
       run_vm_program(vm,"stl/udp_64B_no_crc.pcap","stl_vm_rand_size_64B_127_128",20);
     } catch (const TrexException &ex) {
        fail=true;
    }

    EXPECT_EQ(true, fail);

}

/* should have exception packet size is smaller than range */
TEST_F(basic_vm, vm_random_size_64_127_128_list) {

    StreamVm vm;
    std::vector<uint64_t> value_list;
    srand(0x1234);

    int n;
    for (n=128; n<=256; n++) {
        value_list.push_back(n);
    }

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        value_list));

    vm.add_instruction( new StreamVmInstructionChangePktSize( "rand_pkt_size_var"));

    /* src ip */
    /*14+ 2 , remove the */

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",16, -14,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* update UDP length */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",32+6, -(14+20),true)
                        );

    bool fail=false;

    try {
       run_vm_program(vm,"stl/udp_64B_no_crc.pcap","stl_vm_rand_size_64B_127_128",20);
     } catch (const TrexException &ex) {
        fail=true;
    }

    EXPECT_EQ(true, fail);

}

TEST_F(basic_vm, vm_random_size_500b_0_9k) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        0,
                                                        0,
                                                        9*1024));

    vm.add_instruction( new StreamVmInstructionChangePktSize( "rand_pkt_size_var"));

    /* src ip */
    /*14+ 2 , remove the */

    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",16, -14,true)
                        );

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );

    /* update UDP length */
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "rand_pkt_size_var",32+6, -(14+20),true)
                        );

    run_vm_program(vm,"stl/udp_594B_no_crc.pcap","stl_vm_rand_size_512B_64_128",10);

}

TEST_F(basic_vm, vm_fix_icmpv6) {
    const uint8_t PKT_SIZE = 14+40+32;

    StreamVm vm;
    vm.add_instruction(new StreamVmInstructionFlowMan("ip_src",
                                                       1,
                                                       StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,
                                                       1,
                                                       1,
                                                       10));
    vm.add_instruction(new StreamVmInstructionWriteToPkt("ip_src", 37, 0, true));
    vm.add_instruction(new StreamVmInstructionFixChecksumIcmpv6(14, 40));
    vm.compile(PKT_SIZE);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);

    vm.Dump(stdout);

    
    uint8_t test_icmpv6_pkt[PKT_SIZE]={

        0x33, 0x33, 0x00, 0x01, 0x00, 0x01, // Ether dst
        0x08, 0x00, 0x27, 0x1c, 0x77, 0x79, // Ether src
        0x86, 0xdd, // l2_len is 14
        

        0x60, 0x00, 0x00, 0x00, 0x00, 0x20, 0x3a, 0xff,
        // IPv6 src
        0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 

        // IPv6 dst
        0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0xff, 0x01, 0x00, 0x01,


        0x87, 0x00, 0x76, 0x91, 0x00, 0x00, 0x00, 0x00,
        // ICMPv6 tgt
        0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,

        0x01, 0x01, 0x08, 0x00, 0x27, 0x1c, 0x77, 0x79
    };


    StreamDPVmInstructionsRunner runner;
    uint16_t ex_checksum[] = {
        0x7691,
        0x7690,
        0x768f,
        0x768e,
        0x768d,
        0x768c,
        0x768b,
        0x768a,
        0x7689,
        0x7688,
    };

    uint32_t random_per_thread=0;

    int i;
    for (i=0; i<10; i++) {
        runner.run(&random_per_thread,
                   program_size,
                   vm.get_dp_instruction_buffer()->get_program(),
                   vm.get_bss_ptr(),
                   test_icmpv6_pkt);

        /* validate checksum is correct */
        uint16_t result_checksum = test_icmpv6_pkt[56] << 8 | test_icmpv6_pkt[57];
        fprintf(stdout, " %d: checksum %x, expected %x \n", i, result_checksum, ex_checksum[i]);
        EXPECT_EQ(result_checksum, ex_checksum[i]);
    }
}


//////////////////////////////////////////////////////


#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))



/* basic stateless test */
class basic_stl  : public trexStlTest {
 protected:
  virtual void SetUp() {
      set_op_debug_mode(OP_MODE_STL);
  }
  virtual void TearDown() {
  }
public:
};


/**
 * Queue of RPC msgs for test
 *
 * @author hhaim
 */

class CBasicStl;


struct CBasicStlDelayCommand {

    CBasicStlDelayCommand(){
        m_node=NULL;
    }
    CGenNodeCommand * m_node;
};



class CBasicStlMsgQueue {

friend CBasicStl;

public:
    CBasicStlMsgQueue(){
    }

    /* user will allocate the message,  no need to free it by this module */
    void add_msg(TrexCpToDpMsgBase * msg){
        m_msgs.push_back(msg);
    }

    void add_command(CBasicStlDelayCommand & command){
        m_commands.push_back(command);
    }

        /* only if both port are idle we can exit */
    void add_command(CFlowGenListPerThread   * core,
                     TrexCpToDpMsgBase       * msg,
                     double time){

        CGenNodeCommand *node = (CGenNodeCommand *)core->create_node() ;

        node->m_type = CGenNode::COMMAND;

        node->m_cmd = msg;

        /* make sure it will be scheduled after the current node */
        node->m_time = time ;

        CBasicStlDelayCommand command;
        command.m_node =node;

        add_command(command);
    }


    void clear(){
        m_msgs.clear();
        m_commands.clear();
    }


protected:
    std::vector<TrexCpToDpMsgBase *> m_msgs;

    std::vector<CBasicStlDelayCommand> m_commands;
};



class CBasicStlSink {

public:
    CBasicStlSink(){
        m_core=0;
    }
    virtual void call_after_init(CBasicStl * m_obj)=0;
    virtual void call_after_run(CBasicStl * m_obj)=0;

    CFlowGenListPerThread   * m_core;
};


/**
 * handler for DP to CP messages
 *
 * @author imarom (19-Nov-15)
 */
class DpToCpHandler {
public:
    virtual void handle(TrexDpToCpMsgBase *msg) = 0;
};

class CBasicStl {

public:


    CBasicStl(){
        m_time_diff=0.001;
        m_threads=1;
        m_dump_json=false;
        m_dp_to_cp_handler = NULL;
        m_msg = NULL;
        m_sink = NULL;
    }


    void flush_dp_to_cp_messages() {

        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(0);

        while ( true ) {
            CGenNode * node = NULL;
            if (ring->Dequeue(node) != 0) {
                break;
            }
            assert(node);

            TrexDpToCpMsgBase * msg = (TrexDpToCpMsgBase *)node;
            if (m_dp_to_cp_handler) {
                m_dp_to_cp_handler->handle(msg);
            }

            delete msg;
        }

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

        if ( m_sink ){
            m_sink->m_core =lpt;
        }

        char buf[100];
        char buf_ex[100];
        sprintf(buf,"generated/%s-%d.erf",CGlobalInfo::m_options.out_file.c_str(),0);
        sprintf(buf_ex,"exp/%s-%d-ex.erf",CGlobalInfo::m_options.out_file.c_str(),0);


        /* add stream to the queue */
        if ( m_msg ) {
            assert(m_ring_from_cp->Enqueue((CGenNode *)m_msg)==0);
        }
        if (m_msg_queue.m_msgs.size()>0) {
            for (auto msg : m_msg_queue.m_msgs) {
                assert(m_ring_from_cp->Enqueue((CGenNode *)msg)==0);
            }
        }

        if (m_sink) {
            m_sink->call_after_init(this);
        }

        /* add the commands */
        if (m_msg_queue.m_commands.size()>0) {
            for (auto cmd : m_msg_queue.m_commands) {
                /* add commands nodes */
                lpt->m_node_gen.add_node((CGenNode *)cmd.m_node);
            }
        }

        lpt->start_sim(buf, CGlobalInfo::m_options.preview);

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

        if (m_sink) {
            m_sink->call_after_run(this);
        }

        flush_dp_to_cp_messages();
        m_msg_queue.clear();


        fl.Delete();
        return (res);
    }

public:
    int               m_threads;
    double            m_time_diff;
    bool              m_dump_json;
    DpToCpHandler     *m_dp_to_cp_handler;
    CBasicStlSink     * m_sink;

    TrexCpToDpMsgBase   *m_msg;
    CNodeRing           *m_ring_from_cp;
    CBasicStlMsgQueue    m_msg_queue;
    CFlowGenList  fl;
};



TEST_F(basic_stl, load_pcap_file) {
    printf (" stateles %d \n",(int)sizeof(CGenNodeStateless));

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000001);
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000001);

    //pcap.dump_packet();
}



class CBBStartPause0: public CBasicStlSink {
public:

    virtual void call_after_init(CBasicStl * m_obj);
    virtual void call_after_run(CBasicStl * m_obj){
    };
    uint8_t m_port_id;
    uint32_t m_profile_id;
};



void CBBStartPause0::call_after_init(CBasicStl * m_obj){

    TrexStatelessDpPause * lpPauseCmd = new TrexStatelessDpPause(m_port_id, m_profile_id);
    TrexStatelessDpResume * lpResumeCmd1 = new TrexStatelessDpResume(m_port_id, m_profile_id);

    m_obj->m_msg_queue.add_command(m_core,lpPauseCmd, 4.99); /* command in delay of 4.99 sec */
    m_obj->m_msg_queue.add_command(m_core,lpResumeCmd1, 6.99);/* command in delay of 6.99 sec */

}



/* start/stop/stop back to back */
TEST_F(basic_stl, basic_pause_resume0) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_basic_pause_resume0";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;
     uint32_t profile_id=0x12345678;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean

     std::vector<TrexStreamsCompiledObj *> objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, profile_id, 0, objs[0], 9.99 /*sec */ );

     t1.m_msg_queue.add_msg(lpStartCmd);


     CBBStartPause0 sink;
     sink.m_port_id = port_id;
     sink.m_profile_id = profile_id;
     t1.m_sink =  &sink;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


//////////////////////////////////////////////////////////////


class CBBStartStopDelay3: public CBasicStlSink {
public:

    virtual void call_after_init(CBasicStl * m_obj);
    virtual void call_after_run(CBasicStl * m_obj){
    };
    uint8_t m_port_id;
    uint32_t m_profile_id;
};



void CBBStartStopDelay3::call_after_init(CBasicStl * m_obj){

    TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(m_port_id, m_profile_id);
    TrexStatelessDpStop * lpStopCmd1 = new TrexStatelessDpStop(m_port_id, m_profile_id);


    TrexStreamsCompiler compile;

    uint8_t port_id=0;
    uint32_t profile_id=m_profile_id+1;

    std::vector<TrexStream *> streams;

    TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
    stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


    stream1->m_enabled = true;
    stream1->m_self_start = true;
    stream1->m_port_id= port_id;


    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000002);
    pcap.clone_packet_into_stream(stream1);

    streams.push_back(stream1);

    // stream - clean
    std::vector<TrexStreamsCompiledObj *>objs;
    assert(compile.compile(port_id, streams, objs));
    TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, profile_id, 1, objs[0], 10.0 /*sec */ );


    m_obj->m_msg_queue.add_command(m_core,lpStopCmd, 5.0); /* command in delay of 5 sec */
    m_obj->m_msg_queue.add_command(m_core,lpStopCmd1, 7.0);/* command in delay of 7 sec */
    m_obj->m_msg_queue.add_command(m_core,lpStartCmd, 3.5);/* command in delay of 3.5 sec */

    delete stream1 ;


}



/* start/stop/stop back to back */
TEST_F(basic_stl, single_pkt_bb_start_stop_delay3) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop_delay3";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;
     uint32_t profile_id=1;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, profile_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg_queue.add_msg(lpStartCmd);


     CBBStartStopDelay3 sink;
     sink.m_port_id = port_id;
     sink.m_profile_id = profile_id;
     t1.m_sink =  &sink;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}







class CBBStartStopDelay2: public CBasicStlSink {
public:

    virtual void call_after_init(CBasicStl * m_obj);
    virtual void call_after_run(CBasicStl * m_obj){
    };
    uint8_t m_port_id;
};



void CBBStartStopDelay2::call_after_init(CBasicStl * m_obj){

    TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(m_port_id);
    TrexStatelessDpStop * lpStopCmd1 = new TrexStatelessDpStop(m_port_id);


    TrexStreamsCompiler compile;

    uint8_t port_id=0;

    std::vector<TrexStream *> streams;

    TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
    stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


    stream1->m_enabled = true;
    stream1->m_self_start = true;
    stream1->m_port_id= port_id;


    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000002);
    pcap.clone_packet_into_stream(stream1);

    streams.push_back(stream1);

    // stream - clean
    std::vector<TrexStreamsCompiledObj *>objs;
    assert(compile.compile(port_id, streams, objs));
    TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 1, objs[0], 10.0 /*sec */ );


    m_obj->m_msg_queue.add_command(m_core,lpStopCmd, 5.0); /* command in delay of 5 sec */
    m_obj->m_msg_queue.add_command(m_core,lpStopCmd1, 7.0);/* command in delay of 7 sec */
    m_obj->m_msg_queue.add_command(m_core,lpStartCmd, 7.5);/* command in delay of 7 sec */

    delete stream1 ;


}



/* start/stop/stop back to back */
TEST_F(basic_stl, single_pkt_bb_start_stop_delay2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop_delay2";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg_queue.add_msg(lpStartCmd);


     CBBStartStopDelay2 sink;
     sink.m_port_id = port_id;
     t1.m_sink =  &sink;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}







class CBBStartStopDelay1: public CBasicStlSink {
public:

    virtual void call_after_init(CBasicStl * m_obj);
    virtual void call_after_run(CBasicStl * m_obj){
    };
    uint8_t m_port_id;
};



void CBBStartStopDelay1::call_after_init(CBasicStl * m_obj){

    TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(m_port_id);
    TrexStatelessDpStop * lpStopCmd1 = new TrexStatelessDpStop(m_port_id);

    m_obj->m_msg_queue.add_command(m_core,lpStopCmd, 5.0); /* command in delay of 5 sec */
    m_obj->m_msg_queue.add_command(m_core,lpStopCmd1, 7.0);/* command in delay of 7 sec */
}



/* start/stop/stop back to back */
TEST_F(basic_stl, single_pkt_bb_start_stop_delay1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop_delay1";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg_queue.add_msg(lpStartCmd);


     CBBStartStopDelay1 sink;
     sink.m_port_id = port_id;
     t1.m_sink =  &sink;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


/* start/stop/stop back to back */
TEST_F(basic_stl, single_pkt_bb_start_stop3) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop3";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(port_id);
     TrexStatelessDpStop * lpStopCmd1 = new TrexStatelessDpStop(port_id);


     t1.m_msg_queue.add_msg(lpStartCmd);
     t1.m_msg_queue.add_msg(lpStopCmd);
     t1.m_msg_queue.add_msg(lpStopCmd1);

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic_stl, single_pkt_bb_start_stop2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop2";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(port_id);
     TrexStatelessDpStart * lpStartCmd1 = new TrexStatelessDpStart(port_id, 0, objs[0]->clone(), 10.0 /*sec */ );


     t1.m_msg_queue.add_msg(lpStartCmd);
     t1.m_msg_queue.add_msg(lpStopCmd);
     t1.m_msg_queue.add_msg(lpStartCmd1);

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



/* back to back send start/stop */
TEST_F(basic_stl, single_pkt_bb_start_stop) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_bb_start_stop";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     TrexStatelessDpStop * lpStopCmd = new TrexStatelessDpStop(port_id);


     t1.m_msg_queue.add_msg(lpStartCmd);
     t1.m_msg_queue.add_msg(lpStopCmd);

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}




TEST_F(basic_stl, simple_prog4) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_simple_prog4";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;


     /* stream0 */
     TrexStream * stream0 = new TrexStream(TrexStream::stCONTINUOUS, 0,300);
     stream0->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream0->m_enabled = true;
     stream0->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000000);
     pcap.clone_packet_into_stream(stream0);
     streams.push_back(stream0);


     /* stream1 */
     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST, 0,100);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(5);
     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_next_stream_id=200;

     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     /* stream1 */

     TrexStream * stream2 = new TrexStream(TrexStream::stMULTI_BURST, 0,200);
     stream2->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream2->m_isg_usec = 1000000; /*time betwean stream 1 to stream 2 */
     stream2->m_enabled = true;
     stream2->m_self_start = false;
     stream2->set_multi_burst(5,
                              3,
                              2000000.0);

     // next stream is 100 - loop
     stream2->m_next_stream_id=100;


     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000002);
     pcap.clone_packet_into_stream(stream2);
     streams.push_back(stream2);


     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 20.0 /*sec */ );


     t1.m_msg = lpStartCmd;

     bool res=t1.init();

     delete stream0 ;
     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



TEST_F(basic_stl, simple_prog3) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_simple_prog3";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     /* stream1 */
     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST, 0,100);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(5);
     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_next_stream_id=200;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     /* stream1 */

     TrexStream * stream2 = new TrexStream(TrexStream::stMULTI_BURST, 0,200);
     stream2->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream2->m_isg_usec = 1000000; /*time betwean stream 1 to stream 2 */
     stream2->m_enabled = true;
     stream2->m_self_start = false;
     stream2->set_multi_burst(5,
                              3,
                              2000000.0);

     // next stream is 100 - loop
     stream2->m_next_stream_id=100;


     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000002);
     pcap.clone_packet_into_stream(stream2);
     streams.push_back(stream2);


     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 50.0 /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic_stl, simple_prog2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_simple_prog2";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     /* stream1 */
     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST, 0,100);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(5);
     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_next_stream_id=200;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     /* stream1 */

     TrexStream * stream2 = new TrexStream(TrexStream::stSINGLE_BURST, 0,200);
     stream2->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream2->set_single_burst(5);
     stream2->m_isg_usec = 2000000; /*time betwean stream 1 to stream 2 */
     stream2->m_enabled = true;
     stream2->m_self_start = false;

     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000002);
     pcap.clone_packet_into_stream(stream2);
     streams.push_back(stream2);

     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



TEST_F(basic_stl, simple_prog1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_simple_prog1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     /* stream1 */
     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST, 0,100);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(5);
     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_next_stream_id=200;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     /* stream1 */

     TrexStream * stream2 = new TrexStream(TrexStream::stSINGLE_BURST, 0,200);
     stream2->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream2->set_single_burst(5);
     stream2->m_enabled = true;
     stream2->m_self_start = false;

     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000002);
     pcap.clone_packet_into_stream(stream2);
     streams.push_back(stream2);


     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}



TEST_F(basic_stl, single_pkt_burst1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_single_pkt_burst1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST, 0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(5);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}





TEST_F(basic_stl, single_pkt) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_single_stream";

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean

     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

void test_mac_replace(bool replace_src_by_pkt,
                      int replace_dst_mode,
                      std::string file){
    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file = file;

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_override_src_mac_by_pkt_data(replace_src_by_pkt);
     stream1->set_override_dst_mac_mode((TrexStream::stream_dst_mac_t)replace_dst_mode);

     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean

     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic_stl, single_pkt_mac0) {

    test_mac_replace(false,
                     TrexStream::stCFG_FILE,
                     "stl_single_stream_mac0");
}

TEST_F(basic_stl, single_pkt_mac11) {

    test_mac_replace(true,
                     TrexStream::stPKT,
                     "stl_single_stream_mac11");
}

TEST_F(basic_stl, single_pkt_mac10) {

    test_mac_replace(false,
                     TrexStream::stPKT,
                     "stl_single_stream_mac01");
}

TEST_F(basic_stl, single_pkt_mac01) {

    test_mac_replace(true,
                     TrexStream::stCFG_FILE,
                     "stl_single_stream_mac10");
}


TEST_F(basic_stl, multi_pkt1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_multi_pkt1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     TrexStream * stream2 = new TrexStream(TrexStream::stCONTINUOUS,0,1);
     stream2->set_rate(TrexStreamRate::RATE_PPS,2.0);

     stream2->m_enabled = true;
     stream2->m_self_start = true;
     stream2->m_isg_usec = 1000.0; /* 1 msec */
     pcap.update_ip_src(0x20000001);
     pcap.clone_packet_into_stream(stream2);

     streams.push_back(stream2);


     // stream - clean
     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


class CEnableVm {
public:
    void run(bool full_packet,double duration,uint16_t cache );
    CEnableVm() {
        m_pg_id = -1;
    }
public:
    std::string    m_input_packet; //"cap2/udp_64B.pcap"
    std::string    m_out_file;     //"exp/stl_vm_enable0";
    int32_t        m_pg_id; // if >= 0, pg_id for flow stat testing
};

void CEnableVm::run(bool full_packet,double duration=10.0,uint16_t cache=0){
    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file =m_out_file;

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);

     if ( cache ){
         stream1->m_cache_size=cache;
     }

     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);

     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;

     CPcapLoader pcap;
     pcap.load_pcap_file(m_input_packet,0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     uint16_t pkt_size=pcap.m_raw.pkt_len;

     vm_build_program_seq(stream1->m_vm,pkt_size, false);
#if 0
     if ( full_packet ){
         EXPECT_EQ(stream1->m_vm_prefix_size,pkt_size);
     }else{
         EXPECT_EQ(stream1->m_vm_prefix_size,35);
     }
#endif

     if (m_pg_id >= 0) {
         stream1->m_rx_check.m_enabled = true;
         stream1->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_PAYLOAD;
         stream1->m_rx_check.m_pg_id = m_pg_id;
         CFlowStatRuleMgr::instance()->init_stream(stream1);
     } else {
         stream1->m_rx_check.m_enabled = false;
     }

     streams.push_back(stream1);

     // stream - clean
     std::vector<TrexStreamsCompiledObj *> objs;

     assert(compile.compile(port_id,streams, objs) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], duration /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic_stl, vm_enable_cache_10) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable0_cache_10";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.run(true,10.0,100);
}

TEST_F(basic_stl, vm_enable_cache_500) {
    /* multi mbuf cache */
    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable1_cache_500";
    vm_test.m_input_packet = "stl/udp_594B_no_crc.pcap";
    vm_test.run(false,20.0,19);
}


TEST_F(basic_stl, vm_enable0) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable0";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.run(true);
}

#if 0
// todo: does not work with valgrind, because we need to free the dp->rx message queue in simulation mode
TEST_F(basic_stl, vm_enable0_flow_stat) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable0_flow_stat";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.m_pg_id = 5;
    vm_test.run(true);
}
#endif

TEST_F(basic_stl, vm_enable1) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable1";
    vm_test.m_input_packet = "stl/udp_594B_no_crc.pcap";
    vm_test.run(false);
}

#if 0
//todo: does not work. need to check
TEST_F(basic_stl, vm_enable1_flow_stat) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable1_flow_stat";
    vm_test.m_input_packet = "stl/udp_594B_no_crc.pcap";
    vm_test.m_pg_id = 5;
    vm_test.run(false);
}
#endif

TEST_F(basic_stl, vm_enable2) {

    CEnableVm vm_test;
    vm_test.m_out_file = "stl_vm_enable2";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.run(true,50.0);
}




/* check disabled stream with multiplier of 5*/
TEST_F(basic_stl, multi_pkt2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_multi_pkt2";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;


     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);


     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     TrexStream * stream2 = new TrexStream(TrexStream::stCONTINUOUS,0,1);
     stream2->set_rate(TrexStreamRate::RATE_PPS,2.0);

     stream2->m_enabled = false;
     stream2->m_self_start = false;
     stream2->m_isg_usec = 1000.0; /* 1 msec */
     pcap.update_ip_src(0x20000001);
     pcap.clone_packet_into_stream(stream2);

     streams.push_back(stream2);


     // stream - clean
     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs, 1, 5.0));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic_stl, multi_burst1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="stl_multi_burst1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stMULTI_BURST,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_multi_burst(5,
                              3,
                              2000000.0);

     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     uint8_t port_id = 0;
     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpstart = new TrexStatelessDpStart(port_id, 0, objs[0], 40.0 /*sec */ );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

/********************************************* Itay Tests Start *************************************/

/**
 * check that continuos stream does not point to another stream
 * (makes no sense)
 */
TEST_F(basic_stl, compile_bad_1) {

     TrexStreamsCompiler compile;
     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,2);
     stream1->m_enabled = true;
     stream1->set_rate(TrexStreamRate::RATE_PPS,52.0);
     stream1->m_next_stream_id = 3;

     streams.push_back(stream1);

     std::string err_msg;
     std::vector<TrexStreamsCompiledObj *>objs;
     EXPECT_FALSE(compile.compile(0, streams, objs, 1, 1, &err_msg));

     delete stream1;

}

/**
 * check for streams pointing to non existant streams
 *
 * @author imarom (16-Nov-15)
 */
TEST_F(basic_stl, compile_bad_2) {

     TrexStreamsCompiler compile;
     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST,0,1);
     stream1->m_enabled = true;
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(200);

     /* non existant next stream */
     stream1->m_next_stream_id = 5;

     TrexStream * stream2 = new TrexStream(TrexStream::stCONTINUOUS,0,2);
     stream1->set_rate(TrexStreamRate::RATE_PPS,52.0);

     streams.push_back(stream1);
     streams.push_back(stream2);


     uint8_t port_id = 0;
     std::string err_msg;
     std::vector<TrexStreamsCompiledObj *>objs;
     EXPECT_FALSE(compile.compile(port_id, streams, objs, 1, 1, &err_msg));


     delete stream1;
     delete stream2;

}

/**
 * check for "dead streams" in the mesh
 * a streams that cannot be reached
 *
 * @author imarom (16-Nov-15)
 */
TEST_F(basic_stl, compile_bad_3) {

     TrexStreamsCompiler compile;
     std::vector<TrexStream *> streams;
     TrexStream *stream;

     /* stream 1 */
     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 231);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = 5481;
     stream->m_self_start = true;

     streams.push_back(stream);

     /* stream 2 */
     stream = new TrexStream(TrexStream::stCONTINUOUS, 0, 5481);
     stream->m_enabled = true;
     stream->m_next_stream_id = -1;
     stream->m_self_start = false;
     stream->set_rate(TrexStreamRate::RATE_PPS,52.0);

     streams.push_back(stream);

     /* stream 3 */

     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 1928);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = -1;
     stream->m_self_start = true;

     streams.push_back(stream);

      /* stream 4 */

     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 41231);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = 3928;
     stream->m_self_start = false;

     streams.push_back(stream);

     /* stream 5 */

     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 3928);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = 41231;
     stream->m_self_start = false;

     streams.push_back(stream);

     /* compile */
     std::string err_msg;
     std::vector<TrexStreamsCompiledObj *>objs;
     EXPECT_FALSE(compile.compile(0, streams, objs, 1, 1, &err_msg));


     for (auto stream : streams) {
         delete stream;
     }

}

TEST_F(basic_stl, compile_with_warnings) {

    TrexStreamsCompiler compile;
     std::vector<TrexStream *> streams;
     TrexStream *stream;

     /* stream 1 */
     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 231);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = 1928;
     stream->m_self_start = true;

     streams.push_back(stream);

     /* stream 2 */
     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 5481);
     stream->m_enabled = true;
     stream->m_next_stream_id = 1928;
     stream->m_self_start = true;
     stream->set_rate(TrexStreamRate::RATE_PPS,52.0);

     streams.push_back(stream);

     /* stream 3 */

     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 1928);
     stream->m_enabled = true;
     stream->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream->set_single_burst(200);

     stream->m_next_stream_id = -1;
     stream->m_self_start = true;

     streams.push_back(stream);



     /* compile */
     std::string err_msg;
     std::vector<TrexStreamsCompiledObj *>objs;
     EXPECT_TRUE(compile.compile(0, streams, objs, 1, 1, &err_msg));
     delete objs[0];

     EXPECT_TRUE(compile.get_last_compile_warnings().size() == 1);

     for (auto stream : streams) {
         delete stream;
     }

}


TEST_F(basic_stl, compile_good_stream_id_compres) {

     TrexStreamsCompiler compile;
     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST,0,700);
     stream1->m_self_start = true;
     stream1->m_enabled = true;
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(200);

     /* non existant next stream */
     stream1->m_next_stream_id = 800;


     TrexStream * stream2 = new TrexStream(TrexStream::stSINGLE_BURST,0,800);
     stream2->set_rate(TrexStreamRate::RATE_PPS,52.0);
     stream2->m_enabled = true;
     stream2->m_next_stream_id = 700;
     stream2->set_single_burst(300);


     streams.push_back(stream1);
     streams.push_back(stream2);

     uint8_t port_id = 0;
     std::string err_msg;
     std::vector<TrexStreamsCompiledObj *>objs;
     EXPECT_TRUE(compile.compile(port_id, streams, objs, 1, 1, &err_msg));

     printf(" %s \n",err_msg.c_str());

     objs[0]->Dump(stdout);

     EXPECT_EQ_UINT32(objs[0]->get_objects()[0].m_stream->m_stream_id,0);
     EXPECT_EQ_UINT32(objs[0]->get_objects()[0].m_stream->m_next_stream_id,1);

     EXPECT_EQ_UINT32(objs[0]->get_objects()[1].m_stream->m_stream_id,1);
     EXPECT_EQ_UINT32(objs[0]->get_objects()[1].m_stream->m_next_stream_id,0);

     delete objs[0];

     delete stream1;
     delete stream2;

}



class DpToCpHandlerStopEvent: public DpToCpHandler {
public:
    DpToCpHandlerStopEvent(int event_id) {
        m_event_id = event_id;
    }

    virtual void handle(TrexDpToCpMsgBase *msg) {
        /* first the message must be an event */
        TrexDpPortEventMsg *event = dynamic_cast<TrexDpPortEventMsg *>(msg);
        EXPECT_TRUE(event != NULL);

        EXPECT_TRUE(event->get_event_id() == m_event_id);
        EXPECT_TRUE(event->get_port_id() == 0);

    }

private:
    int m_event_id;
};

TEST_F(basic_stl, dp_stop_event) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="ignore";

    TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stSINGLE_BURST,0,0);
     stream1->set_rate(TrexStreamRate::RATE_PPS, 1.0);
     stream1->set_single_burst(100);

     stream1->m_enabled = true;
     stream1->m_self_start = true;
     stream1->m_port_id= port_id;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     // stream - clean

     std::vector<TrexStreamsCompiledObj *>objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 17, objs[0], 10.0 /*sec */ );


     t1.m_msg = lpStartCmd;

     /* let me handle these */
     DpToCpHandlerStopEvent handler(17);
     t1.m_dp_to_cp_handler = &handler;

     bool res=t1.init();
     EXPECT_EQ_UINT32(1, res?1:0);

     delete stream1 ;

}

TEST_F(basic_stl, graph_generator1) {
    std::vector<TrexStream *> streams;
    TrexStreamsGraph graph;
    TrexStream *stream;

     /* stream 1 */
     stream = new TrexStream(TrexStream::stSINGLE_BURST, 0, 1);
     stream->m_enabled = true;
     stream->m_self_start = true;

     stream->m_isg_usec = 42;
     stream->set_rate(TrexStreamRate::RATE_PPS,10);
     stream->set_single_burst(43281);
     stream->m_pkt.len = 512;

     stream->m_next_stream_id = 2;


     streams.push_back(stream);

     /* stream 2 */
     stream = new TrexStream(TrexStream::stMULTI_BURST, 0, 2);
     stream->m_enabled = true;
     stream->m_self_start = false;

     stream->set_rate(TrexStreamRate::RATE_PPS,20);
     stream->set_multi_burst(4918, 13, 7);
     stream->m_next_stream_id = -1;
     stream->m_pkt.len = 64;

     streams.push_back(stream);

     /* stream 3 */
     stream = new TrexStream(TrexStream::stCONTINUOUS, 0, 3);
     stream->m_enabled = true;
     stream->m_self_start = true;

     stream->m_isg_usec = 50;
     stream->set_rate(TrexStreamRate::RATE_PPS,30);
     stream->m_next_stream_id = -1;
     stream->m_pkt.len = 1512;

     streams.push_back(stream);


     const TrexStreamsGraphObj *obj = graph.generate(streams);
     EXPECT_EQ(obj->get_max_bps_l2(), 405120);
     EXPECT_EQ(obj->get_max_pps(), 50);

     for (auto stream : streams) {
         delete stream;
     }

     delete obj;
}


TEST_F(basic_stl, graph_generator2) {
    std::vector<TrexStream *> streams;
    TrexStreamsGraph graph;
    TrexStream *stream;

    /* add some multi burst streams */
    stream = new TrexStream(TrexStream::stMULTI_BURST, 0, 1);
    stream->m_enabled = true;
    stream->m_self_start = true;


    stream->set_rate(TrexStreamRate::RATE_PPS, 1000);

    /* a burst of 2000 packets with a delay of 1 second */
    stream->m_isg_usec = 0;
    stream->set_multi_burst(1500, 500, 1000 * 1000);
    stream->m_pkt.len = 64;

    stream->m_next_stream_id = -1;

    streams.push_back(stream);

    /* another multi burst stream but with a shorter burst ( less 2 ms ) and
       higher ibg (2 ms) , one milli for each side
     */
    stream = new TrexStream(TrexStream::stMULTI_BURST, 0, 2);
    stream->m_enabled = true;
    stream->m_self_start = true;

    stream->set_rate(TrexStreamRate::RATE_PPS,1000);
    stream->m_isg_usec = 1000 * 1000 + 1000;
    stream->set_multi_burst(1500 - 2, 1000, 1000 * 1000 + 2000);
    stream->m_pkt.len = 128;

    stream->m_next_stream_id = -1;

    streams.push_back(stream);

    const TrexStreamsGraphObj *obj = graph.generate(streams);
    EXPECT_EQ(obj->get_max_pps(), 2000.0);

    EXPECT_EQ(obj->get_max_bps_l2(), (1000 * (128 + 4) * 8) + (1000 * (64 + 4) * 8));


    for (auto stream : streams) {
        delete stream;
    }

    delete obj;
}

class VmSplitTest {

public:

    VmSplitTest(const char *erf_filename) {
        m_erf_filename = erf_filename;
        m_stream = NULL;

        pcap.load_pcap_file("cap2/udp_64B.pcap",0);
        pcap.update_ip_src(0x10000001);

    }

    ~VmSplitTest() {
    }

    void set_stream(TrexStream *stream) {

        if (m_stream) {
            delete m_stream;
            m_stream = NULL;
        }

        m_stream = stream;
        m_stream->m_enabled = true;
        m_stream->m_self_start = true;

        pcap.clone_packet_into_stream(stream);
    }

    void set_flow_var_as_split(StreamVmInstructionFlowMan::flow_var_op_e op,
                               uint64_t start,
                               uint64_t end,
                               uint64_t init) {

        assert(m_stream);

        StreamVmInstructionVar *split_instr = new StreamVmInstructionFlowMan("var1",
                                                                             8,
                                                                             op,
                                                                             init,
                                                                             start,
                                                                             end);

        StreamVm &vm = m_stream->m_vm;

        vm.add_instruction(split_instr);

        vm.add_instruction(new StreamVmInstructionWriteToPkt( "var1", 60 - 8 - 4, 0,true));

        vm.add_instruction(new StreamVmInstructionFixChecksumIpv4(14));

    }

    void set_client_var_as_split(uint32_t client_min_value,
                                 uint32_t client_max_value,
                                 uint16_t port_min,
                                 uint16_t port_max) {


        assert(m_stream);

        StreamVmInstructionVar *split_instr = new StreamVmInstructionFlowClient("var1",
                                                                                client_min_value,
                                                                                client_max_value,
                                                                                port_min,
                                                                                port_max,
                                                                                0,
                                                                                0);


        StreamVm &vm = m_stream->m_vm;

        vm.add_instruction(split_instr);

        /* src ip */
        vm.add_instruction(new StreamVmInstructionWriteToPkt( "var1.ip",26, 0,true));
        vm.add_instruction(new StreamVmInstructionFixChecksumIpv4(14));

        /* src port */
        vm.add_instruction(new StreamVmInstructionWriteToPkt("var1.port",34, 0,true));
    }

    void run(uint8_t dp_core_count, uint8_t dp_core_to_check) {
        TrexStreamsCompiler compile;
        std::vector<TrexStreamsCompiledObj *> objs;
        std::vector<TrexStream *> streams;

        if (m_stream->m_vm.is_vm_empty()) {
            set_flow_var_as_split(StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,
                                  0,
                                  1000,
                                  0);
        }

        streams.push_back(m_stream);

        /* compiling for 8 cores */
        assert(compile.compile(0, streams, objs, dp_core_count));

        /* choose one DP object */
        assert(objs[dp_core_to_check]);

        TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(0, 0, objs[dp_core_to_check], 1 /*sec */ );
        objs[dp_core_to_check] = NULL;
        /* free all the non used DP objects */
        for (auto obj : objs) {
            if (obj) {
                delete obj;
            }
        }


        CParserOption * po =&CGlobalInfo::m_options;
        po->preview.setVMode(7);
        po->preview.setFileWrite(true);
        po->out_file = m_erf_filename;

        CBasicStl t1;
        t1.m_msg = lpStartCmd;
        bool res=t1.init();
        EXPECT_EQ_UINT32(1, res?1:0);

    }

private:
    const char *m_erf_filename;
    TrexStream *m_stream;
    CPcapLoader pcap;
};



TEST_F(basic_stl, vm_split_flow_var_inc) {

    VmSplitTest split("stl_vm_split_flow_var_inc.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.run(8, 4);

}

TEST_F(basic_stl, vm_split_flow_var_small_range) {
    /* small range */
    VmSplitTest split("stl_vm_split_flow_var_small_range.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_flow_var_as_split(StreamVmInstructionFlowMan::FLOW_VAR_OP_INC, 0, 1, 0);

    split.run(8, 4);

}

TEST_F(basic_stl, vm_split_flow_var_big_range) {
    VmSplitTest split("stl_vm_split_flow_var_big_range.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_flow_var_as_split(StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC, 1, 1000, 1000);

    split.run(8, 7);


}

TEST_F(basic_stl, vm_split_client_var) {
     VmSplitTest split("stl_vm_split_client_var.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_client_var_as_split(0x10000001, 0x100000fe, 5000, 5050);

    split.run(8, 7);


}

TEST_F(basic_stl, pcap_remote_basic) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="pcap_remote_basic";

    TrexCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(0,
                                                              0,
                                                              "exp/remote_test.cap",
                                                              10,
                                                              0,
                                                              1,
                                                              1,
                                                              -1,
                                                              false);
    t1.m_msg = push_msg;


    bool res = t1.init();
    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic_stl, pcap_remote_loop) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="pcap_remote_loop";

    TrexCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(0,
                                                              0,
                                                              "exp/remote_test.cap",
                                                              1,
                                                              0,
                                                              1,
                                                              3,
                                                              -1,
                                                              false);
    t1.m_msg = push_msg;

    bool res = t1.init();
    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic_stl, pcap_remote_duration) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="pcap_remote_duration";

    TrexCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(0,
                                                              0,
                                                              "exp/remote_test.cap",
                                                              100000,
                                                              0,
                                                              1,
                                                              0,
                                                              0.5,
                                                              false);
    t1.m_msg = push_msg;

    bool res = t1.init();
    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

TEST_F(basic_stl, pcap_remote_dual) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="pcap_remote_dual";

    TrexCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(0,
                                                              0,
                                                              "exp/remote_test_dual.erf",
                                                              10000,
                                                              0,
                                                              1,
                                                              0,
                                                              0.5,
                                                              true);
    t1.m_msg = push_msg;

    bool res = t1.init();
    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}

/********************************************* Itay Tests End *************************************/
class flow_stat_pkt_parse  : public trexStlTest {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};



class flow_stat  : public trexStlTest {
    protected:
     virtual void SetUp() {
         set_op_debug_mode(OP_MODE_STL);
     }
     virtual void TearDown() {
     }
   public:
};


static const uint8_t TEST_L4_PROTO = IPPROTO_TCP;

TEST_F(flow_stat, add_del_stream) {
    return;
    
    uint8_t test_pkt[] = {
        // ether header
        0x74, 0xa2, 0xe6, 0xd5, 0x39, 0x25,
        0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x02,
        0x81, 0x00,
        0x0a, 0xbc, 0x08, 0x00, // vlan
        // IP header
        0x45,0x02,0x00,0x30,
        0x01,0x02,0x40,0x00,
        0xff, TEST_L4_PROTO, 0xbd,0x04,
        0x10,0x0,0x0,0x1,
        0x30,0x0,0x0,0x1,
        // TCP header
        0xab, 0xcd, 0x00, 0x80, // src, dst ports
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // seq num, ack num
        0x50, 0x00, 0xff, 0xff, // Header size, flags, window size
        0x00, 0x00, 0x00, 0x00, // checksum ,urgent pointer
        // some extra bytes
        0x1, 0x2, 0x3, 0x4
    };

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    TrexStream stream2(TrexStream::stSINGLE_BURST, 0, 0);
    TrexStream stream3(TrexStream::stSINGLE_BURST, 0, 0);

    stream.m_rx_check.m_enabled = true;

    stream.m_rx_check.m_rule_type = 7;
    stream.m_rx_check.m_pg_id = 5;
    stream.m_pkt.binary = (uint8_t *)test_pkt;
    stream.m_pkt.len = sizeof(test_pkt);

    CFlowStatRuleMgr::instance()->init_stream(&stream);

    try {
        CFlowStatRuleMgr::instance()->del_stream(&stream);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_NO_STREAMS_EXIST);
    }

    try {
        CFlowStatRuleMgr::instance()->add_stream(&stream);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
    }

    stream.m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_PAYLOAD;
    try {
        CFlowStatRuleMgr::instance()->add_stream(&stream);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
    }

    // change to UDP packet so it will be fine to work with
    test_pkt[27] = IPPROTO_UDP;
    int ret = CFlowStatRuleMgr::instance()->add_stream(&stream);
    assert (ret == 0);

    stream3.m_rx_check.m_enabled = true;
    stream3.m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_PAYLOAD;
    stream3.m_rx_check.m_pg_id = 5; // same as first stream
    stream3.m_pkt.binary = (uint8_t *)test_pkt;
    stream3.m_pkt.len = sizeof(test_pkt);
    try {
        ret = CFlowStatRuleMgr::instance()->add_stream(&stream3);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_DUP_PG_ID);
    }

    ret = CFlowStatRuleMgr::instance()->del_stream(&stream);
    assert (ret == 0);

    stream2.m_rx_check.m_enabled = true;
    stream2.m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_IPV4_ID;
    stream2.m_rx_check.m_pg_id = 5; // same as first stream
    stream2.m_pkt.binary = (uint8_t *)test_pkt;
    stream2.m_pkt.len = sizeof(test_pkt);
    ret = CFlowStatRuleMgr::instance()->add_stream(&stream2);
    assert (ret == 0);

    ret = CFlowStatRuleMgr::instance()->del_stream(&stream2);
    assert (ret == 0);
    try {
        CFlowStatRuleMgr::instance()->del_stream(&stream2);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_DEL_NON_EXIST);
    }

    // do not want the destructor to try to free it
    stream.m_pkt.binary = NULL;
    stream2.m_pkt.binary = NULL;
    stream3.m_pkt.binary = NULL;
}

TEST_F(flow_stat, alloc_mbuf_const) {
     CGenNodeStateless cg;

     cg.alloc_flow_stat_mbuf_test_const();
}

class flow_stat_lat  : public trexStlTest {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};

// test different cases of the function utl_rte_pktmbuf_get_last_bytes
TEST_F(flow_stat_lat, pkt_decode) {
    return;
    uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
    struct flow_stat_payload_header fsp_head;
    fsp_head.magic = 0xab;
    fsp_head.flow_seq = 0xdc;
    fsp_head.hw_id = 0x1234;
    fsp_head.seq = 0x87654321;
    fsp_head.time_stamp = 0x8765432187654321;

    rte_mempool_t * mp1=utl_rte_mempool_create("big-const", 10, 2048, 32, 0, false);

    // case 1 - data split between two mbufs
    rte_mbuf_t * m1 = rte_pktmbuf_alloc(mp1);
    rte_mbuf_t * m2 = rte_pktmbuf_alloc(mp1);
    char *p1 = rte_pktmbuf_append(m1, 10);
    char *p2 = rte_pktmbuf_append(m2, sizeof (struct flow_stat_payload_header) - 10);
    utl_rte_pktmbuf_add_after(m1, m2);

    bcopy((uint8_t *)&fsp_head, p1, 10);
    bcopy(((uint8_t *)&fsp_head) + 10, p2, sizeof (struct flow_stat_payload_header) - 10);

    struct flow_stat_payload_header *fsp_head2 = (flow_stat_payload_header *)
        utl_rte_pktmbuf_get_last_bytes(m1, sizeof(struct flow_stat_payload_header), tmp_buf);

    assert(memcmp(&fsp_head, fsp_head2, sizeof(struct flow_stat_payload_header)) == 0);
    rte_pktmbuf_free(m1);
    rte_pktmbuf_free(m2);

    // case 2 - data contained in last mbuf
    fsp_head.magic = 0xcd;
    m1 = rte_pktmbuf_alloc(mp1);
    m2 = rte_pktmbuf_alloc(mp1);
    p1 = rte_pktmbuf_append(m1, 100);
    p2 = rte_pktmbuf_append(m2, 100);
    utl_rte_pktmbuf_add_after(m1, m2);
    bcopy((uint8_t *)&fsp_head, p2 + m2->data_len - sizeof(fsp_head), sizeof(fsp_head));
    fsp_head2 = (flow_stat_payload_header *)
        utl_rte_pktmbuf_get_last_bytes(m1, sizeof(struct flow_stat_payload_header), tmp_buf);

    assert(memcmp(&fsp_head, fsp_head2, sizeof(struct flow_stat_payload_header)) == 0);

    utl_rte_mempool_delete(mp1);
}

using flow_stat_payload_headers = std::vector<flow_stat_payload_header>;
using flow_stat_payload_headers_ieee_1588 = std::vector<flow_stat_payload_header_ieee_1588>;
using stats_for_pkt = std::tuple<rfc2544_info_t_, CRxCoreErrCntrs>;

stats_for_pkt calculate_stats_for_pkts(
        flow_stat_payload_headers& fs_headers,
        bool rcv_all = true) {
    // TODO: after further refactoring RXLatency::create should be used
    //       to initialize RXLatency instance.
    RXLatency latency_counter;

    // TODO: Creating CRFC2544Info with unpredictable values and then calling
    //       create() to reset the counters is unfortunate and should be fixed.
    CRFC2544Info rfc2544_info;
    rfc2544_info.create();

    CRxCoreErrCntrs error_cntrs;

    latency_counter.m_rcv_all = rcv_all;
    latency_counter.m_rfc2544 = &rfc2544_info;
    latency_counter.m_err_cntrs = &error_cntrs;
    latency_counter.m_ip_id_base = 0;

    std::for_each(fs_headers.begin(), fs_headers.end(),
                  [&latency_counter](flow_stat_payload_header& fsh){
        latency_counter.update_stats_for_pkt(&fsh, 10, 10);
    });

    rfc2544_info_t_ results;
    rfc2544_info.export_data(results);
    return stats_for_pkt(results, error_cntrs);
}

stats_for_pkt calculate_stats_for_pkts_ieee_1588(
        flow_stat_payload_headers_ieee_1588& fs_headers,
        bool rcv_all = true) {
    // TODO: after further refactoring RXLatency::create should be used
    //       to initialize RXLatency instance.
    RXLatency latency_counter;

    // TODO: Creating CRFC2544Info with unpredictable values and then calling
    //       create() to reset the counters is unfortunate and should be fixed.
    CRFC2544Info rfc2544_info;
    rfc2544_info.create();

    CRxCoreErrCntrs error_cntrs;

    latency_counter.m_rcv_all = rcv_all;
    latency_counter.m_rfc2544 = &rfc2544_info;
    latency_counter.m_err_cntrs = &error_cntrs;
    latency_counter.m_ip_id_base = 0;

    std::for_each(fs_headers.begin(), fs_headers.end(),
                  [&latency_counter](flow_stat_payload_header_ieee_1588& fsh){
        latency_counter.update_stats_for_pkt_ieee_1588(&fsh, 10, 10);
    });

    rfc2544_info_t_ results;
    rfc2544_info.export_data(results);
    return stats_for_pkt(results, error_cntrs);
}

TEST(latency_stats, no_duplicates) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers);
    EXPECT_EQ(std::get<0>(results).get_dup_cnt(), 0);
}

TEST(latency_stats, single_duplicate) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers);
    //EXPECT_EQ(std::get<0>(results).get_dup_cnt(), 1);
}

TEST(latency_stats, simple_duplicates) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers);

    // Packet is counted as duplicate only if its seq number is equal to
    // seq number of previous packet.
    //EXPECT_EQ(std::get<0>(results).get_dup_cnt(), 3);
}

TEST(latency_stats, interleaved_duplicates) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 4, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers);

    // Packet is counted as duplicate only if its seq number is equal to
    // seq number of previous packet.
    //EXPECT_EQ(std::get<0>(results).get_dup_cnt(), 0);
}

TEST(latency_stats, no_bad_packets) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers, false);
    //EXPECT_EQ(std::get<1>(results).get, 0);
}

TEST(latency_stats, incorrect_magic) {
    constexpr auto bad_magic = decltype(flow_stat_payload_header::magic)(FLOW_STAT_PAYLOAD_MAGIC) + 1;
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {bad_magic, 0, 0, 0, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {bad_magic, 0, 0, 0, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers, false);
    //EXPECT_EQ(std::get<1>(results).get_bad_header(), 2);
}

TEST(latency_stats, bad_flow) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, MAX_FLOW_STATS_PAYLOAD, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers, false);
    //EXPECT_EQ(std::get<1>(results).get_bad_header(), 1);
}

TEST(latency_stats, out_of_order) {
    flow_stat_payload_headers fs_headers = {
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 4, 0},
    };

    auto results = calculate_stats_for_pkts(fs_headers, false);
    EXPECT_EQ(std::get<0>(results).get_ooo_cnt(), 1);
    EXPECT_EQ(std::get<0>(results).get_seq_err_ev_low(), 1);
}

#define PTP_DUMMY_PKT  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
TEST(latency_stats, out_of_order_ieee_1588) {
    flow_stat_payload_headers_ieee_1588 fs_headers = {
        {PTP_DUMMY_PKT, FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 1, 0},
        {PTP_DUMMY_PKT, FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 3, 0},
        {PTP_DUMMY_PKT, FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 2, 0},
        {PTP_DUMMY_PKT, FLOW_STAT_PAYLOAD_MAGIC, 0, 0, 4, 0},
    };

    auto results = calculate_stats_for_pkts_ieee_1588(fs_headers, false);
    EXPECT_EQ(std::get<0>(results).get_ooo_cnt(), 1);
    EXPECT_EQ(std::get<0>(results).get_seq_err_ev_low(), 1);
}

/**************************** Tagged Packet Grouping *********************************/

class TaggedPktGroupTest : public trexStlTest {
protected:
  virtual void SetUp() { set_op_debug_mode(OP_MODE_STL); }
  virtual void TearDown() {}

public:
    void VALIDATE_STATS_JSON(Json::Value& stats, CTPGTagCntr* tag_cntr) {
        EXPECT_EQ(stats["pkts"].asUInt64(), tag_cntr->m_pkts);
        EXPECT_EQ(stats["bytes"].asUInt64(), tag_cntr->m_bytes);
        EXPECT_EQ(stats["seq_err"].asUInt64(), tag_cntr->m_seq_err);
        EXPECT_EQ(stats["seq_err_too_big"].asUInt64(), tag_cntr->m_seq_err_too_big);
        EXPECT_EQ(stats["seq_err_too_small"].asUInt64(), tag_cntr->m_seq_err_too_small);
        EXPECT_EQ(stats["dup"].asUInt64(), tag_cntr->m_dup);
        EXPECT_EQ(stats["ooo"].asUInt64(), tag_cntr->m_ooo);
    }
};

TEST_F(TaggedPktGroupTest, PacketGroupTag) {
    EXPECT_NO_THROW(Dot1QTag(20, 0));
    EXPECT_THROW(Dot1QTag(0, 0), TrexException);
    EXPECT_NO_THROW(Dot1QTag(4094, 0));
    EXPECT_THROW(Dot1QTag(4095, 0), TrexException);
    EXPECT_THROW(QinQTag(4095, 0, 1), TrexException);
    EXPECT_THROW(QinQTag(0, 0, 1), TrexException);
    EXPECT_THROW(QinQTag(1, 10000, 1), TrexException);
    EXPECT_NO_THROW(QinQTag(1, 1, 1));
    Dot1QTag vlan = Dot1QTag(10, 7);
    EXPECT_EQ(vlan.get_vlan(), 10);
    EXPECT_EQ(vlan.get_tag(), 7);
    QinQTag qinq = QinQTag(20, 30, 3);
    EXPECT_EQ(qinq.get_inner_vlan(), 20);
    EXPECT_EQ(qinq.get_outter_vlan(), 30);
    EXPECT_EQ(qinq.get_tag(), 3);
}

TEST_F(TaggedPktGroupTest, PacketGroupTagMgr) {

    PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
    EXPECT_EQ(tag_mgr->get_num_tags(), 0);
    EXPECT_FALSE(tag_mgr->dot1q_tag_exists(7));
    EXPECT_FALSE(tag_mgr->qinq_tag_exists(20, 30));
    EXPECT_THROW(tag_mgr->get_dot1q_tag(7), TrexException);
    EXPECT_THROW(tag_mgr->get_qinq_tag(20, 30), TrexException);
    EXPECT_TRUE(tag_mgr->add_dot1q_tag(7, 0));       // Vlan 7 - Tag 0
    EXPECT_EQ(tag_mgr->get_num_tags(), 1);
    EXPECT_TRUE(tag_mgr->dot1q_tag_exists(7));
    EXPECT_EQ(tag_mgr->get_dot1q_tag(7), 0);
    EXPECT_FALSE(tag_mgr->add_dot1q_tag(7, 0));      // Add the same vlan again - Fail
    EXPECT_TRUE(tag_mgr->add_qinq_tag(20, 30, 1));   // QinQ(20, 30) - Tag 1
    EXPECT_EQ(tag_mgr->get_num_tags(), 2);
    EXPECT_TRUE(tag_mgr->qinq_tag_exists(20, 30));
    EXPECT_EQ(tag_mgr->get_qinq_tag(20, 30), 1);
    EXPECT_FALSE(tag_mgr->add_qinq_tag(20, 30, 1));  // Add the same QinQ again - Fail
    EXPECT_FALSE(tag_mgr->add_dot1q_tag(10000, 3)); // Fail, this is an invalid Vlan
    EXPECT_FALSE(tag_mgr->add_qinq_tag(1, 10000, 3)); // Fail, this is an invalid QinQ
    EXPECT_FALSE(tag_mgr->add_qinq_tag(10000, 1, 3)); // Fail, this is an invalid QinQ

    PacketGroupTagMgr* cloned = new PacketGroupTagMgr(tag_mgr);
    EXPECT_EQ(cloned->get_num_tags(), 2);
    EXPECT_TRUE(tag_mgr->dot1q_tag_exists(7));
    EXPECT_TRUE(tag_mgr->qinq_tag_exists(20, 30));

    /********************************************
     The following tests can change. At the moment
     we support multiple Dot1Q, QinQ -> Tag Id.
     This way for example we can map Vlan 7, and Vlan 10
     to the same statistics. However this can change
     in the future. Hence I am grouping these tests 
     so they can be easily found and changed.
    ********************************************/
    EXPECT_TRUE(tag_mgr->add_dot1q_tag(10, 0));      // Add another Vlan to a Tag that exists.
    EXPECT_EQ(tag_mgr->get_num_tags(), 3);
    EXPECT_EQ(tag_mgr->get_dot1q_tag(7), 0);
    EXPECT_EQ(tag_mgr->get_dot1q_tag(10), 0);
    EXPECT_EQ(cloned->get_num_tags(), 2);
    EXPECT_FALSE(cloned->dot1q_tag_exists(10));

    delete cloned;
    delete tag_mgr;
}

TEST_F(TaggedPktGroupTest, TPGCpCtx) {
    std::vector<uint8_t> acquired_ports {0, 1};
    std::vector<uint8_t> rx_ports {1};
    std::set<uint8_t> cores {0};
    uint32_t num_tpgids = 20;
    const std::string username = "bdollma";
    TPGCpCtx* tpg_ctx = new TPGCpCtx(acquired_ports, rx_ports, cores, num_tpgids, username);
    EXPECT_EQ(num_tpgids, tpg_ctx->get_num_tpgids());

    const std::vector<uint8_t>& rcv_acq_ports = tpg_ctx->get_acquired_ports();
    ASSERT_EQ(acquired_ports.size(), rcv_acq_ports.size());
    for (int i = 0; i < acquired_ports.size(); ++i) {
        EXPECT_EQ(acquired_ports[i], rcv_acq_ports[i]) << "Vectors acquired_ports and rcv_acq_ports differ at index " << i;
    }

    const std::vector<uint8_t>& rcv_rx_ports = tpg_ctx->get_rx_ports();
    ASSERT_EQ(rx_ports.size(), rcv_rx_ports.size());
    for (int i = 0; i < rx_ports.size(); ++i) {
        EXPECT_EQ(rx_ports[i], rcv_rx_ports[i]) << "Vectors rx_ports and rcv_rx_ports differ at index " << i;
    }
    EXPECT_TRUE(tpg_ctx->is_port_collecting(1));      // Port 1 is collecting
    EXPECT_FALSE(tpg_ctx->is_port_collecting(0));     // Port 2 is not collecting
    EXPECT_FALSE(tpg_ctx->is_port_collecting(3));     // Port 3 is collecting

    EXPECT_EQ(username, tpg_ctx->get_username());

    const std::set<uint8>& rcv_cores = tpg_ctx->get_cores();
    ASSERT_EQ(cores.size(), rcv_cores.size());

    PacketGroupTagMgr* tag_mgr = tpg_ctx->get_tag_mgr();
    EXPECT_EQ(tag_mgr->get_num_tags(), 0);
    EXPECT_FALSE(tag_mgr->dot1q_tag_exists(1));
    EXPECT_TRUE(tag_mgr->add_dot1q_tag(7, 0));

    // Create another TPG context
    acquired_ports = {2, 3};
    rx_ports = {3};
    cores = {1};
    num_tpgids = 2;
    const std::string other_user = "bes";
    TPGCpCtx* second_ctx = new TPGCpCtx(acquired_ports, rx_ports, cores, num_tpgids, other_user);

    EXPECT_EQ(num_tpgids, second_ctx->get_num_tpgids());
    EXPECT_EQ(other_user, second_ctx->get_username());
    EXPECT_TRUE(second_ctx->is_port_collecting(3));
    EXPECT_FALSE(second_ctx->is_port_collecting(2));
    EXPECT_FALSE(second_ctx->is_port_collecting(1));

    tag_mgr = second_ctx->get_tag_mgr();
    EXPECT_EQ(tag_mgr->get_num_tags(), 0);

    delete tpg_ctx;
    delete second_ctx;
}

TEST_F(TaggedPktGroupTest, TPGStreamMgr) {

    uint32_t tpgid = 7;

    uint8_t test_short_pkt[] = {
        // Ether header
        0x24, 0x8a, 0x07, 0x14, 0xfc, 0x59, // Dst Mac
        0x24, 0x8a, 0x07, 0x14, 0xfc, 0x58, // Src Mac
        0x81, 0x00,                         // Eth Type
        // QinQ Header
        0x00, 0x14, 0x81, 0x00, // Vlan 20
        0x00, 0x1e, 0x08, 0x00, // Vlan 30
        // IP Header
        0x45, 0x00, 0x00, 0x24, // Version, Header Length, DSCP, Length
        0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment
        0x40, 0x11, 0x3a, 0xbf, // TTL, Protocol, Checksum
        0x10, 0x00, 0x00, 0x01, // Src Ip
        0x30, 0x00, 0x00, 0x01, // Dst Ip
        // UDP Header
        0x04, 0x01, 0x00, 0x0c, // Src Port, Dst Port
        0x00, 0x0a, 0x18, 0x0e,  // Length, Checksum
        // TPG Header in Half
        0xc1, 0x5c, 0x0b, 0xe5, 0x07, 0x00, 0x00, 0x00
    };

    uint8_t test_pkt[] = {
        // Ether header
        0x24, 0x8a, 0x07, 0x14, 0xfc, 0x59, // Dst Mac
        0x24, 0x8a, 0x07, 0x14, 0xfc, 0x58, // Src Mac
        0x81, 0x00,                         // Eth Type
        // QinQ Header
        0x00, 0x14, 0x81, 0x00, // Vlan 20
        0x00, 0x1e, 0x08, 0x00, // Vlan 30
        // IP Header
        0x45, 0x00, 0x00, 0x2c, // Version, Header Length, DSCP, Length
        0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment
        0x40, 0x11, 0x3a, 0xbf, // TTL, Protocol, Checksum
        0x10, 0x00, 0x00, 0x01, // Src Ip
        0x30, 0x00, 0x00, 0x01, // Dst Ip
        // UDP Header
        0x04, 0x01, 0x00, 0x0c, // Src Port, Dst Port
        0x00, 0x18, 0x18, 0x0e,  // Length, Checksum
        // TPG Header
        0xc1, 0x5c, 0x0b, 0xe5, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.m_rx_check.m_enabled = true;
    stream.m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_TPG_PAYLOAD;
    stream.m_rx_check.m_pg_id = tpgid;
    stream.m_pkt.binary = (uint8_t *)test_short_pkt;
    stream.m_pkt.len = sizeof(test_short_pkt);

    // Delete non existing stream
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->del_stream(&stream);
        } catch (TrexFStatEx& e) {
            // Validate the type and throw again
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_NON_EXIST_ID);
            throw;
        }
    }, TrexFStatEx);

    // Stop non existing stream
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->stop_stream(&stream);
        } catch (TrexFStatEx& e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_NON_EXIST_ID);
            throw;
        }
    }, TrexFStatEx);

    // Reset non existing stream
    try {
        TPGStreamMgr::instance()->reset_stream(&stream);
    } catch (TrexFStatEx &e) {
        assert(e.type() == TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    // Add stream without creating TPG Context.
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_TPG_NOT_ENABLED);
            throw;
        }
    }, TrexFStatEx);

    // Add context but don't enable yet
    std::vector<uint8_t> acquired_ports = {0, 1};
    std::vector<uint8_t> rx_ports = {0, 1};
    std::set<uint8_t> cores = {0};
    uint32_t num_tpgids = 5;
    std::string username = "bdollma";
    TPGCpCtx* tpg_ctx = new TPGCpCtx(acquired_ports, rx_ports, cores, num_tpgids, username);
    TrexStateless* stl = get_stateless_obj();
    stl->get_port_by_id(0)->set_tpg_ctx(tpg_ctx);

    // TPG is not enabled yet
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_TPG_NOT_ENABLED);
            throw;
        }
    }, TrexFStatEx);

    tpg_ctx->set_tpg_state(TPGState::ENABLED); // Enable TPG

    // Num of tpgids is too small
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_INVALID_TPGID);
            throw;
        }
    }, TrexFStatEx);

    num_tpgids = tpgid+1;
    delete tpg_ctx;
    tpg_ctx = new TPGCpCtx(acquired_ports, rx_ports, cores, num_tpgids, username);
    tpg_ctx->set_tpg_state(TPGState::ENABLED);
    stl->get_port_by_id(0)->set_tpg_ctx(tpg_ctx);

    // Stream too short
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
            throw;
        }
    }, TrexFStatEx);

    // Set the packet binary to a long packet
    stream.m_pkt.binary = (uint8_t *)test_pkt;
    stream.m_pkt.len = sizeof(test_pkt);

    // Stream added successfully
    TPGStreamMgr::instance()->add_stream(&stream);

    // Add again
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_ALREADY_EXIST);
            throw;
        }
    }, TrexFStatEx);

    // Change stream id
    stream.m_stream_id += 1;

    // Same TPGID
    EXPECT_THROW({
        try {
            TPGStreamMgr::instance()->add_stream(&stream);
        } catch (TrexFStatEx &e) {
            EXPECT_EQ(e.type(), TrexException::T_FLOW_STAT_DUP_PG_ID);
            throw;
        }
    }, TrexFStatEx);

    // Reset stream id
    stream.m_stream_id -= 1;

    TPGStreamMgr::instance()->del_stream(&stream);
    TPGStreamMgr::instance()->add_stream(&stream);
    TPGStreamMgr::instance()->stop_stream(&stream);
    TPGStreamMgr::instance()->del_stream(&stream);

    // Do not want the destructor to try to free it
    stream.m_pkt.binary = NULL;

    delete tpg_ctx;
}

TEST_F(TaggedPktGroupTest, TPGDpMgrPerSide) {
    uint32_t num_tpgids = 3;
    TPGDpMgrPerSide* dp_mgr = new TPGDpMgrPerSide(num_tpgids);
    for (int i = 0; i < num_tpgids; ++i) {
    EXPECT_EQ(dp_mgr->get_seq(i), 0);
    }

    dp_mgr->inc_seq(1);
    EXPECT_EQ(dp_mgr->get_seq(1), 1);
    EXPECT_EQ(dp_mgr->get_seq(0), 0);

    for (int i = 0; i < 10; ++i) {
        dp_mgr->inc_seq(2);
    }

    EXPECT_EQ(dp_mgr->get_seq(0), 0);
    EXPECT_EQ(dp_mgr->get_seq(1), 1);
    EXPECT_EQ(dp_mgr->get_seq(2), 10);

    /***********************************************
     This functionality can change in the future.
     At the moment, if someone tries to increment the
     sequence on a tpgid that is not in range, we
     ignore.
    ************************************************/
    EXPECT_NO_THROW(dp_mgr->inc_seq(3));
    EXPECT_NO_THROW(dp_mgr->get_seq(3));
    EXPECT_EQ(dp_mgr->get_seq(3), 0);

    delete dp_mgr;
}

class TPGTagCntrTest : public TaggedPktGroupTest {
    /**
     * This class is defined as a friend of CTPGTagCntr.
    **/

public:

    void TestUpdateInOrder() {
        /**
         * First few packets are received in order.
        **/
        CTPGTagCntr expected = CTPGTagCntr();
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        tag_cntr.update_cntrs(0, 100); // First packet, 100 bytes
        expected.set_cntrs(1, 104, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(1, 200); // Second packet, 200 bytes
        expected.set_cntrs(2, 308, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
    }

    void TestMissFirstPackets() {
        /**
         * Test the case we miss the first packets
        **/
        CTPGTagCntr expected = CTPGTagCntr();
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        tag_cntr.update_cntrs(10, 100);  // Start at 10th packet
        expected.set_cntrs(1, 104, 10, 1, 0, 0, 0); // Seq too big == 1, Seq Err = 10
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(11, 200); // Second packet, 200 bytes
        expected.set_cntrs(2, 308, 10, 1, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
    }

    void TestOutOfOrderDupPackets() {
        /**
         * Test the case of a packet received twice out of order.
        **/
        CTPGTagCntr expected = CTPGTagCntr();
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        tag_cntr.update_cntrs(0, 100);
        expected.set_cntrs(1, 104, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(1, 200); // Second packet, 200 bytes
        expected.set_cntrs(2, 308, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(0, 100); // First Packet is received again
        expected.set_cntrs(3, 412, 0, 0, 1, 0, 1); // one ooo and one_too_small
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(2, 100); // Third Packet
        expected.set_cntrs(4, 516, 0, 0, 1, 0, 1);
        EXPECT_EQ(expected, tag_cntr);
    }

    void TestOutOfOrderPackets() {
        /**
         * Test the case of a packet received out of order
        **/
        CTPGTagCntr expected = CTPGTagCntr();
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        tag_cntr.update_cntrs(0, 100);
        expected.set_cntrs(1, 104, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(3, 200); // Fourth packet, 200 bytes
        expected.set_cntrs(2, 308, 2, 1, 0, 0, 0); // seq err = 2, seq_err_too_big = 1
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(1, 100); // Second Packet is received
        expected.set_cntrs(3, 412, 1, 1, 1, 0, 1); // seq err = 1, seq_err_too_big = 1, seq_err_too_small = 1, ooo = 1
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(2, 100); // Third Packet
        expected.set_cntrs(4, 516, 0, 1, 2, 0, 2); //  seq_err_too_big = 1, seq_err_too_small = 2, ooo = 2
        EXPECT_EQ(expected, tag_cntr);
    }

    void TestDupPackets() {
        /**
         * Test the case of a duplicate packet.
        **/
        CTPGTagCntr expected = CTPGTagCntr();
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        tag_cntr.update_cntrs(0, 100);  // Start at 10th packet
        expected.set_cntrs(1, 104, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(1, 200); // Second packet, 200 bytes
        expected.set_cntrs(2, 308, 0, 0, 0, 0, 0);
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(1, 100); // First Packet is received again
        expected.set_cntrs(3, 412, 0, 0, 1, 1, 0); // One dup, one err too low
        EXPECT_EQ(expected, tag_cntr);
        tag_cntr.update_cntrs(2, 100); // Third Packet
        expected.set_cntrs(4, 516, 0, 0, 1, 1, 0);
        EXPECT_EQ(expected, tag_cntr);
    }

    void TestDump() {
        /**
         * Test the JSON dumping.
        **/
        CTPGTagCntr tag_cntr = CTPGTagCntr();
        Json::Value stats;
        tag_cntr.set_cntrs(150, 251051, 2, 31, 25, 1, 5);
        tag_cntr.dump_json(stats);
        VALIDATE_STATS_JSON(stats, &tag_cntr);
    }
};

TEST_F(TPGTagCntrTest, RecvSeqLogicTest) {
    TestUpdateInOrder();
    TestMissFirstPackets();
    TestOutOfOrderDupPackets();
    TestOutOfOrderPackets();
    TestDupPackets();
    TestDump();
}

class TPGRxStatsTest : public TaggedPktGroupTest {
    /**
     * This class is defined as a friend of CTPGTagCntr.
    **/
public:

    void TestSanity() {
        uint8_t port_id = 0;
        uint32_t num_tpgids = 10;
        uint16_t num_tags = 25;

        PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
        for (int i = 1; i <= num_tags; i++) {
            tag_mgr->add_dot1q_tag(i, i-1);
        }

        CTPGTagCntr* port_cntr = (CTPGTagCntr*)calloc((num_tags + 1) * num_tpgids, sizeof(CTPGTagCntr)); // 25 Tags + 1 Unknown , 10 TPGIDS
        RxTPGPerPort* rx_tpg_port = new RxTPGPerPort(port_id, num_tpgids, tag_mgr, port_cntr);

        uint16_t tpgid = 3;
        uint16_t tag = 6;
        rx_tpg_port->update_cntrs(tpgid, 0, 60, tag, true); // Tag 6
        rx_tpg_port->update_cntrs(tpgid, 0, 72, 0, false); // Unknown
        tag = 24;
        rx_tpg_port->update_cntrs(tpgid, 0, 80, tag, true); // Tag 24


        Json::Value stats;
        rx_tpg_port->get_tpg_stats(stats, tpgid, 0, num_tags, true);
        rx_tpg_port->get_tpg_stats(stats, tpgid, 6, num_tags, false);
        rx_tpg_port->get_tpg_stats(stats, tpgid, 7, num_tags, true);
        rx_tpg_port->get_tpg_stats(stats, tpgid, 24, num_tags, true);


        CTPGTagCntr exp = CTPGTagCntr();
        VALIDATE_STATS_JSON(stats[to_string(tpgid)]["0-5"], &exp);
        VALIDATE_STATS_JSON(stats[to_string(tpgid)]["7-23"], &exp);
        exp.set_cntrs(1, 64, 0, 0, 0, 0, 0);
        VALIDATE_STATS_JSON(stats[to_string(tpgid)]["6"], &exp);
        exp.set_cntrs(1, 84, 0, 0, 0, 0, 0);
        VALIDATE_STATS_JSON(stats[to_string(tpgid)]["24"], &exp);
        exp.set_cntrs(1, 76, 0, 0, 0, 0, 0);
        VALIDATE_STATS_JSON(stats[to_string(tpgid)]["unknown_tag"], &exp);

        tpgid = 0;
        tag = 0;
        rx_tpg_port->update_cntrs(tpgid, 0, 60, tag, true); // Tag 0

        CTPGTagCntr* tag_ptr = rx_tpg_port->get_tag_cntr(tpgid, tag);
        exp.set_cntrs(1, 64, 0, 0, 0, 0, 0);
        EXPECT_EQ(*tag_ptr, exp);

        tpgid = 9;
        rx_tpg_port->update_cntrs(tpgid, 0, 60, 0, false); // Unknown
        tag_ptr = rx_tpg_port->get_unknown_tag_cntr(tpgid);
        EXPECT_EQ(*tag_ptr, exp);

        tag_ptr = rx_tpg_port->get_tag_cntr(5, 30);
        EXPECT_EQ(nullptr, tag_ptr);

        tag_ptr = rx_tpg_port->get_tag_cntr(15, 2);
        EXPECT_EQ(nullptr, tag_ptr);

        delete(rx_tpg_port);
        free(port_cntr);
        delete tag_mgr;

    }

    void TestUnknownTags() {
        uint8_t port_id = 0;
        uint32_t num_tpgids = 2;
        uint16_t num_tags = 2;

        PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
        tag_mgr->add_qinq_tag(20, 30, 0);    // QinQ (20, 30) = Tag 0
        tag_mgr->add_dot1q_tag(7, 1); // Dot1Q(7) = Tag 1

        CTPGTagCntr* port_cntr = (CTPGTagCntr*)calloc((num_tags + 1) * num_tpgids, sizeof(CTPGTagCntr)); // 2 Tags + 1 Unknown , 2 TPGIDS
        RxTPGPerPort* rx_tpg_port = new RxTPGPerPort(port_id, num_tpgids, tag_mgr, port_cntr);


        uint16_t tpgid = 1;
        // Sequence 0 and 1 is received with unknown tag

        rx_tpg_port->update_cntrs(tpgid, 0, 60, 0, false);
        rx_tpg_port->update_cntrs(tpgid, 1, 60, 0, false);

        Json::Value stats;
        rx_tpg_port->get_tpg_stats(stats, tpgid, 0, 2, true);
        stats = stats[std::to_string(tpgid)];

        CTPGTagCntr exp = CTPGTagCntr();
        VALIDATE_STATS_JSON(stats["0-1"], &exp);

        exp.set_cntrs(2, 128, 0, 0, 0, 0, 0);
        VALIDATE_STATS_JSON(stats["unknown_tag"], &exp);

        delete rx_tpg_port;
        free(port_cntr);
    }

    void TestWithSomeErrors() {
        uint8_t port_id = 0;
        uint32_t num_tpgids = 10;
        uint16_t num_tags = 25;

        PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
        for (int i = 1; i <= num_tags; i++) {
            tag_mgr->add_dot1q_tag(i, i-1);
        }

        CTPGTagCntr* port_cntr = (CTPGTagCntr*)calloc((num_tags + 1) * num_tpgids, sizeof(CTPGTagCntr)); // 25 Tags + 1 Unknown , 10 TPGIDS
        RxTPGPerPort* rx_tpg_port = new RxTPGPerPort(port_id, num_tpgids, tag_mgr, port_cntr);

        uint16_t tpgid = 2;
        rx_tpg_port->update_cntrs(tpgid, 0, 60,  2, true); // Tag 2
        rx_tpg_port->update_cntrs(tpgid, 1, 100, 2, true);
        rx_tpg_port->update_cntrs(tpgid, 1, 60,  2, true);
        rx_tpg_port->update_cntrs(tpgid, 2, 100, 2, true);

        rx_tpg_port->update_cntrs(tpgid, 0, 60,  3, true);  // Tag 3
        rx_tpg_port->update_cntrs(tpgid, 1, 100, 3, true);
        rx_tpg_port->update_cntrs(tpgid, 3, 60,  3, true);
        rx_tpg_port->update_cntrs(tpgid, 2, 100, 3, true);

        CTPGTagCntr exp = CTPGTagCntr();
        Json::Value stats;
        rx_tpg_port->get_tpg_stats(stats, tpgid, 0, num_tags, false);
        Json::Value& tpgid_stats = stats[std::to_string(tpgid)];

        VALIDATE_STATS_JSON(tpgid_stats["0-1"], &exp); // Compressed the first ones.

        rx_tpg_port->get_tpg_stats(stats, tpgid, 2, num_tags, false);
        exp.set_cntrs(4, 336, 0, 0, 1, 1, 0);
        VALIDATE_STATS_JSON(tpgid_stats["2"], &exp);

        rx_tpg_port->get_tpg_stats(stats, tpgid, 3, num_tags, false);
        exp.set_cntrs(4, 336, 0, 1, 1, 0, 1);
        VALIDATE_STATS_JSON(tpgid_stats["3"], &exp);

        rx_tpg_port->get_tpg_stats(stats, tpgid, 4, num_tags, false);
        exp.set_cntrs(0, 0, 0, 0, 0, 0, 0);
        VALIDATE_STATS_JSON(tpgid_stats["4-9"], &exp);

        delete rx_tpg_port;
        free(port_cntr);
    }

    void TestRxHandleValidTag() {

        /**
         * Short Sanity Check for TPG Rx Handle
         **/

        uint8_t port_id = 0;
        uint32_t num_tpgids = 10;
        uint16_t num_tags = 1;
        PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
        tag_mgr->add_qinq_tag(20, 30, 0);    // QinQ (20, 30) = Tag 0

        CTPGTagCntr* port_cntr = (CTPGTagCntr*)calloc((num_tags + 1) * num_tpgids, sizeof(CTPGTagCntr)); // 1 Tag + 1 Unknown * 10 tpgid

        RxTPGPerPort* rx_tpg_port = new RxTPGPerPort(port_id, num_tpgids, tag_mgr, port_cntr);

        uint8_t test_pkt[] = {
            // Ether header
            0x24, 0x8a, 0x07, 0x14, 0xfc, 0x59, // Dst Mac
            0x24, 0x8a, 0x07, 0x14, 0xfc, 0x58, // Src Mac
            0x81, 0x00,                         // Eth Type
            // QinQ Header
            0x00, 0x14, 0x81, 0x00, // Vlan 20
            0x00, 0x1e, 0x08, 0x00, // Vlan 30
            // IP Header
            0x45, 0x00, 0x00, 0x2c, // Version, Header Length, DSCP, Length
            0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment
            0x40, 0x11, 0x3a, 0xbf, // TTL, Protocol, Checksum
            0x10, 0x00, 0x00, 0x01, // Src Ip
            0x30, 0x00, 0x00, 0x01, // Dst Ip
            // UDP Header
            0x04, 0x01, 0x00, 0x0c, // Src Port, Dst Port
            0x00, 0x18, 0x18, 0x0e,  // Length, Checksum
            // TPG Header
            0xc1, 0x5c, 0x0b, 0xe5, 0x00, 0x00, 0x00, 0x00, // Magic, TPGID = 0
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Seq = 3, Reserved = 0
        };

        rte_mempool_t* mp1 = utl_rte_mempool_create("big-const", sizeof(test_pkt), 2048, 32, 0, false);
        rte_mbuf_t* m1 = rte_pktmbuf_alloc(mp1);
        char* p1 = rte_pktmbuf_append(m1, sizeof(test_pkt));
        bcopy(test_pkt, p1, sizeof(test_pkt));
        EXPECT_EQ(m1->pkt_len, sizeof(test_pkt));

        uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
        struct tpg_payload_header* tpg_header = (tpg_payload_header*)
            utl_rte_pktmbuf_get_last_bytes(m1, sizeof(struct tpg_payload_header), tmp_buf);

        EXPECT_EQ(tpg_header->tpgid, 0);
        EXPECT_EQ(tpg_header->seq, 3);

        rx_tpg_port->handle_pkt(m1);

        rte_pktmbuf_free(m1);

        // let's see the stats are successfully parsed

        Json::Value stats;
        rx_tpg_port->get_tpg_stats(stats, tpg_header->tpgid, 0, 1, false);

        Json::Value& tpgid_stats = stats[std::to_string(tpg_header->tpgid)];
        Json::Value& tag_stats = tpgid_stats[std::to_string(tag_mgr->get_qinq_tag(20, 30))];

        CTPGTagCntr exp = CTPGTagCntr();
        exp.set_cntrs(1, sizeof(test_pkt) + 4, 3, 1, 0, 0, 0);
        VALIDATE_STATS_JSON(tag_stats, &exp);

        delete rx_tpg_port;
        free(port_cntr);
        delete tag_mgr;
    }

    void TestRxHandleInvalidTag() {

        /**
         * Short Check if an unkown tag is received.
         **/

        uint8_t port_id = 0;
        uint32_t num_tpgids = 10;
        uint16_t num_tags = 1;
        PacketGroupTagMgr* tag_mgr = new PacketGroupTagMgr();
        tag_mgr->add_dot1q_tag(7, 0);    // Vlan 7 - Tag 0

        CTPGTagCntr* port_cntr = (CTPGTagCntr*)calloc((num_tags + 1) * num_tpgids, sizeof(CTPGTagCntr)); // 1 Tag + 1 Unknown * 10 tpgid

        RxTPGPerPort* rx_tpg_port = new RxTPGPerPort(port_id, num_tpgids, tag_mgr, port_cntr);

        uint8_t test_pkt[] = {
            // Ether header
            0x24, 0x8a, 0x07, 0x14, 0xfc, 0x59, // Dst Mac
            0x24, 0x8a, 0x07, 0x14, 0xfc, 0x58, // Src Mac
            0x81, 0x00,                         // Eth Type
            // QinQ Header
            0x00, 0x14, 0x81, 0x00, // Vlan 20
            0x00, 0x1e, 0x08, 0x00, // Vlan 30
            // IP Header
            0x45, 0x00, 0x00, 0x2c, // Version, Header Length, DSCP, Length
            0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment
            0x40, 0x11, 0x3a, 0xbf, // TTL, Protocol, Checksum
            0x10, 0x00, 0x00, 0x01, // Src Ip
            0x30, 0x00, 0x00, 0x01, // Dst Ip
            // UDP Header
            0x04, 0x01, 0x00, 0x0c, // Src Port, Dst Port
            0x00, 0x18, 0x18, 0x0e,  // Length, Checksum
            // TPG Header
            0xc1, 0x5c, 0x0b, 0xe5, 0x00, 0x00, 0x00, 0x00, // Magic, TPGID = 0
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Seq = 3, Reserved = 0
        };

        rte_mempool_t* mp1 = utl_rte_mempool_create("big-const", sizeof(test_pkt), 2048, 32, 0, false);
        rte_mbuf_t* m1 = rte_pktmbuf_alloc(mp1);
        char* p1 = rte_pktmbuf_append(m1, sizeof(test_pkt));
        bcopy(test_pkt, p1, sizeof(test_pkt));
        EXPECT_EQ(m1->pkt_len, sizeof(test_pkt));

        uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
        struct tpg_payload_header* tpg_header = (tpg_payload_header*)
            utl_rte_pktmbuf_get_last_bytes(m1, sizeof(struct tpg_payload_header), tmp_buf);

        EXPECT_EQ(tpg_header->tpgid, 0);
        EXPECT_EQ(tpg_header->seq, 3);

        rx_tpg_port->handle_pkt(m1);

        rte_pktmbuf_free(m1);

        // let's see the stats are successfully parsed

        Json::Value stats;
        rx_tpg_port->get_tpg_stats(stats, tpg_header->tpgid, 0, 1, true);

        Json::Value& tpgid_stats = stats[std::to_string(tpg_header->tpgid)];
        Json::Value& tag_stats = tpgid_stats[std::to_string(tag_mgr->get_dot1q_tag(7))];
        Json::Value& unknown_stats = tpgid_stats["unknown_tag"];

        CTPGTagCntr exp = CTPGTagCntr();
        VALIDATE_STATS_JSON(tag_stats, &exp);
        exp.set_cntrs(1, sizeof(test_pkt) + 4, 3, 1, 0, 0, 0);
        VALIDATE_STATS_JSON(unknown_stats, &exp);

        delete rx_tpg_port;
        free(port_cntr);
        delete tag_mgr;
    }

};

TEST_F(TPGRxStatsTest, PortCntrTest) {
    TestSanity();
    TestUnknownTags();
    TestWithSomeErrors();
    TestRxHandleValidTag();
    TestRxHandleInvalidTag();
}
