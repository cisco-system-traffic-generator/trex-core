#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <assert.h>
#include <json/json.h>
#include "common/base64.h"
#include <string.h>
#include "44bsd/tcp_socket.h"
#include "json_reader.h"

// make the class singelton
CJsonData* CJsonData::m_pInstance = NULL;

CTcpAppProgram * CTcpDataAssocTranslation::get_prog(const CTcpDataAssocParams &params) {
    if (m_vec.size() == 0) {
        assoc_map_it_t it = m_map.find(params);

        if (it == m_map.end()) {
            return NULL;
        } else {
            return it->second;
        }
    }

    for (int i = 0; i < m_vec.size(); i++) {
        if (params == m_vec[i].m_params)
            return m_vec[i].m_prog;
    }

    return NULL;
}

void CTcpDataAssocTranslation::dump(FILE *fd) {
    if (m_vec.size() != 0) {
        fprintf(fd, "CTcpDataAssocTranslation - Dumping vector:\n");
        for (int i = 0; i < m_vec.size(); i++) {
            fprintf(fd, "  port %d mapped to %p\n", m_vec[i].m_params.m_port, m_vec[i].m_prog);
        }
    } else {
        fprintf(fd, "CTcpDataAssocTranslation - Dumping map:\n");
        //        std::map<>::const_iterator
        assoc_map_it_t it;
        for (it = m_map.begin(); it != m_map.end(); it++) {
            fprintf(fd, "  port %d mapped to %p\n", it->first.m_port, it->second);
        }
    }
}

void CTcpDataAssocTranslation::insert_vec(const CTcpDataAssocParams &params, CTcpAppProgram *prog) {
    CTcpDataAssocTransHelp trans_help(params, prog);
    m_vec.push_back(trans_help);
}

void CTcpDataAssocTranslation::insert_hash(const CTcpDataAssocParams &params, CTcpAppProgram *prog) {
    m_map.insert(std::pair<CTcpDataAssocParams, CTcpAppProgram *>(params, prog));
}

bool CJsonData::parse_file(std::string file) {
    static bool parsed = false;
    if (parsed)
        return true;
    else
        parsed = true;

    Json::Reader reader;
    std::ifstream t(file);
    if (!  t.is_open()) {
        std::cerr << "Failed openeing json file " << file << std::endl;
        exit(1);
    }
    std::string m_msg((std::istreambuf_iterator<char>(t)),
                                    std::istreambuf_iterator<char>());
    bool rc = reader.parse(m_msg, m_val, false);

    if (!rc) {
        std::cerr << "Failed parsing json file " << file << std::endl;
        return false;
    }

    m_json_initiated = true;
    return true;
}

void CJsonData::convert_from_json(uint8_t socket_id, uint8_t level) {
    if (level == 1) {
        convert_bufs(socket_id);
        convert_progs(socket_id);
        m_tcp_data[socket_id].m_init = 1;
    }
    if (level == 2) {
        build_assoc_translation(socket_id);
        m_tcp_data[socket_id].m_init = 2;
    }
}

void CJsonData::dump() {
    std::cout << m_val << std::endl;
}

uint16_t CJsonData::get_buf_index(uint16_t program_index, uint16_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert(cmd["name"] == "tx");

    return cmd["buf_index"].asInt();
}

std::string CJsonData::get_buf(uint16_t temp_index, uint16_t cmd_index, int side) {
    std::string temp_str;
    Json::Value cmd;

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }
    try {
        uint16_t program_index = m_val["templates"][temp_index][temp_str]["program_index"].asInt();
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "tx");

    uint16_t buf_index = cmd["buf_index"].asInt();
    std::string output = m_val["buf_list"][buf_index].asString();

    return base64_decode(output);
}


tcp_app_cmd_enum_t CJsonData::get_cmd(uint16_t program_index, uint16_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        return tcNO_CMD;
    }

    if (cmd["name"] == "tx") {
        return tcTX_BUFFER;
    }
    if (cmd["name"] == "rx")
        return tcRX_BUFFER;

    return tcNO_CMD;
}

uint32_t CJsonData::get_num_bytes(uint16_t program_index, uint16_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

     assert (cmd["name"] == "rx");

    return cmd["min_bytes"].asInt();
}

void CJsonData::verify_init(uint16_t socket_id, uint16_t level) {
    if (! m_tcp_data[socket_id].is_init(level)) {
        std::unique_lock<std::mutex> my_lock(m_mtx[socket_id]);
        for (int i = 1; i <= level; i++) {
            if (! m_tcp_data[socket_id].is_init(i)) {
                convert_from_json(socket_id, i);
            }
        }
        my_lock.unlock();
    }
}
/*
 * Get program associated with template index and side
 * temp_index - template index
 * side - 0 - client, 1 - server
 * Return pointer to program
 */
