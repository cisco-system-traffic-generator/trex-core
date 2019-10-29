/* This file should be refactored, it is a total mess !!!!!
   Hanoh
*/

#include <iostream>
#include <unordered_set>
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
#include "44bsd/tcp_var.h"
#include "utl_split.h"
#include "inet_pton.h"
#include "astf/astf_json_validator.h"
#include "stx/astf/trex_astf_topo.h"

#define MAX_TG_NAME_LENGTH 20

inline std::string methodName(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return prettyFunction.substr(begin,end) + "()";
}

#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)

// make the class singleton
astf_db_map_t CAstfDB::m_instances;


CAstfDB::CAstfDB(){
    m_client_config_info=0;
    m_validator=0;
    m_validator = new CAstfJsonValidator();
    if (!m_validator->Create("astf_schema.json")) {
        printf("Could not create ASTF validator using file astf_schema.json\n");
        exit(-1);
    }
    m_topo_mngr = new TopoMngr();
    m_factor = -1.0;
}


CAstfDB::~CAstfDB(){
    if ( m_validator ) {
        m_validator->Delete();
        delete m_validator;
    }
    delete m_topo_mngr;
    for (auto it : m_smart_gen) {
        delete it.second;
    }
    m_smart_gen.clear();
}


CAstfDB* CAstfDB::instance(profile_id_t profile_id) {
    if ( m_instances.find(profile_id) == m_instances.end() ) {
        CAstfDB *new_inst = new CAstfDB();
        new_inst->m_json_initiated = false;
        m_instances[profile_id] = new_inst;
        return new_inst;
    }
    return m_instances[profile_id];
}

void CAstfDB::free_instance(profile_id_t profile_id) {
    if ( m_instances.find(profile_id) != m_instances.end() ){
        delete m_instances[profile_id];
        m_instances.erase(profile_id);
    }
}


bool CAstfDB::validate_profile(Json::Value profile,std::string & err){
    bool res=m_validator->validate_profile(profile, err);
    return (res);
}



double CAstfDB::cps_factor(double cps){
    if (m_factor > 0.0) {
        return ( m_factor*cps );
    } else {
        return ( CGlobalInfo::m_options.m_factor*cps );
    }
}

void CTcpTuneables::dump(FILE *fd) {
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

void CTcpDataAssocTranslation::insert_vec(const CTcpDataAssocParams &params, CEmulAppProgram *prog, CTcpTuneables *tune
                                          , uint32_t temp_idx) {
    CTcpDataAssocTransHelp trans_help(params, prog, tune, temp_idx);
    m_vec.push_back(trans_help);
}

void CTcpDataAssocTranslation::insert_hash(const CTcpDataAssocParams &params, CEmulAppProgram *prog, CTcpTuneables *tune
                                           , uint32_t temp_idx) {
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

void CTcpDataAssocTranslation::enumerate_server_ports(std::vector<uint16_t>& ports, bool is_stream) {
    for (CTcpDataAssocTransHelp& trans_help : m_vec) {
        if (trans_help.m_params.m_stream == is_stream) {
            ports.push_back(trans_help.m_params.m_port);
        }
    }
    for (assoc_map_it_t it = m_map.begin(); it != m_map.end(); it++) {
        if (it->first.m_stream == is_stream) {
            ports.push_back(it->first.m_port);
        }
    }
}

bool CAstfDB::start_profile_no_buffer(Json::Value msg){
    /* free buffer */
    clear_buffers();
    set_profile_one_msg(msg);
    return (true);
}

/* clear buffers */
bool CAstfDB::clear_buffers(){
    m_buffers = Json::nullValue;
    return (true);
}

bool CAstfDB::add_buffers(Json::Value msg,std::string & err){

    if (!msg.isArray()){
        std::stringstream ss;
        ss << "val should be an array of strings  ";
        err = ss.str();
        clear_buffers();
        return (false);
    }

    int i;
    /* check val array type of string */
    for (i=0; i<msg.size(); i++) {
        if (!msg[i].isString()){
            std::stringstream ss;
            ss << "msg[x] should be an array of strings  ";
            err = ss.str();
            return (false);
        }
        m_buffers.append(msg[i]);
    }

    return(true);

}

bool CAstfDB::compile_profile(std::string & err){
    if (!m_val.isObject()){
        std::stringstream ss;
        ss << "val should be an object ";
        err = ss.str();
        return (false);
    }

    if (!m_buffers.isArray()){
        std::stringstream ss;
        ss << "buffers should be an array of strings  ";
        err = ss.str();
        return (false);
    }

    m_val["buf_list"]=m_buffers;
    return (true);
}


bool CAstfDB::compile_profile_dp(uint8_t socket_id){

    return(true);
}


void CAstfDB::dump_profile(FILE *fd){
    Json::StreamWriterBuilder builder;
    std::string output = Json::writeString(builder, m_val);

    fprintf(fd," %s ",output.c_str());
}

void CAstfDB::set_profile_one_msg(Json::Value msg){
    m_val =msg;
}

bool CAstfDB::set_profile_one_msg(const std::string &msg, std::string & err){


    Json::Reader reader;

    err="";
    bool rc = reader.parse(msg, m_val, false);

    if (!rc) {
        std::stringstream ss;
        ss << "Failed parsing json message : " << reader.getFormattedErrorMessages() ;
        err = ss.str();
        return false;
    }

    // Disable verification on too large profiles (empiric size of sfr_full.py * 5)
    if ( msg.size() > 25e6 ) {
        return true;
    }

    return validate_profile(m_val, err);
}


bool CAstfDB::parse_file(std::string file) {

    std::ifstream t(file);
    if ( !t.is_open() ) {
        return false;
    }

    std::string msg((std::istreambuf_iterator<char>(t)),
                                    std::istreambuf_iterator<char>());

    std::string err;
    bool rc = set_profile_one_msg(msg, err);
    if ( !rc ) {
        printf("Parsing ASTF JSON file failed, error:\n%s\n", err.c_str());
        return false;
    }

    m_json_initiated = true;
    return true;
}

bool CAstfDB::convert_from_json(uint8_t socket_id) {
    if ( !convert_bufs(socket_id) ) {
        return false;
    }
    if ( !convert_progs(socket_id) ) {
        return false;
    }
    m_tcp_data[socket_id].m_init = 1;
    if ( !build_assoc_translation(socket_id) ) {
        return false;
    }
    m_tcp_data[socket_id].m_init = 2;
    return true;
}

void CAstfDB::dump() {
    std::cout << m_val << std::endl;
}

uint32_t CAstfDB::get_buf_index(uint32_t program_index, uint32_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert(cmd["name"] == "tx");

    return cmd["buf_index"].asInt();
}

std::string CAstfDB::get_buf(uint32_t temp_index, uint32_t cmd_index, int side) {
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
        uint32_t program_index = m_val["templates"][temp_index][temp_str]["program_index"].asInt();
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "tx");

    uint32_t buf_index = cmd["buf_index"].asInt();
    std::string output = m_val["buf_list"][buf_index].asString();

    return base64_decode(output);
}


bool CAstfDB::get_emul_stream(uint32_t program_index){
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index];
    } catch(std::exception &e) {

        return true;
    }
    if (cmd["stream"] == Json::nullValue){
        return(true);
    }
    return(cmd["stream"].asBool());
}


