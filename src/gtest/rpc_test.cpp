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
        TrexRpcServerConfig cfg = TrexRpcServerConfig(TrexRpcServerConfig::RPC_PROT_TCP, 5050);

        m_rpc = new TrexRpcServer(cfg);
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

    TrexRpcServer *m_rpc;
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
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], Json::Value::null);
    EXPECT_EQ(response["error"]["code"], -32700);

    // check bad version
    req_str = "{\"jsonrpc\": \"1.5\", \"method\": \"foobar\", \"id\": \"1\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], "1");
    EXPECT_EQ(response["error"]["code"], -32600);

    // no method name present
    req_str = "{\"jsonrpc\": \"2.0\", \"id\": 482}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 482);
    EXPECT_EQ(response["error"]["code"], -32600);

    /* method does not exist */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"jfgldjlfds\", \"id\": 482}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 482);
    EXPECT_EQ(response["error"]["code"], -32601);

    /* error but as notification */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"jfgldjlfds\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_TRUE(response == Json::Value::null);


}

TEST_F(RpcTest, test_add_command) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    string req_str;
    string resp_str;

    /* simple add - missing paramters */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"id\": 488}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 488);
    EXPECT_EQ(response["error"]["code"], -32602);

    /* simple add that works */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": 17, \"y\": -13} , \"id\": \"itay\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], "itay");
    EXPECT_EQ(response["result"], 4);

    /* add with bad paratemers types */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": \"blah\", \"y\": -13} , \"id\": 17}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 17);
    EXPECT_EQ(response["error"]["code"], -32602);

    /* add with invalid count of parameters */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"y\": -13} , \"id\": 17}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 17);
    EXPECT_EQ(response["error"]["code"], -32602);


    /* big numbers */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": 4827371, \"y\": -39181273} , \"id\": \"itay\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], "itay");
    EXPECT_EQ(response["result"], -34353902);

}

TEST_F(RpcTest, batch_rpc_test) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    string req_str;
    string resp_str;

    req_str = "[ \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": 22, \"y\": 17}, \"id\": \"1\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_sub\", \"params\": {\"x\": 22, \"y\": 17}, \"id\": \"2\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": 22, \"y\": \"itay\"}, \"id\": \"2\"}, \
            {\"foo\": \"boo\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_sheker\", \"params\": {\"name\": \"myself\"}, \"id\": 5}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_add\", \"params\": {\"x\": 22, \"y\": 17} } \
               ]";

    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_TRUE(response.isArray());

    // message 1
    EXPECT_TRUE(response[0]["jsonrpc"] == "2.0");
    EXPECT_TRUE(response[0]["id"] == "1");
    EXPECT_TRUE(response[0]["result"] == 39);

    // message 2
    EXPECT_TRUE(response[1]["jsonrpc"] == "2.0");
    EXPECT_TRUE(response[1]["id"] == "2");
    EXPECT_TRUE(response[1]["result"] == 5);

    // message 3
    EXPECT_TRUE(response[2]["jsonrpc"] == "2.0");
    EXPECT_TRUE(response[2]["id"] == "2");
    EXPECT_TRUE(response[2]["error"]["code"] == -32602);

    // message 4
    EXPECT_TRUE(response[3] == Json::Value::null);

    // message 5
    EXPECT_TRUE(response[4]["jsonrpc"] == "2.0");
    EXPECT_TRUE(response[4]["id"] == 5);
    EXPECT_TRUE(response[4]["error"]["code"] == -32601);

    // message 6 - no ID but a valid command
    EXPECT_TRUE(response[5] == Json::Value::null);

    return;
}
