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
#include "trex_rpc_cmds.h"
#include <trex_rpc_server_api.h>
#include <trex_stream_api.h>
#include <trex_stateless_api.h>

#include <iostream>

using namespace std;

/***************************
 * add new stream
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    uint8_t  port_id    = parse_int(params, "port_id", result);
    uint32_t stream_id  = parse_int(params, "stream_id", result);

    const Json::Value &section = parse_object(params, "stream", result);

    /* get the type of the stream */
    const Json::Value &mode = parse_object(section, "mode", result);
    string type = parse_string(mode, "type", result);

    /* allocate a new stream based on the type */
    TrexStream *stream = allocate_new_stream(section, port_id, stream_id, result);

    /* some fields */
    stream->m_enabled         = parse_bool(section, "enabled", result);
    stream->m_self_start      = parse_bool(section, "self_start", result);

    /* inter stream gap */
    stream->m_isg_usec  = parse_double(section, "isg", result);

    stream->m_next_stream_id = parse_int(section, "next_stream_id", result);

    const Json::Value &pkt = parse_object(section, "packet", result);
    const Json::Value &pkt_binary = parse_array(pkt, "binary", result);

    /* fetch the packet from the message */

    stream->m_pkt.len    = pkt_binary.size();
    stream->m_pkt.binary = new uint8_t[pkt_binary.size()];
    if (!stream->m_pkt.binary) {
        generate_internal_err(result, "unable to allocate memory");
    }

    /* parse the packet */
    for (int i = 0; i < pkt_binary.size(); i++) {
        stream->m_pkt.binary[i] = parse_byte(pkt_binary, i, result);
    }

    /* meta data */
    stream->m_pkt.meta = parse_string(pkt, "meta", result);

    /* parse RX info */
    const Json::Value &rx = parse_object(section, "rx_stats", result);

    stream->m_rx_check.m_enable = parse_bool(rx, "enabled", result);

    /* if it is enabled - we need more fields */
    if (stream->m_rx_check.m_enable) {
        stream->m_rx_check.m_stream_id   = parse_int(rx, "stream_id", result);
        stream->m_rx_check.m_seq_enabled = parse_bool(rx, "seq_enabled", result);
        stream->m_rx_check.m_latency     = parse_bool(rx, "latency", result);
    }

    /* make sure this is a valid stream to add */
    validate_stream(stream, result);

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(stream->m_port_id);
    port->get_stream_table()->add_stream(stream);

    result["result"] = "ACK";

    return (TREX_RPC_CMD_OK);
}



TrexStream *
TrexRpcCmdAddStream::allocate_new_stream(const Json::Value &section, uint8_t port_id, uint32_t stream_id, Json::Value &result) {

    TrexStream *stream;

    const Json::Value &mode = parse_object(section, "mode", result);
    std::string type = parse_string(mode, "type", result);

    if (type == "continuous") {

        uint32_t pps = parse_int(mode, "pps", result);
        stream = new TrexStreamContinuous(port_id, stream_id, pps);

    } else if (type == "single_burst") {

        uint32_t total_pkts      = parse_int(mode, "total_pkts", result);
        uint32_t pps             = parse_int(mode, "pps", result);

        stream = new TrexStreamBurst(port_id, stream_id, total_pkts, pps);

    } else if (type == "multi_burst") {

        uint32_t  pps              = parse_int(mode, "pps", result);
        double    ibg_usec         = parse_double(mode, "ibg", result);
        uint32_t  num_bursts       = parse_int(mode, "number_of_bursts", result);
        uint32_t  pkts_per_burst   = parse_int(mode, "pkts_per_burst", result);

        stream = new TrexStreamMultiBurst(port_id, stream_id, pkts_per_burst, pps, num_bursts, ibg_usec);
        

    } else {
        generate_parse_err(result, "bad stream type provided: '" + type + "'");
    }

    /* make sure we were able to allocate the memory */
    if (!stream) {
        generate_internal_err(result, "unable to allocate memory");
    }

    return (stream);

}

