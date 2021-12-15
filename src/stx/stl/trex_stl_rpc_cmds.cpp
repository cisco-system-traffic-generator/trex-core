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
#include "trex_global.h"

#include <string>


using namespace std;

/****************************** commands declerations ******************************/

/**
 * ownership
 */
TREX_RPC_CMD(TrexRpcCmdAcquire,          "acquire");
TREX_RPC_CMD_OWNED(TrexRpcCmdRelease,    "release");

/**
 * PG ids
 */
TREX_RPC_CMD(TrexRpcCmdGetActivePGIds,   "get_active_pgids");
TREX_RPC_CMD(TrexRpcCmdGetPGIdsStats,    "get_pgid_stats");


/**
 * profiles
 */
TREX_RPC_CMD(TrexRpcCmdGetProfileList,   "get_profile_list");


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
void validate_stream(string profile_id, const std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm(const Json::Value &vm, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_checksum(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_checksum_hw(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_checksum_icmpv6(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);

void parse_vm_instr_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void parse_vm_instr_flow_var_rand_limit(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result);
void check_min_max(uint8_t flow_var_size, uint64_t init_value,uint64_t step,uint64_t min_value,uint64_t max_value,Json::Value &result);
void check_value_list(uint8_t flow_var_size, uint64_t step, std::vector<uint64_t> value_list, Json::Value &result);

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

/**
 * Tagged Packet Grouping
 */
TREX_RPC_CMD_EXT(TrexRpcCmdEnableTaggedPacketGroupCpRx, "enable_tpg_cp_rx", // Self verifies Ownership

/* extended part */
void _verify_ownership(Json::Value& result, const std::string& username, const uint32_t session_id, uint8_t port_id);
std::vector<uint8_t> parse_validate_ports(const Json::Value& ports, Json::Value& result, const std::string& username, const uint32_t session_id);
); 
TREX_RPC_CMD(TrexRpcCmdEnableTaggedPacketGroupDp, "enable_tpg_dp");      // Self verifies Ownership
TREX_RPC_CMD(TrexRpcCmdDisableTaggedPacketGroup, "disable_tpg");         // Need username to disable
TREX_RPC_CMD(TrexRpcCmdDestroyTaggedPacketGroupCtx, "destroy_tpg_ctx");  // Need username to destroy
TREX_RPC_CMD(TrexRpcCmdGetTaggedPktGroupState, "get_tpg_state");         // Should not be used by user which is not us.
TREX_RPC_CMD(TrexRpcCmdGetTaggedPktGroupStatus, "get_tpg_status");       // Can provide username/port.
TREX_RPC_CMD(TrexRpcCmdGetTaggedPktGroupStats, "get_tpg_stats");         // Any user and any port. No need to own.



/****************************** commands implementation ******************************/


/**
 * acquire device
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAcquire::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    const std::string  &new_owner  = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);
    uint32_t session_id = parse_uint32(params, "session_id", result);

    /* if not free and not you and not force - fail */
    TrexPort *port = get_stx()->get_port_by_id(port_id);

    try {
        port->acquire(new_owner, session_id, force);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    get_stx()->add_session_id(port_id, session_id);

    result["result"] = port->get_owner().get_handler();

    return (TREX_RPC_CMD_OK);
}


/**
 * release device
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdRelease::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort* port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->release();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    get_stx()->remove_session_id(port_id);

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}


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

    for (auto &it : active_flow_stat) {
        if (it.m_type == PGID_FLOW_STAT) {
            section["ids"]["flow_stats"][i++] = it.m_pg_id;
        } else if (it.m_type == PGID_LATENCY) {
            section["ids"]["latency"][j++] = it.m_pg_id;
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

    /* iterate over list */
    for (auto &itr : pgids) {
        try {
            pgids_arr.push_back(itr.asUInt());
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
 * get all profiles configured 
 * on a specific port 
 * 
 **************************/
trex_rpc_cmd_rc_e
TrexRpcCmdGetProfileList::_run(const Json::Value &params, Json::Value &result) {
    std::vector<string> profile_list;

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    port->get_profile_id_list(profile_list);

    Json::Value json_profile_list = Json::arrayValue;

    for (auto &profile_id : profile_list) {
        json_profile_list.append(profile_id);
    }

    result["result"] = json_profile_list;

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

    string profile_id = parse_profile(params, result);
    Json::Value json_list = Json::arrayValue;

    if (profile_id == "*") {
        Json::Value json_profile_stream_list = Json::objectValue;
        std::vector<string> profile_list;
        port->get_profile_id_list(profile_list);

        for (auto &profile_id : profile_list) {
            port->get_id_list(profile_id, stream_list);
            Json::Value json_list = Json::arrayValue;

            for (auto &stream_id : stream_list) {
                json_list.append(stream_id);
            }
            
            std::stringstream ss;
            ss << profile_id;
            json_profile_stream_list[ss.str()] = json_list; 
        }

        result["result"] = json_profile_stream_list;

    } else {
        port->get_id_list(profile_id, stream_list);
        Json::Value json_list = Json::arrayValue;

        for (auto &stream_id : stream_list) {
            json_list.append(stream_id);
        }
    
        result["result"] = json_list;
    }

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

    string profile_id = parse_profile(params, result);
    
    Json::Value streams_json = Json::objectValue;
    Json::Value profiles_json = Json::objectValue;

    if (profile_id == "*") {
        std::vector <TrexStatelessProfile *> profiles;
        port->get_profile_object_list(profiles);

        for (auto &mprofile : profiles) {
            string profile_id_prt = mprofile->m_profile_id;
            
            std::vector <TrexStream *> streams;
            port->get_object_list(profile_id_prt, streams);
    
            std::stringstream ps;
            ps << profile_id_prt;
    
            std::vector<uint32_t> stream_list;
            port->get_id_list(profile_id_prt, stream_list);
            Json::Value json_list = Json::arrayValue;

            for (auto &stream : streams) {
                Json::Value j = stream->get_stream_json();
                std::stringstream ss;
                ss << stream->m_stream_id;
                streams_json[ss.str()] = j;
                profiles_json[ps.str()] = streams_json;
            }
            streams_json.clear();

            for (auto &stream_id : stream_list) {
                json_list.append(stream_id);
            }
        }
           result["result"]["profiles"] = profiles_json;

    } else {
        std::vector <TrexStream *> streams;
        if (port->get_profile_by_id(profile_id)) {
            port->get_object_list(profile_id, streams);

            for (auto &stream : streams) {
                Json::Value j = stream->get_stream_json();
                std::stringstream ss;
                ss << stream->m_stream_id;
                streams_json[ss.str()] = j; 
            }    
         }    
         result["result"]["streams"] = streams_json;
    }

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

        string profile_id = parse_profile(params, result);

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

        /* use specific core */
        int core_id = parse_int(section, "core_id", result, -1);
        /* if core_id is negative then it wasn't specified, default value is negative */
        stream->m_core_id_specified = (core_id < 0) ? false : true ;
        /* if core ID is specified, check if core ID is smaller than the number of cores */
        if (stream->m_core_id_specified) {
            uint8_t m_dp_core_count = get_platform_api().get_dp_core_count();
            if  (core_id >= m_dp_core_count) {
                std::stringstream ss;
                if (m_dp_core_count == 1) {
                    ss << "There is only one core, hence core ID must be 0." ;
                } else {
                    ss << "Core ID is: " << core_id << ". It must be an integer between 0 and " << m_dp_core_count - 1 << " (inclusive).";
                }
                generate_execute_err(result, ss.str());
            }
            stream->m_core_id = (uint8_t)core_id;
        }

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
            stream->m_rx_check.m_vxlan_skip = parse_bool(rx, "vxlan", result, false);
            stream->m_rx_check.m_ieee_1588  = parse_bool(rx, "ieee_1588", result, false);
            if(stream->m_rx_check.m_ieee_1588 == true) {
                /* User enabled IEEE 1588 for Latency Measurement. Verify if its possible */
                const TrexPlatformApi &api = get_platform_api();
                if ( !(api.getPortAttrObj(stream->m_port_id)->is_ieee1588_supported())) {
                    std::stringstream ss;
                    ss << "Driver doesn't support IEEE-1588.";
                    generate_execute_err(result, ss.str());
                }
            }
            std::string type = parse_string(rx, "rule_type", result);
            if (type == "latency") {
                stream->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_PAYLOAD;
                if (stream->m_core_id_specified) {
                    std::stringstream ss;
                    ss << "Core ID pinning is not supported for latency streams.";
                    generate_execute_err(result, ss.str());
                }
            } else if (type == "tpg") {
                // Tagged Packet Group
                stream->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_TPG_PAYLOAD;
            } else {
                // Default to Flow Stats with IP Id
                stream->m_rx_check.m_rule_type = TrexPlatformApi::IF_STAT_IPV4_ID;
            }
        }

        /* make sure this is a valid stream to add */
        validate_stream(profile_id, stream, result);
        port->add_stream(profile_id, stream.get());
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

        if (total_pkts == 0) {
            generate_parse_err(result, type + ": 'total_pkts' cannot be zero.");
        }
        stream.reset(new TrexStream(TrexStream::stSINGLE_BURST, port_id, stream_id));
        stream->set_single_burst(total_pkts);


    } else if (type == "multi_burst") {

        double    ibg_usec         = parse_udouble(mode, "ibg", result);
        uint32_t  num_bursts       = parse_uint32(mode, "count", result);
        uint32_t  pkts_per_burst   = parse_uint32(mode, "pkts_per_burst", result);

        if (pkts_per_burst == 0) {
            generate_parse_err(result, type + ": 'pkts_per_burst' cannot be zero.");
        }
        stream.reset(new TrexStream(TrexStream::stMULTI_BURST, port_id, stream_id));
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
TrexRpcCmdAddStream::parse_vm_instr_checksum_icmpv6(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    uint16_t l2_len = parse_uint16(inst, "l2_len", result); 
    uint16_t l3_len = parse_uint16(inst, "l3_len", result); 

    stream->m_vm.add_instruction(new StreamVmInstructionFixChecksumIcmpv6(l2_len, l3_len));
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

    /* Architecture limitation - limit_flows must be greater or equal to DP core count */
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

    if (init_value < min_value || init_value > max_value) {
        std::stringstream ss;
        ss << "VM: requested flow var init value: '" << init_value << "' should be in range from min value '" << min_value << "' to max value '" << max_value << "'";
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
TrexRpcCmdAddStream::check_value_list(uint8_t flow_var_size,
                                      uint64_t step,
                                      std::vector<uint64_t> value_list,
                                      Json::Value &result) {

    if (step == 0) {
        generate_parse_err(result, "VM: step cannot be 0");
    }

    if (step > (uint64_t)value_list.size()) {
        generate_parse_err(result, "VM: step cannot be bigger than list size");
    }

    if (value_list.size() > UINT16_MAX) {
        std::stringstream ss;
        ss << "VM: value_list size is bigger than " << UINT16_MAX;
        generate_parse_err(result, ss.str());
    }

    for (int i = 0; i < (int)value_list.size(); i++) {
        if (flow_var_size == 1 && value_list[i] > UINT8_MAX) {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT8_MAX;
            generate_parse_err(result, ss.str());
        }
        else if (flow_var_size == 2 && value_list[i] > UINT16_MAX) {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT16_MAX;
            generate_parse_err(result, ss.str());
        }
        else if (flow_var_size == 4 && value_list[i] > UINT32_MAX) {
            std::stringstream ss;
            ss << "VM: requested value is bigger than " << UINT32_MAX;
            generate_parse_err(result, ss.str());
        }
    }
}

void 
TrexRpcCmdAddStream::parse_vm_instr_flow_var_rand_limit(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);
    std::string next_var_name = parse_string(inst, "next_var", result, "");
    auto sizes = {1, 2, 4, 8};
    uint8_t      flow_var_size = parse_choice(inst, "size", sizes, result);
    uint64_t seed    = parse_uint64(inst, "seed", result);
    uint64_t limit   = parse_uint64(inst, "limit", result);
    uint64_t min_value   = parse_uint64(inst, "min_value", result);
    uint64_t max_value   = parse_uint64(inst, "max_value", result);
    bool is_split_needed   = parse_bool(inst, "split_to_cores", result, true);

    /* architecture limitation - limit_flows must be greater or equal to DP core count */
    if (limit < 1) {
        std::stringstream ss;
        ss << "VM: request random flow var variable with limit of zero '";
        generate_parse_err(result, ss.str());
    }

    check_min_max(flow_var_size, min_value, 1, min_value, max_value, result);

    stream->m_vm.add_instruction(new StreamVmInstructionFlowRandLimit(flow_var_name,
                                                                      flow_var_size,
                                                                      limit,
                                                                      min_value,
                                                                      max_value,
                                                                      seed,
                                                                      is_split_needed,
                                                                      next_var_name)
                                 );
}

void 
TrexRpcCmdAddStream::parse_vm_instr_flow_var(const Json::Value &inst, std::unique_ptr<TrexStream> &stream, Json::Value &result) {
    std::string  flow_var_name = parse_string(inst, "name", result);

    std::string next_var_name = parse_string(inst, "next_var", result, "");

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

    const Json::Value &array = parse_array(inst, "value_list", result, Json::Value::null);
    uint64_t step            = parse_uint64(inst, "step", result);
    bool is_split_needed   = parse_bool(inst, "split_to_cores", result, true);

    if (array == Json::Value::null) {
        uint64_t init_value  = parse_uint64(inst, "init_value", result);
        uint64_t min_value   = parse_uint64(inst, "min_value", result);
        uint64_t max_value   = parse_uint64(inst, "max_value", result);

        check_min_max(flow_var_size, init_value, step, min_value, max_value, result);

        /* implicit range padding if possible */
        handle_range_padding(max_value,min_value,step, op_type, result);

        stream->m_vm.add_instruction(new StreamVmInstructionFlowMan(flow_var_name,
                                                                    flow_var_size,
                                                                    op_type,
                                                                    init_value,
                                                                    min_value,
                                                                    max_value,
                                                                    step,
                                                                    is_split_needed,
                                                                    next_var_name)
                                     );
    }
    else {
        std::vector<uint64_t> value_list;

        for (Json::Value::ArrayIndex i = 0; i != array.size(); i++) {
            value_list.push_back(array[i].asUInt64());
        }

        check_value_list(flow_var_size, step, value_list, result);

        stream->m_vm.add_instruction(new StreamVmInstructionFlowMan(flow_var_name,
                                                                    flow_var_size,
                                                                    op_type,
                                                                    value_list,
                                                                    step,
                                                                    is_split_needed,
                                                                    next_var_name)
                                     );
    }
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

        auto vm_types = {"fix_checksum_hw", "fix_checksum_ipv4", "fix_checksum_icmpv6", "flow_var", "write_flow_var","tuple_flow_var","trim_pkt_size","write_mask_flow_var","flow_var_rand_limit"};
        std::string vm_type = parse_choice(inst, "type", vm_types, result);

        // checksum instruction
        if (vm_type == "fix_checksum_ipv4") {
            parse_vm_instr_checksum(inst, stream, result);

        } else if (vm_type == "fix_checksum_hw") {

            parse_vm_instr_checksum_hw(inst, stream, result);

        } else if (vm_type == "fix_checksum_icmpv6") {
            parse_vm_instr_checksum_icmpv6(inst, stream, result);

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
TrexRpcCmdAddStream::validate_stream(string profile_id, const std::unique_ptr<TrexStream> &stream, Json::Value &result) {

    /* add the stream to the port's stream table */
    TrexStatelessPort * port = get_stateless_obj()->get_port_by_id(stream->m_port_id);

    /* does such a stream exists ? */
    if ( port->get_profile_by_id(profile_id)) {
        if (port->get_stream_by_id(profile_id, stream->m_stream_id)) {
            std::stringstream ss;
            ss << "stream " << stream->m_stream_id << " already exists in profile_id(" << profile_id << ")";
            generate_execute_err(result, ss.str());
        }
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

    string profile_id = parse_profile(params, result);
       
    uint32_t stream_id = parse_uint32(params, "stream_id", result);
    TrexStream *stream = port->get_stream_by_id(profile_id, stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream " << stream_id << " does not exists in profile_id(" << profile_id << ")";
        generate_execute_err(result, ss.str());
    }

    try {
        port->remove_stream(profile_id, stream);
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
    string profile_id = parse_profile(params, result);

    if (port->get_profile_by_id(profile_id)) {
        try {
            port->remove_and_delete_all_streams(profile_id);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }
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

    string profile_id = parse_profile(params, result);

    bool     get_pkt   = parse_bool(params, "get_pkt", result);
    uint32_t stream_id = parse_uint32(params, "stream_id", result);

    if (!(port->get_profile_by_id(profile_id))) {
        std::stringstream ss;
        ss << "profile_id " << profile_id << " does not exists";
        generate_execute_err(result, ss.str());
    }

    TrexStream *stream = port->get_stream_by_id(profile_id, stream_id);

    if (!stream) {
        std::stringstream ss;
        ss << "stream id " << stream_id << ", profile id " << profile_id <<" on port " << (int)port_id << " does not exists";
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

    string profile_id = parse_profile(params, result);

    if ( get_platform_api().hw_rx_stat_supported() && port->has_flow_stats(profile_id) ) {
        for (auto &port : get_stateless_obj()->get_port_map()) {
            if ( port.second->is_service_mode_on() ) {
                generate_execute_err(result, "Port " + to_string(port.first) + " is under service mode, can't use flow_stats."
                                             " (see https://trex-tgn.cisco.com/youtrack/issue/trex-545)");
            }
        }
    }

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
    
    if ( value == 0 ) {
        generate_parse_err(result, "multiplier can't be zero");
    }

    if (op != "abs") {
        generate_parse_err(result, "start message can only specify absolute speed rate");
    }

    dsec_t ts = now_sec();
    TrexPortMultiplier mul(type, op, value);

    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    try {
        port->start_traffic(profile_id, mul, duration, force, core_mask, start_at_ts);

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier(profile_id);
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
    string profile_id = parse_profile(params, result);

    try {
        port->stop_traffic(profile_id);
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
    string profile_id = parse_profile(params, result);

    try {
        port->remove_rx_filters(profile_id);
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

    string profile_id = parse_profile(params, result);

    try {
        port->pause_traffic(profile_id);
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
    string profile_id = parse_profile(params, result);

    const Json::Value &stream_ids_json = parse_array(params, "stream_ids", result);
    stream_ids_t stream_ids;

    for( const Json::Value &itr : stream_ids_json ) {
        uint32_t stream_id = itr.asUInt();
        TrexStream *stream = port->get_stream_by_id(profile_id, stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "pause_streams: stream " << stream_id << " does not exists in profile_id(" << profile_id << ")";
            generate_execute_err(result, ss.str());
        }
        stream_ids.insert(stream_id);
    }

    if ( stream_ids.empty() ) {
        generate_parse_err(result, "please specify streams to pause");
    }

    try {
        port->pause_streams(profile_id, stream_ids);
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

    string profile_id = parse_profile(params, result);

    try {
        port->resume_traffic(profile_id);
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

    string profile_id = parse_profile(params, result);

    const Json::Value &stream_ids_json = parse_array(params, "stream_ids", result);
    stream_ids_t stream_ids;

    for( const Json::Value &itr : stream_ids_json ) {
        uint32_t stream_id = itr.asUInt();
        TrexStream *stream = port->get_stream_by_id(profile_id, stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "resume_streams: stream " << stream_id << " does not exists in profile_id(" << profile_id << ")";
            generate_execute_err(result, ss.str());
        }
        stream_ids.insert(stream_id);
    }

    if ( stream_ids.empty() ) {
        generate_parse_err(result, "please specify streams to resume");
    }

    try {
        port->resume_streams(profile_id, stream_ids);
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

    string profile_id = parse_profile(params, result);

    bool force = parse_bool(params, "force", result);

    /* multiplier */

    const Json::Value &mul_obj  = parse_object(params, "mul", result);

    std::string type   = parse_choice(mul_obj, "type", TrexPortMultiplier::g_types, result);
    std::string op     = parse_choice(mul_obj, "op", TrexPortMultiplier::g_ops, result);
    double      value  = parse_double(mul_obj, "value", result);

    TrexPortMultiplier mul(type, op, value);

    try {
        port->update_traffic(profile_id, mul, force);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier(profile_id);

    return (TREX_RPC_CMD_OK);
}


trex_rpc_cmd_rc_e
TrexRpcCmdUpdateStreams::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    string profile_id = parse_profile(params, result);

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
        TrexStream *stream = port->get_stream_by_id(profile_id, stream_id);
        if (!stream) {
            std::stringstream ss;
            ss << "update_streams: stream " << stream_id << " does not exists in profile_id(" << profile_id << ")";
            generate_execute_err(result, ss.str());
        }
        streams.push_back(stream);
    }

    if ( streams.empty() ) {
        generate_parse_err(result, "please specify streams to update");
    }

    try {
        port->update_streams(profile_id, mul, force, streams);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["multiplier"] = port->get_multiplier(profile_id);

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

    string profile_id = parse_profile(params, result);

    const TrexStreamsGraphObj *graph = NULL;

    try {
        graph = port->validate(profile_id);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    Json::Value &res = result["result"];
    /* max values */
    res["rate"]["max_bps_l2"]    = graph->get_max_bps_l2();
    res["rate"]["max_bps_l1"]    = graph->get_max_bps_l1();
    res["rate"]["max_pps"]       = graph->get_max_pps();
    res["rate"]["max_line_util"] = (graph->get_max_bps_l1() / port->get_port_speed_bps()) * 100.01;

    /* min values */
    res["rate"]["min_bps_l2"]    = graph->get_max_bps_l2(0);
    res["rate"]["min_bps_l1"]    = graph->get_max_bps_l1(0);
    res["rate"]["min_pps"]       = graph->get_max_pps(0);
    res["rate"]["min_line_util"] = (graph->get_max_bps_l1(0) / port->get_port_speed_bps()) * 100.01;

    res["graph"]["expected_duration"] = graph->get_duration();
    res["graph"]["events_count"] = (int)graph->get_events().size();

    res["graph"]["events"] = Json::arrayValue;

    int index = 0;
    for (const auto &ev : graph->get_events()) {
        Json::Value ev_json;

        ev_json["time_usec"]     = ev.time;
        ev_json["diff_bps_l2"]   = ev.diff_bps_l2;
        ev_json["diff_bps_l1"]   = ev.diff_bps_l1;
        ev_json["diff_pps"]      = ev.diff_pps;
        ev_json["stream_id"]     = ev.stream_id;

        result["result"]["graph"]["events"].append(ev_json);

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
    bool filtered = parse_bool(params, "filtered", result, false);
    uint8_t mask = parse_byte(params, "mask", result, 0);

    if ( filtered ) {
        if ( CGlobalInfo::m_dpdk_mode.get_mode()->is_hardware_filter_needed() ) {
            generate_execute_err(result, "TRex must be at --software mode for filtered service!");
        }
    }

    if ( enabled && get_platform_api().hw_rx_stat_supported() ) {
        for (auto &port : get_stateless_obj()->get_port_map()) {
            if ( port.second->is_running_flow_stats() ) {
                string err = "port " + to_string(port.first) + " is using flow_stats."
                             " (see https://trex-tgn.cisco.com/youtrack/issue/trex-545)";
                generate_execute_err(result, err);
            }
        }
    }

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    try {
        port->set_service_mode(enabled, filtered, mask);
    } catch (TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}


/***************************
 * Tagged Packet Group
 **************************/

void TrexRpcCmdEnableTaggedPacketGroupCpRx::_verify_ownership(Json::Value& result,
                                                              const std::string& username,
                                                              const uint32_t session_id,
                                                              uint8_t port_id) {
    // Verifies ownership on the port at username + session level.
    TrexPort *port = get_stx()->get_port_by_id(port_id);

    if (port->get_owner().is_free()) {
        std::string err_msg = "Please acquire port " + std::to_string(unsigned(port_id)) + " before using it for Tagged Packet Group";
        generate_execute_err(result, err_msg);
    }

    if (!port->get_owner().is_owned_by(username)) {
        std::string err_msg = "Port " + std::to_string(unsigned(port_id)) + " is not owned by you.";
        generate_execute_err(result, err_msg);
    }

    if (port->get_owner().get_session_id() != session_id) {
        std::string err_msg = "Port " + std::to_string(unsigned(port_id)) + " is owned by another session of " + username;
        generate_execute_err(result, err_msg);
    }
}

std::vector<uint8_t> TrexRpcCmdEnableTaggedPacketGroupCpRx::parse_validate_ports(const Json::Value& ports,
                                                                                 Json::Value& result,
                                                                                 const std::string& username,
                                                                                 const uint32_t session_id) {
    std::vector<uint8_t> ports_vec;
    uint8_t num_ports = CGlobalInfo::m_options.get_expected_ports(); // Num of ports in the system
    for (auto &itr : ports) {
        uint8_t port_id = itr.asUInt();
        if (port_id >= num_ports) {
            std::string err_msg = "Invalid port number: " + std::to_string(port_id) + " Max port number: " + std::to_string(num_ports - 1);
            generate_execute_err(result, err_msg);
            }
            _verify_ownership(result, username, session_id, port_id);
            ports_vec.push_back(port_id);
    }
    return ports_vec;
}

trex_rpc_cmd_rc_e
TrexRpcCmdEnableTaggedPacketGroupCpRx::_run(const Json::Value &params, Json::Value &result) {

    // Verify software mode
    if (CGlobalInfo::m_dpdk_mode.get_mode()->is_hardware_filter_needed()) {
        generate_execute_err(result, "Tagged Packet Group can be enabled only in software mode");
    }

    // Let's verify username and ports
    const std::string username = parse_string(params, "username", result);    // Username
    const uint32_t session_id = parse_uint32(params, "session_id", result);   // Session Id
    const Json::Value& acquired_ports = parse_array(params, "ports", result); // List of acquired ports the user provided.
    const Json::Value& rx_ports = parse_array(params, "rx_ports", result);    // List of Rx ports the user provided.

    std::vector<uint8_t> acquired_ports_vec = parse_validate_ports(acquired_ports, result, username, session_id);
    std::vector<uint8_t> rx_ports_vec = parse_validate_ports(rx_ports, result, username, session_id);

    std::set<uint8_t> acquired_ports_set(acquired_ports_vec.begin(), acquired_ports_vec.end()); // Convert vector to set for fast lookup

    // Verify Rx ports are acquired.
    for (auto& rx_port : rx_ports_vec) {
        if (acquired_ports_set.find(rx_port) == acquired_ports_set.end()) {
            generate_execute_err(result, "Rx Ports must be acquired!");
        }
    }

    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = stl->get_tpg_ctx(username);

    if (tpg_ctx != nullptr && tpg_ctx->get_tpg_state() != TPGState::DISABLED) {
        // Should be able to enable iff the feature is disabled.
        generate_execute_err(result, "Tagged Packet Grouping is already enabled for this user!");
    }

    uint32_t num_tpgids = parse_uint32(params, "num_tpgids", result);  // Num tpgids the user provided

    // If we got here, the feature is not enabled.
    if (!stl->create_tpg_ctx(username, acquired_ports_vec, rx_ports_vec, num_tpgids)) {
        std::string err_msg = "Failed creating TPG context for " + username;
        generate_execute_err(result, err_msg);
    }
    tpg_ctx = stl->get_tpg_ctx(username);
    assert(tpg_ctx != nullptr); // This can't be nullptr now.
    PacketGroupTagMgr* tag_mgr = tpg_ctx->get_tag_mgr();

    const Json::Value& tags = parse_array(params, "tags", result);
    uint16_t tag = 0;
    std::string err_msg = "";
    bool error = false;
    for (auto& itr: tags) {
        const Json::Value& value = parse_object(itr, "value", result);
        std::string type = parse_string(itr, "type", result);
        if (type == "Dot1Q") {
            uint16_t vlan = parse_uint16(value, "vlan", result);
            if (!tag_mgr->add_dot1q_tag(vlan, tag)) {
                err_msg = "Vlan " + std::to_string(vlan) + " already exists or is invalid!";
                error = true;
                break;
            }
        } else if (type == "QinQ") {
            const Json::Value& vlans = parse_array(value, "vlans", result);
            if (vlans.size() != 2) {
                err_msg = "QinQ must contain 2 Vlans!";
                error = true;
                break;
            }
            uint16_t inner_vlan = vlans[0].asUInt();
            uint16_t outter_vlan = vlans[1].asUInt();
            if (!tag_mgr->add_qinq_tag(inner_vlan, outter_vlan, tag)) {
                err_msg = "QinQ (" + std::to_string(inner_vlan) + "," + std::to_string(outter_vlan) + ") already exists or is invalid!";
                error = true;
                break;
            }
        } else {
            err_msg = "Tag type " + type + " is not supported!";
            error = true;
            break;
        }
        tag++;
    }

    if (error) {
        stl->destroy_tpg_ctx(username);
        generate_execute_err(result, err_msg);
    }

    // Notify CP, Rx. Rx can take a long time to allocate, so we do this async.
    stl->enable_tpg_cp_rx(username);

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdEnableTaggedPacketGroupDp::_run(const Json::Value &params, Json::Value &result) {

    const std::string username = parse_string(params, "username", result);

    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = stl->get_tpg_ctx(username);

    if (tpg_ctx == nullptr || tpg_ctx->get_tpg_state() != TPGState::ENABLED_CP_RX) {
        // Enabling the feature in DP is possible iff the feature is previously enabled in CP and Rx.
        generate_execute_err(result, "Tagged Packet Group in Dp can be enabled only after Cp and Rx!");
    }

    stl->enable_tpg_dp(username);

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdDisableTaggedPacketGroup::_run(const Json::Value &params, Json::Value &result) {

    const std::string username = parse_string(params, "username", result);
    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = stl->get_tpg_ctx(username);

    if (tpg_ctx == nullptr || tpg_ctx->get_tpg_state() != TPGState::ENABLED) {
        // Disabling the feature is possible only if the feature is already enabled.
        generate_execute_err(result, "Tagged Packet Group is not enabled for this user!");
    }

    /* No need to verify the result of the following function since we already 
       verified context exists */
    stl->disable_tpg(username);    // Notify DPs, CP and Rx (async)

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdDestroyTaggedPacketGroupCtx::_run(const Json::Value &params, Json::Value &result) {

    const std::string username = parse_string(params, "username", result);
    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = stl->get_tpg_ctx(username);

    if (tpg_ctx == nullptr || tpg_ctx->get_tpg_state() != TPGState::DISABLED_DP_RX) {
        // Disabling the feature is possible only if the feature is already enabled.
        generate_execute_err(result, "Tagged Packet Group is not disabled for this user! Can't destroy.");
    }

    /* No need to verify the result of the following function since we already 
       verified context exists */
    stl->destroy_tpg_ctx(username);

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdGetTaggedPktGroupState::_run(const Json::Value &params, Json::Value &result) {

    const std::string username = parse_string(params, "username", result);

    TrexStateless* stl = get_stateless_obj();

    TPGState state = stl->update_tpg_state(username);

    // TPGState is an enum class, hence we need to cast it back to an integer.
    result["result"] = static_cast<int>(state);

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdGetTaggedPktGroupStatus::_run(const Json::Value &params, Json::Value &result) {

    const std::string username = parse_string(params, "username", result, "");
    uint8_t INVALID_PORT = 0xFF;
    uint8_t port_id = INVALID_PORT;
    if (params["port_id"] != Json::Value::null) {
            port_id = parse_port(params, result);
    }

    if (!username.empty() && port_id != INVALID_PORT) {
        generate_execute_err(result, "Should provide only one between port_id and username to get TPG status");
    }

    if (username.empty() && port_id == INVALID_PORT) {
        generate_execute_err(result, "Should provide at least one between port_id and username to get TPG status");
    }

    // Verified that only one parameter between username and port_id provided.
    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = nullptr;
    if (!username.empty()) {
        tpg_ctx = stl->get_tpg_ctx(username);
    } else {
        // Port is provided
        tpg_ctx = stl->get_port_by_id(port_id)->get_tpg_ctx();
    }

    Json::Value& status = result["result"];

    bool enabled = false;
    if (tpg_ctx) {
        enabled = tpg_ctx->get_tpg_state() == TPGState::ENABLED;
    }

    status["enabled"] = enabled;

    Json::Value& status_data = status["data"];

    std::vector<uint8_t> acquired_ports;
    std::vector<uint8_t> rx_ports;
    uint32_t num_tpgids = 0;
    uint16_t num_tags = 0;
    std::string tpg_user = "-";

    if (enabled) {
        acquired_ports = tpg_ctx->get_acquired_ports();
        rx_ports = tpg_ctx->get_rx_ports();
        num_tpgids = tpg_ctx->get_num_tpgids();
        num_tags = tpg_ctx->get_tag_mgr()->get_num_tags();
        tpg_user = tpg_ctx->get_username();
    }

    status_data["acquired_ports"] = Json::arrayValue;
    for (int i = 0; i < acquired_ports.size(); i++) {
        status_data["acquired_ports"][i] = acquired_ports[i];
    }
    status_data["rx_ports"] = Json::arrayValue;
    for (int i = 0; i < rx_ports.size(); i++) {
        status_data["rx_ports"][i] = rx_ports[i];
    }
    status_data["username"] = tpg_user;
    status_data["num_tpgids"] = num_tpgids;
    status_data["num_tags"] = num_tags;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdGetTaggedPktGroupStats::_run(const Json::Value &params, Json::Value &result) {


    uint8_t port_id = parse_port(params, result);  // Validates the port aswell.
    uint32_t tpgid = parse_uint32(params, "tpgid", result);  // Tagged Packet Group Identifier
    uint16_t min_tag = parse_uint16(params, "min_tag", result);  // Min Tag
    uint16_t max_tag = parse_uint16(params, "max_tag", result);  // Max Tag
    bool unknown_tag = parse_bool(params, "unknown_tag", result, false);  // Unknown Tag, default False.

    TrexStateless* stl = get_stateless_obj();
    TPGCpCtx* tpg_ctx = stl->get_port_by_id(port_id)->get_tpg_ctx();

    if (tpg_ctx == nullptr || tpg_ctx->get_tpg_state() != TPGState::ENABLED) {
        // Collecting stats is possible only if the feature is already enabled.
        generate_execute_err(result, "Tagged Packet Group is not enabled on this port.");
    }

    // Server Side Validation
    if (min_tag >= max_tag) {
        generate_execute_err(result, "Min Tag = " + std::to_string(min_tag) + " must be smaller than Max Tag = " + std::to_string(max_tag));
    }

    uint16_t num_tags = tpg_ctx->get_tag_mgr()->get_num_tags();

    if (max_tag > num_tags) {
        generate_execute_err(result, "Max Tag " + std::to_string(max_tag) + " is larger than number of tags " + std::to_string(num_tags));
    }

    if (!tpg_ctx->is_port_collecting(port_id)) {
        generate_execute_err(result, "Port " + std::to_string(unsigned(port_id)) + " is not a valid port for TPG stats.");
    }

    if (tpgid >= tpg_ctx->get_num_tpgids()) {
        generate_execute_err(result, "tpgid " + std::to_string(tpgid) + " is bigger than the max allowed tpgid.");
    }

    // Collect Stats from Rx
    Json::Value& section = result["result"];

    stl->get_stl_rx()->get_tpg_stats(section, port_id, tpgid, min_tag, max_tag, unknown_tag);

    return (TREX_RPC_CMD_OK);
}

/****************************** component implementation ******************************/

/**
 * STL RPC component
 * 
 */
TrexRpcCmdsSTL::TrexRpcCmdsSTL() : TrexRpcComponent("STL") {

    /* ownership */
    m_cmds.push_back(new TrexRpcCmdAcquire(this));
    m_cmds.push_back(new TrexRpcCmdRelease(this));

    /* PG IDs */
    m_cmds.push_back(new TrexRpcCmdGetActivePGIds(this));
    m_cmds.push_back(new TrexRpcCmdGetPGIdsStats(this));

    /* profiles */
    m_cmds.push_back(new TrexRpcCmdGetProfileList(this));

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

    /* Tagged Packet Group */
    m_cmds.push_back(new TrexRpcCmdEnableTaggedPacketGroupCpRx(this));
    m_cmds.push_back(new TrexRpcCmdEnableTaggedPacketGroupDp(this));
    m_cmds.push_back(new TrexRpcCmdDisableTaggedPacketGroup(this));
    m_cmds.push_back(new TrexRpcCmdDestroyTaggedPacketGroupCtx(this));
    m_cmds.push_back(new TrexRpcCmdGetTaggedPktGroupState(this));
    m_cmds.push_back(new TrexRpcCmdGetTaggedPktGroupStatus(this));
    m_cmds.push_back(new TrexRpcCmdGetTaggedPktGroupStats(this));
}



