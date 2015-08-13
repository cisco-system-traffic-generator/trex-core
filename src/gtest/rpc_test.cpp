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

using namespace std;

class RpcTest : public testing::Test {

    virtual void SetUp() {
        m_rpc = new TrexRpcServerArray(TrexRpcServerArray::RPC_PROT_TCP, 5050);
        m_rpc->start();

        m_context = zmq_ctx_new ();
        m_socket = zmq_socket (m_context, ZMQ_REQ);
        zmq_connect (m_socket, "tcp://localhost:5050");
    }

    virtual void TearDown() {
        m_rpc->stop();

        delete m_rpc;
        zmq_close(m_socket);
        zmq_term(m_context);
    }

public:
    string send_msg(const string &msg) {
        char buffer[512];

        zmq_send (m_socket, msg.c_str(), msg.size(), 0);
        int len = zmq_recv(m_socket, buffer, sizeof(buffer), 0);

        return string(buffer, len);
    }

    TrexRpcServerArray *m_rpc;
    void *m_context;
    void *m_socket;
};

TEST_F(RpcTest, basic_rpc_test) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    string req_str;
    string resp_str;

    // check bad JSON format
    req_str = "bad format message";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_TRUE(response["jsonrpc"] == "2.0");
    EXPECT_TRUE(response["id"] == Json::Value::null);
    EXPECT_TRUE(response["error"]["code"] == -32700);

    // check bad version
    req_str = "{\"jsonrpc\": \"1.5\", \"method\": \"foobar\", \"id\": \"1\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_TRUE(response["jsonrpc"] == "2.0");
    EXPECT_TRUE(response["id"] == "1");
    EXPECT_TRUE(response["error"]["code"] == -32600);

    // no method name present
    req_str = "{\"jsonrpc\": \"1.5\", \"id\": 482}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_TRUE(response["jsonrpc"] == "2.0");
    EXPECT_TRUE(response["id"] == 482);
    EXPECT_TRUE(response["error"]["code"] == -32600);

    #if 0
    
    

    int id = 1;
    request["jsonrpc"] = "2.0";
    //request["method"]  = "test_func";

    Json::Value &params = request["params"];
    params["num"] = 12;
    params["msg"] = "hello, method test_func";

    for (int request_nbr = 0; request_nbr != 1; request_nbr++) {
        //request["id"] = "itay_id";

        stringstream ss;
        ss << request;

        cout << "Sending : '" << ss.str() << "'\n";
        
        zmq_send (requester, ss.str().c_str(), ss.str().size(), 0);

        int len = zmq_recv (requester, buffer, 250, 0);
        string resp(buffer, buffer + len);
        cout << "Got: " << resp << "\n";
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);

    sleep(1);

    rpc.stop();
    #endif
}

