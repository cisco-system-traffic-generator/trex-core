#ifndef ASTF_JSON_READER_H
#define ASTF_JSON_READER_H

/*
 Ido Barnea
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#include <json/json.h>
#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <stdio.h>
#include <mutex>
#include <trex_defs.h>
#include "44bsd/tcp_socket.h"
#include "rpc-server/trex_rpc_cmd_api.h"
#include "tuple_gen.h"


    
class CTRexDummyCommand : public  TrexRpcCommand {

public:
    CTRexDummyCommand(): TrexRpcCommand ("dummy",
                                         (TrexRpcComponent *)NULL,
                                         false,
                                         false){
    }

    virtual ~CTRexDummyCommand() {}

protected:
    virtual trex_rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result) {
        return (TREX_RPC_CMD_OK);
    }
};


class CTcpTuneables {
 public:
    enum {
        tcp_mss_bit      = 0x1,
        tcp_initwnd_bit  = 0x2,
        ipv6_src_addr    = 0x4,
        ipv6_dst_addr    = 0x8,
        ipv6_enable      = 0x10,

        tcp_rx_buf_size  = 0x20,
        tcp_tx_buf_size  = 0x40,

        tcp_rexmtthresh  = 0x80,
        tcp_do_rfc1323   = 0x100,
        tcp_keepinit     = 0x200,
        tcp_keepidle     = 0x400,

        tcp_keepintvl   =  0x800,
        tcp_delay_ack   =  0x1000,
        tcp_no_delay    =  0x2000,
        sched_rampup    =  0x4000,
        sched_accurate  =  0x8000,
        tcp_blackhole   =  0x10000,
    };


 public:
    CTcpTuneables() {
        m_bitfield = 0;
        m_flags=0;
        m_tcp_mss=0;
        m_tcp_initwnd=0;
        m_tcp_txbufsize=0;
        m_tcp_rxbufsize=0;

        m_tcp_rexmtthresh=0;
        m_tcp_do_rfc1323=0;
        m_tcp_keepinit=0;;
        m_tcp_keepidle=0;
        m_tcp_keepintvl=0;
        m_tcp_blackhole=0;
        m_tcp_delay_ack_msec=0;
        m_tcp_no_delay=0; /* disable nagel */
        m_scheduler_rampup=0;
        m_scheduler_accurate=0;

        memset(m_ipv6_src,0,16);
        memset(m_ipv6_dst,0,16);
    }


    bool is_empty() { return m_bitfield == 0;}

    void set_ipv6_enable(uint32_t val){
        if (val) {
            add_value(ipv6_enable);
        }
    }

    bool is_valid_field(uint32_t val){
        return ( ((m_bitfield & val)==val)?true:false);
    }

    void add_value(uint32_t val) {m_bitfield |= val;}
    uint32_t get_bitfield() {return m_bitfield;}
    void dump(FILE *fd);

 public:
    uint8_t  m_tcp_rexmtthresh; /* ACK retransmition */
    uint8_t  m_tcp_do_rfc1323; /* 1/0 */
    uint8_t  m_tcp_keepinit;
    uint8_t  m_tcp_keepidle;

    uint8_t  m_tcp_keepintvl;
    uint8_t  m_tcp_blackhole;
    uint16_t m_tcp_delay_ack_msec; /* 20-500msec */

    uint16_t m_tcp_mss;
    uint16_t m_tcp_initwnd; /* init window*/
    uint32_t m_tcp_txbufsize;
    uint32_t m_tcp_rxbufsize;


    uint8_t  m_ipv6_src[16];
    uint8_t  m_ipv6_dst[16];
    uint8_t  m_tcp_no_delay; /* 1/0 */
    uint8_t  m_scheduler_accurate; /* more accorate  */
    uint16_t m_scheduler_rampup; /* time in sec for rampup*/
    

 private:
    uint32_t m_bitfield;
    uint32_t m_flags;

};


class CTcpDataAssocParams {
    friend class CAstfDB;
    friend class CTcpDataAssocTransHelp;
    friend class CTcpDataAssocTranslation;
    friend class CAstfDbRO;
    friend bool operator== (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);
    friend bool operator< (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);

    CTcpDataAssocParams() {}
    CTcpDataAssocParams(uint16_t port,bool stream) {
        m_port = port;
        m_stream =stream;
    }

