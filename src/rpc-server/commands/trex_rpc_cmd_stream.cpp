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

using namespace std;

/**
 * Stateless stream mode
 * abstract class
 */
class TrexStreamMode {
public:
    enum mode_e {
        CONTINUOUS,
        SINGLE_BURST,
        MULTI_BURST
    };

    virtual mode_e get_runtime_type() = 0;
    virtual ~TrexStreamMode() {}
};

/**
 * stream mode continuous
 * 
 * @author imarom (30-Aug-15)
 */
class TrexStreamModeContinuous : public TrexStreamMode {
public:
    mode_e get_runtime_type() {
        return (CONTINUOUS);
    }
private:
    uint32_t pps;
};

/**
 * single burst mode
 * 
 */
class TrexStreamModeSingleBurst : public TrexStreamMode {
public:
    mode_e get_runtime_type() {
        return (SINGLE_BURST);
    }
private:

    uint32_t packets;
    uint32_t pps;
};

class TrexStreamModeMultiBurst : public TrexStreamMode {
public:

    mode_e get_runtime_type() {
        return (MULTI_BURST);
    }

private:

    uint32_t pps;
    double   ibg_usec;
    uint32_t number_of_bursts;
    uint32_t pkts_per_burst;
};


/**
 * Stateless Stream
 * 
 */
class TrexStatelessStream {
    friend class TrexRpcCmdAddStream;

public:

private:
    /* config */
    uint32_t      stream_id;
    uint8_t       port_id;
    double        isg_usec;
    uint32_t      next_stream_id;
    uint32_t      loop_count;

    /* indicators */
    bool          enable;
    bool          start;
    
    /* pkt */
    uint8_t      *pkt;
    uint16_t      pkt_len;

    /* stream mode */
    TrexStreamMode *mode;

    /* VM */

    /* RX check */
    struct {
        bool      enable;
        bool      seq_enable;
        bool      latency;
        uint32_t  stream_id;

    } rx_check;

};
/**
 * add new stream
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    TrexStatelessStream stream;

    check_param_count(params, 1, result);
    check_field_type(params, "stream", FIELD_TYPE_OBJ, result);

    Json::Value &section = result["stream"];
    
    /* create a new steram and populate it */
    
    check_field_type(section, "stream_id", FIELD_TYPE_INT, result);
    stream.stream_id = section["stream_id"].asInt();

    check_field_type(section, "port_id", FIELD_TYPE_INT, result);
    stream.port_id = section["port_id"].asInt();

    check_field_type(section, "Is", FIELD_TYPE_DOUBLE, result);
    stream.isg_usec = section["Is"].asDouble();

    return (TREX_RPC_CMD_OK);
}

