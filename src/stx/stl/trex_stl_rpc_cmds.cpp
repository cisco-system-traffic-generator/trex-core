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

/**
 * STL specific RPC commands
 */

#include "trex_stl_rpc_cmds.h"

#include "trex_stl.h"
#include "trex_stl_port.h"
#include "trex_stl_streams_compiler.h"

#include <string>

using namespace std;

/****************************** commands declerations ******************************/

/**
 * PG ids
 */
TREX_RPC_CMD(TrexRpcCmdGetActivePGIds,   "get_active_pgids");
TREX_RPC_CMD(TrexRpcCmdGetPGIdsStats,    "get_pgid_stats");


/**
 * streams status
 */
TREX_RPC_CMD(TrexRpcCmdGetStreamList,   "get_stream_list");
TREX_RPC_CMD(TrexRpcCmdGetAllStreams,   "get_all_streams");
TREX_RPC_CMD(TrexRpcCmdGetStream,       "get_stream");


/**
 * streams
 */

TREX_RPC_CMD_OWNED_EXT(TrexRpcCmdAddStream, "add_stream",

/* extended part */
std::unique_ptr<TrexStream> allocate_new_stream(const Json::Value &section, uint8_t port_id, uint32_t stream_id, Json::Value &result);
void validate_stream(const std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm(const Json::Value &vm, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_checksum(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_checksum_hw(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);

void parse_vm_instr_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_flow_var_rand_limit(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void check_min_max(uint8_t flow_var_size, uint64_t init_value,uint64_t step,uint64_t min_value,uint64_t max_value,Json::Value &result);


void parse_vm_instr_tuple_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_trim_pkt_size(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_rate(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_write_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_write_mask_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void handle_range_padding(uint64_t &max_value, uint64_t &min_value, uint64_t step, int op, Json::Value &result);

);


TREX_RPC_CMD_OWNED(TrexRpcCmdRemoveStream,      "remove_stream");
TREX_RPC_CMD_OWNED(TrexRpcCmdRemoveAllStreams,  "remove_all_streams");
TREX_RPC_CMD_OWNED(TrexRpcCmdStartTraffic,      "start_traffic");
TREX_RPC_CMD_OWNED(TrexRpcCmdStopTraffic,       "stop_traffic");
TREX_RPC_CMD_OWNED(TrexRpcCmdPauseTraffic,      "pause_traffic");
TREX_RPC_CMD_OWNED(TrexRpcCmdPauseStreams,      "pause_streams");
TREX_RPC_CMD_OWNED(TrexRpcCmdResumeTraffic,     "resume_traffic");
TREX_RPC_CMD_OWNED(TrexRpcCmdResumeStreams,     "resume_streams");
TREX_RPC_CMD_OWNED(TrexRpcCmdUpdateTraffic,     "update_traffic");
TREX_RPC_CMD_OWNED(TrexRpcCmdUpdateStreams,     "update_streams");
TREX_RPC_CMD_OWNED(TrexRpcCmdValidate,          "validate");

/**
 * RX filters
 */
TREX_RPC_CMD_OWNED(TrexRpcCmdRemoveRXFilters,   "remove_rx_filters");


/**
 * PCAP
 */
TREX_RPC_CMD_OWNED(TrexRpcCmdPushRemote, "push_remote");

/**
 * service mode
 */
TREX_RPC_CMD_OWNED(TrexRpcCmdSetServiceMode, "service");


/****************************** commands implementation ******************************/


/**
 * get active packet group IDs
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetActivePGIds::_run(const Json::Value &params, Json::Value &result) {
    flow_stat_active_t_new active_flow_stat;
    flow_stat_active_it_t_new it;
    int i = 0, j = 0;

    Json::Value &section = result["result"];
    section["ids"]["flow_stats"] = Json::arrayValue;
    section["ids"]["latency"] = Json::arrayValue;

    if (get_platform_api().get_active_pgids(active_flow_stat) < 0)
        return TREX_RPC_CMD_INTERNAL_ERR;

    for (it = active_flow_stat.begin(); it != active_flow_stat.end(); it++) {
        if ((*it).m_type == PGID_FLOW_STAT) {
            section["ids"]["flow_stats"][i++] = (*it).m_pg_id;
        } else {
            section["ids"]["latency"][j++] = (*it).m_pg_id;
        }
    }

    return TREX_RPC_CMD_OK;
}


/**
 * get packet group IDs stats
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPGIdsStats::_run(const Json::Value &params, Json::Value &result) {
    flow_stat_active_t active_flow_stat;
    flow_stat_active_it_t it;
    std::vector<uint32_t> pgids_arr;

    const Json::Value &pgids = parse_array(params, "pgids", result);

    /* iterate over all attributes in the dict */
    for( Json::ValueIterator itr = pgids.begin() ; itr != pgids.end() ; itr++ ) {
        try {
            pgids_arr.push_back(itr->asUInt());
        } catch (const std::exception &ex) {
            generate_execute_err(result, ex.what());
        }
    }

    Json::Value &section = result["result"];

    try {
        if (get_platform_api().get_pgid_stats(section, pgids_arr) != 0) {
            return TREX_RPC_CMD_INTERNAL_ERR;
        }
    } catch (const std::exception &ex) {
        generate_execute_err(result, ex.what());
    }


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
 * get all streams
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdGetAllStreams::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    std::vector <TrexStream *> streams;
    port->get_object_list(streams);

    Json::Value streams_json = Json::objectValue;
    for (auto stream : streams) {

        Json::Value j = stream->get_stream_json();

        std::stringstream ss;
        ss << stream->m_stream_id;

        streams_json[ss.str()] = j;
    }

    result["result"]["streams"] = streams_json;

    return (TREX_RPC_CMD_OK);
}



/***************************
 * add new stream
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    try {
        uint8_t port_id = parse_port(params, result);

        uint32_t stream_id  = parse_uint32(params, "stream_id", result);

        const Json::Value &section = parse_object(params, "stream", result);

        /* get the type of the stream */
        const Json::Value &mode = parse_object(section, "mode", result);
        string type = parse_string(mode, "type", result);

        /* allocate a new stream based on the type */
        std::unique_ptr<TrexStream> stream = allocate_new_stream(section, port_id, stream_id, result);

        /* save this for future queries */
        stream->store_stream_json(section);

        /* some fields */
        stream->m_enabled         = parse_bool(section, "enabled", result);
        stream->m_self_start      = parse_bool(section, "self_start", result);
        stream->m_start_paused    = parse_bool(section, "start_paused", result, false); /* default is false */
        stream->m_flags           = parse_int(section, "flags", result);
        stream->m_action_count    = parse_uint32(section, "action_count", result);
        stream->m_random_seed     = parse_uint32(section, "random_seed", result,0); /* default is zero */
        stream->set_null_stream(stream->m_flags & 8);

        /* inter stream gap */
        stream->m_isg_usec  = parse_udouble(section, "isg", result);

        stream->m_next_stream_id = parse_int(section, "next_stream_id", result);

        const Json::Value &pkt = parse_object(section, "packet", result);
        std::string pkt_binary = base64_decode(parse_string(pkt, "binary", result));

        /* check packet size */
        if ( (pkt_binary.size() < MIN_PKT_SIZE) || (pkt_binary.size() > MAX_PKT_SIZE) ) {
            std::stringstream ss;
            ss << "Bad packet size provided: " << pkt_binary.size() <<  ". Should be between " << MIN_PKT_SIZE << " and " << MAX_PKT_SIZE;
            generate_execute_err(result, ss.str()); 
        }

        /* fetch the packet from the message */

        stream->m_pkt.len    = std::max(pkt_binary.size(), 60UL);

        /* allocate and init to zero ( with () ) */
        stream->m_pkt.binary = new uint8_t[stream->m_pkt.len]();
        if (!stream->m_pkt.binary) {
            generate_internal_err(result, "unable to allocate memory");
        }

        const char *pkt_buffer = pkt_binary.c_str();

        /* copy the packet - if less than 60 it will remain zeroes */
        for (int i = 0; i < pkt_binary.size(); i++) {
            stream->m_pkt.binary[i] = pkt_buffer[i];
        }

        /* meta data */
        stream->m_pkt.meta = parse_string(pkt, "meta", result);

        /* parse VM */
        const Json::Value &vm =  parse_object(section ,"vm", result);
        parse_vm(vm, stream, result);

        /* parse RX info */
        const Json::Value &rx = parse_object(section, "flow_stats", result);

        stream->m_rx_check.m_enabled = parse_bool(rx, "enabled", result);

        TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(stream->m_port_id);

        /* if it is enabled - we need more fields */
        if (stream->m_rx_check.m_enabled) {

            if (port->get_rx_caps() == 0) {
                generate_parse_err(result, "RX stats is not supported on this interface");
            }

            stream->m_rx_check.m_pg_id      = parse_uint32(rx, "stream_id", result);
            std::string type = parse_string(rx, "rule_type", result);
            if (type == "latency") {
                stream->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_PAYLOAD;
            } else {
                stream->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_IPV4_ID;
            }
        }

        /* make sure this is a valid stream to add */
        validate_stream(stream, result);
        port->add_stream(stream.get());
        stream.release();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}



std::unique_ptr<TrexStream>
TrexRpcCmdAddStream::allocate_new_stream(const Json::Value &section, uint8_t port_id, uint32_t stream_id, Json::Value &result) {

    std::unique_ptr<TrexStream> stream;

    const Json::Value &mode = parse_object(section, "mode", result);
    std::string type = parse_string(mode, "type", result);


    if (type == "continuous") {

        stream.reset(new TrexStream( TrexStream::stCONTINUOUS, port_id, stream_id));

    } else if (type == "single_burst") {

        uint32_t total_pkts      = parse_uint32(mode, "total_pkts", result);

        stream.reset(new TrexStream(TrexStream::stSINGLE_BURST, port_id, stream_id));
        stream->set_single_burst(total_pkts);


    } else if (type == "multi_burst") {

        double    ibg_usec         = parse_udouble(mode, "ibg", result);
        uint32_t  num_bursts       = parse_uint32(mode, "count", result);
        uint32_t  pkts_per_burst   = parse_uint32(mode, "pkts_per_burst", result);

        stream.reset(new TrexStream(TrexStream::stMULTI_BURST,port_id, stream_id));
        stream->set_multi_burst(pkts_per_burst,num_bursts,ibg_usec);


    } else {
        generate_parse_err(result, "bad stream type provided: '" + type + "'");
    }

     /* parse the rate of the stream */
    const Json::Value &rate =  parse_object(mode ,"rate", result);
    parse_rate(rate, stream, result);

    return (stream);

}

void 
TrexRpcCmdAddStream::parse_rate(const Json::Value &rate, std::unique_ptr<TrexStream> &stream, Json::Value &result) {

    double value = parse_udouble(rate, "value", result);

    auto rate_types = {"pps", "bps_L1", "bps_L2", "percentage"};
    std::string rate_type = parse_choice(rate, "type", rate_types, result);

    if (rate_type == "pps") {
        stream->set_rate(TrexStreamRate::RATE_PPS, value);
    } else if (rate_type == "bps_L1") {
        stream->set_rate(TrexStreamRate::RATE_BPS_L1, value);
    } else if (rate_type == "bps_L2") {
        stream->set_rate(TrexStreamRate::RATE_BPS_L2, value);
    } else if (rate_type == "percentage") {
        stream->set_rate(TrexStreamRate::RATE_PERCENTAGE, value);
    } else {
        /* impossible */
        assert(0);
    }

}

void 
TrexRpcCmdAddStream::parse_vm_instr_checksum_hw(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    uint16_t l2_len = parse_uint16(inst, "l2_len", result); 
    uint16_t l3_len = parse_uint16(inst, "l3_len", result); 
    uint16_t l4_type = parse_uint16(inst, "l4_type", result); 

    stream->m_vm.add_instruction(new StreamVmInstructionFixHwChecksum(l2_len,l3_len,l4_type));
}

void 
TrexRpcCmdAddStream::parse_vm_instr_checksum(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {

    uint16_t pkt_offset = parse_uint16(inst, "pkt_offset", result); 
    stream->m_vm.add_instruction(new StreamVmInstructionFixChecksumIpv4(pkt_offset));
}


void 
TrexRpcCmdAddStream::parse_vm_instr_trim_pkt_size(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result){

    std::string  flow_var_name = parse_string(inst, "name", result);

    stream->m_vm.add_instruction(new StreamVmInstructionChangePktSize(flow_var_name));
}


void 
TrexRpcCmdAddStream::parse_vm_instr_tuple_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result){


    std::string  flow_var_name = parse_string(inst, "name", result);

    uint32_t ip_min       = parse_uint32(inst, "ip_min", result);
    uint32_t ip_max       = parse_uint32(inst, "ip_max", result);
    uint16_t port_min     = parse_uint16(inst, "port_min", result);
    uint16_t port_max     = parse_uint16(inst, "port_max", result);
    uint32_t limit_flows  = parse_uint32(inst, "limit_flows", result);
    uint16_t flags        = parse_uint16(inst, "flags", result);

    /* archiecture limitation - limit_flows must be greater or equal to DP core count */
    if ( (limit_flows != 0) && (limit_flows < get_platform_api().get_dp_core_count())) {
        std::stringstream ss;
        ss << "cannot limit flows to less than " << (uint32_t)get_platform_api().get_dp_core_count();
        generate_execute_err(result, ss.str());
    }

    stream->m_vm.add_instruction(new StreamVmInstructionFlowClient(flow_var_name,
                                                                   ip_min,
                                                                   ip_max,
                                                                   port_min,
                                                                   port_max,
                                                                   limit_flows,
                                                                   flags
                                                                   ));
}


/**
 * if a user specify min_value and max_value with a step 
 * that does not create a full cycle - pad the values to allow 
 * a true cycle 
 * 
 */
void
TrexRpcCmdAddStream::handle_range_padding(uint64_t &max_value,
                                          uint64_t &min_value,
                                          uint64_t step,
                                          int op,
                                          Json::Value &result) {

    /* pad for the step */
    uint64_t pad = (max_value - min_value + 1) % step; 
    if (pad == 0) {
        return;
    }

    /* the leftover rounded up */
    uint64_t step_pad = step - pad;

    switch (op) {
    case StreamVmInstructionFlowMan::FLOW_VAR_OP_INC:

        if ( (UINT64_MAX - max_value) < step_pad ) {
            generate_parse_err(result, "VM: could not pad range to be a true cycle - '(max_value - min_value + 1) % step' should be zero");
        }
        max_value += step_pad;

        break;

    case StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC:
        if ( min_value < step_pad ) {
            generate_parse_err(result, "VM: could not pad range to be a true cycle - '(max_value - min_value + 1) % step' should be zero");
        }
        min_value -= step_pad;

        break;

    default:
        break;
    }
}

void 
TrexRpcCmdAddStream::check_min_max(uint8_t flow_var_size, 
                                   uint64_t init_value,
                                   uint64_t step,
                                   uint64_t min_value,
                                   uint64_t max_value, 
                                   Json::Value &result){

    if (step == 0) {
        generate_parse_err(result, "VM: step cannot be 0");
    }

    if (max_value < min_value ) {
        std::stringstream ss;
        ss << "VM: requested flow var max value: '" << max_value << "' is smaller than min value: " << min_value;
        generate_parse_err(result, ss.str());
    }

    if (flow_var_size == 1 ) {
        if ( (init_value > UINT8_MAX) || (min_value > UINT8_MAX) || (max_value > UINT8_MAX) || (step >UINT8_MAX) )  {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT8_MAX;
            generate_parse_err(result, ss.str());
        }
    }

    if (flow_var_size == 2 ) {
        if ( (init_value > UINT16_MAX) || (min_value > UINT16_MAX) || (max_value > UINT16_MAX) || (step > UINT16_MAX) )  {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT16_MAX;
            generate_parse_err(result, ss.str());
        }
    }

    if (flow_var_size == 4 ) {
        if ( (init_value > UINT32_MAX) || (min_value > UINT32_MAX) || (max_value > UINT32_MAX) || (step > UINT32_MAX) )  {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT32_MAX;
            generate_parse_err(result, ss.str());
        }
    }

}

void 
TrexRpcCmdAddStream::parse_vm_instr_flow_var_rand_limit(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);

    auto sizes = {1, 2, 4, 8};
    uint8_t      flow_var_size = parse_choice(inst, "size", sizes, result);
    uint64_t seed    = parse_uint64(inst, "seed", result);
    uint64_t limit   = parse_uint64(inst, "limit", result);
    uint64_t min_value   = parse_uint64(inst, "min_value", result);
    uint64_t max_value   = parse_uint64(inst, "max_value", result);

	/* archiecture limitation - limit_flows must be greater or equal to DP core count */
	if (limit < 1) {
		std::stringstream ss;
        ss << "VM: request random flow var variable with limit of zero '";
		generate_parse_err(result, ss.str());
	}

    check_min_max(flow_var_size, 0, 1, min_value, max_value, result);

    stream->m_vm.add_instruction(new StreamVmInstructionFlowRandLimit(flow_var_name,
                                                                      flow_var_size,
                                                                      limit,
                                                                      min_value,
                                                                      max_value,
                                                                      seed)
                                 );
}

void 
TrexRpcCmdAddStream::parse_vm_instr_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
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
    uint64_t step        = parse_uint64(inst, "step", result);

    check_min_max(flow_var_size, init_value, step, min_value, max_value, result);

    /* implicit range padding if possible */
    handle_range_padding(max_value,min_value,step, op_type, result);

    stream->m_vm.add_instruction(new StreamVmInstructionFlowMan(flow_var_name,
                                                                flow_var_size,
                                                                op_type,
                                                                init_value,
                                                                min_value,
                                                                max_value,
                                                                step)
                                 );
}


void 
TrexRpcCmdAddStream::parse_vm_instr_write_mask_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);
    uint16_t     pkt_offset    = parse_uint16(inst, "pkt_offset", result);
    uint16_t      pkt_cast_size = parse_uint16(inst, "pkt_cast_size", result);
    uint32_t     mask          = parse_uint32(inst, "mask", result);
    int          shift         = parse_int(inst, "shift", result);
    int          add_value     = parse_int(inst, "add_value", result);
    bool         is_big_endian = parse_bool(inst,   "is_big_endian", result);

    stream->m_vm.add_instruction(new StreamVmInstructionWriteMaskToPkt(flow_var_name,
                                                                       pkt_offset,
                                                                       (uint8_t)pkt_cast_size,
                                                                       mask,
                                                                       shift,
                                                                       add_value,
                                                                       is_big_endian));
}


