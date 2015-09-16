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

protected:

    void set_verbose(bool verbose) {
        m_verbose = verbose;
    }
    
    virtual void SetUp() {

        m_verbose = false;
         
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

    void create_request(Json::Value &request, const string &method, int id = 1) {
        request.clear();

        request["jsonrpc"] = "2.0";
        request["id"] = id;
        request["method"] = method;

    }

    void send_request(const Json::Value &request, Json::Value &response) {
        Json::FastWriter writer;
        Json::Reader reader;

        response.clear();

        string request_str = writer.write(request);

        if (m_verbose) {
            cout << "\n" << request_str << "\n";
        }

        string ret = send_msg(request_str);

        if (m_verbose) {
            cout << "\n" << ret << "\n";
        }

        EXPECT_TRUE(reader.parse(ret, response, false));
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], request["id"]);
    }

    string send_msg(const string &msg) {
        char buffer[1024 * 20];

        zmq_send (m_socket, msg.c_str(), msg.size(), 0);
        int len = zmq_recv(m_socket, buffer, sizeof(buffer), 0);

        return string(buffer, len);
    }

    TrexRpcServer *m_rpc;
    void *m_context;
    void *m_socket;
    bool m_verbose;
};

class RpcTestOwned : public RpcTest {
public:

    void create_request(Json::Value &request, const string &method, int id = 1)  {
        RpcTest::create_request(request, method, id);
        request["params"]["handler"] = m_ownership_handler; 
    }

protected:

    virtual void SetUp() {
        RpcTest::SetUp();
        take_ownership();
    }


    void take_ownership(void) {
        Json::Value request;
        Json::Value response;

        RpcTest::create_request(request, "acquire", 1);

        request["params"]["user"] = "test";
        request["params"]["force"] = true;

        send_request(request, response);

        EXPECT_TRUE(response["result"] != Json::nullValue);
        m_ownership_handler = response["result"].asString();
    }

    void release_ownership() {
        Json::Value request;
        Json::Value response;

        RpcTest::create_request(request, "release", 1);
        request["params"]["handler"] = m_ownership_handler;

        send_request(request, response);
        EXPECT_TRUE(response["result"] == "ACK");
    }

    string m_ownership_handler;
};

TEST_F(RpcTest, basic_rpc_negative_cases) {
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

    /* missing parameters */
    create_request(request, "test_add");
    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["error"]["code"], -32602);

    /* bad paramters */
    create_request(request, "test_add");
    request["params"]["x"] = 5;
    request["params"]["y"] = "itay";
    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["error"]["code"], -32602);

    /* simple add that works */
    create_request(request, "test_add");
    request["params"]["x"] = 5;
    request["params"]["y"] = -13;
    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["result"], -8);

    /* big numbers */
    create_request(request, "test_add");
    request["params"]["x"] = 4827371;
    request["params"]["y"] = -39181273;
    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
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

/* ping command */
TEST_F(RpcTest, ping) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    create_request(request, "ping");
    send_request(request, response);
    EXPECT_TRUE(response["result"] == "ACK");
}

static bool 
find_member_in_array(const Json::Value &array, const string &member) {
    for (auto x : array) {
        if (x == member) {
            return true;
        }
    }

    return false;
}

/* get registered commands */
TEST_F(RpcTest, get_supported_cmds) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    create_request(request, "get_supported_cmds");
    send_request(request, response);
    EXPECT_TRUE(response["result"].size() > 0);

    EXPECT_TRUE(find_member_in_array(response["result"], "ping"));
    EXPECT_TRUE(find_member_in_array(response["result"], "get_supported_cmds"));
}

/* get version */
TEST_F(RpcTest, get_version) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    create_request(request, "get_version");
    send_request(request, response);

    EXPECT_TRUE(response["result"] != Json::nullValue);
    EXPECT_TRUE(response["result"]["built_by"] == "MOCK");
    EXPECT_TRUE(response["result"]["version"] == "v0.0");
}

