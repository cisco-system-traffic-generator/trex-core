#include "trex_cmd_mngr.h"
#include "trex_exception.h"
#include "trex_global.h"
#include <zmq.h>
#include <errno.h>
#include <sys/wait.h>
#include <string>


CCmdsMngr::CCmdsMngr() {
    CParserOption * po =&CGlobalInfo::m_options;
    m_zmq_ctx = zmq_ctx_new();
    if ( !m_zmq_ctx ) {
        printf("Could not create ZMQ context\n");
    }
    m_zmq_cmds_socket = zmq_socket(m_zmq_ctx, ZMQ_REQ);
    if (!m_zmq_cmds_socket) {
        zmq_ctx_term(m_zmq_ctx);
        m_zmq_ctx = nullptr;
        printf("Could not create ZMQ socket\n");
    }
    int linger = 0;
    int timeout_ms = 5000;
    zmq_setsockopt(m_zmq_cmds_socket, ZMQ_LINGER, &linger, sizeof(linger));
    //receive and send timeout
    zmq_setsockopt(m_zmq_cmds_socket, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    zmq_setsockopt(m_zmq_cmds_socket, ZMQ_SNDTIMEO, &timeout_ms, sizeof(timeout_ms));

    std::string file_suffix = "";
    if (po->prefix != "") {
        file_suffix += "_" + po->prefix;
    }
    std::string file_path_ipc = "ipc://" + po->m_cmds_ipc_file_path + file_suffix + ".ipc";
    if ( zmq_connect(m_zmq_cmds_socket, file_path_ipc.c_str()) != 0 ) {
        zmq_close(m_zmq_cmds_socket);
        zmq_ctx_term(m_zmq_ctx);
        m_zmq_ctx = nullptr;
        m_zmq_cmds_socket = nullptr;
        printf("Could not connect to ZMQ socket\n");
    }
    Json::Value request;
    Json::Value response;
    request["type"] = NEW_CLIENT_REQ;
    send_request(request, response);
}


CCmdsMngr::~CCmdsMngr() {
    Json::Value request;
    Json::Value response;
    request["type"] = KILL_REQ;
    send_request(request, response);
    if (m_zmq_cmds_socket) {
        zmq_close(m_zmq_cmds_socket);
        m_zmq_cmds_socket=nullptr;
    }
    if (m_zmq_ctx) {
        zmq_ctx_term(m_zmq_ctx);
        m_zmq_ctx=nullptr;
    }
}


std::string CCmdsMngr::error_type_to_string(error_type_t error) {
    std::string error_str;
    switch(error) {
        case NONE: {
            error_str = "NONE";
            break;
        }
        case ZMQ_ERROR: {
            error_str = "ZMQ_ERROR";
            break;
        }
        case PARSING_ERROR: {
            error_str = "PARSING_ERROR";
            break;
        }
        default: {
            error_str = "UNKNOWN_ERROR";
        }
    }
    return error_str;
}

void CCmdsMngr::verify_field(Json::Value &value, std::string field_name, field_type_t type) {
    std::string err;
    if (value.isMember("err")) {
        err = value["err"].asString();
        err += "; ";
    }
    if (!value.isMember(field_name)) {
        err += "no such field: " + field_name;
        value["err"] = err;
        value["error_type"] = PARSING_ERROR;
        return;
    }
    const Json::Value &field = value[field_name];
    err += "expecting " + field_name + "to be ";
    bool rc = true;
    switch(type) {
        case FIELD_TYPE_STR:
            err += "'string'";
            if (!field.isString()) {
                rc = false;
            }
            break;
        case FIELD_TYPE_UINT32:
            err += "'uint32'";
            if (!field.isUInt64()) {
                rc = false;
            } else if (field.asUInt64() > 0xFFFFFFFF) {
                err += ",the value has size bigger than uint32.";
                rc = false;
            }
            break;
        default:
            rc = false;
    }
    if (!rc) {
        value["err"] = err;
        value["error_type"] = PARSING_ERROR;
    }
}


void CCmdsMngr::send_request(Json::Value &req, Json::Value &resp) {
    std::lock_guard<std::mutex> l(m_cmds_mutex);
    Json::FastWriter writer;
    Json::Reader reader;
    std::string request = writer.write(req);
    char resp_buffer[1024] = {0};
    int ret  = zmq_send(m_zmq_cmds_socket, request.c_str(), request.size(), 0);
    if (ret == -1) {
        resp["error_type"] = ZMQ_ERROR;
        std::string err;
        if (errno == EAGAIN){
            err = "Could not send the request: timeout in zmq_send";
        }else {
            err = "Could not send the request";
        }
        resp["err"] = err;
        return;
    }
    ret = zmq_recv(m_zmq_cmds_socket, resp_buffer, sizeof(resp_buffer), 0);
    if (ret == -1) {
        resp["error_type"] = ZMQ_ERROR;
        std::string err;
        if (errno == EAGAIN){
            err = "Could not receive the response: timeout in zmq_recv";
        }else {
            err = "Could not receive the response";
        }
        resp["err"] = err;
        return;
    }
    std::string response_str(resp_buffer);
    reader.parse(response_str, resp, false);
    verify_field(resp, "output", FIELD_TYPE_STR);
    verify_field(resp, "returncode", FIELD_TYPE_UINT32);
}


int CCmdsMngr::popen_general(const std::string &cmd, std::string &output, error_type_t& error, std::string &error_msg) {
    Json::Value request;
    Json::Value response;
    request["type"] = POPEN_REQ;
    request["cmd"] = cmd;
    send_request(request, response);
    if (response.isMember("err")) {
        error_msg += response["err"].asString();
        error = (error_type_t) response["error_type"].asInt();
        return -1;
    }

    output += response["output"].asString();
    int return_code = response["returncode"].asInt();
    return (return_code == 0) ? 0 : -1;
}