tcp_app_cmd_enum_t CAstfDB::get_cmd(uint32_t program_index, uint32_t cmd_index) {
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

    if (cmd["name"] == "delay")
        return tcDELAY;

    if (cmd["name"] == "nc")
        return tcDONT_CLOSE;

    if (cmd["name"] == "reset")
        return tcRESET;

    if (cmd["name"] == "connect")
        return tcCONNECT_WAIT;

    if (cmd["name"] == "delay_rnd")
        return tcDELAY_RAND;

    if (cmd["name"] == "set_var")
        return tcSET_VAR;

    if (cmd["name"] == "jmp_nz")
        return tcJMPNZ;

    if (cmd["name"] == "tx_msg")
        return tcTX_PKT;

    if (cmd["name"] == "rx_msg")
        return tcRX_PKT;

    if (cmd["name"] == "keepalive")
        return tcKEEPALIVE;

    if (cmd["name"] == "close_msg")
        return tcCLOSE_PKT;

    if (cmd["name"] == "tx_mode")
        return tcTX_MODE;

    /* TBD need to check the value and put an error  !!! */
    return tcNO_CMD;
}

uint32_t CAstfDB::get_delay_ticks(uint32_t program_index, uint32_t cmd_index) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "delay");

    return tw_time_usec_to_ticks(cmd["usec"].asInt());
}

void CAstfDB::fill_tx_mode(uint32_t program_index,
                            uint32_t cmd_index,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "tx_mode");

    res.u.m_tx_mode.m_flags = cmd["flags"].asUInt();
}


void CAstfDB::fill_delay_rnd(uint32_t program_index,
                            uint32_t cmd_index,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "delay_rnd");

    res.u.m_delay_rnd.m_min_ticks = tw_time_usec_to_ticks(cmd["min_usec"].asUInt());
    res.u.m_delay_rnd.m_max_ticks = tw_time_usec_to_ticks(cmd["max_usec"].asUInt());
}

void CAstfDB::fill_set_var(uint32_t program_index,
                            uint32_t cmd_index,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "set_var");

    res.u.m_var.m_var_id = cmd["id"].asUInt();
    res.u.m_var.m_val    = cmd["val"].asUInt64();
}

void CAstfDB::fill_jmpnz(uint32_t program_index,
                            uint32_t cmd_index,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "jmp_nz");

    res.u.m_jmpnz.m_var_id = cmd["id"].asUInt();
    res.u.m_jmpnz.m_offset = cmd["offset"].asInt();
}

