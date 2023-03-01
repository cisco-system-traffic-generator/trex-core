#include <common/gtest.h>
#include "tunnels/tunnel_db.h"
#include <json/json.h>
#include "arpa/inet.h"
#include "common/basic_utils.h"
#include "tunnels/gtp_man.h"
#include "utils/utl_ip.h"
#include "trex_client_config.h"
#include<algorithm>


class gt_tunnel_topo  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};


void initialize_tunnels_topo_json(Json::Value &tunnels_topo, std::vector<CTunnelsCtxGroup>& group_vec) {
    tunnels_topo["tunnels"] = Json::arrayValue;
    Json::Value * p = &tunnels_topo["tunnels"];
    for (CTunnelsCtxGroup& group : group_vec) {
        Json::Value group_json;
        group.to_json(group_json);
        p->append(group_json);
    }
    tunnels_topo["latency"] = Json::arrayValue;
}


void initialize_client_tunnel_data(client_tunnel_data_t* client_data,
                                    uint32_t client_ip,
                                    uint32_t teid,
                                    uint32_t version,
                                    uint16_t src_port,
                                    uint8_t  type,
                                    std::string src_ipv46,
                                    std::string dst_ipv46) {
    memset(client_data, 0 , sizeof(client_tunnel_data_t));
    client_data->client_ip = client_ip;
    client_data->teid = teid;
    client_data->version = version;
    client_data->src_port = src_port;
    client_data->type = type;
    if (version == 6) {
        inet_pton(AF_INET6, src_ipv46.c_str(), client_data->src.ipv6.addr);
        inet_pton(AF_INET6, dst_ipv46.c_str(), client_data->dst.ipv6.addr);
    } else {
        inet_pton(AF_INET, src_ipv46.c_str(), &client_data->src.ipv4);
        inet_pton(AF_INET, dst_ipv46.c_str(), &client_data->dst.ipv4);
    }
}


void initialize_tunnels_group(std::vector<CTunnelsCtxGroup> & group_vec,
                              uint16_t ipv4_count,
                              uint16_t ipv6_count,
                              std::string first_group_src_start,
                              std::string first_group_src_end) {
    uint32_t src_start;
    uint32_t src_end;
    utl_ipv4_to_uint32(first_group_src_start.c_str(), src_start);
    utl_ipv4_to_uint32(first_group_src_end.c_str(), src_end);
    ipv6_count = std::min(ipv6_count, (uint16_t)15);
    for (int i=0;i<ipv4_count;i++) {
        client_tunnel_data_t data;
        std::string index = std::to_string(i);
        initialize_client_tunnel_data(&data, 0, i, 4, 5000 + i, TUNNEL_TYPE_GTP, "11.1.1." + index , "1.1.1." + index);
        group_vec.push_back(CTunnelsCtxGroup(src_start, src_end, 2, data));
        uint32_t dif = src_end - src_start + 1;
        src_start = src_end + 1;
        src_end += dif;
    }
    for (int i=0;i<ipv6_count;i++) {
        client_tunnel_data_t data;
        std::string index = std::to_string(i);
        initialize_client_tunnel_data(&data, 0, 300 + i, 6, 6000 + i, TUNNEL_TYPE_GTP, "ff05::b0b:" + index , "ff04::3000:1" + index);
        group_vec.push_back(CTunnelsCtxGroup(src_start, src_end, 2, data));
        uint32_t dif = src_end - src_start + 1;
        src_start = src_end + 1;
        src_end += dif;
    }
}

bool comapre_ipv6_addr(ipv6_addr_t& ipv6_1, ipv6_addr_t& ipv6_2) {
    for (int i=0;i<8;i++) {
        if (ipv6_1.addr[i] != ipv6_2.addr[i]) {
            return false;
        }
    }
    return true;
}

bool compare_client_data(client_tunnel_data_t& data1, client_tunnel_data_t& data2) {
    bool res = true;
    res = res && (data1.client_ip == data2.client_ip);
    res = res && (data1.teid == data2.teid);
    res = res && (data1.version == data2.version);
    res = res && (data1.src_port == data2.src_port);
    res = res && (data1.type == data2.type);
    if (!res) {
        return false;
    }
    if (data1.version == 6) {
        res = res && comapre_ipv6_addr(data1.src.ipv6, data2.src.ipv6);
        res = res && comapre_ipv6_addr(data1.dst.ipv6, data2.dst.ipv6);
    } else {
        res = res && (data1.src.ipv4 == data2.src.ipv4);
        res = res && (data1.dst.ipv4 == data2.dst.ipv4);
    }
    return res;
}

