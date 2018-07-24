/*
  TRex team
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

#include "trex_driver_ntacc.h"
#include "trex_driver_defines.h"

#include <rte_tailq.h>

std::string CTRexExtendedDriverBaseNtAcc::ntacc_so_str = "";

std::string& get_ntacc_so_string(void) {
    return CTRexExtendedDriverBaseNtAcc::ntacc_so_str;
}


CTRexExtendedDriverBaseNtAcc::CTRexExtendedDriverBaseNtAcc(){
    m_cap = TREX_DRV_CAP_DROP_Q | TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN ;
    TAILQ_INIT(&lh_fid);
    // The current rte_flow.h is not C++ includable so rte_flow wrappers
    // have been made in libntacc
    void *libntacc = dlopen(ntacc_so_str.c_str(), RTLD_NOW);
    if (libntacc == NULL) {
      /* Library does not exist. */
      fprintf(stderr, "Failed to find needed library : %s\n", ntacc_so_str.c_str());
      exit(-1);
    }
    ntacc_add_rules = (void* (*)(uint8_t, uint16_t,
        uint8_t, int, char *))dlsym(libntacc, "ntacc_add_rules");
    if (ntacc_add_rules == NULL) {
      fprintf(stderr, "Failed to find \"ntacc_add_rules\" in %s\n", ntacc_so_str.c_str());
      exit(-1);
    }
    ntacc_del_rules = (void * (*)(uint8_t, void*))dlsym(libntacc, "ntacc_del_rules");
    if (ntacc_del_rules == NULL) {
      fprintf(stderr, "Failed to find \"ntacc_del_rules\" in %s\n", ntacc_so_str.c_str());
      exit(-1);
    }
}

int CTRexExtendedDriverBaseNtAcc::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}

void CTRexExtendedDriverBaseNtAcc::get_dpdk_drv_params(CTrexDpdkParams &p) {
    p.rx_data_q_num = 1;
    p.rx_drop_q_num = 1;
    p.rx_desc_num_data_q = RX_DESC_NUM_DATA_Q;
    p.rx_desc_num_drop_q = RX_DESC_NUM_DROP_Q;
    p.tx_desc_num = TX_DESC_NUM;
    p.rx_mbuf_type = MBUF_9k;
}
void CTRexExtendedDriverBaseNtAcc::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.rxmode.max_rx_pkt_len =9000;
}

CTRexExtendedDriverBaseNtAcc::~CTRexExtendedDriverBaseNtAcc() {
    struct fid_s *fid, *tfid;
    TAILQ_FOREACH_SAFE(fid, &lh_fid, leTQ, tfid) {
        TAILQ_REMOVE(&lh_fid, fid, leTQ);
        ntacc_del_rules(fid->port_id, fid->rte_flow);
        free(fid);
    }
}

void CTRexExtendedDriverBaseNtAcc::add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type,
    uint8_t l4_proto, int queue, uint32_t f_id, char *ntpl_str) {
    int ret=rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_GENERIC);
    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, port_id);
    }

    // rte_flow.h cannot be included from C++ so we need to call a NtAcc specific C function.
    if (op == RTE_ETH_FILTER_ADD) {
        void *rte_flow = ntacc_add_rules(port_id, type, l4_proto, queue, ntpl_str);

        if (rte_flow == NULL) {
            rte_exit(EXIT_FAILURE, "Failed to add RTE_FLOW\n");
        }

        fid_s *fid = (fid_s*)malloc(sizeof(fid_s));
        if (fid == NULL) {
            rte_exit(EXIT_FAILURE, "Failed to allocate memory\n");
        }

        fid->id = f_id;
        fid->port_id = port_id;
        fid->rte_flow = rte_flow;
        TAILQ_INSERT_TAIL(&lh_fid, fid, leTQ);
    } else {
        fid_s *fid, *tfid;
        TAILQ_FOREACH_SAFE(fid, &lh_fid, leTQ, tfid) {
            if ((fid->id == f_id) && (fid->port_id == port_id)){
                TAILQ_REMOVE(&lh_fid, fid, leTQ);
                ntacc_del_rules(port_id, fid->rte_flow);
                free(fid);
            }
        }
    }
}