 private:
    uint16_t m_port;
    bool     m_stream;
};

class CTcpServreInfo {
    friend class CTcpDataAssocTransHelp;

 public:
    CTcpServreInfo() {}
    CTcpServreInfo(CEmulAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx) {
        m_prog = prog;
        m_tune = tune;
        m_temp_idx = temp_idx;
    }
    CEmulAppProgram *get_prog() {return m_prog;}
    CTcpTuneables *get_tuneables() {return m_tune;}
    uint16_t get_temp_idx() {return m_temp_idx;}

 private:
    CEmulAppProgram *m_prog;
    CTcpTuneables *m_tune;
    uint16_t m_temp_idx;
};

typedef std::map<CTcpDataAssocParams, CTcpServreInfo*> assoc_map_t;
typedef std::map<CTcpDataAssocParams, CTcpServreInfo*>::iterator assoc_map_it_t;

inline bool operator== (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs) {
    if (lhs.m_stream != rhs.m_stream){
        return false;
    }

    if (lhs.m_port != rhs.m_port)
        return false;

    return true;
}

inline bool operator< (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs) {
    uint32_t v1 =(lhs.m_stream?0x10000:0) + lhs.m_port;
    uint32_t v2 =(rhs.m_stream?0x10000:0) + rhs.m_port;
    if (v1 < v2)
        return true;

    return false;
}

class CTcpDataAssocTransHelp {
    friend class CTcpDataAssocTranslation;

    CTcpDataAssocTransHelp(const CTcpDataAssocParams& params, CEmulAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx) {
        m_params = params;
        m_server_info.m_prog = prog;
        m_server_info.m_tune = tune;
        m_server_info.m_temp_idx = temp_idx;
    }

 private:
    CTcpDataAssocParams m_params;
    CTcpServreInfo m_server_info;
};

class CTcpDataAssocTranslation {
    friend class CAstfDB;
    friend class CAstfDbRO;

    CTcpServreInfo * get_server_info(const CTcpDataAssocParams& params);
    void insert_hash(const CTcpDataAssocParams &params, CEmulAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx);
    void insert_vec(const CTcpDataAssocParams &params, CEmulAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx);
    void dump(FILE *fd);
    void clear();

 private:
    assoc_map_t m_map;
    std::vector<CTcpDataAssocTransHelp> m_vec;
};

class CTcpDataFlowInfo {
    uint16_t m_tcp_win;
};

class CTcpTemplateInfo {
    friend class CAstfDB;
    friend class CAstfDbRO;

 public:
    void Delete(){
    }
 private:
    uint16_t            m_dport;
    CEmulAppProgram *    m_client_prog; /* client program per template */
    uint32_t m_num_bytes;
};

typedef enum {
    CJsonData_err_pool_ok,
    CJsonData_err_pool_too_small,
} CJsonData_err_type;

class CJsonData_err {
 public:
    CJsonData_err( CJsonData_err_type type, std::string desc) {
        m_desc = desc;
        m_type = type;
    }
    bool is_error() {return m_type != CJsonData_err_pool_ok;}
    std::string description() {return m_desc;}

 private:
    std::string m_desc;
    CJsonData_err_type m_type;
};

// This is used by all threads. Should not be changed after initialization stage, in order not to reduce performance
class CAstfDbRO {
    friend class CAstfDB;

 public:
    CAstfDbRO() {
        m_init = 0;
        m_cps_sum=0.0;
    }
    void dump(FILE *fd);
    void Delete();
    bool is_init() {return (m_init == 2);}
    uint16_t get_dport(uint16_t temp_id) {return m_templates[temp_id].m_dport;}
    CEmulAppProgram * get_client_prog(uint16_t temp_id) const {
        return m_templates[temp_id].m_client_prog;
    }


    CTcpServreInfo * get_server_info_by_port(uint16_t port,bool stream=true);
    double  get_total_cps(){
        return (m_cps_sum);
    }
    double  get_total_cps_per_thread(uint16_t max_threads){
        return (m_cps_sum/(double)max_threads);
    }
    double  get_delta_tick_sec_thread(uint16_t max_threads){
        return (1.0/get_total_cps_per_thread(max_threads));
    }

