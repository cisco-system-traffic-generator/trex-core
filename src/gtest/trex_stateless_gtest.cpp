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

#include "bp_sim.h"
#include "flow_stat_parser.h"
#include <common/gtest.h>
#include <common/basic_utils.h>
#include <trex_stateless.h>
#include <trex_stateless_dp_core.h>
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>
#include <trex_stream_node.h>
#include <trex_stream.h>
#include <trex_stateless_port.h>
#include <trex_rpc_server_api.h>
#include <iostream>
#include <vector>




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




class basic_vm  : public testing::Test {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};




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


/* start/stop/stop back to back */
TEST_F(basic_vm, vm0) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(20) );
    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",8,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0,1,7 ) 
                        );
    vm.add_instruction( new StreamVmInstructionWriteToPkt( "var1",14, 0,true)
                        );

    vm.Dump(stdout);
}

TEST_F(basic_vm, vm1) {

    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0,1,7 ) 
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

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm6.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm6.pcap","exp/udp_64B_vm6-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
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

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm7.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm7.pcap","exp/udp_64B_vm7-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


////////////////////////////////////////////////////////

TEST_F(basic_vm, vm_mask_err) {

    bool fail=false;
    /* should fail */

    try {
        StreamVm vm;
        vm.add_instruction( new StreamVmInstructionFixChecksumIpv4(14) );
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


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00ff,0,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask1.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask1.pcap","exp/udp_64B_vm_mask1-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_mask2) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000007,0x10000007,0x100000fe)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0xff00,8,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask2.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask2.pcap","exp/udp_64B_vm_mask2-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask3) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,0x10000007,0x10000007,0x100000fe)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,1,0x2,1,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask3.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask3.pcap","exp/udp_64B_vm_mask3-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask4) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,10)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0xFF00,8,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask4.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask4.pcap","exp/udp_64B_vm_mask4-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}

TEST_F(basic_vm, vm_mask5) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",1 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,10)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,4,0x00FF0000,16,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask5.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask5.pcap","exp/udp_64B_vm_mask5-ex.pcap");
    EXPECT_EQ(1, res1?1:0);
}


TEST_F(basic_vm, vm_mask6) {



    StreamVm vm;

    vm.add_instruction( new StreamVmInstructionFlowMan( "var1",4 /* size */,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_INC,1,1,20)  );


    vm.add_instruction( new StreamVmInstructionWriteMaskToPkt("var1", 36,2,0x00FF,-1,1) );

    vm.compile(128);


    uint32_t program_size=vm.get_dp_instruction_buffer()->get_program_size();

    printf (" program size : %lu \n",(ulong)program_size);


    vm.Dump(stdout);

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm_mask6.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm_mask6.pcap","exp/udp_64B_vm_mask6-ex.pcap");
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

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm8.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm8.pcap","exp/udp_64B_vm8-ex.pcap");
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

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm9.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm9.pcap","exp/udp_64B_vm9-ex.pcap");
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

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/udp_64B_vm9.pcap");
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

    bool res1=cmp.compare("exp/udp_64B_vm9.pcap","exp/udp_64B_vm9-ex.pcap");
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
                                                        0,
                                                        1025,
                                                        65000));

    vm.add_instruction( new StreamVmInstructionFlowMan( "dst_port",
                                                        2,
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        0,
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
    pcap.load_pcap_file("stl/yaml/syn_packet.pcap",0);

    

    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(LIBPCAP,(char *)"exp/stl_syn_attack.pcap");
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

    bool res1=cmp.compare("exp/stl_syn_attack.pcap","exp/stl_syn_attack-ex.pcap");
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

    std::string out_file_full ="exp/"+out_file_name +".pcap";
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
                                                        127,
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

    run_vm_program(vm,"stl/yaml/udp_1518B_no_crc.pcap","stl_vm_inc_size_64_128",20);
}

TEST_F(basic_vm, vm_random_size_64_128) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size 
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        0,
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


    run_vm_program(vm,"stl/yaml/udp_1518B_no_crc.pcap","stl_vm_rand_size_64_128",20);

}



