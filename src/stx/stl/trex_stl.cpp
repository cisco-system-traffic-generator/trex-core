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

#include <iostream>
#include <unistd.h>


#include "rpc-server/trex_rpc_cmds_table.h"

#include "common/trex_rpc_cmds_common.h"

#include "trex_stl_rpc_cmds.h"
#include "trex_stl.h"
#include "trex_stl_port.h"
#include "trex_stl_messaging.h"
#include "trex_stl_dp_core.h"



using namespace std;

/***********************************************************
 * Trex stateless object
 * 
 **********************************************************/

/**
 * 
 */
TrexStateless::TrexStateless(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 4;
    const int API_VER_MINOR = 4;
    
    /* init the RPC table */
    TrexRpcCommandsTable::get_instance().init("STL", API_VER_MAJOR, API_VER_MINOR);
    
    /* load the RPC components for stateless */
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsCommon());
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsSTL());
    
    /* create stateless ports */
    for (int i = 0; i < get_platform_api().get_port_count(); i++) {
        if ( !CGlobalInfo::m_options.m_dummy_port_map[i] ) {
            m_ports[i] = (TrexPort *)new TrexStatelessPort(i);
        }
    }

    /* create RX core */
    CRxCore *rx = new CRxCore();
    rx->create(cfg.m_rx_cfg);
    
    m_rx = rx;
}


/** 
 * release all memory 
 * 
 * @author imarom (08-Oct-15)
 */
TrexStateless::~TrexStateless() {
    
    /* release memory for ports */
    for (auto &port : m_ports) {
        delete port.second;
    }
    
    /* RX core */
    delete m_rx;
    m_rx = nullptr;
}


void TrexStateless::launch_control_plane() {
    /* start RPC server */
    m_rpc_server.start();
}


/**
* shutdown the server
*/
void TrexStateless::shutdown() {
    /* stop ports */
    for (auto &port : get_port_map()) {
        /* safe to call stop even if not active */
        port.second->stop_traffic();
    }
    
    /* shutdown the RPC server */
    m_rpc_server.stop();
    
    /* shutdown all DP cores */
    send_msg_to_all_dp(new TrexDpQuit());
    
    /* shutdown RX */
    send_msg_to_rx(new TrexRxQuit());
}


/**
 * fetch a port by ID
 * 
 */
TrexStatelessPort * TrexStateless::get_port_by_id(uint8_t port_id) {
    return (TrexStatelessPort *)TrexSTX::get_port_by_id(port_id);
}



TrexDpCore *
TrexStateless::create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) {
    TrexStatelessDpCore * lp=new TrexStatelessDpCore(thread_id, core);
    lp->set_need_to_rx(get_dpdk_mode()->dp_rx_queues()>0?true:false);
    return lp;
}

void
TrexStateless::publish_async_data() {
    // json from this class is sent only when requested. Still, we need to maintain the counters periodically.
    CFlowStatRuleMgr::instance()->periodic_update();
}