/* get system info */
TEST_F(RpcTest, get_system_info) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    create_request(request, "get_system_info");
    send_request(request, response);

    EXPECT_TRUE(response["result"] != Json::nullValue);
    EXPECT_TRUE(response["result"]["core_type"].isString());
    EXPECT_TRUE(response["result"]["hostname"].isString());
    EXPECT_TRUE(response["result"]["uptime"].isString());
    EXPECT_TRUE(response["result"]["dp_core_count"] > 0);
    EXPECT_TRUE(response["result"]["port_count"] > 0);

    EXPECT_TRUE(response["result"]["ports"].isArray());

    const Json::Value &ports = response["result"]["ports"];


    for (int i = 0; i < ports.size(); i++) {
        EXPECT_TRUE(ports[i]["index"] == i);
        EXPECT_TRUE(ports[i]["driver"].isString());
        EXPECT_TRUE(ports[i]["speed"].isString());
    }
}

/* get owner, acquire and release */
TEST_F(RpcTest, get_owner_acquire_release) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    /* no user before acquring */
    create_request(request, "get_owner");
    send_request(request, response);
    EXPECT_TRUE(response["result"] != Json::nullValue);

    EXPECT_TRUE(response["result"]["owner"] == "none");

    /* soft acquire */
    create_request(request, "acquire");
    request["params"]["user"] = "itay";
    request["params"]["force"] = false;

    send_request(request, response);
    EXPECT_TRUE(response["result"] != Json::nullValue);

    create_request(request, "get_owner");
    send_request(request, response);
    EXPECT_TRUE(response["result"] != Json::nullValue);

    EXPECT_TRUE(response["result"]["owner"] == "itay");

    /* hard acquire */
    create_request(request, "acquire");
    request["params"]["user"] = "moshe";
    request["params"]["force"] = false;

    send_request(request, response);
    EXPECT_TRUE(response["result"] == Json::nullValue);

    request["params"]["force"] = true;

    send_request(request, response);
    EXPECT_TRUE(response["result"] != Json::nullValue);

    string handler = response["result"].asString();

    /* make sure */
    create_request(request, "get_owner");
    send_request(request, response);
    EXPECT_TRUE(response["result"] != Json::nullValue);

    EXPECT_TRUE(response["result"]["owner"] == "moshe");

    /* release */
    create_request(request, "release");
    request["params"]["handler"] = handler;
    send_request(request, response);

    EXPECT_TRUE(response["result"] == "ACK");
}


static void
create_simple_stream(Json::Value &obj) {
    obj["mode"]["type"] = "continuous";
    obj["mode"]["pps"] = (rand() % 1000 + 1) * 0.99;
    obj["isg"] = (rand() % 100 + 1) * 0.99;;
    obj["enabled"] = true;
    obj["self_start"] = true;
    obj["next_stream_id"] = -1;

    obj["packet"]["meta"] = "dummy";

    int packet_size = (rand() % 1500 + 1);
    for (int i = 0; i < packet_size; i++) {
        obj["packet"]["binary"][i] = (rand() % 0xff);
    }

    obj["vm"] = Json::arrayValue;
    obj["rx_stats"]["enabled"] = false;
}

static bool
compare_streams(const Json::Value &s1, const Json::Value &s2) {
    return s1 == s2;
}

TEST_F(RpcTestOwned, add_remove_stream) {
    Json::Value request;
    Json::Value response;
    Json::Reader reader;

    /* verify no such stream */
    create_request(request, "get_stream", 1);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["error"]["code"], -32000);

    /* add it */
    create_request(request, "add_stream", 1);
    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    Json::Value stream;
    create_simple_stream(stream);

    request["params"]["stream"] = stream;
    send_request(request, response);

    EXPECT_EQ(response["result"], "ACK");

    /* get it */
    create_request(request, "get_stream", 1);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    EXPECT_TRUE(compare_streams(stream, response["result"]["stream"]));

    // remove it
    create_request(request, "remove_stream", 1);

    request["params"]["port_id"] = 1;
    request["params"]["stream_id"] = 5;

    send_request(request, response);

    EXPECT_EQ(response["result"], "ACK");

    // should not be present anymore
    send_request(request, response);

    EXPECT_EQ(response["error"]["code"], -32000);

}


