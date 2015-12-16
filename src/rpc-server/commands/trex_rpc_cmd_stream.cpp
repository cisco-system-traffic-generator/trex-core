/*
 Itay Marom, Dan Klein
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
#include <trex_stream.h>
#include <trex_stateless.h>
#include <trex_stateless_port.h>
#include <trex_streams_compiler.h>

#include <iostream>

using namespace std;


/***************************
 * add new stream
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    uint32_t stream_id  = parse_int(params, "stream_id", result);

    const Json::Value &section = parse_object(params, "stream", result);

    /* get the type of the stream */
    const Json::Value &mode = parse_object(section, "mode", result);
    string type = parse_string(mode, "type", result);

    /* allocate a new stream based on the type */
    TrexStream *stream = allocate_new_stream(section, port_id, stream_id, result);

    /* save this for future queries */
    stream->store_stream_json(section);

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

    /* parse VM */
    const Json::Value &vm =  parse_array(section ,"vm", result);
    parse_vm(vm, stream, result);

    /* parse RX info */
    const Json::Value &rx = parse_object(section, "rx_stats", result);

    stream->m_rx_check.m_enable = parse_bool(rx, "enabled", result);

    /* if it is enabled - we need more fields */
    if (stream->m_rx_check.m_enable) {
        stream->m_rx_check.m_stream_id   = parse_int(rx, "stream_id", result);
        stream->m_rx_check.m_seq_enabled = parse_bool(rx, "seq_enabled", result);
        stream->m_rx_check.m_latency     = parse_bool(rx, "latency_enabled", result);
    }

    /* make sure this is a valid stream to add */
    validate_stream(stream, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(stream->m_port_id);

    try {
        port->add_stream(stream);
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}



TrexStream *
TrexRpcCmdAddStream::allocate_new_stream(const Json::Value &section, uint8_t port_id, uint32_t stream_id, Json::Value &result) {

    TrexStream *stream = NULL;

    const Json::Value &mode = parse_object(section, "mode", result);
    std::string type = parse_string(mode, "type", result);

    if (type == "continuous") {

        double pps = parse_double(mode, "pps", result);
        stream = new TrexStream( TrexStream::stCONTINUOUS, port_id, stream_id);
        stream->set_pps(pps);

        if (stream->m_next_stream_id != -1) {
            generate_parse_err(result, "continious stream cannot provide next stream id - only -1 is valid");
        }

    } else if (type == "single_burst") {

        uint32_t total_pkts      = parse_int(mode, "total_pkts", result);
        double pps               = parse_double(mode, "pps", result);

        stream = new TrexStream(TrexStream::stSINGLE_BURST,port_id, stream_id);
        stream->set_pps(pps);
        stream->set_single_burst(total_pkts);


    } else if (type == "multi_burst") {

        double    pps              = parse_double(mode, "pps", result);
        double    ibg_usec         = parse_double(mode, "ibg", result);
        uint32_t  num_bursts       = parse_int(mode, "number_of_bursts", result);
        uint32_t  pkts_per_burst   = parse_int(mode, "pkts_per_burst", result);

        stream = new TrexStream(TrexStream::stMULTI_BURST,port_id, stream_id );
        stream->set_pps(pps);
        stream->set_multi_burst(pkts_per_burst,num_bursts,ibg_usec);


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
TrexRpcCmdAddStream::parse_vm_instr_checksum(const Json::Value &inst, TrexStream *stream, Json::Value &result) {

    uint16_t pkt_offset = parse_uint16(inst, "pkt_offset", result); 
    stream->m_vm.add_instruction(new StreamVmInstructionFixChecksumIpv4(pkt_offset));
}

void 
TrexRpcCmdAddStream::parse_vm_instr_tuple_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result){


    std::string  flow_var_name = parse_string(inst, "name", result);

    uint32_t ip_min       = parse_uint32(inst, "ip_min", result);
    uint32_t ip_max       = parse_uint32(inst, "ip_max", result);
    uint16_t port_min     = parse_uint16(inst, "port_min", result);
    uint16_t port_max     = parse_uint16(inst, "port_max", result);
    uint32_t limit_flows  = parse_uint32(inst, "limit_flows", result);
    uint16_t flags        = parse_uint16(inst, "flags", result);

    stream->m_vm.add_instruction(new StreamVmInstructionFlowClient(flow_var_name,
                                                                ip_min,
                                                                ip_max,
                                                                port_min,
                                                                port_max,
                                                                limit_flows,
                                                                flags
                                                                ));
}


void 
TrexRpcCmdAddStream::parse_vm_instr_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);

    auto sizes = {1, 2, 4, 8};
    uint8_t      flow_var_size = parse_choice(inst, "size", sizes, result);

    auto ops = {"inc", "dec", "random"};
    std::string  op_type_str = parse_choice(inst, "op", ops, result);

    StreamVmInstructionFlowMan::flow_var_op_e op_type;

    if (op_type_str == "inc") {
        op_type = StreamVmInstructionFlowMan::FLOW_VAR_OP_INC;
    } else if (op_type_str == "dec") {
        op_type = StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC;
    } else if (op_type_str == "random") {
        op_type = StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM;
    } else {
        throw TrexRpcException("internal error");
    }

    uint64_t init_value  = parse_uint64(inst, "init_value", result);
    uint64_t min_value   = parse_uint64(inst, "min_value", result);
    uint64_t max_value   = parse_uint64(inst, "max_value", result);

    stream->m_vm.add_instruction(new StreamVmInstructionFlowMan(flow_var_name,
                                                                flow_var_size,
                                                                op_type,
                                                                init_value,
                                                                min_value,
                                                                max_value
                                                                ));
}

