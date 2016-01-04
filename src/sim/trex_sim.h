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

int gtest_main(int argc, char **argv);

class TrexStateless;
class TrexPublisher;
class DpToCpHandler;

/**
 * interface for a sim target
 * 
 */
class SimInterface {
public:

    SimInterface() {

        time_init();
        CGlobalInfo::m_socket.Create(0);
        CGlobalInfo::init_pools(1000);
        assert( CMsgIns::Ins()->Create(4) );
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
class SimGtest : public SimInterface {
public:

    int run(int argc, char **argv) {
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

    
    int run(const std::string &json_filename, const std::string &out_filename);

    TrexStateless * get_stateless_obj() {
        return m_trex_stateless;
    }

    void set_verbose(bool enable) {
        m_verbose = enable;
    }

private:
    SimStateless();
    ~SimStateless();

    void prepare_control_plane();
    void prepare_dataplane();
    void execute_json(const std::string &json_filename);
    void run_dp(const std::string &out_filename);
    void flush_dp_to_cp_messages();

    void validate_response(const Json::Value &resp);

    bool is_verbose() {
        return m_verbose;
    }

    TrexStateless   *m_trex_stateless;
    DpToCpHandler   *m_dp_to_cp_handler;
    TrexPublisher   *m_publisher;
    CFlowGenList     m_fl;
    CErfIFStl        m_erf_vif;
    bool             m_verbose;
};

#endif /* __TREX_SIM_H__ */
