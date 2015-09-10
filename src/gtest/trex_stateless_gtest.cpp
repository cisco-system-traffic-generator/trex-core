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


#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))

// one stream info with const packet , no VM
class CTRexDpStatelessVM {

};

//- add dump function 
// - check one object 
// create frame work 

class CTRexDpStreamModeContinues{
public:
    void set_pps(double pps){
        m_pps=pps;
    }
    double get_pps(){
        return (m_pps);
    }

    void dump(FILE *fd);
private:
    double      m_pps;
};


void CTRexDpStreamModeContinues::dump(FILE *fd){
    fprintf (fd," pps       : %f \n",m_pps);
}


class CTRexDpStreamModeSingleBurst{
public:
    void set_pps(double pps){
        m_pps=pps;
    }
    double get_pps(){
        return (m_pps);
    }

    void set_total_packets(uint64_t total_packets){
        m_total_packets =total_packets;
    }

    uint64_t get_total_packets(){
        return (m_total_packets);
    }

    void dump(FILE *fd);

private:
    double      m_pps;
    uint64_t    m_total_packets;
};


void CTRexDpStreamModeSingleBurst::dump(FILE *fd){
    fprintf (fd," pps           : %f \n",m_pps);
    fprintf (fd," total_packets : %llu \n",m_total_packets);
}


class CTRexDpStreamModeMultiBurst{
public:
    void set_pps(double pps){
        m_pps=pps;
    }
    double get_pps(){
        return (m_pps);
    }

    void set_pkts_per_burst(uint64_t pkts_per_burst){
        m_pkts_per_burst =pkts_per_burst;
    }

    uint64_t get_pkts_per_burst(){
        return (m_pkts_per_burst);
    }

    void set_ibg(double ibg){
        m_ibg = ibg;
    }

    double get_ibg(){
        return ( m_ibg );
    }

    void set_number_of_bursts(uint32_t number_of_bursts){
        m_number_of_bursts = number_of_bursts;
    }

    uint32_t get_number_of_bursts(){
        return (m_number_of_bursts);
    }

    void dump(FILE *fd);

private:
    double      m_pps;
    double      m_ibg; // inter burst gap 
    uint64_t    m_pkts_per_burst;
    uint32_t    m_number_of_bursts;
};

void CTRexDpStreamModeMultiBurst::dump(FILE *fd){
    fprintf (fd," pps           : %f \n",m_pps);
    fprintf (fd," total_packets : %llu \n",m_pkts_per_burst);
    fprintf (fd," ibg           : %f \n",m_ibg);
    fprintf (fd," num_of_bursts : %llu \n",m_number_of_bursts);
}



class CTRexDpStreamMode {
public:
    enum MODES {
        moCONTINUES     = 0x0, 
        moSINGLE_BURST  = 0x1,
        moMULTI_BURST   = 0x2
    } ;
    typedef  uint8_t  MODE_TYPE_t;

    void reset();

    void    set_mode(MODE_TYPE_t mode ){
        m_type = mode;
    }

    MODE_TYPE_t get_mode(){
        return (m_type);
    }


    CTRexDpStreamModeContinues & cont(void){
        return (m_data.m_cont);
    }
    CTRexDpStreamModeSingleBurst & single_burst(void){
        return (m_data.m_signle_burst);
    }

    CTRexDpStreamModeMultiBurst & multi_burst(void){
        return (m_data.m_multi_burst);
    }

    void dump(FILE *fd);

private:
    uint8_t                            m_type;
    union Data {
        CTRexDpStreamModeContinues     m_cont;
        CTRexDpStreamModeSingleBurst   m_signle_burst;
        CTRexDpStreamModeMultiBurst    m_multi_burst;
    }  m_data;
};


void CTRexDpStreamMode::reset(){
    m_type =CTRexDpStreamMode::moCONTINUES;
    memset(&m_data,0,sizeof(m_data));
}
    
