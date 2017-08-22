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
    uint16_t tcp_win;
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

 private:
    uint8_t m_init;
    std::vector<CMbufBuffer *> m_buf_list;
    std::vector<CTcpAppProgram *> m_prog_list;
    std::vector<CTcpDataFlowInfo> m_flow_info;
    CTcpDataAssocTranslation m_assoc_trans;
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

 private:
    bool m_json_initiated;
    static CJsonData *m_pInstance;
    Json::Value  m_val;
    std::mutex m_mtx[2];
    CTcpData m_tcp_data[2];
};
