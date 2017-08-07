#include <json/json.h>
#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <stdio.h>
#include "44bsd/tcp_socket.h"

class CTcpData {
    friend class CJsonData;

 public:
    void dump(FILE *fd);
    void free();

 private:
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
            m_pInstance->m_initiated = false;
        }
        return m_pInstance;
    }
    ~CJsonData();

    bool parse_file(std::string file);
    void dump();
    std::string get_buf(uint16_t temp_index, uint16_t cmd_index, int side);
    CTcpAppProgram * get_prog(uint16_t temp_index, int side);
    bool is_initiated() {return m_initiated;}
    void clear();

 private:
    uint16_t get_buf_index(uint16_t program_index, uint16_t cmd_index);
    uint32_t get_num_bytes(uint16_t program_index, uint16_t cmd_index);
    tcp_app_cmd_enum_t get_cmd(uint16_t program_index, uint16_t cmd_index);
    bool convert_bufs();
    bool convert_progs();

 private:
    bool m_initiated;
    static CJsonData *m_pInstance;
    Json::Value  m_val;
    CTcpData m_tcp_data;
};