bool comapre_client_data_to_gtpu_context(client_tunnel_data_t* client_data,
                                        CGtpuCtx* ctx) {
    bool res = true;
    bool is_ipv6 = (client_data->version == 6) ? true : false;
    res = res && (client_data->teid == ctx->get_teid());
    res = res && (is_ipv6 == ctx->is_ipv6());
    res = res && (client_data->src_port == ctx->get_src_port());
    if (ctx->is_ipv6()) {
        ipv6_addr_t ipv6;
        ctx->get_src_ipv6(&ipv6);
        res = res && comapre_ipv6_addr(client_data->src.ipv6, ipv6);
        ctx->get_dst_ipv6(&ipv6);
        res = res && comapre_ipv6_addr(client_data->dst.ipv6, ipv6);
    } else {
        res = res && (client_data->src.ipv4 == ctx->get_src_ipv4());
        res = res && (client_data->dst.ipv4 == ctx->get_dst_ipv4());
    }
    return res;
}

void initialize_ClientCfgEntryTunnel(CClientCfgEntryTunnel* entry,
                                    client_tunnel_data_t* tunnel_data,
                                    std::string src_ip_start,
                                    std::string src_ip_end,
                                    uint32_t teid_jump) {
    uint32_t start_ip;
    uint32_t end_ip;
    utl_ipv4_to_uint32(src_ip_start.c_str(), start_ip);
    utl_ipv4_to_uint32(src_ip_end.c_str(), end_ip);
    entry->m_tunnel_group = CTunnelsCtxGroup(start_ip, end_ip, teid_jump, *tunnel_data);
    //assert(entry->m_ip_end >= entry->m_ip_start);
}

bool contain(CClientCfgEntryTunnel* entry,
                std::string ip_str) {
    uint32_t ip;
    utl_ipv4_to_uint32(ip_str.c_str(), ip);
    return entry->contains(ip);
}


void test_tunnel_entry_assign(uint32_t ip_version,
                             std::string ip_src,
                             std::string ip_dst,
                             uint32_t initial_teid,
                             uint32_t teid_jump,
                             uint16_t src_port) {
    client_tunnel_data_t data;
    CClientCfgEntryTunnel entry;
    ClientCfgBase info;
    initialize_client_tunnel_data(&data, 0, initial_teid, ip_version, src_port, TUNNEL_TYPE_GTP, ip_src, ip_dst);
    initialize_ClientCfgEntryTunnel(&entry, &data, "16.0.0.0", "16.0.0.255", teid_jump);
    uint32_t ip;
    utl_ipv4_to_uint32("16.0.0.200", ip);
    //should allocate the tunnel context of client: "16.0.0.200"
    entry.assign(info, ip);
    bool res = (info.m_tunnel_ctx != nullptr);
    EXPECT_EQ(1, res?1:0);
    // the context type is CGtpuCtx since we used TUNNEL_TYPE_GTP
    CGtpuCtx* ctx = (CGtpuCtx*)info.m_tunnel_ctx;
    // the right teid of client: "16.0.0.200" should be: teid = inital_teid + (index) * teid_jump = 0 + 200*2 = 400
    data.teid = data.teid + 200 * teid_jump;
    res = comapre_client_data_to_gtpu_context(&data, ctx);
    EXPECT_EQ(1, res?1:0);
    delete ctx;
}

bool comapre_tunnels_group(const CTunnelsCtxGroup& group1, const CTunnelsCtxGroup& group2) {
    client_tunnel_data_t data1;
    client_tunnel_data_t data2;
    group1.get_tunnel_data(data1);
    group2.get_tunnel_data(data2);
    if (!compare_client_data(data1, data2)) {
        return false;
    }
    bool matched = true;
    matched = matched && (group1.get_start_ip() ==  group2.get_start_ip());
    matched = matched && (group1.get_end_ip() ==  group2.get_end_ip());
    matched = matched && (group1.get_teid_jump() ==  group2.get_teid_jump());
    matched = matched && (group1.is_activate() ==  group2.is_activate());
    return matched;
}

bool validate_groups_vec(const std::vector<CTunnelsCtxGroup>& group_vec1, const std::vector<CTunnelsCtxGroup>& group_vec2) {
         if (group_vec1.size() != group_vec2.size()) {
             return false;
         }
         for(int i=0;i<group_vec1.size();i++) {
            bool matched = false;
            for (int j=0;j<group_vec2.size();j++) {
                matched = comapre_tunnels_group(group_vec2[i], group_vec2[j]);
                if (matched) {
                    break;
                }
            }
            if (!matched) {
                return false;
            }
         }
         return true;
}