    // for tests in simulation
    void set_test_assoc_table(uint16_t port, CEmulAppProgram *prog, CTcpTuneables *tune) {
        CTcpDataAssocParams params(port,true);
        m_assoc_trans.insert_vec(params, prog, tune, 0);
    }
 private:
    uint8_t                         m_init;
    double                          m_cps_sum;
    std::vector<CMbufBuffer *>      m_buf_list;
    std::vector<CEmulAppProgram *>   m_prog_list;
    std::vector<CTcpDataFlowInfo>   m_flow_info;
    std::vector<CTcpTemplateInfo>   m_templates;
    CTcpDataAssocTranslation        m_assoc_trans;
};

class CAstfTemplatesRW;
class CTupleGeneratorSmart;
class ClientCfgDB;

class CTcpLatency {
    friend class CAstfDB;

 public:
    uint32_t get_c_ip() {return m_c_ip;}
    uint32_t get_s_ip() {return m_s_ip;}
    uint32_t get_mask() {return m_dual_mask;}

 private:
    uint32_t m_c_ip;
    uint32_t m_s_ip;
    uint32_t m_dual_mask;
};

typedef enum {
    CJsonData_ipv6_addr  = 1,
} CJsonData_read_type_t ;


class CAstfJsonValidator;

class CAstfDB  : public CTRexDummyCommand  {

    struct json_handle {
        std::string str;
        int (*func)(Json::Value val);
    };

 public:
    // make the class singelton
    static CAstfDB *instance() {
        if (! m_pInstance) {
            m_pInstance = new CAstfDB();
            m_pInstance->m_json_initiated = false;
        }
        return m_pInstance;
    }

    static void free_instance(){
        if (m_pInstance){
            delete m_pInstance;
            m_pInstance=0;
        }
    }

    static bool has_instance() {
        return m_pInstance != nullptr;
    }

    CAstfDB();
    virtual ~CAstfDB();

    /***************************/

    void set_profile_one_msg(Json::Value msg);

    /* set profile as one message, if profile message  is small. for interactive mode */
    bool set_profile_one_msg(const std::string &msg, std::string &err);

    /***************************/
    /* split profile to a few steps */
    /*  
        start_profile_no_buffer(..);

        loops:        
          add_buffers(msg,err);
        
        compile_profile(err);

        m_val will have a valid json after this
        
    */

    /* set profile as message without buffers  */
    bool start_profile_no_buffer(Json::Value msg);

    /* clear buffers */
    bool clear_buffers();

    bool add_buffers(Json::Value msg,std::string & err);

    bool compile_profile(std::string & err);

    void dump_profile(FILE *fd);

    bool compile_profile_dp(uint8_t socket_id);

    /***************************/

    // Parsing json file called from master. used for batch mode, read the JSON from file
    bool parse_file(std::string file);

    void set_client_cfg_db(ClientCfgDB * client_config_info){
        m_client_config_info = client_config_info;
    }

    // called *once* by each core, using socket_id associated with the core 
    // multi-threaded need to be protected / per socket read-only data 
    CAstfDbRO *get_db_ro(uint8_t socket_id);
    void clear_db_ro(uint8_t socket_id);
    // called by each core once. Allocating memory that will be freed in clear()
    // multi-threaded need to be protected 
    CAstfTemplatesRW *get_db_template_rw(uint8_t socket_id, CTupleGeneratorSmart *g_gen,
                                             uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id);
    void clear_db_ro_rw(CTupleGeneratorSmart *g_gen);
    void get_latency_params(CTcpLatency &lat);
    CJsonData_err verify_data(uint16_t max_threads);
    CTcpTuneables *get_s_tune(uint32_t index) {return m_s_tuneables[index];}

    /* Update for client cluster mode. Should be deprecated */
    void get_tuple_info(CTupleGenYamlInfo & tuple_info);
    bool get_latency_info(uint32_t & src_ipv4,
                          uint32_t & dst_ipv4,
                          uint32_t & dual_port_mask);


private:
    bool validate_profile(Json::Value profile,std::string & err);

