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
#include <../linux_dpdk/version.h>
#include <trex_rpc_server_api.h>
#include <trex_stream_api.h>
#include <trex_stateless_api.h>

#include <iostream>

using namespace std;

/**
 * add new stream
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    TrexStream *stream;

    check_param_count(params, 1, result);

    const Json::Value &section = parse_object(params, "stream", result);

    /* get the type of the stream */
    const Json::Value &mode = parse_object(section, "mode", result);
    string type = parse_string(mode, "type", result);

    if (type == "continuous") {
        stream = new TrexStreamContinuous();
    } else if (type == "single_burst") {
        stream = new TrexStreamSingleBurst();
    } else if (type == "multi_burst") {
        stream = new TrexStreamMultiBurst();
    } else {
        generate_err(result, "bad stream type provided: '" + type + "'");
    }

    if (!stream) {
        generate_internal_err(result, "unable to allocate memory");
    }

    /* create a new steram and populate it */
    stream->stream_id = parse_int(section, "stream_id", result);
    stream->port_id   = parse_int(section, "port_id", result);
    stream->isg_usec  = parse_double(section, "Is", result);

    stream->next_stream_id = parse_int(section, "next_stream_id", result);
    stream->loop_count     = parse_int(section, "loop_count", result);

    const Json::Value &pkt = parse_array(section, "packet", result);

    if ( (pkt.size() < TrexStream::MIN_PKT_SIZE_BYTES) || (pkt.size() > TrexStream::MAX_PKT_SIZE_BYTES) ) {
        generate_err(result, "bad packet size provided: should be between 64B and 9K"); 
    }

    stream->pkt = new uint8_t[pkt.size()];
    if (!stream->pkt) {
        generate_internal_err(result, "unable to allocate memory");
    }

    for (int i = 0; i < pkt.size(); i++) {
        stream->pkt[i] = parse_byte(pkt, i, result);
    }

    /* register the stream to the port */

    /* port id should be between 0 and count - 1 */
    if (stream->port_id >= get_trex_stateless()->get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << get_trex_stateless()->get_port_count();
        generate_err(result, ss.str());
    }

    TrexStatelessPort * port = get_trex_stateless()->get_port_by_id(stream->port_id);
    port->get_stream_table()->add_stream(stream);

    return (TREX_RPC_CMD_OK);
}

