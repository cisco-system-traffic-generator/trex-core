/*
Copyright (c) 2015-2018 Cisco Systems, Inc.

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

#include "trex_port_attr.h"
#include "main_dpdk.h"

/*
Get user friendly devices description from saved env. var
Changes certain attributes based on description
*/
void DpdkTRexPortAttr::update_description(){
    char * envvar;
    std::string pci_envvar_name;

    pci_envvar_name = "pci" + intf_info_st.pci_addr;
    std::replace(pci_envvar_name.begin(), pci_envvar_name.end(), ':', '_');
    std::replace(pci_envvar_name.begin(), pci_envvar_name.end(), '.', '_');
    envvar = std::getenv(pci_envvar_name.c_str());
    if (envvar) {
        intf_info_st.description = envvar;
    } else {
        intf_info_st.description = "Unknown";
    }
    if (intf_info_st.description.find("82599ES") != std::string::npos) { // works for 82599EB etc. DPDK does not distinguish them
        flag_is_link_change_supported = false;
    }
    if (intf_info_st.description.find("82545EM") != std::string::npos) { // in virtual E1000, DPDK claims fc is supported, but it's not
        flag_is_fc_change_supported = false;
        flag_is_led_change_supported = false;
    }
    if ( CGlobalInfo::m_options.preview.getVMode() > 0){
        printf("port %d desc: %s\n", m_repid, intf_info_st.description.c_str());
    }
}

int DpdkTRexPortAttr::set_led(bool on){
    if (on) {
        return rte_eth_led_on(m_repid);
    }else{
        return rte_eth_led_off(m_repid);
    }
}

int DpdkTRexPortAttr::get_flow_ctrl(int &mode) {
    int ret = rte_eth_dev_flow_ctrl_get(m_repid, &fc_conf_tmp);
    if (ret) {
        mode = -1;
        return ret;
    }
    mode = (int) fc_conf_tmp.mode;
    return 0;
}

int DpdkTRexPortAttr::set_flow_ctrl(int mode) {
    if (!flag_is_fc_change_supported) {
        return -ENOTSUP;
    }
    int ret = rte_eth_dev_flow_ctrl_get(m_repid, &fc_conf_tmp);
    if (ret) {
        return ret;
    }
    fc_conf_tmp.mode = (enum rte_eth_fc_mode) mode;
    return rte_eth_dev_flow_ctrl_set(m_repid, &fc_conf_tmp);
}

void DpdkTRexPortAttr::reset_xstats() {
    rte_eth_xstats_reset(m_repid);
}

int DpdkTRexPortAttr::get_xstats_values(xstats_values_t &xstats_values) {
    int size = rte_eth_xstats_get(m_repid, NULL, 0);
    if (size < 0) {
        return size;
    }
    xstats_values_tmp.resize(size);
    xstats_values.resize(size);
    size = rte_eth_xstats_get(m_repid, xstats_values_tmp.data(), size);
    if (size < 0) {
        return size;
    }
    for (int i=0; i<size; i++) {
        xstats_values[xstats_values_tmp[i].id] = xstats_values_tmp[i].value;
    }
    return 0;
}

int DpdkTRexPortAttr::get_xstats_names(xstats_names_t &xstats_names){
    int size = rte_eth_xstats_get_names(m_repid, NULL, 0);
    if (size < 0) {
        return size;
    }
    xstats_names_tmp.resize(size);
    xstats_names.resize(size);
    size = rte_eth_xstats_get_names(m_repid, xstats_names_tmp.data(), size);
    if (size < 0) {
        return size;
    }
    for (int i=0; i<size; i++) {
        xstats_names[i] = xstats_names_tmp[i].name;
    }
    return 0;
}