void 
TrexRpcCmdAddStream::parse_vm_instr_write_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
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
TrexRpcCmdAddStream::parse_vm(const Json::Value &vm, std::unique_ptr<TrexStream> &stream, Json::Value &result) {

    const Json::Value &instructions =  parse_array(vm ,"instructions", result);

    /* array of VM instructions on vm */
    for (int i = 0; i < instructions.size(); i++) {
        const Json::Value & inst = parse_object(instructions, i, result);

        auto vm_types = {"fix_checksum_hw", "fix_checksum_ipv4", "flow_var", "write_flow_var","tuple_flow_var","trim_pkt_size","write_mask_flow_var","flow_var_rand_limit"};
        std::string vm_type = parse_choice(inst, "type", vm_types, result);

        // checksum instruction
        if (vm_type == "fix_checksum_ipv4") {
            parse_vm_instr_checksum(inst, stream, result);

        } else if (vm_type == "fix_checksum_hw") {

            parse_vm_instr_checksum_hw(inst, stream, result);

        } else if (vm_type == "flow_var") {
            parse_vm_instr_flow_var(inst, stream, result);

        } else if (vm_type == "flow_var_rand_limit") {
            parse_vm_instr_flow_var_rand_limit(inst, stream, result);

        } else if (vm_type == "write_flow_var") {
            parse_vm_instr_write_flow_var(inst, stream, result);

        } else if (vm_type == "tuple_flow_var") {
           parse_vm_instr_tuple_flow_var(inst, stream, result);

        } else if (vm_type == "trim_pkt_size") {
            parse_vm_instr_trim_pkt_size(inst, stream, result);
        }else if (vm_type == "write_mask_flow_var") {
            parse_vm_instr_write_mask_flow_var(inst, stream, result);
        } else {
            /* internal error */
            throw TrexRpcException("internal error");
        }
    }

    stream->m_cache_size      = parse_uint16(vm, "cache", result,0); /* default is zero */

}

