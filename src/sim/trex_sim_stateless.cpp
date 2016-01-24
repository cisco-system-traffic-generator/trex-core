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
#include <stdexcept>
#include <sstream>
#include <trex_streams_compiler.h>

using namespace std;

/****** utils ******/
static string format_num(double num, const string &suffix = "") {
    const char x[] = {' ','K','M','G','T','P'};

    double my_num = num;

    for (int i = 0; i < sizeof(x); i++) {
        if (std::abs(my_num) < 1000.0) {
            stringstream ss;

            char buf[100];
            snprintf(buf, sizeof(buf), "%.2f", my_num);

            ss << buf << " " << x[i] << suffix;
            return ss.str();

        } else {
            my_num /= 1000.0;
        }
    }

    return "NaN";
}

TrexStateless * get_stateless_obj() {
    return SimStateless::get_instance().get_stateless_obj();
}


class SimRunException : public std::runtime_error 
{
public:
    SimRunException() : std::runtime_error("") {

    }
    SimRunException(const std::string &what) : std::runtime_error(what) {
    }
};

/*************** hook for platform API **************/
class SimPlatformApi : public TrexPlatformApi {
public:
    SimPlatformApi(int dp_core_count) {
        m_dp_core_count = dp_core_count;
    }

    virtual uint8_t get_dp_core_count() const {
        return m_dp_core_count;
    }

    virtual void get_global_stats(TrexPlatformGlobalStats &stats) const {
    }

    virtual void get_interface_info(uint8_t interface_id, std::string &driver_name, driver_speed_e &speed) const {
        driver_name = "TEST";
        speed = TrexPlatformApi::SPEED_10G;
    }

    virtual void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const {
    }

    virtual void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {
        for (int i = 0; i < m_dp_core_count; i++) {
             cores_id_list.push_back(std::make_pair(i, 0));
        }
    }

    virtual void publish_async_data_now(uint32_t key) const {

    }

private:
    int m_dp_core_count;
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
    m_dp_core_count     = -1;
    m_dp_core_index     = -1;
    m_port_count        = -1;
    m_limit             = 0;
    m_is_dry_run        = false;

    /* override ownership checks */
    TrexRpcCommand::test_set_override_ownership(true);
}


int
SimStateless::run(const string &json_filename,
                  const string &out_filename,
                  int port_count,
                  int dp_core_count,
                  int dp_core_index,
                  int limit,
                  bool is_dry_run) {

    assert(dp_core_count > 0);

    /* -1 means its not set or positive value between 0 and the dp core count - 1*/
    assert( (dp_core_index == -1) || ( in_range(dp_core_index, 0, dp_core_count - 1)) );

    m_dp_core_count = dp_core_count;
    m_dp_core_index = dp_core_index;
    m_port_count    = port_count;
    m_limit         = limit;
    m_is_dry_run    = is_dry_run;

    prepare_dataplane();
    prepare_control_plane();

    try {
        execute_json(json_filename);
    } catch (const SimRunException &e) {
        std::cout << "*** test failed ***\n\n" << e.what() << "\n";
        return (-1);
    }

    run_dp(out_filename);

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

    cfg.m_port_count         = m_port_count;
    cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
    cfg.m_rpc_async_cfg      = NULL;
    cfg.m_rpc_server_verbose = false;
    cfg.m_platform_api       = new SimPlatformApi(m_dp_core_count);
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
    
    CGlobalInfo::m_options.m_expected_portd = m_port_count;

    assert(CMsgIns::Ins()->Create(m_dp_core_count));
    m_fl.Create();
    m_fl.generate_p_thread_info(m_dp_core_count);

    for (int i = 0; i < m_dp_core_count; i++) {
        if (should_capture_core(i)) {
            m_fl.m_threads_info[i]->set_vif(&m_erf_vif);
        } else {
            m_fl.m_threads_info[i]->set_vif(&m_null_erf_vif);
        }
    }
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

    validate_response(root);

}

void
SimStateless::validate_response(const Json::Value &resp) {
    std::stringstream ss;

    if (resp.isArray()) {
        for (const auto &single : resp) {
            if (single["error"] != Json::nullValue) {
                ss << "failed with:\n\n" << single["error"] << "\n\n";
                throw SimRunException(ss.str());
            }
        }
    } else {
        if (resp["error"] != Json::nullValue) {
            ss << "failed with:\n\n" << resp["error"] << "\n\n";
            throw SimRunException(ss.str());
        }
    }
  
}

