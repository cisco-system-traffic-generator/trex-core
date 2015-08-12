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

#include <common/gtest.h>
#include <trex_rpc_server_api.h>
#include <zmq.h>
#include <json/json.h>
#include <sstream>

class RpcTest : public testing::Test {

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(RpcTest, basic_rpc_test) {
    TrexRpcServerArray rpc(TrexRpcServerArray::RPC_PROT_TCP, 5050);
    rpc.start();

    sleep(1);

    printf ("Connecting to hello world server…\n");
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://localhost:5050");

    
    char buffer[250];
    Json::Value request;

    int id = 1;
    request["jsonrpc"] = "2.0";
    //request["method"]  = "test_func";

    Json::Value &params = request["params"];
    params["num"] = 12;
    params["msg"] = "hello, method test_func";

    for (int request_nbr = 0; request_nbr != 1; request_nbr++) {
        //request["id"] = "itay_id";

        std::stringstream ss;
        ss << request;

        std::cout << "Sending : '" << ss.str() << "'\n";
        
        zmq_send (requester, ss.str().c_str(), ss.str().size(), 0);

        int len = zmq_recv (requester, buffer, 250, 0);
        std::string resp(buffer, buffer + len);
        std::cout << "Got: " << resp << "\n";
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);

    sleep(1);

    rpc.stop();
}