void
TrexRpcCmdAddStream::validate_stream(const TrexStream *stream, Json::Value &result) {

    /* check packet size */
    if ( (stream->m_pkt.len < TrexStream::MIN_PKT_SIZE_BYTES) || (stream->m_pkt.len > TrexStream::MAX_PKT_SIZE_BYTES) ) {
        std::stringstream ss;
        ss << "bad packet size provided: should be between " << TrexStream::MIN_PKT_SIZE_BYTES << " and " << TrexStream::MAX_PKT_SIZE_BYTES;
        delete stream;
        generate_execute_err(result, ss.str()); 
    }

    /* port id should be between 0 and count - 1 */
    if (stream->m_port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        delete stream;
        generate_execute_err(result, ss.str());
    }

     /* add the stream to the port's stream table */
    TrexStatelessPort * port = TrexStateless::get_instance().get_port_by_id(stream->m_port_id);

    /* does such a stream exists ? */
    if (port->get_stream_table()->get_stream_by_id(stream->m_stream_id)) {
        std::stringstream ss;
        ss << "stream " << stream->m_stream_id << " already exists";
        delete stream;
        generate_execute_err(result, ss.str());
    }

}

/***************************
 * remove stream
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdRemoveStream::_run(const Json::Value &params, Json::Value &result) {
    uint8_t  port_id = parse_byte(params, "port_id", result);
    uint32_t stream_id = parse_int(params, "stream_id", result);


    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);
    TrexStream *stream = port->get_stream_table()->get_stream_by_id(stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream " << stream_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    port->get_stream_table()->remove_stream(stream);
    delete stream;

    result["result"] = "ACK";

    return (TREX_RPC_CMD_OK);
}

/***************************
 * remove all streams
 * for a port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdRemoveAllStreams::_run(const Json::Value &params, Json::Value &result) {
    uint8_t  port_id = parse_byte(params, "port_id", result);

    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

       TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);
       port->get_stream_table()->remove_and_delete_all_streams();

       result["result"] = "ACK";

       return (TREX_RPC_CMD_OK);
}

/***************************
 * get all streams configured 
 * on a specific port 
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdGetStreamList::_run(const Json::Value &params, Json::Value &result) {
    std::vector<uint32_t> stream_list;

    uint8_t  port_id = parse_byte(params, "port_id", result);

    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

       TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

       port->get_stream_table()->get_stream_list(stream_list);

       Json::Value json_list = Json::arrayValue;

       for (auto stream_id : stream_list) {
           json_list.append(stream_id);
       }

       result["result"] = json_list;

       return (TREX_RPC_CMD_OK);
}

/***************************
 * get stream by id
 * on a specific port 
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdGetStream::_run(const Json::Value &params, Json::Value &result) {
    uint8_t  port_id = parse_byte(params, "port_id", result);

    uint32_t stream_id = parse_int(params, "stream_id", result);

    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    TrexStream *stream = port->get_stream_table()->get_stream_by_id(stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream id " << stream_id << " on port " << (int)port_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    Json::Value stream_json;

    stream_json["enabled"]     = stream->m_enabled;
    stream_json["self_start"]  = stream->m_self_start;

    stream_json["isg"]            = stream->m_isg_usec;
    stream_json["next_stream_id"] = stream->m_next_stream_id;

    stream_json["packet"]["binary"]      = Json::arrayValue;
    for (int i = 0; i < stream->m_pkt.len; i++) {
        stream_json["packet"]["binary"].append(stream->m_pkt.binary[i]);
    }

    stream_json["packet"]["meta"] = stream->m_pkt.meta;

    if (TrexStreamContinuous *cont = dynamic_cast<TrexStreamContinuous *>(stream)) {
        stream_json["mode"]["type"] = "continuous";
        stream_json["mode"]["pps"]  = cont->get_pps();

    }

    result["result"]["stream"] = stream_json;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * start traffic on port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdStartTraffic::_run(const Json::Value &params, Json::Value &result) {
    uint8_t  port_id = parse_byte(params, "port_id", result);

    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    TrexStatelessPort::traffic_rc_e rc = port->start_traffic();

    if (rc == TrexStatelessPort::TRAFFIC_OK) {
        result["result"] = "ACK";
    } else {
        std::stringstream ss;
        switch (rc) {
        case TrexStatelessPort::TRAFFIC_ERR_ALREADY_STARTED:
            ss << "traffic has already started on that port";
            break;
        case TrexStatelessPort::TRAFFIC_ERR_NO_STREAMS:
            ss << "no active streams on that port";
            break;
        default:
            ss << "failed to start traffic";
            break;
        }

        generate_execute_err(result, ss.str());
    }

    return (TREX_RPC_CMD_OK);
}

/***************************
 * start traffic on port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdStopTraffic::_run(const Json::Value &params, Json::Value &result) {
    uint8_t  port_id = parse_byte(params, "port_id", result);

    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
        generate_execute_err(result, ss.str());
    }

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    port->stop_traffic();
    result["result"] = "ACK";

    return (TREX_RPC_CMD_OK);
}