void CAstfDB::fill_tx_pkt(uint32_t program_index,
                            uint32_t cmd_index,
                            uint8_t socket_id,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "tx_msg");

    uint32_t indx=cmd["buf_index"].asUInt();
    res.u.m_tx_pkt.m_buf = m_tcp_data[socket_id].m_buf_list[indx];
}

void CAstfDB::fill_rx_pkt(uint32_t program_index,
                            uint32_t cmd_index,
                            CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "rx_msg");

    uint32_t min_pkts=cmd["min_pkts"].asInt();
    res.u.m_rx_pkt.m_rx_pkts =min_pkts;
    res.u.m_rx_pkt.m_flags =CEmulAppCmdRxPkt::rxcmd_WAIT;

    if (cmd["clear"] != Json::nullValue) {
        if (cmd["clear"].asBool()) {
            res.u.m_rx_cmd.m_flags |= CEmulAppCmdRxPkt::rxcmd_CLEAR;
        }
    }
}

void CAstfDB::fill_keepalive_pkt(uint32_t program_index,
                                 uint32_t cmd_index,
                                 CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "keepalive");

    res.u.m_keepalive.m_keepalive_msec =cmd["msec"].asInt();
}


void CAstfDB::get_rx_cmd(uint32_t program_index, uint32_t cmd_index,CEmulAppCmd &res) {
    Json::Value cmd;

    try {
        cmd = m_val["program_list"][program_index]["commands"][cmd_index];
    } catch(std::exception &e) {
        assert(0);
    }

    assert (cmd["name"] == "rx");

    res.u.m_rx_cmd.m_rx_bytes_wm = cmd["min_bytes"].asInt();
    if (cmd["clear"] != Json::nullValue) {
        if (cmd["clear"].asBool()) {
            res.u.m_rx_cmd.m_flags |= CEmulAppCmdRxPkt::rxcmd_CLEAR;
        }
    }
}

// verify correctness of json data
CJsonData_err CAstfDB::verify_data(uint16_t max_threads) {
    uint32_t ip_start;
    uint32_t ip_end;
    uint32_t num_ips;
    std::string err_str;
    const char* ip_start_str;
    const char* ip_end_str;
    std::string s;


    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    for (int i = 0; i < ip_gen_list.size(); i++) {
        s = ip_gen_list[i]["ip_start"].asString();
        ip_start_str = s.c_str();
        ip_start = ip_from_str(ip_start_str);
        s = ip_gen_list[i]["ip_end"].asString();
        ip_end_str = s.c_str();
        ip_end = ip_from_str(ip_end_str);
        if (ip_end < ip_start) {
            err_str = std::string("IP start: ") + ip_start_str + " is bigger than IP end: " + ip_end_str;
            return CJsonData_err(CJsonData_err_pool_err, err_str);
        }
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

bool CAstfDB::verify_init(uint16_t socket_id) {
    bool success = true;
    if ( !m_tcp_data[socket_id].is_init() ) {
        // json data should not be accessed by multiple threads in parallel
        std::unique_lock<std::mutex> my_lock(m_global_mtx);
        if ( !m_tcp_data[socket_id].is_init() ) {
            success = convert_from_json(socket_id);
        }
        my_lock.unlock();
    }
    return success;
}

void CAstfDB::clear_db_ro(uint8_t socket_id) {
    if ( m_tcp_data[socket_id].is_init() ) {
        // json data should not be accessed by multiple threads in parallel
        std::unique_lock<std::mutex> my_lock(m_global_mtx);
        if ( m_tcp_data[socket_id].is_init() ) {
            m_tcp_data[socket_id].Delete();
        }
        my_lock.unlock();
    }
}

CAstfDbRO *CAstfDB::get_db_ro(uint8_t socket_id) {
    if ( !verify_init(socket_id) ) {
        m_tcp_data[socket_id].Delete();
        return nullptr;
    }

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

bool CAstfDB::read_tunable_uint8(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  uint8_t & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_BYTE, result);
    val = (uint8_t)parent[param].asUInt();
    tune->add_value(enum_val);
    return true;
}

/* raise an Exception in case of an error */
bool CAstfDB::read_tunable_uint16(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  uint16_t & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_UINT16, result);
    val = (uint16_t)parent[param].asUInt();
    tune->add_value(enum_val);
    return true;
}

bool CAstfDB::read_tunable_uint32(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  uint32_t & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_UINT32, result);
    val = (uint32_t)parent[param].asUInt();
    tune->add_value(enum_val);
    return true;
}

bool CAstfDB::read_tunable_uint64(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  uint64_t & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_UINT64, result);
    val = (uint64_t)parent[param].asUInt64();
    tune->add_value(enum_val);
    return true;
}

bool CAstfDB::read_tunable_double(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  double & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_DOUBLE, result);
    val = parent[param].asDouble();
    tune->add_value(enum_val);
    return true;
}

bool CAstfDB::read_tunable_bool(CTcpTuneables *tune,
                                  const Json::Value &parent,
                                  const std::string &param,
                                  uint32_t enum_val,
                                  double & val){

    if (parent[param] == Json::nullValue){
        return false;
    }
    Json::Value result;
    /* no value */
    check_field_type(parent, param, FIELD_TYPE_BOOL, result);
    val = parent[param].asBool();
    tune->add_value(enum_val);
    return true;
}