void DpdkTRexPortAttr::dump_link(FILE *fd){
    fprintf(fd,"port : %d \n",(int)m_tvpid);
    fprintf(fd,"------------\n");

    fprintf(fd,"link         : ");
    if (m_link.link_status) {
        fprintf(fd," link : Link Up - speed %u Mbps - %s\n",
                (unsigned) m_link.link_speed,
                (m_link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                ("full-duplex") : ("half-duplex\n"));
    } else {
        fprintf(fd," Link Down\n");
    }
    fprintf(fd,"promiscuous  : %d \n",get_promiscuous());
}

bool fill_pci_dev(struct rte_eth_dev_info *dev_info, struct rte_pci_device* pci_dev) {
    if ( dev_info->device ) {
        const struct rte_bus *bus = nullptr;
        bus = rte_bus_find_by_device(dev_info->device);
        if ( bus && !strcmp(bus->name, "pci") ) {
            *pci_dev = *RTE_DEV_TO_PCI(dev_info->device);
            return true;
        }
    }
    return false;
}

void DpdkTRexPortAttr::update_device_info(){
    rte_eth_dev_info_get(m_repid, &m_dev_info);
    bool ret = fill_pci_dev(&m_dev_info, &m_pci_dev);
    if ( has_pci() ) {
        assert(ret);
        char pci[18];
        rte_pci_device_name(&m_pci_dev.addr, pci, sizeof(pci));
        intf_info_st.pci_addr = pci;
        intf_info_st.numa_node = m_dev_info.device->numa_node;
    } else {
        assert(!ret);
        intf_info_st.pci_addr = "N/A";
        intf_info_st.numa_node = -1;
    }
}

void DpdkTRexPortAttr::get_supported_speeds(supp_speeds_t &supp_speeds){
    uint32_t speed_capa = m_dev_info.speed_capa;
    if (speed_capa & ETH_LINK_SPEED_1G)
        supp_speeds.push_back(ETH_SPEED_NUM_1G);
    if (speed_capa & ETH_LINK_SPEED_10G)
        supp_speeds.push_back(ETH_SPEED_NUM_10G);
    if (speed_capa & ETH_LINK_SPEED_40G)
        supp_speeds.push_back(ETH_SPEED_NUM_40G);
    if (speed_capa & ETH_LINK_SPEED_100G)
        supp_speeds.push_back(ETH_SPEED_NUM_100G);
}

void DpdkTRexPortAttr::update_link_status(){
    rte_eth_link_get(m_repid, &m_link);
}

int DpdkTRexPortAttr::set_promiscuous(bool enable){
    if ( !is_prom_change_supported() ) {
        return -ENOTSUP;
    }
    if (enable) {
        rte_eth_promiscuous_enable(m_repid);
    }else{
        rte_eth_promiscuous_disable(m_repid);
    }
    return 0;
}

int DpdkTRexPortAttr::set_multicast(bool enable){
    if (enable) {
        rte_eth_allmulticast_enable(m_repid);
    }else{
        rte_eth_allmulticast_disable(m_repid);
    }
    return 0;
}

int DpdkTRexPortAttr::set_link_up(bool up){
    if (up) {
        return rte_eth_dev_set_link_up(m_repid);
    }else{
        return rte_eth_dev_set_link_down(m_repid);
    }
}

bool DpdkTRexPortAttr::get_promiscuous(){
    int ret=rte_eth_promiscuous_get(m_repid);
    if (ret<0) {
        rte_exit(EXIT_FAILURE, "rte_eth_promiscuous_get: "
                 "err=%d, port=%u\n",
                 ret, m_repid);

    }
    return ( ret?true:false);
}

bool DpdkTRexPortAttr::get_multicast(){
    int ret=rte_eth_allmulticast_get(m_repid);
    if (ret<0) {
        rte_exit(EXIT_FAILURE, "rte_eth_allmulticast_get: "
                 "err=%d, port=%u\n",
                 ret, m_repid);

    }
    return ( ret?true:false);
}


void DpdkTRexPortAttr::get_hw_src_mac(struct ether_addr *mac_addr){
    rte_eth_macaddr_get(m_repid, mac_addr);
}

