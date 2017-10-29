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
#ifndef __TREX_SIM_H__
#define __TREX_SIM_H__

#include <string>
#include <os_time.h>
#include <bp_sim.h>
#include <json/json.h>

#include <vector>

#include "stl/trex_stl.h"
#include "stf/trex_stf.h"
#include "astf/trex_astf.h"
#include "astf_batch/trex_astf_batch.h"

int gtest_main(int argc, char **argv);

class TrexStateless;
class TrexPublisher;
class DpToCpHandler;
class DPCoreStats;

TrexSTX * get_stx();
void set_stx(TrexSTX *obj);

/* a stack-based guard for allocating/deallocating STX object */
class STXSimGuard {
public:
    STXSimGuard(TrexSTX *stx) {
        set_stx(stx);
    }
    
    ~STXSimGuard() {
        delete get_stx();
        set_stx(nullptr);
    }
};


static inline bool 
in_range(int x, int low, int high) {
    return ( (x >= low) && (x <= high) );
}

/**
 * interface for a sim target
 * 
 */
class SimInterface {
public:

    SimInterface() {

        time_init();
        CGlobalInfo::m_socket.Create(0);
        CGlobalInfo::init_pools(1000, MBUF_2048);
    }

    virtual ~SimInterface() {

        CMsgIns::Ins()->Free();
        CGlobalInfo::free_pools();
        CGlobalInfo::m_socket.Delete();
    }

    
};


/**
 * gtest target
 * 
 * @author imarom (28-Dec-15)
 */
class SimGtest : SimInterface {
public:

    int run(int argc, char **argv) {
        assert( CMsgIns::Ins()->Create(4) );
        return gtest_main(argc, argv);
    }
};



/**
 * stateful target
 * 
 */
class SimStateful : public SimInterface {

public:
    int run();
};



/**
 * target for sim stateless
 * 
 * @author imarom (28-Dec-15)
 */
class SimStateless : public SimInterface {

public:
    static SimStateless& get_instance() {
        static SimStateless instance;
        return instance;
    }

    
    int run(const std::string &json_filename,
            const std::string &out_filename,
            int port_count,
            int dp_core_count,
            int dp_core_index,
            int limit,
            bool is_dry_run);


    void set_verbose(bool enable) {
        m_verbose = enable;
    }

private:
    SimStateless();
    ~SimStateless();

    void init();
    void prepare_control_plane();
    void prepare_dataplane();
    void execute_json(const std::string &json_filename);
    void find_active_dp_cores();

    void run_dp(const std::string &out_filename);

    void run_dp_core(int core_index,
                     const std::string &out_filename,
                     std::vector<DPCoreStats> &stats,
                     DPCoreStats &total);

    void cleanup();
    void flush_messages();
    void flush_dp_to_cp_messages_core(int core_index);
    void flush_cp_to_dp_messages_core(int core_index);
    void flush_cp_to_rx_messages();

    void validate_response(const Json::Value &resp);

    bool should_capture_core(int i);
    bool is_multiple_capture();
    uint64_t get_limit_per_core(int core_index);

    void show_intro(const std::string &out_filename);

    bool is_verbose() {
        return m_verbose;
    }

    DpToCpHandler   *m_dp_to_cp_handler;
    TrexPublisher   *m_publisher;
    CFlowGenList     m_fl;
    CErfIFStl        m_erf_vif;
    CErfIFStlNull    m_null_erf_vif;
    bool             m_verbose;

    int              m_port_count;
    int              m_dp_core_count;
    int              m_dp_core_index;
    uint64_t         m_limit;
    bool             m_is_dry_run;

    std::vector<uint8_t> m_active_dp_cores;
};

struct asrtf_args_t {
    bool  full_sim;
    bool dump_json;
    uint8_t sim_mode;
    double sim_arg;
};

/**
 * stateful target
 * 
 */
class SimAstf : public SimInterface {

public:
    int run();
    asrtf_args_t * args;
};


class SimAstfSimple : public SimInterface {

public:
    int run();
    asrtf_args_t * args;
};



#endif /* __TREX_SIM_H__ */
