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

#include <trex_rpc_server_api.h>
#include <trex_stateless.h>

#include <iostream>
#include <unistd.h>

using namespace std;

/**
 * on simulation this is not rebuild every version 
 * (improved stub) 
 * 
 */
extern "C" const char * get_build_date(void){ 
    return (__DATE__);
}      
 
extern "C" const char * get_build_time(void){ 
    return (__TIME__ );
} 

int gtest_main(int argc, char **argv);

int main(int argc, char *argv[]) {

    bool is_gtest = false;

     // gtest ?
    if (argc > 1) {
        if (string(argv[1]) != "--ut") {
            cout << "\n[Usage] " << argv[0] << ": " << " [--ut]\n\n";
            exit(-1);
        }
        is_gtest = true;
    }

    /* configure the stateless object with 4 ports */
    TrexStatelessCfg cfg;

    TrexRpcServerConfig rpc_req_resp_cfg(TrexRpcServerConfig::RPC_PROT_TCP, 5050);
    TrexRpcServerConfig rpc_async_cfg(TrexRpcServerConfig::RPC_PROT_TCP, 5051);

    cfg.m_port_count         = 4;
    cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
    cfg.m_rpc_async_cfg      = &rpc_async_cfg;
    cfg.m_rpc_server_verbose = (is_gtest ? false : true);

    TrexStateless::create(cfg);

    /* gtest handling */
    if (is_gtest) {
        int rc = gtest_main(argc, argv);
        TrexStateless::destroy();
        return rc;
    }

    cout << "\n-= Starting RPC Server Mock =-\n\n";
    cout << "Listening on tcp://localhost:5050 [ZMQ]\n\n";

    cout << "Server Started\n\n";

    while (true) {
        sleep(1);
    }

    TrexStateless::destroy();
}