 private:
    CEmulAppProgram * get_server_prog_by_port(uint16_t port, uint8_t socket_id);
    CEmulAppProgram * get_prog(uint16_t temp_index, int side, uint8_t socket_id);
    CTcpTuneables * get_tunables(uint16_t temp_index, int side, uint8_t socket_id);
    float get_expected_cps() {return m_tcp_data[0].m_cps_sum;}
    float get_expected_bps() {return m_exp_bps;}
    bool is_initiated() {return m_json_initiated;}
    void dump();
    ClientCfgDB  *get_client_db();
    std::string get_buf(uint16_t temp_index, uint16_t cmd_index, int side);
    bool convert_from_json(uint8_t socket_id);
    uint16_t get_buf_index(uint16_t program_index, uint16_t cmd_index);
    void  get_rx_cmd(uint16_t program_index, uint16_t cmd_index,CEmulAppCmd &res);

    uint32_t get_delay_ticks(uint16_t program_index, uint16_t cmd_index);
    void fill_tx_mode(uint16_t program_index, uint16_t cmd_index,CEmulAppCmd &res);

    void fill_delay_rnd(uint16_t program_index,uint16_t cmd_index,CEmulAppCmd &res);
    void fill_set_var(uint16_t program_index,uint16_t cmd_index,CEmulAppCmd &res);
    void fill_jmpnz(uint16_t program_index,uint16_t cmd_index,CEmulAppCmd &res);
    void fill_tx_pkt(uint16_t program_index, 
                     uint16_t cmd_index,
                     uint8_t socket_id,
                     CEmulAppCmd &res);
    void fill_rx_pkt(uint16_t program_index, 
                     uint16_t cmd_index,
                     CEmulAppCmd &res);

    void fill_keepalive_pkt(uint16_t program_index, 
                     uint16_t cmd_index,
                     CEmulAppCmd &res);


    tcp_app_cmd_enum_t get_cmd(uint16_t program_index, uint16_t cmd_index);
    bool get_emul_stream(uint16_t program_index);

    bool read_tunables(CTcpTuneables *tune, Json::Value json);
    bool convert_bufs(uint8_t socket_id);
    bool convert_progs(uint8_t socket_id);
    bool build_assoc_translation(uint8_t socket_id);
    bool verify_init(uint16_t socket_id);
    uint32_t ip_from_str(const char*c_ip);

private:
    bool read_tunables_ipv6_field(CTcpTuneables *tune,
                                  Json::Value json,
                                  void *field,
                                  uint32_t enum_val);

    bool read_tunable_uint8(CTcpTuneables *tune,
                             const Json::Value &parent, 
                             const std::string &param,
                             uint32_t enum_val,
                             uint8_t & val);

    bool read_tunable_uint16(CTcpTuneables *tune,
                             const Json::Value &parent, 
                             const std::string &param,
                             uint32_t enum_val,
                             uint16_t & val);

    bool read_tunable_uint32(CTcpTuneables *tune,
                             const Json::Value &parent, 
                             const std::string &param,
                             uint32_t enum_val,
                             uint32_t & val);

    bool read_tunable_uint64(CTcpTuneables *tune,
                             const Json::Value &parent, 
                             const std::string &param,
                             uint32_t enum_val,
                             uint64_t & val);



    bool read_tunable_double(CTcpTuneables *tune,
                             const Json::Value &parent, 
                             const std::string &param,
                             uint32_t enum_val,
                            double & val);

    bool read_tunable_bool(CTcpTuneables *tune,
                          const Json::Value &parent, 
                          const std::string &param,
                          uint32_t enum_val,
                          double & val);    

    void tunable_min_max_u32(std::string param,
                             uint32_t val,
                             uint32_t min,
                             uint32_t max);

    void tunable_min_max_u64(std::string param,
                             uint64_t val,
                             uint64_t min,
                             uint64_t max);

    void tunable_min_max_d(std::string param,
                           double val,
                           double min,
                           double max);


 private:
    bool m_json_initiated;
    static CAstfDB *m_pInstance;
    Json::Value  m_val;
    Json::Value  m_buffers;

    std::vector<uint32_t> m_prog_lens; // program lengths in bytes
    std::vector<CAstfTemplatesRW *> m_rw_db;
    std::vector<CTcpTuneables *> m_s_tuneables;
    float m_exp_bps; // total expected bit per second for all templates
    std::mutex          m_global_mtx;
    // Data duplicated per memory socket
    CAstfDbRO            m_tcp_data[MAX_SOCKETS_SUPPORTED];

    ClientCfgDB   *      m_client_config_info;
    CAstfJsonValidator  * m_validator;
};

#endif