void 
TrexRpcCmdAddStream::parse_vm_instr_write_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);
    uint16_t     pkt_offset    = parse_uint16(inst, "pkt_offset", result);
    int          add_value     = parse_int(inst,    "add_value", result);
    bool         is_big_endian = parse_bool(inst,   "is_big_endian", result);

    stream->m_vm.add_instruction(new StreamVmInstructionWriteToPkt(flow_var_name,
                                                                   pkt_offset,
                                                                   add_value,
                                                                   is_big_endian));
}

void 
TrexRpcCmdAddStream::parse_vm(const Json::Value &vm, TrexStream *stream, Json::Value &result) {
    /* array of VM instructions on vm */
    for (int i = 0; i < vm.size(); i++) {
        const Json::Value & inst = parse_object(vm, i, result);

        auto vm_types = {"fix_checksum_ipv4", "flow_var", "write_flow_var","tuple_flow_var"};
        std::string vm_type = parse_choice(inst, "type", vm_types, result);

        // checksum instruction
        if (vm_type == "fix_checksum_ipv4") {
            parse_vm_instr_checksum(inst, stream, result);

        } else if (vm_type == "flow_var") {
            parse_vm_instr_flow_var(inst, stream, result);

        } else if (vm_type == "write_flow_var") {
            parse_vm_instr_write_flow_var(inst, stream, result);

        } else if (vm_type == "tuple_flow_var") {
            parse_vm_instr_tuple_flow_var(inst, stream, result);
        } else {
            /* internal error */
            throw TrexRpcException("internal error");
        }
    }
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

    /* add the stream to the port's stream table */
    TrexStatelessPort * port = get_stateless_obj()->get_port_by_id(stream->m_port_id);

    /* does such a stream exists ? */
    if (port->get_stream_by_id(stream->m_stream_id)) {
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

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    uint32_t stream_id = parse_int(params, "stream_id", result);
    TrexStream *stream = port->get_stream_by_id(stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream " << stream_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    try {
        port->remove_stream(stream);
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    delete stream;

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * remove all streams
 * for a port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdRemoveAllStreams::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->remove_and_delete_all_streams();
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }


    result["result"] = Json::objectValue;

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

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    port->get_id_list(stream_list);

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

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    bool     get_pkt   = parse_bool(params, "get_pkt", result);
    uint32_t stream_id = parse_int(params, "stream_id", result);

    TrexStream *stream = port->get_stream_by_id(stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream id " << stream_id << " on port " << (int)port_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    /* return the stored stream json (instead of decoding it all over again) */
    Json::Value j = stream->get_stream_json();
    if (!get_pkt) {
        j.removeMember("packet");
    }

    result["result"]["stream"] = j;

    return (TREX_RPC_CMD_OK);

}

/***************************
 * start traffic on port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdStartTraffic::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    double duration  = parse_double(params, "duration", result);

    /* multiplier */
    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_string(mul_obj, "op", result);
    double      value  = parse_double(mul_obj, "value", result);

    if (op != "abs") {
        generate_parse_err(result, "start message can only specify absolute speed rate");
    }

    TrexPortMultiplier mul(type, op, value);

    try {
        port->start_traffic(mul, duration);

    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier();

    return (TREX_RPC_CMD_OK);
}

/***************************
 * stop traffic on port
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdStopTraffic::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->stop_traffic();
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * get all streams
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdGetAllStreams::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    bool    get_pkt = parse_bool(params, "get_pkt", result);

    std::vector <TrexStream *> streams;
    port->get_object_list(streams);

    Json::Value streams_json = Json::objectValue;
    for (auto stream : streams) {

        Json::Value j = stream->get_stream_json();

        /* should we include the packet as well ? */
        if (!get_pkt) {
            j.removeMember("packet");
        }

        std::stringstream ss;
        ss << stream->m_stream_id;

        streams_json[ss.str()] = j;
    }

    result["result"]["streams"] = streams_json;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * pause traffic
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdPauseTraffic::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

     try {
        port->pause_traffic();
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * resume traffic
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdResumeTraffic::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

     try {
        port->resume_traffic();
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * update traffic
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdUpdateTraffic::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    /* multiplier */

    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_choice(mul_obj, "op", TrexPortMultiplier::g_ops, result);
    double      value  = parse_double(mul_obj, "value", result);

    TrexPortMultiplier mul(type, op, value);


    try {
        port->update_traffic(mul);
    } catch (const TrexRpcException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier();

    return (TREX_RPC_CMD_OK);
}

/***************************
 * validate
 *  
 * checks that the port
 * attached streams are 
 * valid as a program 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdValidate::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    const TrexStreamsGraphObj *graph = NULL;

    try {
        graph = port->validate();
    }
    catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    

    result["result"]["rate"]["max_bps"] = graph->get_max_bps();
    result["result"]["rate"]["max_pps"] = graph->get_max_pps();
    result["result"]["rate"]["max_line_util"] = graph->get_max_bps() / port->get_port_speed_bps();

    result["result"]["graph"]["expected_duration"] = graph->get_duration();
    result["result"]["graph"]["events_count"] = (int)graph->get_events().size();

    result["result"]["graph"]["events"] = Json::arrayValue;
    Json::Value &events_json = result["result"]["graph"]["events"];

    int index = 0;
    for (const auto &ev : graph->get_events()) {
        Json::Value ev_json;

        ev_json["time_usec"]  = ev.time;
        ev_json["diff_bps"]   = ev.diff_bps;
        ev_json["diff_pps"]   = ev.diff_pps;
        ev_json["stream_id"]  = ev.stream_id;

        events_json.append(ev_json);

        index++;
        if (index >= 100) {
            break;
        }
    }


    return (TREX_RPC_CMD_OK);
}