void CTRexDpStreamMode::dump(FILE *fd){
    const char * table[3] = {"CONTINUES","SINGLE_BURST","MULTI_BURST"};

    fprintf(fd," mode      : %s \n", (char*)table[m_type]);
    switch (m_type) {
    case CTRexDpStreamMode::moCONTINUES :
        cont().dump(fd);
        break;
    case CTRexDpStreamMode::moSINGLE_BURST :
        single_burst().dump(fd);
        break;
    case CTRexDpStreamMode::moMULTI_BURST :
        multi_burst().dump(fd);
        break;
    default:
        fprintf(fd," ERROR type if not valid %d \n",m_type);
        break;
    }
}




class CTRexDpStatelessStream {

public:
    enum FLAGS_0{
        _ENABLE = 0,
        _SELF_START = 1,
        _VM_ENABLE =2,
        _END_STREAM =-1
    };

    CTRexDpStatelessStream(){
        reset();
    }

    void reset(){
        m_packet =0;
        m_vm=0;
        m_flags=0;
        m_isg_sec=0.0;
        m_next_stream = CTRexDpStatelessStream::_END_STREAM ; // END
        m_mode.reset();
    }

    void set_enable(bool enable){
        btSetMaskBit32(m_flags,_ENABLE,_ENABLE,enable?1:0);
    }

    bool get_enabled(){
        return (btGetMaskBit32(m_flags,_ENABLE,_ENABLE)?true:false);
    }

    void set_self_start(bool enable){
        btSetMaskBit32(m_flags,_SELF_START,_SELF_START,enable?1:0);
    }

    bool get_self_start(bool enable){
        return (btGetMaskBit32(m_flags,_SELF_START,_SELF_START)?true:false);
    }


    /* if we don't have VM we could just replicate the mbuf  and allocate it once */
    void set_vm_enable(bool enable){
        btSetMaskBit32(m_flags,_VM_ENABLE,_VM_ENABLE,enable?1:0);
    }

    bool get_vm_enabled(bool enable){
        return (btGetMaskBit32(m_flags,_VM_ENABLE,_VM_ENABLE)?true:false);
    }

    void set_inter_stream_gap(double  isg_sec){
        m_isg_sec =isg_sec;
    }

    double get_inter_stream_gap(){
        return (m_isg_sec);
    }

    CTRexDpStreamMode  &   get_mode();


    // CTRexDpStatelessStream::_END_STREAM for END
    void set_next_stream(int32_t next_stream){
        m_next_stream =next_stream;
    }

    int32_t get_next_stream(void){
        return ( m_next_stream );
    }

    void dump(FILE *fd);

private:
    char *                m_packet; 
    CTRexDpStatelessVM *  m_vm;
    uint32_t              m_flags;
    double                m_isg_sec; // in second
    CTRexDpStreamMode     m_mode;
    int32_t               m_next_stream; // next stream id
};

//- list of streams info with const packet , no VM
// - object that include the stream /scheduler/ packet allocation / need to create an object for one thread that works for test
// generate pcap file and compare it 

#if 0
void CTRexDpStatelessStream::dump(FILE *fd){

    fprintf(fd,"  enabled     : %d \n",get_enabled()?1:0);
    fprintf(fd,"  self_start  : %d \n",get_self_start()?1:0);
    fprintf(fd,"  vm          : %d \n",get_vm_enabled()?1:0);
    fprintf("  isg     : %f \n",m_isg_sec);
    m_mode.dump(fd);
    if (m_next_stream == CTRexDpStatelessStream::_END_STREAM ) {
        fprintf(fd," action    : End of Stream \n");
    }else{
        fprintf("  next        : %d \n",m_next_stream);
    }
}



class CTRexStatelessBasic {

public:
    CTRexStatelessBasic(){
        m_threads=1;
    }

    bool  init(void){
        return (true);
    }

public:
    bool m_threads;
};


/* stateless basic */
class dp_sl_basic  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};



TEST_F(dp_sl_basic, test1) {
    CTRexDpStatelessStream s1;
    s1.set_enable(true);
    s1.set_self_start(true);
    s1.set_inter_stream_gap(0.77);
    s1.get_mode().set_mode(CTRexDpStreamMode::moCONTINUES);
    s1.get_mode().cont().set_pps(100.2);
    s1.dump(stdout);
}




#endif
