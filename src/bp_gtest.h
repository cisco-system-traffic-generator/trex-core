/*
Copyright (c) 2017-2017 Cisco Systems, Inc.

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

#ifndef _BP_GTEST_H_
#define _BP_GTEST_H_
#include "sim/trex_sim.h"
#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))


/**
 * stateful test
 * 
 */
class trexStfTest  : public testing::Test {
public:
    trexStfTest() {
        TrexSTXCfg cfg;
        set_stx(new TrexStateful(cfg, nullptr));  
    }
    ~trexStfTest() {
         delete get_stx();
         set_stx(NULL);
    }
};


/**
 * stateless test
 * 
 */
class trexStlTest  : public testing::Test {
public:
    trexStlTest() {
        TrexSTXCfg cfg;
        set_stx(new TrexStateless(cfg));
    }
    ~trexStlTest() {
        delete get_stx();
        set_stx(NULL);
    }
};


/**
 * ASTF batch test
 * 
 */
class trexAstfBatchTest  : public testing::Test {
public:
    trexAstfBatchTest() {
        TrexSTXCfg cfg;
        set_stx(new TrexAstfBatch(cfg, nullptr));
    }
    ~trexAstfBatchTest() {
        delete get_stx();
        set_stx(NULL);
    }
};

/**
 * ASTF interactive test
 * 
 */
class trexAstfInteractiveTest  : public testing::Test {
public:
    trexAstfInteractiveTest() {
        TrexSTXCfg cfg;
        set_stx(new TrexAstf(cfg));
    }
    ~trexAstfInteractiveTest() {
        delete get_stx();
        set_stx(nullptr);
    }
};


class trexTest  : public trexStfTest {
 protected:
  virtual void SetUp() {
      CGlobalInfo::m_options.reset();
  }
  virtual void TearDown() {
  }
public:
};

class CTestBasic {

public:
    CTestBasic() {
        m_threads=1;
        m_time_diff=0.001;
        m_req_ports=0;
        m_dump_json=false;
    }

    bool  init(void) {
        uint16 * ports = NULL;
        CTupleBase tuple;
        CErfIF erf_vif;

        fl.Create();
        m_saved_packet_padd_offset=0;
        fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,m_threads);

        if (CGlobalInfo::m_options.client_cfg_file != "") {
            try {
                fl.load_client_config_file(CGlobalInfo::m_options.client_cfg_file);
                // The simulator only test MAC address configs, so this parameter is not used
                CManyIPInfo pretest_result;
                fl.set_client_config_resolved_macs(pretest_result);
        } catch (const std::runtime_error &e) {
                std::cout << "\n*** " << e.what() << "\n\n";
                exit(-1);
            }
            CGlobalInfo::m_options.preview.set_client_cfg_enable(true);
        }

        fl.generate_p_thread_info(m_threads);
        fl.m_threads_info[0]->set_vif(&erf_vif);
        CErfCmp cmp;
        cmp.dump = 1;
        bool res = true;
        int i;
        CFlowGenListPerThread * lpt;

        for (i=0; i<m_threads; i++) {
            lpt=fl.m_threads_info[i];

            CFlowPktInfo * pkt=lpt->m_cap_gen[0]->m_flow_info->GetPacket(0);
            m_saved_packet_padd_offset = pkt->m_pkt_indication.m_packet_padding;

            char buf[100];
            char buf_ex[100];
            sprintf(buf,"%s-%d.erf", CGlobalInfo::m_options.out_file.c_str(), i);
            sprintf(buf_ex,"%s-%d-ex.erf", CGlobalInfo::m_options.out_file.c_str(), i);

            if ( m_req_ports ) {
                /* generate from first template m_req_ports ports */
                int i;
                CTupleTemplateGeneratorSmart * lpg=&lpt->m_cap_gen[0]->tuple_gen;
                ports = new uint16_t[m_req_ports];
                lpg->GenerateTuple(tuple);
                for (i=0 ; i<m_req_ports;i++) {
                    ports[i]=lpg->GenerateOneSourcePort();
                }
            }
            CGlobalInfo::m_options.m_op_mode = CParserOption::OP_MODE_STF;
            lpt->start_sim(buf,CGlobalInfo::m_options.preview);
            lpt->m_node_gen.DumpHist(stdout);
            cmp.d_sec = m_time_diff;
            //compare generated file to expected file
            if ( cmp.compare(std::string(buf), std::string(buf_ex)) != true ) {
                res=false;
            }
        }

        if ( m_dump_json ) {
            printf(" dump json ...........\n");
            std::string s;
            fl.m_threads_info[0]->m_node_gen.dump_json(s);
            printf(" %s \n",s.c_str());
        }

        if ( m_req_ports ) {
            int i;
            fl.m_threads_info[0]->m_smart_gen.FreePort(0, tuple.getClientId(),tuple.getClientPort());
            for (i=0 ; i < m_req_ports; i++) {
                fl.m_threads_info[0]->m_smart_gen.FreePort(0,tuple.getClientId(),ports[i]);
            }
            delete []ports;
        }

        printf(" active %d \n", fl.m_threads_info[0]->m_smart_gen.ActiveSockets());
        EXPECT_EQ_UINT32(fl.m_threads_info[0]->m_smart_gen.ActiveSockets(),0);
        fl.Delete();
        return (res);
    }

    uint16_t  get_padd_offset_first_packet() {
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

#endif