void CAstfDB::tunable_min_max_u32(std::string param,
                                  uint32_t val,
                                  uint32_t min,
                                  uint32_t max){
    Json::Value result;

    if (val<min) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is smaller than " +std::to_string(min));
    }
    if (val>max) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is greater than " +std::to_string(max));
    }
}

void CAstfDB::tunable_min_max_u64(std::string param,
                                  uint64_t val,
                                  uint64_t min,
                                  uint64_t max){
    Json::Value result;
    if (val<min) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is smaller than " +std::to_string(min));
    }
    if (val>max) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is greater than " +std::to_string(max));
    }
}

void CAstfDB::tunable_min_max_d(std::string param,
                                double val,
                                double min,
                                double max){
    Json::Value result;
    if (val<min) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is smaller than " +std::to_string(min));
    }
    if (val>max) {
        generate_parse_err(result, "field '" + param + "' value " + std::to_string(val)+ " is greater than " +std::to_string(max));
    }
}



bool CAstfDB::read_tunables_ipv6_field(CTcpTuneables *tune,
                                      Json::Value json,
                                      void *field,
                                      uint32_t enum_val){
    if ( json == Json::nullValue) {
        return false;
    }
    Json::Value result;
    if ((json.type() != Json::arrayValue) || (json.size() !=16) ){
        generate_parse_err(result, "ipv6 addr should have Json::arrayValue type with size of 16");
    }else{
        int i;
        uint8_t *p=(uint8_t *)field;
        for (i=0; i<16; i++) {
            p[i]=(uint8_t)(json[i].asUInt());
        }
        tune->add_value(enum_val);
    }
    return(true);
}



bool CAstfDB::read_tunables(CTcpTuneables *tune, Json::Value tune_json) {
    /* TBD for now the exception is handeled only here
       in interactive mode should be in higher level.
       CAstfDB  does not handle errors
    */

    if (tune_json == Json::nullValue) {
        return true;
    }
    try {

        if (tune_json["scheduler"] != Json::nullValue) {
            Json::Value json = tune_json["scheduler"];
            if (read_tunable_uint16(tune,json,"rampup_sec",CTcpTuneables::sched_rampup,tune->m_scheduler_rampup)){
                tunable_min_max_u32("rampup_sec",tune->m_scheduler_rampup,3,60000);
            }
            if (read_tunable_uint8(tune,json,"accurate",CTcpTuneables::sched_accurate,tune->m_scheduler_accurate)){
                tunable_min_max_u32("accurate",tune->m_scheduler_accurate,0,1);
            }
        }

        if (tune_json["ip"] != Json::nullValue) {
            Json::Value json = tune_json["ip"];
            read_tunable_uint8(tune,json,"tos",CTcpTuneables::ip_tos,tune->m_ip_tos);
            /* some devices uses the LSB for filter */
            if (tune->m_ip_tos&0x1) {
                tune->m_ip_tos &=(~0x1);
            }
            read_tunable_uint8(tune,json,"ttl",CTcpTuneables::ip_ttl,tune->m_ip_ttl);
            /* some devices uses the TTL for filters of low latency  */
            if (tune->m_ip_ttl>0x7f) {
                tune->m_ip_ttl = 0x7f;
            }
        }

        if (tune_json["tcp"] != Json::nullValue) {
            Json::Value json = tune_json["tcp"];
            if (read_tunable_uint16(tune,json,"mss",CTcpTuneables::tcp_mss_bit,tune->m_tcp_mss)){
                tunable_min_max_u32("mss",tune->m_tcp_mss,10,9*1024);
            }
            if (read_tunable_uint16(tune,json,"initwnd",CTcpTuneables::tcp_initwnd_bit,tune->m_tcp_initwnd)){
                tunable_min_max_u32("initwnd",tune->m_tcp_initwnd,1,20);
            }

            if (read_tunable_uint32(tune,json,"rxbufsize",CTcpTuneables::tcp_rx_buf_size,tune->m_tcp_rxbufsize)){
                tunable_min_max_u32("rxbufsize",tune->m_tcp_rxbufsize,1*1024,1024*1024*1024);
            }

            if (read_tunable_uint32(tune,json,"txbufsize",CTcpTuneables::tcp_tx_buf_size,tune->m_tcp_txbufsize)){
                tunable_min_max_u32("txbufsize",tune->m_tcp_txbufsize,1*1024,1024*1024*1024);
            }

            if (read_tunable_uint8(tune,json,"rexmtthresh",CTcpTuneables::tcp_rexmtthresh,tune->m_tcp_rexmtthresh)){
                tunable_min_max_u32("rexmtthresh",tune->m_tcp_rexmtthresh,1,10);
            }

            if (read_tunable_uint8(tune,json,"do_rfc1323",CTcpTuneables::tcp_do_rfc1323,tune->m_tcp_do_rfc1323)){
                tunable_min_max_u32("do_rfc1323",tune->m_tcp_do_rfc1323,0,1);
            }

            if (read_tunable_uint8(tune,json,"no_delay",CTcpTuneables::tcp_no_delay,tune->m_tcp_no_delay)){
                tunable_min_max_u32("no_delay",tune->m_tcp_no_delay,0,1);
            }

            if (read_tunable_uint8(tune,json,"keepinit",CTcpTuneables::tcp_keepinit,tune->m_tcp_keepinit)){
                tunable_min_max_u32("keepinit",tune->m_tcp_keepinit,2,253);
            }

            if (read_tunable_uint8(tune,json,"keepidle",CTcpTuneables::tcp_keepidle,tune->m_tcp_keepidle)){
                tunable_min_max_u32("keepidle",tune->m_tcp_keepidle,2,253);
            }

            if (read_tunable_uint8(tune,json,"keepintvl",CTcpTuneables::tcp_keepintvl,tune->m_tcp_keepintvl)){
                tunable_min_max_u32("keepintvl",tune->m_tcp_keepintvl,2,253);
            }

            if (read_tunable_uint8(tune,json,"blackhole",CTcpTuneables::tcp_blackhole,tune->m_tcp_blackhole)){
                tunable_min_max_u32("blackhole",tune->m_tcp_blackhole,0,2);
            }

            if (read_tunable_uint16(tune,json,"delay_ack_msec",CTcpTuneables::tcp_delay_ack,tune->m_tcp_delay_ack_msec)){
                tunable_min_max_u32("delay_ack_msec",tune->m_tcp_delay_ack_msec,20,500);
            }

        }

        if (tune_json["ipv6"] != Json::nullValue) {
            Json::Value json = tune_json["ipv6"];

            if (json["enable"] != Json::nullValue) {
                tune->set_ipv6_enable(json["enable"].asInt());
            }

            Json::Value src_l=json["src_msb"];

            read_tunables_ipv6_field(tune,
                                     src_l,
                                     (void *)tune->m_ipv6_src,
                                     CTcpTuneables::ipv6_src_addr);

            Json::Value dst_l=json["dst_msb"];

            read_tunables_ipv6_field(tune,
                                     dst_l,
                                     (void *)tune->m_ipv6_dst,
                                     CTcpTuneables::ipv6_dst_addr);

        }

    } catch (TrexRpcCommandException &e) {
        printf(" ERROR !!! '%s' \n",e.what());
        /* TBD need a better way to signal this */
        return(true);
    }

    return true;
}