/* should have exception packet size is smaller than range */
TEST_F(basic_vm, vm_random_size_64_127_128) {

    StreamVm vm;
    srand(0x1234);

    vm.add_instruction( new StreamVmInstructionFlowMan( "rand_pkt_size_var",
                                                        2,                // size var  must be 16bit size 
                                                        StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM,
                                                        127,
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
       run_vm_program(vm,"stl/yaml/udp_64B_no_crc.pcap","stl_vm_rand_size_64B_127_128",20);
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

    run_vm_program(vm,"stl/yaml/udp_594B_no_crc.pcap","stl_vm_rand_size_512B_64_128",10);

}



//////////////////////////////////////////////////////

                                           
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
    void add_msg(TrexStatelessCpToDpMsgBase * msg){
        m_msgs.push_back(msg);
    }

    void add_command(CBasicStlDelayCommand & command){
        m_commands.push_back(command);
    }

        /* only if both port are idle we can exit */
    void add_command(CFlowGenListPerThread   * core,
                     TrexStatelessCpToDpMsgBase * msg, 
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
    std::vector<TrexStatelessCpToDpMsgBase *> m_msgs;

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
    virtual void handle(TrexStatelessDpToCpMsgBase *msg) = 0;
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

            TrexStatelessDpToCpMsgBase * msg = (TrexStatelessDpToCpMsgBase *)node;
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
        sprintf(buf,"%s-%d.erf",CGlobalInfo::m_options.out_file.c_str(),0);
        sprintf(buf_ex,"%s-%d-ex.erf",CGlobalInfo::m_options.out_file.c_str(),0);

        lpt->start_stateless_simulation_file(buf,CGlobalInfo::m_options.preview);

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

        lpt->start_stateless_daemon_simulation();
        lpt->stop_stateless_simulation_file();

        //lpt->m_node_gen.DumpHist(stdout);

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

    TrexStatelessCpToDpMsgBase * m_msg;
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
};



void CBBStartPause0::call_after_init(CBasicStl * m_obj){

    TrexStatelessDpPause * lpPauseCmd = new TrexStatelessDpPause(m_port_id);
    TrexStatelessDpResume * lpResumeCmd1 = new TrexStatelessDpResume(m_port_id);

    m_obj->m_msg_queue.add_command(m_core,lpPauseCmd, 5.0); /* command in delay of 5 sec */
    m_obj->m_msg_queue.add_command(m_core,lpResumeCmd1, 7.0);/* command in delay of 7 sec */

}



/* start/stop/stop back to back */
TEST_F(basic_stl, basic_pause_resume0) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_basic_pause_resume0";

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

     std::vector<TrexStreamsCompiledObj *> objs;
     assert(compile.compile(port_id, streams, objs));
     TrexStatelessDpStart *lpStartCmd = new TrexStatelessDpStart(port_id, 0, objs[0], 10.0 /*sec */ );

     t1.m_msg_queue.add_msg(lpStartCmd);


     CBBStartPause0 sink;
     sink.m_port_id = port_id;
     t1.m_sink =  &sink;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


//////////////////////////////////////////////////////////////


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
    po->out_file ="exp/stl_bb_start_stop_delay2";

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
    po->out_file ="exp/stl_bb_start_stop_delay1";

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
    po->out_file ="exp/stl_bb_start_stop3";

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
    po->out_file ="exp/stl_bb_start_stop2";

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
    po->out_file ="exp/stl_bb_start_stop";

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
    po->out_file ="exp/stl_simple_prog4";

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
    po->out_file ="exp/stl_simple_prog3";

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
    po->out_file ="exp/stl_simple_prog2";

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
    po->out_file ="exp/stl_simple_prog1";

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
    po->out_file ="exp/stl_single_pkt_burst1";

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
    po->out_file ="exp/stl_single_stream";

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
                     "exp/stl_single_stream_mac0");
}

TEST_F(basic_stl, single_pkt_mac11) {

    test_mac_replace(true,
                     TrexStream::stPKT,
                     "exp/stl_single_stream_mac11");
}

TEST_F(basic_stl, single_pkt_mac10) {

    test_mac_replace(false,
                     TrexStream::stPKT,
                     "exp/stl_single_stream_mac01");
}

TEST_F(basic_stl, single_pkt_mac01) {

    test_mac_replace(true,
                     TrexStream::stCFG_FILE,
                     "exp/stl_single_stream_mac10");
}


