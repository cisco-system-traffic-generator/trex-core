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
#include "44bsd/tcp_socket.h"

class CTcpDataAssocParams {
    friend class CJsonData;
    friend class CTcpDataAssocTransHelp;
    friend class CTcpDataAssocTranslation;
    friend class CTcpData;
    friend bool operator== (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);
    friend bool operator< (const CTcpDataAssocParams& lhs, const CTcpDataAssocParams& rhs);

    CTcpDataAssocParams() {}
    CTcpDataAssocParams(uint16_t port) {
        m_port = port;
    }

 private:
    uint16_t m_port;
};

typedef std::map<CTcpDataAssocParams, CTcpAppProgram*> assoc_map_t;
typedef std::map<CTcpDataAssocParams, CTcpAppProgram*>::iterator assoc_map_it_t;

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

    CTcpDataAssocTransHelp(const CTcpDataAssocParams& params, CTcpAppProgram *prog) {
        m_params = params;
        m_prog = prog;
    }

 private:
    CTcpDataAssocParams m_params;
    CTcpAppProgram *m_prog;
};

class CTcpDataAssocTranslation {
    friend class CJsonData;
    friend class CTcpData;

    CTcpAppProgram * get_prog(const CTcpDataAssocParams& params);
    void insert_hash(const CTcpDataAssocParams &params, CTcpAppProgram *prog);
    void insert_vec(const CTcpDataAssocParams &params, CTcpAppProgram *prog);
    void dump(FILE *fd);
    void clear() {
        m_map.clear();
        m_vec.clear();
    }

 private:
    assoc_map_t m_map;
    std::vector<CTcpDataAssocTransHelp> m_vec;
};

class CTcpDataFlowInfo {
    uint16_t m_tcp_win;
};

class CTcpTemplateInfo {
    friend class CJsonData;
    friend class CTcpData;

    uint16_t            m_dport;
    CTcpAppProgram *    m_client_prog; /* client program per template */
};

class CTcpData {
    friend class CJsonData;

 public:
    CTcpData() {
        m_init = 0;
    }
    void dump(FILE *fd);
    void free();
    bool is_init(uint8_t num) {return (m_init >= num);}
    uint16_t get_dport(uint16_t temp_id) {return m_templates[temp_id].m_dport;}
    CTcpAppProgram * get_client_prog(uint16_t temp_id){
        return m_templates[temp_id].m_client_prog;
    }
    CTcpAppProgram * get_server_prog_by_port(uint16_t port);
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
    void set_test_assoc_table(uint16_t port, CTcpAppProgram *prog) {
        CTcpDataAssocParams params(port);
        m_assoc_trans.insert_vec(params, prog);
    }
 private:
    uint8_t m_init;
    double m_cps_sum;
    std::vector<CMbufBuffer *> m_buf_list;
    std::vector<CTcpAppProgram *> m_prog_list;
    std::vector<CTcpDataFlowInfo> m_flow_info;
    std::vector<CTcpTemplateInfo> m_templates;
    CTcpDataAssocTranslation m_assoc_trans;
};

class CAstfTemplatesRW;
class CTupleGeneratorSmart;

class CTcpLatency {
    friend class CJsonData;

 public:
    uint32_t get_c_ip() {return m_c_ip;}
    uint32_t get_s_ip() {return m_s_ip;}
    uint32_t get_mask() {return m_dual_mask;}

 private:
    uint32_t m_c_ip;
    uint32_t m_s_ip;
    uint32_t m_dual_mask;
};

class CJsonData {
    struct json_handle {
        std::string str;
        int (*func)(Json::Value val);
    };

 public:
    // make the class singelton
    static CJsonData *instance() {
        if (! m_pInstance) {
            m_pInstance = new CJsonData;
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

    ~CJsonData(){
        clear();
    }

    bool parse_file(std::string file);
    CTcpAppProgram * get_prog(uint16_t temp_index, int side, uint8_t socket_id);
    CTcpAppProgram * get_server_prog_by_port(uint16_t port, uint8_t socket_id);
    CTcpData *get_tcp_data_handle(uint8_t socket_id);
    CAstfTemplatesRW *get_tcp_data_handle_rw(uint8_t socket_id, CTupleGeneratorSmart *g_gen,
                                             uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id);
    void get_latency_params(CTcpLatency &lat);
    bool is_initiated() {return m_json_initiated;}
    void clear();
    void dump();

 private:
    std::string get_buf(uint16_t temp_index, uint16_t cmd_index, int side);
    void convert_from_json(uint8_t socket_id, uint8_t level);
    uint16_t get_buf_index(uint16_t program_index, uint16_t cmd_index);
    uint32_t get_num_bytes(uint16_t program_index, uint16_t cmd_index);
    tcp_app_cmd_enum_t get_cmd(uint16_t program_index, uint16_t cmd_index);
    bool convert_bufs(uint8_t socket_id);
    bool convert_progs(uint8_t socket_id);
    bool build_assoc_translation(uint8_t socket_id);
    void verify_init(uint16_t socket_id, uint16_t level);
    uint32_t ip_from_str(const char*c_ip);

 private:
    bool m_json_initiated;
    static CJsonData *m_pInstance;
    Json::Value  m_val;
    std::mutex m_mtx[2];
    // Data duplicated per memory socket
    CTcpData m_tcp_data[2];
};

#endif
