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
#include "trex_rpc_cmd_api.h"

#include "stl/trex_stl.h"
#include "stl/trex_stl_streams_compiler.h"
#include "stl/trex_stl_messaging.h"
#include "stl/trex_stl_port.h"

#include "publisher/trex_publisher.h"

#include <json/json.h>
#include <stdexcept>
#include <sstream>


using namespace std;

class DPCoreStats {
public:
    DPCoreStats() {
        m_simulated_pkts   = 0;
        m_non_active_pkts  = 0;
        m_written_pkts     = 0;
    }

    uint64_t get_on_wire_count() {
        return (m_simulated_pkts - m_non_active_pkts);
    }

    uint64_t m_simulated_pkts;
    uint64_t m_non_active_pkts;
    uint64_t m_written_pkts;
};

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


class SimRunException : public std::runtime_error 
{
public:
    SimRunException() : std::runtime_error("") {

    }
    SimRunException(const std::string &what) : std::runtime_error(what) {
    }
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
    m_publisher                   = NULL;
    m_dp_to_cp_handler            = NULL;
    m_verbose                     = false;
    m_dp_core_count               = -1;
    m_dp_core_index               = -1;
    m_port_count                  = -1;
    m_limit                       = 0;
    m_is_dry_run                  = false;

    /* override ownership checks */
    TrexRpcCommand::test_set_override_ownership(true);
    TrexRpcCommand::test_set_override_api(true);
  
}


/**
 * on the simulation we first construct CP and then DP 
 * the only way to "assume" which DP will be active during 
 * the run is by checking for pending CP messages on the cores 
 * 
 * @author imarom (8/10/2016)
 */
void
SimStateless::find_active_dp_cores() {
    for (int core_index = 0; core_index < m_dp_core_count; core_index++) {
        TrexDpCore *dp_core = m_fl.m_threads_info[core_index]->get_dp_core();
        if (dp_core->are_any_pending_cp_messages()) {
            m_active_dp_cores.push_back(core_index);
        }
    }
}


void
SimStateless::init() {
    
    /* message queue init */
    assert(CMsgIns::Ins()->Create(m_dp_core_count));
    
    /* a hack for the simulator - change the DP core count in runtime */
    SimPlatformApi &sim_api = dynamic_cast<SimPlatformApi &>(get_platform_api());
    sim_api.set_dp_core_count(m_dp_core_count);

    TrexSTXCfg cfg;

    cfg.m_rpc_req_resp_cfg.create(TrexRpcServerConfig::RPC_PROT_MOCK, 0, nullptr, "");
    
    m_publisher = new SimPublisher();
    cfg.m_publisher = m_publisher;
    
    cfg.m_rx_cfg.create(1, {{}});
    
    set_stx(new TrexStateless(cfg));
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

    init();
    prepare_dataplane();
    prepare_control_plane();

    try {
        execute_json(json_filename);
    } catch (const SimRunException &e) {
        std::cout << "*** test failed ***\n\n" << e.what() << "\n";
        return (-1);
    }

    find_active_dp_cores();
    run_dp(out_filename);
    
    return 0;
}


