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


#include "rpc-server/trex_rpc_cmds_table.h"
#include "common/trex_rpc_cmds_common.h"
#include "publisher/trex_publisher.h"
#include "trex_messaging.h"

#include "trex_astf.h"
#include "trex_astf_port.h"
#include "trex_astf_dp_core.h"
#include "trex_astf_rpc_cmds.h"

#include "stt_cp.h"

TrexAstf::TrexAstf(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 1;
    const int API_VER_MINOR = 0;
    
    /* init the RPC table */
    TrexRpcCommandsTable::get_instance().init("ASTF", API_VER_MAJOR, API_VER_MINOR);
    
    /* load the RPC components for ASTF */
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsCommon());
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsASTF());
    
     /* create ASTF ports */
    for (int i = 0; i < get_platform_api().get_port_count(); i++) {
        m_ports.push_back((TrexPort *)new TrexAstfPort(i));
    }
    
    /* create RX core */
    CRxCore *rx = new CRxCore();
    rx->create(cfg.m_rx_cfg);
    
    m_rx = rx;
}


TrexAstf::~TrexAstf() {
    for (auto port : m_ports) {
        delete port;
    }
    
    delete m_rx;
    m_rx = nullptr;
}


void
TrexAstf::launch_control_plane() {
    /* start RPC server */
    m_rpc_server.start();
}


void
TrexAstf::shutdown() {
    /* stop RPC server */
    m_rpc_server.stop();
        
    /* shutdown all DP cores */
    send_msg_to_all_dp(new TrexDpQuit());
    
    /* shutdown RX */
    send_msg_to_rx(new TrexRxQuit());
}


TrexDpCore *
TrexAstf::create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) {
    return new TrexAstfDpCore(thread_id, core);
}


void
TrexAstf::publish_async_data() {
    std::string json;
    
    CSTTCp *lpstt = get_platform_api().get_fl()->m_stt_cp;
    if (lpstt) {
        //if ( m_stats_cnt%4==0) { /* could be significat, reduce the freq */
        if (lpstt->dump_json(json)) {
            get_publisher()->publish_json(json);
        }
        //}
    }
}

