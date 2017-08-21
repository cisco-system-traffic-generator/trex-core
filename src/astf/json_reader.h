#include <json/json.h>
#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <stdio.h>
#include <mutex>
#include "44bsd/tcp_socket.h"

class CTcpData {
    friend class CJsonData;

 public:
    CTcpData() {
        m_init = false;
    }
    void dump(FILE *fd);
    void free();
    bool is_init() {return m_init;}

 private:
    bool m_init;
    std::vector<CMbufBuffer *> m_buf_list;
    std::vector<CTcpAppProgram *> m_prog_list;
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
    void dump();
    CTcpAppProgram * get_prog(uint16_t temp_index, int side, uint8_t socket_id);
    bool is_initiated() {return m_json_initiated;}
    void clear();

 private:
    std::string get_buf(uint16_t temp_index, uint16_t cmd_index, int side);
    void convert_from_json(uint8_t socket_id);
    uint16_t get_buf_index(uint16_t program_index, uint16_t cmd_index);
    uint32_t get_num_bytes(uint16_t program_index, uint16_t cmd_index);
    tcp_app_cmd_enum_t get_cmd(uint16_t program_index, uint16_t cmd_index);
    bool convert_bufs(uint8_t socket_id);
    bool convert_progs(uint8_t socket_id);

 private:
    bool m_json_initiated;
    static CJsonData *m_pInstance;
    Json::Value  m_val;
    std::mutex m_mtx[2];
    CTcpData m_tcp_data[2];
};
