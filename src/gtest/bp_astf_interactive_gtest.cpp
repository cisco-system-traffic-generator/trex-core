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

#include "bp_gtest.h"
#include "astf/astf_db.h"

/*
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
*/


class gt_astf_inter : public trexAstfInteractiveTest {

protected:
    virtual void SetUp() {
        m_op_mode_prev = get_op_mode();
        set_op_mode(OP_MODE_ASTF);
    }

    virtual void TearDown() {
        set_op_mode(m_op_mode_prev);
    }

    trex_traffic_mode_t m_op_mode_prev;
public:
};


/***********************
*   ASTF Interactive   *
***********************/

TEST_F(gt_astf_inter, astf_positive_1) {
    CFlowGenList fl;
}

TEST_F(gt_astf_inter, astf_positive_2) {
    CFlowGenList fl;
    fl.Create();
    fl.Delete();
}

TEST_F(gt_astf_inter, astf_negative_2) {
    CFlowGenList fl;
    fl.Create();
}

TEST_F(gt_astf_inter, astf_positive_3) {
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);

    fl.clean_p_thread_info();
    fl.Delete();
}

TEST_F(gt_astf_inter, astf_negative_3) {
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);
}

TEST_F(gt_astf_inter, astf_positive_4) {
    CAstfDB * lpastf = CAstfDB::instance();
    bool success;
    success = lpastf->parse_file("automation/regression/data/owuigblskv");
    EXPECT_EQ(success, false);
    lpastf->free_instance();
}

#if 0
TEST_F(gt_astf_inter, astf_negative_4) {
    CAstfDB::instance();
}
#endif

TEST_F(gt_astf_inter, astf_positive_5) {
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);
    CFlowGenListPerThread *lpt = fl.m_threads_info[0];
    lpt->Delete_tcp_ctx(); // Should not fail

    fl.clean_p_thread_info();
    fl.Delete();
}

#if 0
TEST_F(gt_astf_inter, astf_negative_5) {
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);
    CFlowGenListPerThread *lpt = fl.m_threads_info[0];
    lpt->Create_tcp_ctx(); // DP core should allocate it (should assert)
}
#endif

TEST_F(gt_astf_inter, astf_positive_6) {
    bool success;
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);
    CFlowGenListPerThread *lpt = fl.m_threads_info[0];

    success = lpt->load_tcp_profile(); // DB not loaded
    EXPECT_EQ(success, false);

    CAstfDB * lpastf = CAstfDB::instance();
    success = lpastf->parse_file("automation/regression/data/astf_dns.json");
    EXPECT_EQ(success, true);

    success = lpt->load_tcp_profile(); // DB loaded
    EXPECT_EQ(success, true);

    /* TODO: run simulation of traffic here with --nc */

    lpt->unload_tcp_profile();

    lpastf->free_instance();
    fl.clean_p_thread_info();
    fl.Delete();
}