CTcpAppProgram *CJsonData::get_prog(uint16_t temp_index, int side, uint8_t socket_id) {
    std::string temp_str;
    uint16_t program_index;

    verify_init(socket_id, 1);

    if (side == 0) {
        temp_str = "client_template";
    } else if (side == 1) {
        temp_str = "server_template";
    } else {
        fprintf(stderr, "Bad side value %d\n", side);
        assert(0);
    }

    program_index = m_val["templates"][temp_index][temp_str]["program_index"].asInt();

    return m_tcp_data[socket_id].m_prog_list[program_index];
}

CTcpAppProgram *CJsonData::get_server_prog_by_port(uint16_t port, uint8_t socket_id) {
    CTcpDataAssocParams params(port);

    verify_init(socket_id, 2);

    return m_tcp_data[socket_id].m_assoc_trans.get_prog(params);
}

bool CJsonData::build_assoc_translation(uint8_t socket_id) {
    verify_init(socket_id, 1);
    bool is_hash_needed = false;

    if (m_val["templates"].size() > 10) {
        is_hash_needed = true;
    }

    for (uint16_t index = 0; index < m_val["templates"].size(); index++) {
        uint16_t port = m_val["templates"][index]["server_template"]["assoc"][0]["port"].asInt();
        CTcpDataAssocParams tcp_params(port);
        CTcpAppProgram *prog_p = get_prog(index, 1, socket_id);
        assert(prog_p);

        if (is_hash_needed) {
            m_tcp_data[socket_id].m_assoc_trans.insert_hash(tcp_params, prog_p);
        } else {
            m_tcp_data[socket_id].m_assoc_trans.insert_vec(tcp_params, prog_p);
        }
    }

    return true;
}

/* Convert list of buffers from json to CMbufBuffer */
bool CJsonData::convert_bufs(uint8_t socket_id) {
    CMbufBuffer *tcp_buf;
    std::string json_buf;
    uint16_t buf_len;

    if (m_val["buf_list"].size() == 0)
        return false;

    for (int buf_index = 0; buf_index < m_val["buf_list"].size(); buf_index++) {
        tcp_buf = new CMbufBuffer();
        assert(tcp_buf);
        json_buf = m_val["buf_list"][buf_index].asString();
        std::string temp_str = base64_decode(json_buf);
        buf_len = temp_str.size();
        utl_mbuf_buffer_create_and_copy(socket_id, tcp_buf, 2048, (uint8_t *)(temp_str.c_str()), buf_len);
        m_tcp_data[socket_id].m_buf_list.push_back(tcp_buf);
    }

    return true;
}

/* Convert list of programs from json to CMbufBuffer */
bool CJsonData::convert_progs(uint8_t socket_id) {
    CTcpAppCmd cmd;
    CTcpAppProgram *prog;
    uint16_t cmd_index;
    tcp_app_cmd_enum_t cmd_type;

    if (m_val["program_list"].size() == 0)
        return false;

    for (uint16_t program_index = 0; program_index < m_val["program_list"].size(); program_index++) {
        prog = new CTcpAppProgram();
        assert(prog);

        cmd_index = 0;
        do {
            cmd_type = get_cmd(program_index, cmd_index);

            switch(cmd_type) {
            case tcNO_CMD:
                break;
            case tcTX_BUFFER:
                cmd.u.m_tx_cmd.m_buf = m_tcp_data[socket_id].m_buf_list[get_buf_index(program_index, cmd_index)];
                cmd.m_cmd = tcTX_BUFFER;
                prog->add_cmd(cmd);
                break;
            case tcRX_BUFFER:
                cmd.m_cmd = tcRX_BUFFER;
                cmd.u.m_rx_cmd.m_flags = CTcpAppCmdRxBuffer::rxcmd_WAIT;
                cmd.u.m_rx_cmd.m_rx_bytes_wm = get_num_bytes(program_index, cmd_index);
                prog->add_cmd(cmd);
                break;
            case tcDELAY:
                break;
            case tcRESET:
                break;
            default:
                assert(0);
            }

            cmd_index++;
        } while (cmd_type != tcNO_CMD);

        m_tcp_data[socket_id].m_prog_list.push_back(prog);
    }

    return true;
}

void CJsonData::clear() {
    m_tcp_data[0].free();
    m_tcp_data[1].free();
    m_json_initiated = false;
}

void CTcpData::dump(FILE *fd) {
    fprintf(fd, "buf list:\n");
    for (int i = 0; i < m_buf_list.size(); i++) {
        fprintf(fd, "*******%d*******\n", i);
        m_buf_list[i]->Dump(fd);
    }
}

void CTcpData::free() {
    for (int i = 0; i < m_buf_list.size(); i++) {
        m_buf_list[i]->Delete();
        delete m_buf_list[i];
    }
    m_buf_list.clear();

    for (int i = 0; i < m_prog_list.size(); i++) {
        delete m_prog_list[i];
    }
    m_prog_list.clear();
    m_assoc_trans.clear();

    m_init = false;
}