int CTRexExtendedDriverBaseNtAcc::add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type) {
    int ret=rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_GENERIC);

    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, port_id);
    }
    return ret;
}

int CTRexExtendedDriverBaseNtAcc::configure_rx_filter_rules_stateless(CPhyEthIF * _if) {
    set_rcv_all(_if, false);
    repid_t port_id =_if->get_repid();

#if 0
    // Enable this when all NICs have rte_flow support
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 1, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 6, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 17, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 132, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 6, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 17, MAIN_DPDK_RX_Q, 0, NULL);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 132, MAIN_DPDK_RX_Q, 0, NULL);
#else
    // Not all NICs have proper rte_flow support. use the Napatech Filter Language for now.
    char ntpl_str[] =
        "((Data[DynOffset = DynOffIpv4Frame; Offset = 1; DataType = ByteStr1 ; DataMask = [0:0]] == 1) OR "
        " (Data[DynOffset = DynOffIpv6Frame; Offset = 0; DataType = ByteStr2 ; DataMask = [11:11]] == 1)) AND "
        "Layer4Protocol == ICMP,UDP,TCP,SCTP";
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NTPL, 0, MAIN_DPDK_RX_Q, 0, ntpl_str);
#endif
    return 0;
}

int CTRexExtendedDriverBaseNtAcc::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    set_rcv_all(_if, false);
    repid_t port_id =_if->get_repid();

    char ntpl_str[] =
        "((Data[DynOffset = DynOffIpv4Frame; Offset = 1; DataType = ByteStr1 ; DataMask = [0:0]] == 1) OR "
        " (Data[DynOffset = DynOffIpv6Frame; Offset = 0; DataType = ByteStr2 ; DataMask = [11:11]] == 1) OR "
        " (Data[DynOffset = DynOffIpv4Frame; Offset = 8; DataType = ByteStr2] == 0xFF11,0xFF06,0xFF01) OR "
        " (Data[DynOffset = DynOffIpv6Frame; Offset = 6; DataType = ByteStr2] == 0x3CFF)) AND "
        "Layer4Protocol == ICMP,UDP,TCP,SCTP";
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NTPL, 0, MAIN_DPDK_RX_Q, 0, ntpl_str);
    return 0;
}


int CTRexExtendedDriverBaseNtAcc::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    repid_t port_id =_if->get_repid();
    add_del_rules(set_on == true ? RTE_ETH_FILTER_ADD : RTE_ETH_FILTER_DELETE,
        port_id, RTE_ETH_FLOW_RAW, 0, MAIN_DPDK_RX_Q, 1, NULL);
    return 0;
}

void CTRexExtendedDriverBaseNtAcc::clear_extended_stats(CPhyEthIF * _if){
    rte_eth_stats_reset(_if->get_repid());
}

bool CTRexExtendedDriverBaseNtAcc::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 0, 0);
}

int CTRexExtendedDriverBaseNtAcc::verify_fw_ver(int port_id) {
    return 0;
}

int CTRexExtendedDriverBaseNtAcc::configure_rx_filter_rules(CPhyEthIF * _if) {
    if (get_is_stateless()) {
        /* Statefull currently work as stateless */
        return configure_rx_filter_rules_stateless(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }
}

void CTRexExtendedDriverBaseNtAcc::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
}

int CTRexExtendedDriverBaseNtAcc::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
  //TODO:
  return 0;
}

// if fd != NULL, dump fdir stats of _if
// return num of filters
int CTRexExtendedDriverBaseNtAcc::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
 return (0);
}

CFlowStatParser *CTRexExtendedDriverBaseNtAcc::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

