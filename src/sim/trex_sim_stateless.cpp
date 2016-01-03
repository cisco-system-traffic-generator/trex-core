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
#include <trex_stateless.h>
#include <trex_stateless_messaging.h>
#include <trex_rpc_cmd_api.h>
#include <json/json.h>

using namespace std;

TrexStateless * get_stateless_obj() {
    return SimStateless::get_instance().get_stateless_obj();
}


/**
 * handler for DP to CP messages
 * 
 * @author imarom (19-Nov-15)
 */
class DpToCpHandler {
public:
    virtual void handle(TrexStatelessDpToCpMsgBase *msg) = 0;
};

/************************ 
 * 
 *  stateless sim object
 *  
************************/
class SimPublisher : public TrexPublisher {
public:

    /* override create */
    bool Create(uint16_t port, bool disable) {
        return true;
    }

    void Delete() {

    }

    void publish_json(const std::string &s) {
    }

    virtual ~SimPublisher() {
    }
};

/************************ 
 * 
 *  stateless sim object
 *  
************************/

SimStateless::SimStateless() {
    m_trex_stateless    = NULL;
    m_publisher         = NULL;
    m_dp_to_cp_handler  = NULL;
    m_verbose           = false;

    /* override ownership checks */
    TrexRpcCommand::test_set_override_ownership(true);
}


int
SimStateless::run(const string &json_filename, const string &out_filename) {
    prepare_dataplane();
    prepare_control_plane();

    execute_json(json_filename);
    run_dp(out_filename);

    flush_dp_to_cp_messages();

    return 0;
}


SimStateless::~SimStateless() {
    if (m_trex_stateless) {
        delete m_trex_stateless;
        m_trex_stateless = NULL;
    }

    if (m_publisher) {
        delete m_publisher;
        m_publisher = NULL;
    }

    m_fl.Delete();
}

/**
 * prepare control plane for test
 * 
 * @author imarom (28-Dec-15)
 */
void
SimStateless::prepare_control_plane() {
    TrexStatelessCfg cfg;
    
    m_publisher = new SimPublisher();

    TrexRpcServerConfig rpc_req_resp_cfg(TrexRpcServerConfig::RPC_PROT_MOCK, 0);

    cfg.m_port_count         = 4;
    cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
    cfg.m_rpc_async_cfg      = NULL;
    cfg.m_rpc_server_verbose = false;
    cfg.m_platform_api       = new TrexMockPlatformApi();
    cfg.m_publisher          = m_publisher;

    m_trex_stateless = new TrexStateless(cfg);

    m_trex_stateless->launch_control_plane();

    for (auto &port : m_trex_stateless->get_port_list()) {
        port->acquire("test", 0, true);
    }

}


/**
 * prepare the data plane for test
 * 
 */
void
SimStateless::prepare_dataplane() {
    
    m_fl.Create();
    m_fl.generate_p_thread_info(1);
    m_fl.m_threads_info[0]->set_vif(&m_erf_vif);
}



void
SimStateless::execute_json(const std::string &json_filename) {

    std::ifstream test(json_filename);
    std::stringstream buffer;
    buffer << test.rdbuf();

    std::string rep = m_trex_stateless->get_rpc_server()->test_inject_request(buffer.str());

    Json::Value root;
    Json::Reader reader;
    reader.parse(rep, root, false);

    if (is_verbose()) {
        std::cout << "server response: \n\n" << root << "\n\n";
    }
}

void
SimStateless::run_dp(const std::string &out_filename) {

    CFlowGenListPerThread *lpt = m_fl.m_threads_info[0];
    
    lpt->start_stateless_simulation_file((std::string)out_filename, CGlobalInfo::m_options.preview);
    lpt->start_stateless_daemon_simulation();

    flush_dp_to_cp_messages();

    std::cout << "\nwritten " << lpt->m_node_gen.m_cnt << " packets " << "to '" << out_filename << "'\n\n";
}

void
SimStateless::flush_dp_to_cp_messages() {

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
