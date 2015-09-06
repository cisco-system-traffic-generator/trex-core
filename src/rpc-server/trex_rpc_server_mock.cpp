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
#include <trex_stateless_api.h>

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

    /* configure the stateless object with 4 ports */
    TrexStateless::configure(4);

    // gtest ?
    if (argc > 1) {
        if (string(argv[1]) != "--ut") {
            cout << "\n[Usage] " << argv[0] << ": " << " [--ut]\n\n";
            exit(-1);
        }
        return gtest_main(argc, argv);
    }

    cout << "\n-= Starting RPC Server Mock =-\n\n";
    cout << "Listening on tcp://localhost:5050 [ZMQ]\n\n";

    TrexRpcServerConfig rpc_cfg(TrexRpcServerConfig::RPC_PROT_TCP, 5050);
    TrexRpcServer rpc(rpc_cfg);

    /* init the RPC server */
    rpc.start();

    cout << "Setting Server To Full Verbose\n\n";
    rpc.set_verbose(true);

    cout << "Server Started\n\n";

    while (true) {
        sleep(1);
    }

    rpc.stop();


}
