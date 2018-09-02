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
        m_op_mode_prev = CGlobalInfo::m_options.m_op_mode;
        m_op_mode = CParserOption::OP_MODE_ASTF;
    }

    virtual void TearDown() {
        m_op_mode = m_op_mode_prev;
    }

    typedef CParserOption::trex_op_mode_e trex_op_mode_e;
    trex_op_mode_e m_op_mode_prev;
    trex_op_mode_e &m_op_mode = CGlobalInfo::m_options.m_op_mode;
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
    bool res = lpastf->parse_file("automation/regression/data/astf_dns.json");
    EXPECT_EQ(res, true);
    lpastf->free_instance();
}

TEST_F(gt_astf_inter, astf_negative_4) {
    CAstfDB::instance();
}

TEST_F(gt_astf_inter, astf_positive_5) {
    CAstfDB * lpastf = CAstfDB::instance();
    bool res = lpastf->parse_file("automation/regression/data/astf_dns.json");
    EXPECT_EQ(res, true);
    CFlowGenList fl;
    fl.Create();
    fl.generate_p_thread_info(1);

    

    fl.clean_p_thread_info();
    fl.Delete();
    lpastf->free_instance();
}