static inline bool is_debug() {
    #ifdef DEBUG
    return true;
    #else
    return false;
    #endif
}

void
SimStateless::show_intro(const std::string &out_filename) {
    uint64_t bps = 0;
    uint64_t pps = 0;

    std::cout << "\nGeneral info:\n";
    std::cout << "------------\n\n";

    std::cout << "image type:               " << (is_debug() ? "debug" : "release") << "\n";
    std::cout << "I/O output:               " << (m_is_dry_run ? "*DRY*" : out_filename) << "\n";

    if (m_limit > 0) {
        std::cout << "packet limit:             " << m_limit << "\n";
    } else {
        std::cout << "packet limit:             " << "*NO LIMIT*" << "\n";
    }

    if (m_dp_core_index != -1) {
        std::cout << "core recording:           " << m_dp_core_index << "\n";
    } else {
        std::cout << "core recording:           merge all\n";
    }

    std::cout << "\nConfiguration info:\n";
    std::cout << "-------------------\n\n";

    std::cout << "ports:                    " << m_port_count << "\n";
    std::cout << "cores:                    " << m_dp_core_count << "\n";
   

    std::cout << "\nPort Config:\n";
    std::cout << "------------\n\n";

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(0);

    std::cout << "stream count:             " << port->get_stream_count() << "\n";

    port->get_port_effective_rate(bps, pps);

    std::cout << "max BPS:                  " << format_num(bps, "bps") << "\n";
    std::cout << "max PPS:                  " << format_num(pps, "pps") << "\n";

    std::cout << "\n\nStarting simulation...\n";
}

void
SimStateless::run_dp(const std::string &out_filename) {
   uint64_t simulated_pkts_cnt = 0;
   uint64_t written_pkts_cnt = 0;

   show_intro(out_filename);

    if (is_multiple_capture()) {
        for (int i = 0; i < m_dp_core_count; i++) {
            std::stringstream ss;
            ss << out_filename << "-" << i;
            run_dp_core(i, ss.str(), simulated_pkts_cnt, written_pkts_cnt);
        }

    } else {
        for (int i = 0; i < m_dp_core_count; i++) {
            run_dp_core(i, out_filename, simulated_pkts_cnt, written_pkts_cnt);
        }
    }

    std::cout << "\n\nSimulation summary:\n";
    std::cout << "-------------------\n\n";
    std::cout << "simulated " << simulated_pkts_cnt << " packets\n";

    if (m_is_dry_run) {
        std::cout << "*DRY RUN* - no packets were written\n";
    } else {
        std::cout << "written " << written_pkts_cnt << " packets " << "to '" << out_filename << "'\n\n";
    }

    std::cout << "\n";
}


uint64_t 
SimStateless::get_limit_per_core(int core_index) {
    /* global no limit ? */
    if (m_limit == 0) {
        return (0);
    } else {
        uint64_t l = std::max((uint64_t)1, m_limit / m_dp_core_count);
        if (core_index == 0) {
            l += (m_limit % m_dp_core_count);
        }
        return l;
    }
}

void
SimStateless::run_dp_core(int core_index,
                          const std::string &out_filename,
                          uint64_t &simulated_pkts,
                          uint64_t &written_pkts) {

    CFlowGenListPerThread *lpt = m_fl.m_threads_info[core_index];

    lpt->start_stateless_simulation_file((std::string)out_filename, CGlobalInfo::m_options.preview, get_limit_per_core(core_index));
    lpt->start_stateless_daemon_simulation();

    flush_dp_to_cp_messages_core(core_index);

    simulated_pkts += lpt->m_node_gen.m_cnt;

    if (should_capture_core(core_index)) {
        written_pkts += lpt->m_node_gen.m_cnt;
    }
}


void
SimStateless::flush_dp_to_cp_messages_core(int core_index) {

    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(core_index);

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

bool
SimStateless::should_capture_core(int i) {

    /* dry run - no core should be recordered */
    if (m_is_dry_run) {
        return false;
    }

    /* no specific core index ? record all */
    if (m_dp_core_index == -1) {
        return true;
    } else {
        return (i == m_dp_core_index);
    }
}

bool
SimStateless::is_multiple_capture() {
    /* dry run - no core should be recordered */
    if (m_is_dry_run) {
        return false;
    }

    return ( (m_dp_core_count > 1) && (m_dp_core_index == -1) );
}