void init_topo(CTunnelsTopo& topo,
                std::vector<CTunnelsCtxGroup>& group_vec,
                Json::Value& tunnels_topo,
                uint16_t ipv4_count,
                uint16_t ipv6_count,
                std::string first_group_src_start,
                std::string first_group_src_end) {
    Json::FastWriter writer;
    //initialize the group_vec with CTunnelsCtxGroup objects
    initialize_tunnels_group(group_vec, ipv4_count, ipv6_count, first_group_src_start, first_group_src_end);
    //initialize the tunnels_topo json wit the tunnels_group object
    initialize_tunnels_topo_json(tunnels_topo, group_vec);
    //convert the tunnels_topo json to string
    const std::string json_str = writer.write(tunnels_topo);
    //initialize the tunnel topo with str
    topo.from_json_str(json_str);
}

void topo_test_help(size_t ipv4_count=255,
                    size_t ipv6_count=0,
                    std::string first_group_src_start = "16.0.0.0",
                    std::string first_group_src_end = "16.0.0.255") {
    std::vector<CTunnelsCtxGroup> group_vec;
    Json::Value tunnels_topo;
    CTunnelsTopo topo;
    init_topo(topo, group_vec, tunnels_topo, ipv4_count, ipv6_count, first_group_src_start, first_group_src_end);
    const std::vector<CTunnelsCtxGroup>& topo_group_vec = topo.get_tunnel_topo();
    //validates the tunnel topo has all the CTunnelsCtxGroup objects from group_vec
    bool res = validate_groups_vec(topo_group_vec, group_vec);
    EXPECT_EQ(1, res?1:0);
    //test for to_json function
    Json::Value tunnels_topo_2;
    topo.to_json(tunnels_topo_2);
    res = (tunnels_topo_2 == tunnels_topo);
    EXPECT_EQ(1, res?1:0);
    //test for clear function
    topo.clear();
    res = (topo_group_vec.size() == 0);
    EXPECT_EQ(1, res?1:0);
}

void tunnel_db_test(size_t ipv4_count=255,
                    size_t ipv6_count=0,
                    std::string first_group_src_start = "16.0.0.0",
                    std::string fist_group_src_end = "16.0.0.255") {
    CTunnelsDB db;
    std::vector<CTunnelsCtxGroup> group_vec;
    Json::Value tunnels_topo;
    CTunnelsTopo topo;
    init_topo(topo, group_vec, tunnels_topo, ipv4_count, ipv6_count, first_group_src_start, fist_group_src_end);
    db.load_from_tunnel_topo(&topo);
    bool res;
    uint32_t prev_src = 0;
    for(int i=0;i<group_vec.size();i++) {
        CClientCfgEntryTunnel* tunnel_entry = db.lookup(group_vec[i].get_start_ip());
        res = tunnel_entry ? true : false;
        EXPECT_EQ(1, res?1:0);
        res = comapre_tunnels_group(tunnel_entry->m_tunnel_group, group_vec[i]);
        EXPECT_EQ(1, res?1:0);
        prev_src = group_vec[i].get_end_ip();
    }
    res =  (db.lookup(prev_src + 1) == nullptr);
    EXPECT_EQ(1, res?1:0);
}

TEST_F(gt_tunnel_topo, tst1) {
    client_tunnel_data_t data;
    CClientCfgEntryTunnel entry;
    initialize_client_tunnel_data(&data, 0, 0, 4, 5000, TUNNEL_TYPE_GTP, "11.1.1.1", "1.1.1.11");
    initialize_ClientCfgEntryTunnel(&entry, &data, "16.0.0.0", "16.0.0.255", 2);
    bool res = contain(&entry, "16.0.0.200");
    EXPECT_EQ(1, res?1:0);
    res = contain(&entry, "16.0.1.0");
    //should fail
    EXPECT_EQ(0, res?1:0);
}

TEST_F(gt_tunnel_topo, tst2) {
    test_tunnel_entry_assign(4, "11.1.1.1", "1.1.1.11", 498, 2, 5000);
    test_tunnel_entry_assign(6, "ff05::b0b:9", "ff04::3000:12", 90, 10, 4000);
}

TEST_F(gt_tunnel_topo, tst3) {
    topo_test_help();
}

TEST_F(gt_tunnel_topo, tst4) {
    topo_test_help(0, 15);
}

TEST_F(gt_tunnel_topo, tst5) {
    topo_test_help(25, 15, "16.0.0.0", "16.0.2.255");
}

TEST_F(gt_tunnel_topo, tst6) {
    tunnel_db_test();
}

TEST_F(gt_tunnel_topo, tst7) {
    tunnel_db_test(0, 15);
}

TEST_F(gt_tunnel_topo, tst8) {
    tunnel_db_test(25, 15, "16.0.0.0", "16.0.2.255");
}