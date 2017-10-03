#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <assert.h>
#include <json/json.h>
#include "common/base64.h"
#include <string.h>
#include "44bsd/tcp_socket.h"
#include "tuple_gen.h"
#include "astf/astf_template_db.h"
#include "astf_db.h"
#include "bp_sim.h"


extern int my_inet_pton4(const char *src, unsigned char *dst);

inline std::string methodName(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return prettyFunction.substr(begin,end) + "()";
}

#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)

// make the class singelton
CAstfDB* CAstfDB::m_pInstance = NULL;


static double cps_factor(double cps){
    return ( CGlobalInfo::m_options.m_factor*cps );

}

void CTcpTuneables::dump(FILE *fd) {
    if (mss_valid())
        fprintf(fd, "mss: %d\n", m_mss);
}

CTcpServreInfo *CTcpDataAssocTranslation::get_server_info(const CTcpDataAssocParams &params) {
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
            return &m_vec[i].m_server_info;
    }

    return NULL;
}

void CTcpDataAssocTranslation::dump(FILE *fd) {
    if (m_vec.size() != 0) {
        fprintf(fd, "CTcpDataAssocTranslation - Dumping vector:\n");
        for (int i = 0; i < m_vec.size(); i++) {
            fprintf(fd, "  port %d mapped to %p\n", m_vec[i].m_params.m_port, &m_vec[i].m_server_info);
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

void CTcpDataAssocTranslation::insert_vec(const CTcpDataAssocParams &params, CTcpAppProgram *prog, CTcpTuneables *tune
                                          , uint16_t temp_idx) {
    CTcpDataAssocTransHelp trans_help(params, prog, tune, temp_idx);
    m_vec.push_back(trans_help);
}

void CTcpDataAssocTranslation::insert_hash(const CTcpDataAssocParams &params, CTcpAppProgram *prog, CTcpTuneables *tune
                                           , uint16_t temp_idx) {
    CTcpServreInfo *tcp_s_info = new CTcpServreInfo(prog, tune, temp_idx);
    assert(tcp_s_info);

    m_map.insert(std::pair<CTcpDataAssocParams, CTcpServreInfo *>(params, tcp_s_info));
}

void CTcpDataAssocTranslation::clear() {
     for (assoc_map_it_t it = m_map.begin(); it != m_map.end(); it++) {
        delete it->second;
    }
    m_map.clear();
    m_vec.clear();
}

bool CAstfDB::parse_file(std::string file) {
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

void CAstfDB::convert_from_json(uint8_t socket_id) {
    convert_bufs(socket_id);
    convert_progs(socket_id);
    m_tcp_data[socket_id].m_init = 1;
    build_assoc_translation(socket_id);
    m_tcp_data[socket_id].m_init = 2;
}

void CAstfDB::dump() {
    std::cout << m_val << std::endl;
}

uint16_t CAstfDB::get_buf_index(uint16_t program_index, uint16_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert(cmd["name"] == "tx");

    return cmd["buf_index"].asInt();
}

std::string CAstfDB::get_buf(uint16_t temp_index, uint16_t cmd_index, int side) {
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


tcp_app_cmd_enum_t CAstfDB::get_cmd(uint16_t program_index, uint16_t cmd_index) {
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

uint32_t CAstfDB::get_num_bytes(uint16_t program_index, uint16_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "rx");

    return cmd["min_bytes"].asInt();
}

// verify correctness of json data
CJsonData_err CAstfDB::verify_data(uint16_t max_threads) {
    uint32_t ip_start;
    uint32_t ip_end;
    uint32_t num_ips;
    std::string err_str;

    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    for (int i = 0; i < ip_gen_list.size(); i++) {
        ip_start = ip_from_str(ip_gen_list[i]["ip_start"].asString().c_str());
        ip_end = ip_from_str(ip_gen_list[i]["ip_end"].asString().c_str());
        num_ips = ip_end - ip_start + 1;
        if (num_ips < max_threads) {
            err_str = "Pool:(" + ip_gen_list[i]["ip_start"].asString() + "-"
                + ip_gen_list[i]["ip_end"].asString() + ") has only "
                + std::to_string(num_ips) + " address" + ((num_ips == 1) ? "" : "es")
                + ". Number of IPs in each pool must exceed the number of"
                + " Data path threads, which is " + std::to_string(max_threads);
            return CJsonData_err(CJsonData_err_pool_too_small, err_str);
        }
    }

    return CJsonData_err(CJsonData_err_pool_ok, "");
}

void CAstfDB::verify_init(uint16_t socket_id) {
    if (! m_tcp_data[socket_id].is_init()) {
        // json data should not be accessed by multiple threads in parallel
        std::unique_lock<std::mutex> my_lock(m_global_mtx);
        if (! m_tcp_data[socket_id].is_init()) {
            convert_from_json(socket_id);
        }
        my_lock.unlock();
    }
}

CAstfDbRO *CAstfDB::get_db_ro(uint8_t socket_id) {
    verify_init(socket_id);

    return &m_tcp_data[socket_id];
}

uint32_t CAstfDB::ip_from_str(const char *c_ip) {
    int rc;
    uint32_t ip_num;
    rc = my_inet_pton4(c_ip, (unsigned char *)&ip_num);
    if (! rc) {
        fprintf(stderr, "Error: Bad IP address %s in json. Exiting.", c_ip);
        exit(1);
    }

    return ntohl(ip_num);
}

bool CAstfDB::read_tunables(CTcpTuneables *tune, Json::Value tune_json) {
    if (tune_json == Json::nullValue) {
        return true;
    }

    if (tune_json["tcp"] == Json::nullValue) {
        return true;
    }

    Json::Value json = tune_json["tcp"];
    if (json["mss"] != Json::nullValue) {
        tune->m_mss = json["mss"].asInt();
        tune->add_value(CTcpTuneables::mss_bit);
    }

    return true;
}

CAstfTemplatesRW *CAstfDB::get_db_template_rw(uint8_t socket_id, CTupleGeneratorSmart *g_gen,
                                              uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id) {
    CAstfTemplatesRW *ret = new CAstfTemplatesRW();
    assert(ret);

    // json data should not be accessed by multiple threads in parallel
    std::unique_lock<std::mutex> my_lock(m_global_mtx);

    ClientCfgDB g_dummy;
    g_gen->Create(0, thread_id);
    uint32_t active_flows_per_core;
    CIpPortion  portion;
    CTupleGenPoolYaml poolinfo;
    uint16_t last_c_idx=0;
    uint16_t last_s_idx=0;
    std::vector<uint16_t> gen_idx_trans;

    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    for (int i = 0; i < ip_gen_list.size(); i++) {
        IP_DIST_t dist;
        poolinfo.m_ip_start = ip_from_str(ip_gen_list[i]["ip_start"].asString().c_str());
        poolinfo.m_ip_end = ip_from_str(ip_gen_list[i]["ip_end"].asString().c_str());
        if (! strncmp(ip_gen_list[i]["distribution"].asString().c_str(), "seq", 3)) {
            dist = cdSEQ_DIST;
        } else if (! strncmp(ip_gen_list[i]["distribution"].asString().c_str(), "rand", 4)) {
            dist = cdRANDOM_DIST;
        } else if (! strncmp(ip_gen_list[i]["distribution"].asString().c_str(), "normal", 6)) {
            dist = cdNORMAL_DIST;
        } else {
            fprintf(stderr, "wrong distribution string %s in json\n", ip_gen_list[i]["distribution"].asString().c_str());
            exit(1);
        }
        poolinfo.m_dist =  dist;
        poolinfo.m_dual_interface_mask = ip_from_str(ip_gen_list[i]["ip_offset"].asString().c_str());
        split_ips(thread_id, max_threads, dual_port_id, poolinfo, portion);
        active_flows_per_core = (portion.m_ip_end - portion.m_ip_start) * 32000;
        if (ip_gen_list[i]["dir"] == "c") {
            gen_idx_trans.push_back(last_c_idx);
            last_c_idx++;
            g_gen->add_client_pool(dist, portion.m_ip_start, portion.m_ip_end, active_flows_per_core, g_dummy, 0, 0);
        } else {
            gen_idx_trans.push_back(last_s_idx);
            last_s_idx++;
            g_gen->add_server_pool(dist, portion.m_ip_start, portion.m_ip_end, active_flows_per_core, false);
        }
#if 0
        printf("Thread id:%d - orig ip(%s - %s) per thread ip (%s - %s) dist:%d\n", thread_id,
            ip_to_str(poolinfo.m_ip_start).c_str(), ip_to_str(poolinfo.m_ip_end).c_str(),
               ip_to_str(portion.m_ip_start).c_str(), ip_to_str(portion.m_ip_end).c_str(), dist);
#endif

    }

    CTcpTuneables *c_tune = new CTcpTuneables();
    assert (c_tune);

    CTcpTuneables *s_tune = new CTcpTuneables();
    assert (s_tune);

    read_tunables(c_tune, m_val["c_glob_info"]);
    read_tunables(s_tune, m_val["s_glob_info"]);

    ret->set_tuneables(c_tune, s_tune);

    std::vector<double>  dist;
    // loop over all templates
    for (uint16_t index = 0; index < m_val["templates"].size(); index++) {
        CAstfPerTemplateRW *temp_rw = new CAstfPerTemplateRW();
        assert(temp_rw);

        Json::Value c_temp = m_val["templates"][index]["client_template"];
        CAstfPerTemplateRO template_ro;
        template_ro.m_dual_mask = poolinfo.m_dual_interface_mask; // Should be the same for all poolinfo, so just take from last one
        template_ro.m_client_pool_idx = gen_idx_trans[c_temp["ip_gen"]["dist_client"]["index"].asInt()];
        template_ro.m_server_pool_idx = gen_idx_trans[c_temp["ip_gen"]["dist_server"]["index"].asInt()];
        template_ro.m_one_app_server = false;
        template_ro.m_server_addr = 0;
        template_ro.m_w = 1;
        double cps = cps_factor (c_temp["cps"].asDouble() / max_threads);
        template_ro.m_k_cps = cps;
        dist.push_back(cps);
        template_ro.m_destination_port = c_temp["port"].asInt();

        temp_rw->Create(g_gen, index, thread_id, &template_ro, dual_port_id);

        CTcpTuneables *s_tuneable = new CTcpTuneables();
        CTcpTuneables *c_tuneable = new CTcpTuneables();
        assert(s_tuneable);
        assert(c_tuneable);
        read_tunables(s_tuneable, m_val["templates"][index]["server_template"]["glob_info"]);
        assert(CAstfDB::m_pInstance);
        s_tuneable = CAstfDB::m_pInstance->get_s_tune(index);

        read_tunables(c_tuneable, m_val["templates"][index]["client_template"]["glob_info"]);
        temp_rw->set_tuneables(c_tuneable, s_tuneable);

        ret->add_template(temp_rw);
    }

    /* init scheduler */
    ret->init_scheduler(dist);

    m_rw_db.push_back(ret);

    my_lock.unlock();

    return ret;
}
/*
 * Get program associated with template index and side
 * temp_index - template index
 * side - 0 - client, 1 - server
 * Return pointer to program
 */
CTcpAppProgram *CAstfDB::get_prog(uint16_t temp_index, int side, uint8_t socket_id) {
    std::string temp_str;
    uint32_t program_index;

    assert(m_tcp_data[socket_id].m_init > 0);

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

CTcpServreInfo *CAstfDB::get_server_info_by_port(uint16_t port, uint8_t socket_id) {
    CTcpDataAssocParams params(port);

    assert(m_tcp_data[socket_id].m_init > 0);

    return m_tcp_data[socket_id].m_assoc_trans.get_server_info(params);
}

/*
  Building association translation, and all template related info.
 */
bool CAstfDB::build_assoc_translation(uint8_t socket_id) {
    bool is_hash_needed = false;
    double cps_sum=0;
    CTcpTemplateInfo one_template;
    uint32_t num_bytes_in_template=0;
    double template_cps;

    assert(m_tcp_data[socket_id].m_init > 0);

    if (m_val["templates"].size() > 10) {
        is_hash_needed = true;
    }

    for (uint16_t index = 0; index < m_val["templates"].size(); index++) {
        // build association table
        uint16_t port = m_val["templates"][index]["server_template"]["assoc"][0]["port"].asInt();
        CTcpDataAssocParams tcp_params(port);
        CTcpAppProgram *prog_p = get_prog(index, 1, socket_id);
        assert(prog_p);

        CTcpTuneables *s_tuneable = new CTcpTuneables();
        assert(s_tuneable);
        read_tunables(s_tuneable, m_val["templates"][index]["server_template"]["glob_info"]);

        if (is_hash_needed) {
            m_tcp_data[socket_id].m_assoc_trans.insert_hash(tcp_params, prog_p, s_tuneable, index);
        } else {
            m_tcp_data[socket_id].m_assoc_trans.insert_vec(tcp_params, prog_p, s_tuneable, index);
        }
        m_s_tuneables.push_back(s_tuneable);

        // build template info
        template_cps = cps_factor(m_val["templates"][index]["client_template"]["cps"].asDouble());
        one_template.m_dport = m_val["templates"][index]["client_template"]["port"].asInt();
        uint32_t c_prog_index = m_val["templates"][index]["client_template"]["program_index"].asInt();
        uint32_t s_prog_index = m_val["templates"][index]["server_template"]["program_index"].asInt();
        num_bytes_in_template += m_prog_lens[c_prog_index];
        num_bytes_in_template += m_prog_lens[s_prog_index];
        one_template.m_client_prog = m_tcp_data[socket_id].m_prog_list[c_prog_index];
        one_template.m_num_bytes = num_bytes_in_template;
        m_exp_bps += template_cps * num_bytes_in_template * 8;
        num_bytes_in_template = 0;
        assert(one_template.m_client_prog);
        cps_sum += template_cps;
        m_tcp_data[socket_id].m_templates.push_back(one_template);
    }
    m_tcp_data[socket_id].m_cps_sum = cps_sum;

    return true;
}

/* Convert list of buffers from json to CMbufBuffer */
bool CAstfDB::convert_bufs(uint8_t socket_id) {
    CMbufBuffer *tcp_buf;
    std::string json_buf;
    uint32_t buf_len;

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
bool CAstfDB::convert_progs(uint8_t socket_id) {
    CTcpAppCmd cmd;
    CTcpAppProgram *prog;
    uint16_t cmd_index;
    tcp_app_cmd_enum_t cmd_type;
    uint32_t prog_len=0;

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
                prog_len += cmd.u.m_tx_cmd.m_buf->len();
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
        m_prog_lens.push_back(prog_len);
        prog_len = 0;
    }

    return true;
}

void CAstfDB::get_latency_params(CTcpLatency &lat) {
    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    bool client_set = false;
    bool server_set = false;

    for (int i = 0; i < ip_gen_list.size(); i++) {
        if ((ip_gen_list[i]["dir"] == "c") && ! client_set) {
            client_set = true;
            lat.m_c_ip = ip_from_str(ip_gen_list[i]["ip_start"].asString().c_str());
            lat.m_dual_mask = ip_from_str(ip_gen_list[i]["ip_offset"].asString().c_str());
        }
        if ((ip_gen_list[i]["dir"] == "s") && ! server_set) {
            server_set = true;
            lat.m_s_ip = ip_from_str(ip_gen_list[i]["ip_start"].asString().c_str());
        }

        if (server_set && client_set)
            return;
    }
}

void CAstfDB::clear() {
    int i;
    for (i = 0; i < m_rw_db.size(); i++) {
        CAstfTemplatesRW * lp=m_rw_db[i];
        lp->Delete();
        delete lp;
    }
    for (i = 0; i < m_s_tuneables.size(); i++) {
        delete m_s_tuneables[i];
    }
    m_s_tuneables.clear();

    for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
        m_tcp_data[i].Delete();
    }
    m_json_initiated = false;
}

    CTcpServreInfo * CAstfDbRO::get_server_info_by_port(uint16_t port) {
    CTcpDataAssocParams params(port);
    return m_assoc_trans.get_server_info(params);
}

void CAstfDbRO::dump(FILE *fd) {
#if 0
    fprintf(fd, "buf list:\n");
    for (int i = 0; i < m_buf_list.size(); i++) {
        fprintf(fd, "  *******%d*******\n", i);
        m_buf_list[i]->Dump(fd);
    }
#endif
}

void CAstfDbRO::Delete() {
    int i;
    for (i = 0; i < m_buf_list.size(); i++) {
        m_buf_list[i]->Delete();
        delete m_buf_list[i];
    }
    m_buf_list.clear();

    for (i = 0; i < m_prog_list.size(); i++) {
        delete m_prog_list[i];
    }
    m_prog_list.clear();

    for (i = 0; i < m_templates.size(); i++) {
        m_templates[i].Delete();
    }
    m_templates.clear();

    m_assoc_trans.clear();
    m_init = false;
}
