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

class CTcpTuneables {
 public:
    enum {
        mss_bit = 0x1,
        init_win_bit = 0x2
    };

 public:
    CTcpTuneables() {
        m_bitfield = 0;
    }

    bool is_empty() { return m_bitfield == 0;}
    void add_value(uint32_t val) {m_bitfield |= val;}
    uint32_t get_bitfield() {return m_bitfield;}
    uint32_t get_mss() {return m_mss;}
    bool mss_valid() {return m_bitfield & mss_bit;}
    bool init_win_valid() {return m_bitfield & init_win_bit;}
    uint16_t get_init_win() {return m_init_win;}
    void dump(FILE *fd);

 public:
    uint32_t m_mss;
    uint32_t m_window;
    uint16_t m_init_win;

 private:
    uint32_t m_bitfield;
};


class CTcpDataAssocParams {
    friend class CAstfDB;
    friend class CTcpDataAssocTransHelp;
    friend class CTcpDataAssocTranslation;
    friend class CAstfDbRO;
    friend bool operator== (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);
    friend bool operator< (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);

    CTcpDataAssocParams() {}
    CTcpDataAssocParams(uint16_t port) {
        m_port = port;
    }

 private:
    uint16_t m_port;
};

class CTcpServreInfo {
    friend class CTcpDataAssocTransHelp;

 public:
    CTcpServreInfo() {}
    CTcpServreInfo(CTcpAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx) {
        m_prog = prog;
        m_tune = tune;
        m_temp_idx = temp_idx;
    }
    CTcpAppProgram *get_prog() {return m_prog;}
    CTcpTuneables *get_tuneables() {return m_tune;}
    uint16_t get_temp_idx() {return m_temp_idx;}

 private:
    CTcpAppProgram *m_prog;
    CTcpTuneables *m_tune;
    uint16_t m_temp_idx;
};

typedef std::map<CTcpDataAssocParams, CTcpServreInfo*> assoc_map_t;
typedef std::map<CTcpDataAssocParams, CTcpServreInfo*>::iterator assoc_map_it_t;

inline bool operator== (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs) {
    if (lhs.m_port != rhs.m_port)
        return false;

    return true;
}

inline bool operator< (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs) {
    if (lhs.m_port < rhs.m_port)
        return true;

    return false;
}

class CTcpDataAssocTransHelp {
    friend class CTcpDataAssocTranslation;

    CTcpDataAssocTransHelp(const CTcpDataAssocParams& params, CTcpAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx) {
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
    void insert_hash(const CTcpDataAssocParams &params, CTcpAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx);
    void insert_vec(const CTcpDataAssocParams &params, CTcpAppProgram *prog, CTcpTuneables *tune, uint16_t temp_idx);
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
    CTcpAppProgram *    m_client_prog; /* client program per template */
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
    CTcpAppProgram * get_client_prog(uint16_t temp_id) const {
        return m_templates[temp_id].m_client_prog;
    }


    CTcpServreInfo * get_server_info_by_port(uint16_t port);
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
    void set_test_assoc_table(uint16_t port, CTcpAppProgram *prog, CTcpTuneables *tune) {
        CTcpDataAssocParams params(port);
        m_assoc_trans.insert_vec(params, prog, tune, 0);
    }
 private:
    uint8_t                         m_init;
    double                          m_cps_sum;
    std::vector<CMbufBuffer *>      m_buf_list;
    std::vector<CTcpAppProgram *>   m_prog_list;
    std::vector<CTcpDataFlowInfo>   m_flow_info;
    std::vector<CTcpTemplateInfo>   m_templates;
    CTcpDataAssocTranslation        m_assoc_trans;
};

class CAstfTemplatesRW;
class CTupleGeneratorSmart;

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

class CAstfDB {
    struct json_handle {
        std::string str;
        int (*func)(Json::Value val);
    };

 public:
    // make the class singelton
    static CAstfDB *instance() {
        if (! m_pInstance) {
            m_pInstance = new CAstfDB;
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

    ~CAstfDB(){
        clear();
    }

    // Parsing json file called from master 
    bool parse_file(std::string file);
    CTcpServreInfo * get_server_info_by_port(uint16_t port, uint8_t socket_id);
    // called *once* by each core, using socket_id associated with the core 
    // multi-threaded need to be protected / per socket read-only data 
    CAstfDbRO *get_db_ro(uint8_t socket_id);
    // called by each core once. Allocating memory that will be freed in clear()
    // multi-threaded need to be protected 
    CAstfTemplatesRW *get_db_template_rw(uint8_t socket_id, CTupleGeneratorSmart *g_gen,
                                             uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id);
    void get_latency_params(CTcpLatency &lat);
    CJsonData_err verify_data(uint16_t max_threads);
    CTcpTuneables *get_s_tune(uint32_t index) {return m_s_tuneables[index];}

 private:
    CTcpAppProgram * get_server_prog_by_port(uint16_t port, uint8_t socket_id);
    CTcpAppProgram * get_prog(uint16_t temp_index, int side, uint8_t socket_id);
    CTcpTuneables * get_tunables(uint16_t temp_index, int side, uint8_t socket_id);
    float get_expected_cps() {return m_tcp_data[0].m_cps_sum;}
    float get_expected_bps() {return m_exp_bps;}
    bool is_initiated() {return m_json_initiated;}
    void clear();
    void dump();

    std::string get_buf(uint16_t temp_index, uint16_t cmd_index, int side);
    void convert_from_json(uint8_t socket_id);
    uint16_t get_buf_index(uint16_t program_index, uint16_t cmd_index);
    uint32_t get_num_bytes(uint16_t program_index, uint16_t cmd_index);
    tcp_app_cmd_enum_t get_cmd(uint16_t program_index, uint16_t cmd_index);
    bool read_tunables(CTcpTuneables *tune, Json::Value json);
    bool convert_tcp_info(uint8_t socket_id);
    bool convert_bufs(uint8_t socket_id);
    bool convert_progs(uint8_t socket_id);
    bool build_assoc_translation(uint8_t socket_id);
    void verify_init(uint16_t socket_id);
    uint32_t ip_from_str(const char*c_ip);

 private:
    bool m_json_initiated;
    static CAstfDB *m_pInstance;
    Json::Value  m_val;
    std::vector<uint32_t> m_prog_lens; // program lengths in bytes
    std::vector<CAstfTemplatesRW *> m_rw_db;
    std::vector<CTcpTuneables *> m_s_tuneables;
    float m_exp_bps; // total expected bit per second for all templates
    std::mutex          m_global_mtx;
    // Data duplicated per memory socket
    CAstfDbRO            m_tcp_data[MAX_SOCKETS_SUPPORTED];
};

#endif