SimStateless::~SimStateless() {
    
    if (get_stateless_obj()) {
        delete get_stateless_obj();
        set_stx(NULL);
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

    get_stateless_obj()->launch_control_plane();

    for (auto &port : get_stateless_obj()->get_port_map()) {
        port.second->acquire("test", 0, true);
    }

}


/**
 * prepare the data plane for test
 * 
 */
void
SimStateless::prepare_dataplane() {
    
    CGlobalInfo::m_options.m_expected_portd = m_port_count;
    set_op_mode(OP_MODE_STL);


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
    std::string rep;
    std::ifstream test(json_filename);
    std::stringstream buffer;
    buffer << test.rdbuf();

    try {
        rep = get_stateless_obj()->get_rpc_server()->test_inject_request(buffer.str());
    } catch (TrexRpcException &e) {
        throw SimRunException(e.what());
    }

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
    double pps;
    double bps_L1;
    double bps_L2;
    double percentage;

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

    port->get_port_effective_rate(pps, bps_L1, bps_L2, percentage);

    std::cout << "max PPS    :              " << format_num(pps,        "pps") << "\n";
    std::cout << "max BPS L1 :              " << format_num(bps_L1,     "bps") << "\n";
    std::cout << "max BPS L2 :              " << format_num(bps_L2,     "bps") << "\n";
    std::cout << "line util. :              " << format_num(percentage,  "%") << "\n";

    std::cout << "\n\nStarting simulation...\n";
}

void
SimStateless::run_dp(const std::string &out_filename) {
    std::vector<DPCoreStats> core_stats(m_dp_core_count);
    DPCoreStats total;

    show_intro(out_filename);

    if (is_multiple_capture()) {
        for (int i : m_active_dp_cores) {
            std::stringstream ss;
            ss << out_filename << "-" << i;
            run_dp_core(i, ss.str(), core_stats, total);
        }

    } else {
        for (int i : m_active_dp_cores) {
            run_dp_core(i, out_filename, core_stats, total);
        }
    }

    /* cleanup */
    cleanup();


    std::cout << "\n\nSimulation summary:\n";
    std::cout << "-------------------\n\n";

    for (int i = 0; i < m_dp_core_count; i++) {
        std::cout << "core index " << i << "\n";
        std::cout << "-----------------\n\n";
        std::cout << "    simulated packets  : " << core_stats[i].m_simulated_pkts << "\n";
        std::cout << "    non active packets : " << core_stats[i].m_non_active_pkts << "\n";
        std::cout << "    on-wire packets    : " << core_stats[i].get_on_wire_count() << "\n\n";
    }

    std::cout << "Total:" << "\n";
    std::cout << "-----------------\n\n";
    std::cout << "    simulated packets  : " << total.m_simulated_pkts << "\n";
    std::cout << "    non active packets : " << total.m_non_active_pkts << "\n";
    std::cout << "    on-wire packets    : " << total.get_on_wire_count() << "\n\n";

    if (m_is_dry_run) {
        std::cout << "*DRY RUN* - no packets were written\n";
    } else {
        std::cout << "written " << total.m_written_pkts << " packets " << "to '" << out_filename << "'\n\n";
    }

    std::cout << "\n";
}

void
SimStateless::flush_messages() {
    for (int i = 0; i < m_dp_core_count; i++) {
        flush_cp_to_dp_messages_core(i);
        flush_dp_to_cp_messages_core(i);
    }
    flush_cp_to_rx_messages();
}

void
SimStateless::cleanup() {

    for (int port_id = 0; port_id < get_stateless_obj()->get_port_count(); port_id++) {
        get_stateless_obj()->get_port_by_id(port_id)->stop_traffic();
        get_stateless_obj()->get_port_by_id(port_id)->remove_and_delete_all_streams();
    }
    
    flush_messages();
    CFlowStatRuleMgr::cleanup();
}

uint64_t 
SimStateless::get_limit_per_core(int core_index) {
    /* global no limit ? */
    if (m_limit == 0) {
        return (0);
    } else {
        uint64_t l = std::max((uint64_t)1, m_limit / m_active_dp_cores.size());
        if (core_index == m_active_dp_cores[0]) {
            l += (m_limit % m_active_dp_cores.size());
        }
        return l;
    }
}

void
SimStateless::run_dp_core(int core_index,
                          const std::string &out_filename,
                          std::vector<DPCoreStats> &stats,
                          DPCoreStats &total) {

    CFlowGenListPerThread *lpt = m_fl.m_threads_info[core_index];

    lpt->start_sim((std::string)out_filename, CGlobalInfo::m_options.preview, get_limit_per_core(core_index));
    
    flush_dp_to_cp_messages_core(core_index);

    /* core */
    stats[core_index].m_simulated_pkts   = lpt->m_node_gen.m_cnt;
    stats[core_index].m_non_active_pkts  = lpt->m_node_gen.m_non_active;

    /* total */
    total.m_simulated_pkts   += lpt->m_node_gen.m_cnt;
    total.m_non_active_pkts  += lpt->m_node_gen.m_non_active;

    if (should_capture_core(core_index)) {
        stats[core_index].m_written_pkts  = (lpt->m_node_gen.m_cnt - lpt->m_node_gen.m_non_active);
        total.m_written_pkts             += (lpt->m_node_gen.m_cnt - lpt->m_node_gen.m_non_active);
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

        TrexDpToCpMsgBase * msg = (TrexDpToCpMsgBase *)node;
        if (m_dp_to_cp_handler) {
            m_dp_to_cp_handler->handle(msg);
        }

        delete msg;
    }
}

void
SimStateless::flush_cp_to_dp_messages_core(int core_index) {

    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_index);

    while ( true ) {
        CGenNode * node = NULL;
        if (ring->Dequeue(node) != 0) {
            break;
        }
        assert(node);

        TrexCpToDpMsgBase * msg = (TrexCpToDpMsgBase *)node;
        delete msg;
    }
}

void
SimStateless::flush_cp_to_rx_messages() {
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);

    while ( true ) {
        CGenNode * node = NULL;
        if (ring->Dequeue(node) != 0) {
            break;
        }
        assert(node);

        TrexCpToRxMsgBase * msg = (TrexCpToRxMsgBase *)node;
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