ClientCfgDB  *CAstfDB::get_client_cfg_db() {
    static ClientCfgDB g_dummy;
    if (m_client_config_info==0) {
        return  &g_dummy;
    }else{
        return m_client_config_info;
    }
}


bool CAstfDB::get_latency_info(uint32_t & src_ipv4,
                               uint32_t & dst_ipv4,
                               uint32_t & dual_port_mask){

    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    std::string s;

    if (ip_gen_list.size()<2){
        return(false);
    }
    int i;
    uint32_t valid=0;

    for (i=0; i<2; i++) {
        Json::Value g=ip_gen_list[i];

        if (g["dir"] == "c") {
            s = g["ip_offset"].asString();
            dual_port_mask = ip_from_str(s.c_str());
            s = g["ip_start"].asString();
            src_ipv4 = ip_from_str(s.c_str());
            valid|=1;
        }
        if (g["dir"] == "s") {
            s= g["ip_start"].asString();
            dst_ipv4 = ip_from_str(s.c_str());
            valid|=2;
        }
    }
    if (valid != 3) {
        return(false);
    }
    return(true);
}

void CAstfDB::get_tuple_info(CTupleGenYamlInfo & tuple_info){
    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];

    struct CTupleGenPoolYaml pool;
    pool.m_dist = cdSEQ_DIST;
    pool.m_name = "";
    pool.m_number_of_clients_per_gb=0;
    pool.m_min_clients=0;
    pool.m_dual_interface_mask=0;
    pool.m_tcp_aging_sec=0;
    pool.m_udp_aging_sec=0;
    pool.m_is_bundling=false;
    pool.m_per_core_distro =false;
    std::string s;

    for (int i = 0; i < ip_gen_list.size(); i++) {
        Json::Value g=ip_gen_list[i];
        s = g["ip_start"].asString();
        uint32_t ip_start = ip_from_str(s.c_str());
        s = g["ip_end"].asString();
        uint32_t ip_end = ip_from_str(s.c_str());
        s = g["ip_offset"].asString();
        uint32_t mask = ip_from_str(s.c_str());

        /* backward compatible */
        if (g["per_core_distribution"] != Json::nullValue){
            if (g["per_core_distribution"] == "seq") {
                pool.m_per_core_distro =true;
            }
        }

        pool.m_ip_start = ip_start;
        pool.m_ip_end = ip_end;
        pool.m_dual_interface_mask =  mask ;

        std::vector<CTupleGenPoolYaml> *lp;
        if (g["dir"] == "c") {
            lp =&tuple_info.m_client_pool;
            /* client */
        }else{
            /* server */
            lp =&tuple_info.m_server_pool;
        }
        lp->push_back(pool);
    }
}