TEST_F(basic_stl, multi_pkt1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_multi_pkt1";

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
    void run(bool full_packet,double duration );
public:
    std::string    m_input_packet; //"cap2/udp_64B.pcap"
    std::string    m_out_file;     //"exp/stl_vm_enable0";
};

void CEnableVm::run(bool full_packet,double duration=10.0){

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file =m_out_file; 

     TrexStreamsCompiler compile;

     uint8_t port_id=0;

     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStream(TrexStream::stCONTINUOUS,0,0);

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


TEST_F(basic_stl, vm_enable0) {

    CEnableVm vm_test;
    vm_test.m_out_file = "exp/stl_vm_enable0";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.run(true);
}


TEST_F(basic_stl, vm_enable1) {

    CEnableVm vm_test;
    vm_test.m_out_file = "exp/stl_vm_enable1";
    vm_test.m_input_packet = "stl/yaml/udp_594B_no_crc.pcap";
    vm_test.run(false);
}



TEST_F(basic_stl, vm_enable2) {

    CEnableVm vm_test;
    vm_test.m_out_file = "exp/stl_vm_enable2";
    vm_test.m_input_packet = "cap2/udp_64B.pcap";
    vm_test.run(true,50.0);
}




/* check disabled stream with multiplier of 5*/
TEST_F(basic_stl, multi_pkt2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_multi_pkt2";

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
    po->out_file ="exp/stl_multi_burst1";

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
 * check that continous stream does not point to another stream 
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
 * check for streams pointing to non exsistant streams
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

    virtual void handle(TrexStatelessDpToCpMsgBase *msg) {
        /* first the message must be an event */
        TrexDpPortEventMsg *event = dynamic_cast<TrexDpPortEventMsg *>(msg);
        EXPECT_TRUE(event != NULL);
        EXPECT_TRUE(event->get_event_type() == TrexDpPortEvent::EVENT_STOP);

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
    po->out_file ="exp/ignore";

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
    

    stream->set_rate(TrexStreamRate::RATE_PPS,1000);

    /* a burst of 2000 packets with a delay of 1 second */
    stream->m_isg_usec = 0;
    stream->set_multi_burst(1000, 500, 1000 * 1000);
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
    stream->set_multi_burst(1000 - 2, 1000, 1000 * 1000 + 2000);
    stream->m_pkt.len = 128;

    stream->m_next_stream_id = -1;

    streams.push_back(stream);

    const TrexStreamsGraphObj *obj = graph.generate(streams);
    EXPECT_EQ(obj->get_max_pps(), 1000.0);

    EXPECT_EQ(obj->get_max_bps_l2(), (1000 * (128 + 4) * 8));
    

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

        vm.set_split_instruction(split_instr);

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

        vm.set_split_instruction(split_instr);
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

    VmSplitTest split("exp/stl_vm_split_flow_var_inc.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.run(8, 4);

}

TEST_F(basic_stl, vm_split_flow_var_small_range) {
    /* small range */
    VmSplitTest split("exp/stl_vm_split_flow_var_small_range.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_flow_var_as_split(StreamVmInstructionFlowMan::FLOW_VAR_OP_INC, 0, 1, 0);
    
    split.run(8, 4);

}

TEST_F(basic_stl, vm_split_flow_var_big_range) {
    VmSplitTest split("exp/stl_vm_split_flow_var_big_range.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_flow_var_as_split(StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC, 1, 1000, 1000);

    split.run(8, 7);


}

TEST_F(basic_stl, vm_split_client_var) {
     VmSplitTest split("exp/stl_vm_split_client_var.erf");

    TrexStream stream(TrexStream::stSINGLE_BURST, 0, 0);
    stream.set_single_burst(1000);
    stream.set_rate(TrexStreamRate::RATE_PPS,100000);

    split.set_stream(&stream);
    split.set_client_var_as_split(0x10000001, 0x100000fe, 5000, 5050);
    
    split.run(8, 7);


}

/********************************************* Itay Tests End *************************************/
class rx_stat_pkt_parse  : public testing::Test {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};


TEST_F(rx_stat_pkt_parse, x710_parser) {
    Cxl710Parser parser;

    parser.test();
}
