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

    void take_ownership(void) {
        Json::Value request;
        Json::Value response;

        create_request(request, "aquire", 1 , false);

        request["params"]["user"] = "test";
        request["params"]["force"] = true;

        send_request(request, response);

        EXPECT_TRUE(response["result"] != Json::nullValue);
        m_ownership_handler = response["result"].asString();
    }

    void release_ownership() {
        Json::Value request;
        Json::Value response;

        create_request(request, "release", 1 , false);
        request["params"]["handler"] = m_ownership_handler;

        send_request(request, response);
        EXPECT_TRUE(response["result"] == "ACK");
    }

    virtual void SetUp() {
        TrexRpcServerConfig cfg = TrexRpcServerConfig(TrexRpcServerConfig::RPC_PROT_TCP, 5050);

        m_rpc = new TrexRpcServer(cfg);
        m_rpc->start();

        m_context = zmq_ctx_new ();
        m_socket = zmq_socket (m_context, ZMQ_REQ);
        zmq_connect (m_socket, "tcp://localhost:5050");

        take_ownership();
    }

    virtual void TearDown() {
        m_rpc->stop();

        delete m_rpc;
        zmq_close(m_socket);
        zmq_term(m_context);
    }

public:

    void create_request(Json::Value &request, const string &method, int id = 1, bool ownership = false) {
        request.clear();

        request["jsonrpc"] = "2.0";
        request["id"] = id;
        request["method"] = method;

        if (ownership) {
            request["params"]["handler"] = m_ownership_handler; 
        }
    }

    void send_request(const Json::Value &request, Json::Value &response) {
        Json::FastWriter writer;
        Json::Reader reader;

        response.clear();

        string request_str = writer.write(request);
        string ret = send_msg(request_str);

        EXPECT_TRUE(reader.parse(ret, response, false));
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], request["id"]);
    }

    string send_msg(const string &msg) {
        char buffer[512];

        zmq_send (m_socket, msg.c_str(), msg.size(), 0);
        int len = zmq_recv(m_socket, buffer, sizeof(buffer), 0);

        return string(buffer, len);
    }

    TrexRpcServer *m_rpc;
    void *m_context;
    void *m_socket;
    string m_ownership_handler;
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
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"id\": 488}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 488);
    EXPECT_EQ(response["error"]["code"], -32602);

    /* simple add that works */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": 17, \"y\": -13} , \"id\": \"itay\"}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], "itay");
    EXPECT_EQ(response["result"], 4);

    /* add with bad paratemers types */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": \"blah\", \"y\": -13} , \"id\": 17}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 17);
    EXPECT_EQ(response["error"]["code"], -32602);

    /* add with invalid count of parameters */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"y\": -13} , \"id\": 17}";
    resp_str = send_msg(req_str);

    EXPECT_TRUE(reader.parse(resp_str, response, false));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 17);
    EXPECT_EQ(response["error"]["code"], -32602);


    /* big numbers */
    req_str = "{\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": 4827371, \"y\": -39181273} , \"id\": \"itay\"}";
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
            {\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": 22, \"y\": 17}, \"id\": \"1\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_sub\", \"params\": {\"x\": 22, \"y\": 17}, \"id\": \"2\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": 22, \"y\": \"itay\"}, \"id\": \"2\"}, \
            {\"foo\": \"boo\"}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_rpc_sheker\", \"params\": {\"name\": \"myself\"}, \"id\": 5}, \
            {\"jsonrpc\": \"2.0\", \"method\": \"test_add\", \"params\": {\"x\": 22, \"y\": 17} } \
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

TEST_F(RpcTest, add_stream) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    create_request(request, "get_stream", 1, true);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["error"]["code"], -32000);

    // add it
    create_request(request, "add_stream", 1, true);
    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;
    request["params"]["stream"]["mode"]["type"] = "continuous";
    request["params"]["stream"]["mode"]["pps"] = 3; 
    request["params"]["stream"]["isg"] = 4.3;
    request["params"]["stream"]["enabled"] = true;
    request["params"]["stream"]["self_start"] = true;
    request["params"]["stream"]["next_stream_id"] = -1;

    request["params"]["stream"]["packet"]["meta"] = "dummy";
    request["params"]["stream"]["packet"]["binary"][0] = 4;
    request["params"]["stream"]["packet"]["binary"][1] = 1;
    request["params"]["stream"]["packet"]["binary"][2] = 255;

    request["params"]["stream"]["vm"] = Json::arrayValue;
    request["params"]["stream"]["rx_stats"]["enabled"] = false;

    send_request(request, response);

    EXPECT_EQ(response["result"], "ACK");

    /* get it */

    create_request(request, "get_stream", 1, true);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    const Json::Value &stream = response["result"]["stream"];

    EXPECT_EQ(stream["enabled"], true);
    EXPECT_EQ(stream["self_start"], true);

    EXPECT_EQ(stream["packet"]["binary"][0], 4);
    EXPECT_EQ(stream["packet"]["binary"][1], 1);
    EXPECT_EQ(stream["packet"]["binary"][2], 255);

    EXPECT_EQ(stream["packet"]["meta"], "dummy");
    EXPECT_EQ(stream["next_stream_id"], -1);

    double delta = stream["isg"].asDouble() - 4.3;
    EXPECT_TRUE(delta < 0.0001);

    EXPECT_EQ(stream["mode"]["type"], "continuous");
    EXPECT_EQ(stream["mode"]["pps"], 3);

    // remove it
    create_request(request, "remove_stream", 1, true);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    EXPECT_EQ(response["result"], "ACK");

    // should not be present anymore
    send_request(request, response);

    EXPECT_EQ(response["error"]["code"], -32000);

}