void fill_rss_vals(uint16_t thread_id, uint16_t &rss_thread_id, uint16_t &rss_thread_max) {
    rss_thread_id =0;
    rss_thread_max  = CGlobalInfo::m_options.preview.getCores();
    if ( rss_thread_max > 1 ) {
        rss_thread_id = thread_id / CGlobalInfo::m_options.get_expected_dual_ports();
    }
}

void CAstfDB::get_thread_ip_range(uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id,
        std::string ip_start, std::string ip_end, std::string ip_offset, bool per_core_dist, CIpPortion &portion) {

    CTupleGenPoolYaml poolinfo;

    poolinfo.m_per_core_distro = per_core_dist;
    poolinfo.m_ip_start = ip_from_str(ip_start.c_str());
    poolinfo.m_ip_end = ip_from_str(ip_end.c_str());
    poolinfo.m_dual_interface_mask = ip_from_str(ip_offset.c_str());

    if (poolinfo.m_per_core_distro) {
        uint16_t rss_thread_id, rss_thread_max;
        fill_rss_vals(thread_id, rss_thread_id, rss_thread_max);
        split_ips_v2(max_threads, rss_thread_id, rss_thread_max, CGlobalInfo::m_options.get_expected_dual_ports(), dual_port_id, poolinfo, portion);
    }else{
        split_ips(thread_id, max_threads, dual_port_id, poolinfo, portion);
    }
}

