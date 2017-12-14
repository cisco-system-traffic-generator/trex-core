/*
 Ido Barnea

 Cisco Systems, Inc.
*/

/*
Copyright (c) 2016-2016 Cisco Systems, Inc.

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
#include <stdio.h>
#include "../bp_sim.h"
#include <common/gtest.h>
#include <common/basic_utils.h>
#include "bp_gtest.h"

class client_cfg : public trexStfTest {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};

class basic_client_cfg : public trexStfTest {
    protected:
     virtual void SetUp() {
     }
     virtual void TearDown() {
     }
   public:
};

// testing IP resolution relevant classes
TEST_F(basic_client_cfg, test1) {
    uint32_t ip_start = 0x10010101;
    uint32_t ip_end = 0x100101ff;
    uint32_t next_hop_init = 0x01010101;
    uint32_t next_hop_resp = 0x02020202;
    uint16_t vlan_init = 5;
    uint16_t vlan_resp = 7;
    uint32_t dual_if_mask = 0x01000000;
    uint32_t test_count = 2;
    ClientCfgDB cfg_db;
    ClientCfgDB test_db;
    ClientCfgEntry cfg_ent;
    ClientCfgExt cfg_ext;
    std::vector<ClientCfgCompactEntry *> ent_list;
    struct CTupleGenPoolYaml c_pool;

    // Create tuple gen, so we can have ip->port translation
    CTupleGenYamlInfo tg_yam_info;
    struct CTupleGenPoolYaml s_pool;
    s_pool.m_ip_start = ip_start;
    s_pool.m_ip_end = ip_end;
    s_pool.m_dual_interface_mask = dual_if_mask;
    tg_yam_info.m_client_pool.push_back(s_pool);

    CGlobalInfo::m_options.m_expected_portd = 4;
    printf("Expected ports %d\n", CGlobalInfo::m_options.m_expected_portd);

    std::string tmp_file_name = "generated/client_cfg_gtest_GENERATED.yaml";
    FILE *fd = fopen(tmp_file_name.c_str(), "w");

    if (fd == NULL) {
        fprintf(stderr, "Failed opening %s file for write\n", tmp_file_name.c_str());
    }

    // We create config file with 3 groups (Should match 6 ports).
    cfg_ext.m_initiator.set_next_hop(next_hop_init);
    cfg_ext.m_responder.set_next_hop(next_hop_resp);
    cfg_ext.m_initiator.set_vlan(vlan_init);
    cfg_ext.m_responder.set_vlan(vlan_resp);

    cfg_ent.set_params(ip_start, ip_end, test_count);
    cfg_ent.set_cfg(cfg_ext);

    // first group
    cfg_db.set_vlan(true);
    cfg_db.add_group(ip_start, cfg_ent);

    //second group
    cfg_ent.set_params(ip_start + dual_if_mask, ip_end + dual_if_mask
                       , test_count);
    cfg_db.add_group(ip_start + dual_if_mask, cfg_ent);

    // third group
    cfg_ent.set_params(ip_start + 2 * dual_if_mask, ip_end + 2 * dual_if_mask
                       , test_count);
    cfg_db.add_group(ip_start + dual_if_mask * 2, cfg_ent);

    cfg_db.dump(fd);
    fclose(fd);
    test_db.load_yaml_file(tmp_file_name);
    test_db.set_tuple_gen_info(&tg_yam_info);
    test_db.get_entry_list(ent_list);


    // We expect ports for first two groups to be found.
    // This group addresses should not appear in the list, since
    // we simulate system with only 4 ports
    int i = 0;
    for (std::vector<ClientCfgCompactEntry *>::iterator
             it = ent_list.begin(); it != ent_list.end(); it++) {
        uint8_t port = (*it)->get_port();
        uint16_t vlan = (*it)->get_vlan();
        uint32_t count = (*it)->get_count();
        uint32_t dst_ip = (*it)->get_dst_ip();

        assert(count == test_count);
        switch(i) {
        case 0:
        case 2:
            assert(port == i);
            assert(vlan == vlan_init);
            assert(dst_ip == next_hop_init);
            break;
        case 1:
        case 3:
            assert(port == i);
            assert(vlan == vlan_resp);
            assert(dst_ip == next_hop_resp);
            break;
        default:
            fprintf(stderr, "Test failed. Too many entries returned\n");
            exit(1);
        }
        i++;
        delete *it;
    }

    // Simulate the pre test phase, and hand results to client config
    CManyIPInfo many_ip;
    MacAddress mac0, mac1, mac2, mac3;
    mac0.set(0x0, 0x1, 0x2, 0x3, 0x4, 0);
    mac1.set(0x0, 0x1, 0x2, 0x3, 0x4, 0x1);
    mac2.set(0x0, 0x1, 0x2, 0x3, 0x4, 0x2);
    mac3.set(0x0, 0x1, 0x2, 0x3, 0x4, 0x3);
    COneIPv4Info ip0_1(next_hop_init, vlan_init, mac0, 0);
    COneIPv4Info ip0_2(next_hop_init + 1, vlan_init, mac1, 0);
    COneIPv4Info ip1_1(next_hop_resp, vlan_resp, mac2, 1);
    COneIPv4Info ip1_2(next_hop_resp + 1, vlan_resp, mac3, 1);

    many_ip.insert(ip0_1);
    many_ip.insert(ip0_2);
    many_ip.insert(ip1_1);
    many_ip.insert(ip1_2);

    test_db.set_resolved_macs(many_ip);

    ClientCfgBase cfg0;

    ClientCfgEntry *ent0 = test_db.lookup(ip_start);
    ClientCfgEntry *ent1 = test_db.lookup(ip_start + dual_if_mask);

    assert (ent0 != NULL);
    
    ent0->assign(cfg0, ip_start);
    assert (!memcmp(cfg0.m_initiator.get_dst_mac_addr()
                    , mac0.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent0->assign(cfg0, ip_start + 1);
    assert (!memcmp(cfg0.m_initiator.get_dst_mac_addr()
                    , mac1.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent0->assign(cfg0, ip_start + 2);
    assert (!memcmp(cfg0.m_responder.get_dst_mac_addr()
                    , mac2.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent0->assign(cfg0, ip_start + 3);
    assert (!memcmp(cfg0.m_responder.get_dst_mac_addr()
                    , mac3.GetConstBuffer(), ETHER_ADDR_LEN));

    assert(ent1 != NULL);
    
    ent1->assign(cfg0, ip_start + dual_if_mask);
    assert (!memcmp(cfg0.m_initiator.get_dst_mac_addr()
                    , mac0.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent1->assign(cfg0, ip_start + dual_if_mask + 1);
    assert (!memcmp(cfg0.m_initiator.get_dst_mac_addr()
                    , mac1.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent1->assign(cfg0, ip_start + dual_if_mask + 2);
    assert (!memcmp(cfg0.m_responder.get_dst_mac_addr()
                    , mac2.GetConstBuffer(), ETHER_ADDR_LEN));
    
    ent1->assign(cfg0, ip_start + dual_if_mask + 3);
    assert (!memcmp(cfg0.m_responder.get_dst_mac_addr()
                    , mac3.GetConstBuffer(), ETHER_ADDR_LEN));

}

// simulation testing of MAC based client config
// basic_* tests are checked for memory leaks with valgrind. When running yaml file load/parse, test suite name
// should not start with basic_
TEST_F(client_cfg, test2) {
    CTestBasic t1;
    CParserOption * po =&CGlobalInfo::m_options;

    po->reset();
    po->preview.setVMode(3);
    po->preview.setFileWrite(true);
    po->cfg_file = "cap2/dns.yaml";
    po->out_file = "exp/client_cfg_dns";
    po->client_cfg_file = "cap2/cluster_example.yaml";

    bool res = t1.init();

    EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}