void
TrexRpcCmdAddStream::validate_stream(const std::unique_ptr<TrexStream> &stream, Json::Value &result) {

    /* add the stream to the port's stream table */
    TrexStatelessPort * port = get_stateless_obj()->get_port_by_id(stream->m_port_id);

    /* does such a stream exists ? */
    if (port->get_stream_by_id(stream->m_stream_id)) {
        std::stringstream ss;
        ss << "stream " << stream->m_stream_id << " already exists";
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

    uint32_t stream_id = parse_uint32(params, "stream_id", result);
    TrexStream *stream = port->get_stream_by_id(stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream " << stream_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    try {
        port->remove_stream(stream);
    } catch (const TrexException &ex) {
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
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }


    result["result"] = Json::objectValue;

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
    uint32_t stream_id = parse_uint32(params, "stream_id", result);

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

    double    duration    = parse_double(params, "duration", result);
    bool      force       = parse_bool(params, "force", result);
    uint64_t  core_mask   = parse_uint64(params, "core_mask", result, TrexDPCoreMask::MASK_ALL);
    double    start_at_ts = parse_double(params, "start_at_ts", result, 0);

    /* check for a valid core mask */
    if (!TrexDPCoreMask::is_valid_mask(port->get_dp_core_count(), core_mask)) {
        std::stringstream ss;
        ss << "core_mask: '0x" << std::hex << core_mask << "' for " << std::to_string(port->get_dp_core_count()) << " cores generates an empty set"; 
        generate_parse_err(result, ss.str());
    }

    /* multiplier */
    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_string(mul_obj, "op", result);
    double      value  = parse_udouble(mul_obj, "value", result);
    
    if ( value == 0 ){
        generate_parse_err(result, "multiplier can't be zero");
    }

    if (op != "abs") {
        generate_parse_err(result, "start message can only specify absolute speed rate");
    }

    dsec_t ts = now_sec();
    TrexPortMultiplier mul(type, op, value);

    try {
        port->start_traffic(mul, duration, force, core_mask, start_at_ts);

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier();
    result["result"]["ts"]         = ts;
    
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
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/***************************
 * remove all hardware filters
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdRemoveRXFilters::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->remove_rx_filters();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

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
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdPauseStreams::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    const Json::Value &stream_ids_json = parse_array(params, "stream_ids", result);
    stream_ids_t stream_ids;

    for( const Json::Value &itr : stream_ids_json ) {
        uint32_t stream_id = itr.asUInt();
        TrexStream *stream = port->get_stream_by_id(stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "pause_streams: stream " << stream_id << " does not exists";
            generate_execute_err(result, ss.str());
        }
        stream_ids.insert(stream_id);
    }

    if ( stream_ids.empty() ) {
        generate_parse_err(result, "please specify streams to pause");
    }

    try {
        port->pause_streams(stream_ids);
    } catch (const TrexException &ex) {
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
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdResumeStreams::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    const Json::Value &stream_ids_json = parse_array(params, "stream_ids", result);
    stream_ids_t stream_ids;

    for( const Json::Value &itr : stream_ids_json ) {
        uint32_t stream_id = itr.asUInt();
        TrexStream *stream = port->get_stream_by_id(stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "resume_streams: stream " << stream_id << " does not exists";
            generate_execute_err(result, ss.str());
        }
        stream_ids.insert(stream_id);
    }

    if ( stream_ids.empty() ) {
        generate_parse_err(result, "please specify streams to resume");
    }

    try {
        port->resume_streams(stream_ids);
    } catch (const TrexException &ex) {
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

    bool force = parse_bool(params, "force", result);

    /* multiplier */

    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_choice(mul_obj, "op", TrexPortMultiplier::g_ops, result);
    double      value  = parse_double(mul_obj, "value", result);

    TrexPortMultiplier mul(type, op, value);


    try {
        port->update_traffic(mul, force);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier();

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdUpdateStreams::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    const Json::Value &stream_ids_json = parse_array(params, "stream_ids", result);
    std::vector <TrexStream *> streams;

    bool force = parse_bool(params, "force", result);

    /* multiplier */

    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_choice(mul_obj, "op", TrexPortMultiplier::g_ops, result);
    double      value  = parse_double(mul_obj, "value", result);

    TrexPortMultiplier mul(type, op, value);

    for( const Json::Value &itr : stream_ids_json ) {
        uint32_t stream_id = itr.asUInt();
        TrexStream *stream = port->get_stream_by_id(stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "update_streams: stream " << stream_id << " does not exists";
            generate_execute_err(result, ss.str());
        }
        streams.push_back(stream);
    }

    if ( streams.empty() ) {
        generate_parse_err(result, "please specify streams to update");
    }

    try {
        port->update_streams(mul, force, streams);
    } catch (const TrexException &ex) {
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
    
    /* max values */
    result["result"]["rate"]["max_bps_l2"]    = graph->get_max_bps_l2();
    result["result"]["rate"]["max_bps_l1"]    = graph->get_max_bps_l1();
    result["result"]["rate"]["max_pps"]       = graph->get_max_pps();
    result["result"]["rate"]["max_line_util"] = (graph->get_max_bps_l1() / port->get_port_speed_bps()) * 100.01;

    /* min values */
    result["result"]["rate"]["min_bps_l2"]    = graph->get_max_bps_l2(0);
    result["result"]["rate"]["min_bps_l1"]    = graph->get_max_bps_l1(0);
    result["result"]["rate"]["min_pps"]       = graph->get_max_pps(0);
    result["result"]["rate"]["min_line_util"] = (graph->get_max_bps_l1(0) / port->get_port_speed_bps()) * 100.01;

    result["result"]["graph"]["expected_duration"] = graph->get_duration();
    result["result"]["graph"]["events_count"] = (int)graph->get_events().size();

    result["result"]["graph"]["events"] = Json::arrayValue;
    Json::Value &events_json = result["result"]["graph"]["events"];

    int index = 0;
    for (const auto &ev : graph->get_events()) {
        Json::Value ev_json;

        ev_json["time_usec"]     = ev.time;
        ev_json["diff_bps_l2"]   = ev.diff_bps_l2;
        ev_json["diff_bps_l1"]   = ev.diff_bps_l1;
        ev_json["diff_pps"]      = ev.diff_pps;
        ev_json["stream_id"]     = ev.stream_id;

        events_json.append(ev_json);

        index++;
        if (index >= 100) {
            break;
        }
    }


    return (TREX_RPC_CMD_OK);
}


/**
 * push a remote PCAP on a port
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdPushRemote::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    std::string  pcap_filename  = parse_string(params, "pcap_filename", result);
    double       ipg_usec       = parse_double(params, "ipg_usec", result);
    double       min_ipg_sec    = usec_to_sec(parse_udouble(params, "min_ipg_usec", result, 0));
    double       speedup        = parse_udouble(params, "speedup", result);
    uint32_t     count          = parse_uint32(params, "count", result);
    double       duration       = parse_double(params, "duration", result);
    bool         is_dual        = parse_bool(params,   "is_dual", result, false);
    std::string  slave_handler  = parse_string(params, "slave_handler", result, "");

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    /* for dual mode - make sure slave_handler matches */
    if (is_dual) {
        TrexStatelessPort *slave = get_stateless_obj()->get_port_by_id(port_id ^ 0x1);
        if (!slave->get_owner().verify(slave_handler)) {
            generate_execute_err(result, "incorrect or missing slave port handler");
        }
    }


    /* IO might take time, increase timeout of WD inside this function */
    TrexWatchDog::IOFunction dummy;
    (void)dummy;

    try {
        port->push_remote(pcap_filename, ipg_usec, min_ipg_sec, speedup, count, duration, is_dual);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}


/**
 * set service mode on/off
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetServiceMode::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    bool enabled = parse_bool(params, "enabled", result);
    
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    try {
        port->set_service_mode(enabled);
    } catch (TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}

/****************************** component implementation ******************************/

/**
 * STL RPC component
 * 
 */
TrexRpcCmdsSTL::TrexRpcCmdsSTL() : TrexRpcComponent("STL") {
    
    /* PG IDs */
    m_cmds.push_back(new TrexRpcCmdGetActivePGIds(this));
    m_cmds.push_back(new TrexRpcCmdGetPGIdsStats(this));
    
    /* streams */
    m_cmds.push_back(new TrexRpcCmdGetStreamList(this));
    m_cmds.push_back(new TrexRpcCmdGetStream(this));
    m_cmds.push_back(new TrexRpcCmdGetAllStreams(this));
    m_cmds.push_back(new TrexRpcCmdAddStream(this));
    m_cmds.push_back(new TrexRpcCmdRemoveStream(this));
    m_cmds.push_back(new TrexRpcCmdRemoveAllStreams(this));
    
    /* traffic */
    m_cmds.push_back(new TrexRpcCmdStartTraffic(this));
    m_cmds.push_back(new TrexRpcCmdStopTraffic(this));
    m_cmds.push_back(new TrexRpcCmdPauseTraffic(this));
    m_cmds.push_back(new TrexRpcCmdPauseStreams(this));
    m_cmds.push_back(new TrexRpcCmdResumeTraffic(this));
    m_cmds.push_back(new TrexRpcCmdResumeStreams(this));
    m_cmds.push_back(new TrexRpcCmdUpdateTraffic(this));
    m_cmds.push_back(new TrexRpcCmdUpdateStreams(this));
    m_cmds.push_back(new TrexRpcCmdValidate(this));
    m_cmds.push_back(new TrexRpcCmdRemoveRXFilters(this));
    
    /* service mode */
    m_cmds.push_back(new TrexRpcCmdSetServiceMode(this));
    
    /* pcap */
    m_cmds.push_back(new TrexRpcCmdPushRemote(this));
}



