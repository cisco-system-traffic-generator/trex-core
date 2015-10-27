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
#include <sstream>
#include <unistd.h>
#include <string.h>
#include <zmq.h>

using namespace std;

static TrexStateless *g_trex_stateless;
static uint16_t g_rpc_port;

static bool
verify_tcp_port_is_free(uint16_t port) {
    void *m_context = zmq_ctx_new();
    void *m_socket  = zmq_socket (m_context, ZMQ_REP);
    std::stringstream ss;
    ss << "tcp://*:";
    ss << port;

    int rc = zmq_bind (m_socket, ss.str().c_str());

    zmq_close(m_socket);
    zmq_term(m_context);

    return (rc == 0);
}

static uint16_t
find_free_tcp_port(uint16_t start_port = 5050) {
    void *m_context = zmq_ctx_new();
    void *m_socket  = zmq_socket (m_context, ZMQ_REP);

    uint16_t port = start_port;
    while (true) {
        std::stringstream ss;
        ss << "tcp://*:";
        ss << port;

        int rc = zmq_bind (m_socket, ss.str().c_str());
        if (rc == 0) {
            break;
        }

        port++;
    }

    zmq_close(m_socket);
    zmq_term(m_context);

    return port;
}

TrexStateless * get_stateless_obj() {
    return g_trex_stateless;
}

uint16_t gtest_get_mock_server_port() {
    return g_rpc_port;
}

void delay(int msec){

    if (msec == 0) 
    {//user that requested that probebly wanted the minimal delay 
     //but because of scaling problem he have got 0 so we will give the min delay 
     //printf("\n\n\nERROR-Task delay ticks == 0 found in task %s task id = %d\n\n\n\n", 
     //       SANB_TaskName(SANB_TaskIdSelf()), SANB_TaskIdSelf());
     msec =1;

    } 

    struct timespec time1, remain; // 2 sec max delay
    time1.tv_sec=msec/1000;
    time1.tv_nsec=(msec - (time1.tv_sec*1000))*1000000;

    nanosleep(&time1,&remain);
}

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

static bool parse_uint16(const string arg, uint16_t &port) {
    stringstream ss(arg);

    bool x = (ss >> port);

    return (x);
}

int main(int argc, char *argv[]) {
    bool is_gtest = false;

     // gtest ?
    if (argc > 1) {
        string arg = string(argv[1]);

        if (arg == "--ut") {
            g_rpc_port = find_free_tcp_port();
            is_gtest = true;
        } else if (parse_uint16(arg, g_rpc_port)) {
            bool rc = verify_tcp_port_is_free(g_rpc_port);
            if (!rc) {
                cout << "port " << g_rpc_port << " is not available to use\n";
                exit(-1);
            }
        } else {

            cout << "\n[Usage] " << argv[0] << ": " << " [--ut] or [port number < 65535]\n\n";
            exit(-1);
        }

    } else {
        g_rpc_port = find_free_tcp_port();
    }

    /* configure the stateless object with 4 ports */
    TrexStatelessCfg cfg;

    TrexRpcServerConfig rpc_req_resp_cfg(TrexRpcServerConfig::RPC_PROT_TCP, g_rpc_port);
    //TrexRpcServerConfig rpc_async_cfg(TrexRpcServerConfig::RPC_PROT_TCP, 5051);

    cfg.m_port_count         = 4;
    cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
    cfg.m_rpc_async_cfg      = NULL;
    cfg.m_rpc_server_verbose = (is_gtest ? false : true);
    cfg.m_platform_api       = new TrexMockPlatformApi();

    g_trex_stateless = new TrexStateless(cfg);

    g_trex_stateless->launch_control_plane();

    /* gtest handling */
    if (is_gtest) {
        int rc = gtest_main(argc, argv);
        delete g_trex_stateless;
        g_trex_stateless = NULL;
        return rc;
    }

    cout << "\n-= Starting RPC Server Mock =-\n\n";
    cout << "Listening on tcp://localhost:" << g_rpc_port << " [ZMQ]\n\n";

    cout << "Server Started\n\n";

    while (true) {
        sleep(1);
    }

    delete g_trex_stateless;
    g_trex_stateless = NULL;
}