CAstfTemplatesRW *CAstfDB::get_db_template_rw(uint8_t socket_id, CTupleGeneratorSmart *g_gen,
                                              uint16_t thread_id, uint16_t max_threads, uint16_t dual_port_id) {
    CAstfTemplatesRW *ret = new CAstfTemplatesRW();
    assert(ret);
    ret->Create(thread_id,max_threads);

    // json data should not be accessed by multiple threads in parallel
    std::unique_lock<std::mutex> my_lock(m_global_mtx);

    if (g_gen == nullptr) {
        g_gen = get_smart_gen(thread_id);
    }
    g_gen->Create(0, thread_id);

    uint16_t rss_thread_id, rss_thread_max;
    fill_rss_vals(thread_id, rss_thread_id, rss_thread_max);
    if ( rss_thread_max > 1 ) {
        g_gen->set_astf_rss_mode(rss_thread_id ,rss_thread_max, CGlobalInfo::m_options.m_reta_mask); /* configure the generator */
    }

    uint32_t active_flows_per_core;
    CIpPortion  portion;
    CTupleGenPoolYaml poolinfo;
    uint32_t last_c_idx=0;
    uint32_t last_s_idx=0;
    std::vector<uint32_t> gen_idx_trans;
    std::string s;

    Json::Value ip_gen_list = m_val["ip_gen_dist_list"];
    for (int i = 0; i < ip_gen_list.size(); i++) {
        IP_DIST_t dist;
        poolinfo.m_per_core_distro =false;
        s = ip_gen_list[i]["ip_start"].asString();
        poolinfo.m_ip_start = ip_from_str(s.c_str());
        s = ip_gen_list[i]["ip_end"].asString();
        poolinfo.m_ip_end = ip_from_str(s.c_str());
        s = ip_gen_list[i]["distribution"].asString();
        if (! strncmp(s.c_str(), "seq", 3)) {
            dist = cdSEQ_DIST;
        } else if (! strncmp(s.c_str(), "rand", 4)) {
            dist = cdRANDOM_DIST;
        } else if (! strncmp(s.c_str(), "normal", 6)) {
            dist = cdNORMAL_DIST;
        } else {
            fprintf(stderr, "wrong distribution string %s in json\n", s.c_str());
            my_lock.unlock();
            return((CAstfTemplatesRW *)0);
        }

        if (ip_gen_list[i]["per_core_distribution"] != Json::nullValue){
            if (ip_gen_list[i]["per_core_distribution"] == "seq") {
                poolinfo.m_per_core_distro =true;
            }
        }

        poolinfo.m_dist =  dist;
        s= ip_gen_list[i]["ip_offset"].asString();
        poolinfo.m_dual_interface_mask = ip_from_str(s.c_str());
        if (poolinfo.m_per_core_distro) {
            split_ips_v2(max_threads, rss_thread_id,rss_thread_max, CGlobalInfo::m_options.get_expected_dual_ports(),dual_port_id, poolinfo, portion);
        }else{
            split_ips(thread_id, max_threads, dual_port_id, poolinfo, portion);
        }
        active_flows_per_core = (portion.m_ip_end - portion.m_ip_start) * 32000;
        if (ip_gen_list[i]["dir"] == "c") {
            gen_idx_trans.push_back(last_c_idx);
            last_c_idx++;
            ClientCfgDB  * cdb=get_client_cfg_db();
            g_gen->add_client_pool(dist, portion.m_ip_start, portion.m_ip_end, active_flows_per_core, *cdb, 0, 0);
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
    for (uint32_t index = 0; index < m_val["templates"].size(); index++) {
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
        template_ro.m_stream = get_emul_stream(c_temp["program_index"].asInt());
        temp_rw->Create(g_gen, index, thread_id, &template_ro, dual_port_id);

        if (c_temp["limit"] != Json::nullValue ){
            uint32_t cnt= utl_split_int(c_temp["limit"].asUInt(),
                                        thread_id,
                                        max_threads);

            /* there is a limit */
            temp_rw->set_limit(cnt);
        }

        CTcpTuneables *s_tuneable;
        CTcpTuneables *c_tuneable = new CTcpTuneables();
        assert(c_tuneable);
        s_tuneable = get_s_tune(index);
        assert(s_tuneable);

        if (!read_tunables(c_tuneable, m_val["templates"][index]["client_template"]["glob_info"])){
            my_lock.unlock();
            return((CAstfTemplatesRW *)0);
        }
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
CEmulAppProgram *CAstfDB::get_prog(uint32_t temp_index, int side, uint8_t socket_id) {
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


/*
  Building association translation, and all template related info.
 */
bool CAstfDB::build_assoc_translation(uint8_t socket_id) {
    bool is_hash_needed = false;
    double cps_sum=0;
    CTcpTemplateInfo one_template;
    uint32_t num_bytes_in_template=0;
    double template_cps;
    std::unordered_set<uint16_t> tcp_server_ports;
    std::unordered_set<uint16_t> udp_server_ports;

    assert(m_tcp_data[socket_id].m_init > 0);

    if ( m_val["templates"].size() == 0 ) {
        return false;
    }

    if (m_val["templates"].size() > 10) {
        is_hash_needed = true;
    }

    uint16_t num_of_tg_ids = m_val["tg_names"].size()+1;
    m_tcp_data[socket_id].m_num_of_tg_ids = num_of_tg_ids; // CAstfDBRo m_num_of_tg_ids updated.
    m_num_of_tg_ids = num_of_tg_ids; // CAstfDB m_num_of_tg_ids updated.
    auto tg_names = m_val["tg_names"];
    m_tg_names.clear(); // To avoid multiple entry.
    for (uint16_t index = 0; index < num_of_tg_ids - 1; index++) {
        std::string tg_name = tg_names[index].asString();
        if (tg_name.length() > MAX_TG_NAME_LENGTH) {
            throw TrexException("TG name too long. Please limit TG names to " + std::to_string(MAX_TG_NAME_LENGTH) + " characters");
        }
        if ( tg_name.size() == 0 ) {
            throw TrexException("Empty TG name");
        }
        m_tg_names.push_back(tg_name);
    }

    for (uint32_t index = 0; index < m_val["templates"].size(); index++) {
        // build association table
        uint16_t port = m_val["templates"][index]["server_template"]["assoc"][0]["port"].asInt();
        bool is_stream = get_emul_stream(m_val["templates"][index]["server_template"]["program_index"].asInt());
        if (is_stream) {
            if (tcp_server_ports.find(port) != tcp_server_ports.end()) {
                // port already seen for tcp
                throw TrexException("Two servers with port " + std::to_string(port));
            }
            tcp_server_ports.insert(port);
        } else {
            if (udp_server_ports.find(port) != udp_server_ports.end()) {
                // port already seen for udp.
                throw TrexException("Two servers with port " + std::to_string(port));
            }
            udp_server_ports.insert(port);
        }
        CTcpDataAssocParams tcp_params(port,is_stream);
        CEmulAppProgram *prog_p = get_prog(index, 1, socket_id);
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
        uint16_t tg_id = m_val["templates"][index]["tg_id"].asInt();
        if ( tg_id > num_of_tg_ids || tg_id < 0 ) {
            throw TrexException("TG ID not in range, it should be between 0 and " + std::to_string(num_of_tg_ids));
        }
        one_template.m_tg_id = tg_id;
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
        return true;

    for (int buf_index = 0; buf_index < m_val["buf_list"].size(); buf_index++) {
        tcp_buf = new CMbufBuffer();
        assert(tcp_buf);
        json_buf = m_val["buf_list"][buf_index].asString();
        std::string temp_str = base64_decode(json_buf);
        buf_len = temp_str.size();
        utl_mbuf_buffer_create_and_copy(socket_id, tcp_buf, 2048, (uint8_t *)(temp_str.c_str()), buf_len,true);
        m_tcp_data[socket_id].m_buf_list.push_back(tcp_buf);
    }

    return true;
}

/* Convert list of programs from json to CMbufBuffer */
bool CAstfDB::convert_progs(uint8_t socket_id) {
    CEmulAppCmd cmd;
    CEmulAppProgram *prog;
    uint32_t cmd_index;
    tcp_app_cmd_enum_t cmd_type;
    uint32_t prog_len=0;

    if (m_val["program_list"].size() == 0)
        return false;

    for (uint32_t program_index = 0; program_index < m_val["program_list"].size(); program_index++) {
        prog = new CEmulAppProgram();
        assert(prog);
        bool is_stream = get_emul_stream(program_index);
        prog->set_stream(is_stream);

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
                cmd.u.m_rx_cmd.m_flags = CEmulAppCmdRxBuffer::rxcmd_WAIT;
                get_rx_cmd(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;
            case tcDELAY:
                cmd.m_cmd = tcDELAY;
                cmd.u.m_delay_cmd.m_ticks =  get_delay_ticks(program_index, cmd_index);
                prog->add_cmd(cmd);
                break;
            case tcRESET:
                cmd.m_cmd = tcRESET;
                prog->add_cmd(cmd);
                break;
            case tcDONT_CLOSE:
                cmd.m_cmd = tcDONT_CLOSE;
                prog->add_cmd(cmd);
                break;
            case tcCONNECT_WAIT:
                cmd.m_cmd = tcCONNECT_WAIT;
                prog->add_cmd(cmd);
                break;
            case tcDELAY_RAND :
                cmd.m_cmd = tcDELAY_RAND;
                fill_delay_rnd(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;
            case tcSET_VAR :
                cmd.m_cmd = tcSET_VAR;
                fill_set_var(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;
            case tcJMPNZ :
                cmd.m_cmd = tcJMPNZ;
                fill_jmpnz(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;

            case tcTX_PKT :
                cmd.m_cmd = tcTX_PKT;
                fill_tx_pkt(program_index, cmd_index,socket_id,cmd);
                prog->add_cmd(cmd);
                break;

            case tcRX_PKT:
                cmd.m_cmd = tcRX_PKT;
                fill_rx_pkt(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;

            case tcKEEPALIVE:
                cmd.m_cmd = tcKEEPALIVE;
                fill_keepalive_pkt(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;

            case tcCLOSE_PKT:
                cmd.m_cmd = tcCLOSE_PKT;
                prog->add_cmd(cmd);
                break;

            case tcTX_MODE :
                cmd.m_cmd = tcTX_MODE;
                fill_tx_mode(program_index, cmd_index,cmd);
                prog->add_cmd(cmd);
                break;


            default:
                assert(0);
            }
            cmd_index++;
        } while (cmd_type != tcNO_CMD);

        std::string err;
        if (!prog->sanity_check(err)){
            prog->Dump(stdout);
            fprintf(stdout,"ERROR program is not valid '%s' \n",err.c_str());
            return(false);
        }
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
     std::string s;

    for (int i = 0; i < ip_gen_list.size(); i++) {
        if ((ip_gen_list[i]["dir"] == "c") && ! client_set) {
            client_set = true;
            s = ip_gen_list[i]["ip_start"].asString();
            lat.m_c_ip = ip_from_str(s.c_str());
            s = ip_gen_list[i]["ip_offset"].asString();
            lat.m_dual_mask = ip_from_str(s.c_str());
        }
        if ((ip_gen_list[i]["dir"] == "s") && ! server_set) {
            server_set = true;
            s = ip_gen_list[i]["ip_start"].asString();
            lat.m_s_ip = ip_from_str(s.c_str());
        }

        if (server_set && client_set)
            return;
    }
}

const std::vector<std::string>& CAstfDB::get_tg_names() {
    return m_tg_names;
}

void CAstfDB::clear_db_ro_rw(CTupleGeneratorSmart *g_gen, uint16_t thread_id) {
    std::unique_lock<std::mutex> my_lock(m_global_mtx);
    if (g_gen == nullptr) {
        remove_smart_gen(thread_id);
    }
    else {
        g_gen->Delete();
    }
    /* to clear only when there is no thread generator */
    if (m_smart_gen.size() == 0) {
        for ( auto &rw_db : m_rw_db) {
            if ( !rw_db ) {
                continue;
            }
            rw_db->Delete();
            delete rw_db;
        }
        m_rw_db.clear();
        for (auto &s_tuneables : m_s_tuneables) {
            delete s_tuneables;
        }
        m_s_tuneables.clear();
        m_prog_lens.clear();

        for (auto &tcp_data : m_tcp_data) {
            tcp_data.Delete();
        }
        m_json_initiated = false;
    }
    my_lock.unlock();
}

CTcpServreInfo * CAstfDbRO::get_server_info_by_port(uint16_t port,bool stream) {
    CTcpDataAssocParams params(port,stream);
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
    if (m_init==0)
        return;

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
    m_init = 0;
    m_cps_sum=0.0;
}

void CAstfDbRO::set_test_assoc_table(uint16_t port, CEmulAppProgram *prog, CTcpTuneables *tune) {
        CTcpDataAssocParams params(port,true);
        CTcpTemplateInfo one_template;
        one_template.m_tg_id = 0;
        m_assoc_trans.insert_vec(params, prog, tune, 0);
        m_num_of_tg_ids = 1;
        m_templates.push_back(one_template);
    }
