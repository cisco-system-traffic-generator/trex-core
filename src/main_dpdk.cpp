/*
  Hanoh Haim
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_random.h>
#include <rte_version.h>
#include <rte_ip.h>

#include "bp_sim.h"
#include "os_time.h"
#include "common/arg/SimpleGlob.h"
#include "common/arg/SimpleOpt.h"
#include "common/basic_utils.h"
#include "stateless/cp/trex_stateless.h"
#include "stateless/dp/trex_stream_node.h"
#include "stateless/messaging/trex_stateless_messaging.h"
#include "stateless/rx/trex_stateless_rx_core.h"
#include "publisher/trex_publisher.h"
#include "../linux_dpdk/version.h"
extern "C" {
#include "dpdk/drivers/net/ixgbe/base/ixgbe_type.h"
#include "dpdk_funcs.h"
}
#include "dpdk/drivers/net/e1000/base/e1000_regs.h"
#include "global_io_mode.h"
#include "utl_term_io.h"
#include "msg_manager.h"
#include "platform_cfg.h"
#include "pre_test.h"
#include "stateful_rx_core.h"
#include "debug.h"
#include "pkt_gen.h"
#include "trex_port_attr.h"
#include "internal_api/trex_platform_api.h"
#include "main_dpdk.h"
#include "trex_watchdog.h"

#define RX_CHECK_MIX_SAMPLE_RATE 8
#define RX_CHECK_MIX_SAMPLE_RATE_1G 2


#define SOCKET0         0

#define MAX_PKT_BURST   32

#define BP_MAX_CORES 32
#define BP_MAX_TX_QUEUE 16
#define BP_MASTER_AND_LATENCY 2

#define RTE_TEST_RX_DESC_DEFAULT 64
#define RTE_TEST_RX_LATENCY_DESC_DEFAULT (1*1024)
#define RTE_TEST_RX_DESC_DEFAULT_MLX 8

#define RTE_TEST_RX_DESC_VM_DEFAULT 512
#define RTE_TEST_TX_DESC_VM_DEFAULT 512

typedef struct rte_mbuf * (*rte_mbuf_convert_to_one_seg_t)(struct rte_mbuf *m);
struct rte_mbuf *  rte_mbuf_convert_to_one_seg(struct rte_mbuf *m);
extern "C" int rte_eth_dev_get_port_by_addr(const struct rte_pci_addr *addr, uint8_t *port_id);
void reorder_dpdk_ports();

#define RTE_TEST_TX_DESC_DEFAULT 512
#define RTE_TEST_RX_DESC_DROP    0

static int max_stat_hw_id_seen = 0;
static int max_stat_hw_id_seen_payload = 0;

static inline int get_vm_one_queue_enable(){
    return (CGlobalInfo::m_options.preview.get_vm_one_queue_enable() ?1:0);
}

static inline int get_is_rx_thread_enabled() {
    return ((CGlobalInfo::m_options.is_rx_enabled() || CGlobalInfo::m_options.is_stateless()) ?1:0);
}

struct port_cfg_t;

#define MAX_DPDK_ARGS 40
static CPlatformYamlInfo global_platform_cfg_info;
static int global_dpdk_args_num ;
static char * global_dpdk_args[MAX_DPDK_ARGS];
static char global_cores_str[100];
static char global_prefix_str[100];
static char global_loglevel_str[20];
static char global_master_id_str[10];

class CTRexExtendedDriverBase {
public:

    /* by default NIC driver adds CRC */
    virtual bool has_crc_added() {
        return true;
    }

    virtual int get_min_sample_rate(void)=0;
    virtual void update_configuration(port_cfg_t * cfg)=0;
    virtual void update_global_config_fdir(port_cfg_t * cfg)=0;

    virtual bool is_hardware_filter_is_supported(){
        return(false);
    }
    virtual int configure_rx_filter_rules(CPhyEthIF * _if)=0;
    virtual int add_del_rx_flow_stat_rule(uint8_t port_id, enum rte_filter_op op, uint16_t l3, uint8_t l4
                                          , uint8_t ipv6_next_h, uint16_t id) {return 0;}
    virtual bool is_hardware_support_drop_queue(){
        return(false);
    }

    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats)=0;
    virtual void clear_extended_stats(CPhyEthIF * _if)=0;
    virtual int  wait_for_stable_link();
    virtual void wait_after_link_up();
    virtual bool hw_rx_stat_supported(){return false;}
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes
                             , int min, int max) {return -1;}
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {}
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd) { return -1;}
    virtual int get_stat_counters_num() {return 0;}
    virtual int get_rx_stat_capabilities() {return 0;}
    virtual int verify_fw_ver(int i) {return 0;}
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on)=0;
    virtual TRexPortAttr * create_port_attr(uint8_t port_id) = 0;
    virtual uint8_t get_num_crc_fix_bytes() {return 0;}

    /* Does this NIC type support automatic packet dropping in case of a link down?
       in case it is supported the packets will be dropped, else there would be a back pressure to tx queues
       this interface is used as a workaround to let TRex work without link in stateless mode, driver that
       does not support that will be failed at init time because it will cause watchdog due to watchdog hang */
    virtual bool drop_packets_incase_of_linkdown() {
        return (false);
    }

    /* Mellanox ConnectX-4 can drop only 35MPPS per Rx queue. to workaround this issue we will create multi rx queue and enable RSS. for Queue1 we will disable  RSS
       return  zero for disable patch and rx queues number for enable  
    */

    virtual uint16_t enable_rss_drop_workaround(void) {
        return (0);
    }

};


class CTRexExtendedDriverBase1G : public CTRexExtendedDriverBase {

public:
    CTRexExtendedDriverBase1G(){
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        return new DpdkTRexPortAttr(port_id, false, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase1G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg);

    virtual int get_min_sample_rate(void){
        return ( RX_CHECK_MIX_SAMPLE_RATE_1G);
    }
    virtual void update_configuration(port_cfg_t * cfg);

    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }

    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_stateless(CPhyEthIF * _if);
    virtual void clear_rx_filter_rules(CPhyEthIF * _if);
    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }

    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd) {return 0;}
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
            | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual int wait_for_stable_link();
    virtual void wait_after_link_up();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);
};

class CTRexExtendedDriverBase1GVm : public CTRexExtendedDriverBase {

public:
    CTRexExtendedDriverBase1GVm(){
        /* we are working in mode that we have 1 queue for rx and one queue for tx*/
        CGlobalInfo::m_options.preview.set_vm_one_queue_enable(true);
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        return new DpdkTRexPortAttr(port_id, true, true);
    }

    virtual bool has_crc_added() {
        return false;
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase1GVm() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg){

    }

    virtual int get_min_sample_rate(void){
        return ( RX_CHECK_MIX_SAMPLE_RATE_1G);
    }
    virtual void update_configuration(port_cfg_t * cfg);

    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }

    virtual int configure_rx_filter_rules(CPhyEthIF * _if);

    virtual bool is_hardware_support_drop_queue(){
        return(false);
    }

    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
            | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on) {return 0;}
};

class CTRexExtendedDriverBaseE1000 : public CTRexExtendedDriverBase1GVm {
    CTRexExtendedDriverBaseE1000() {
        // E1000 driver is only relevant in VM in our case
        CGlobalInfo::m_options.preview.set_vm_one_queue_enable(true);
    }
public:
    static CTRexExtendedDriverBase * create() {
        return ( new CTRexExtendedDriverBaseE1000() );
    }
    // e1000 driver handing us packets with ethernet CRC, so we need to chop them
    virtual uint8_t get_num_crc_fix_bytes() {return 4;}
};

class CTRexExtendedDriverBase10G : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBase10G(){
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        return new DpdkTRexPortAttr(port_id, false, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase10G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg);

    virtual int get_min_sample_rate(void){
        return (RX_CHECK_MIX_SAMPLE_RATE);
    }
    virtual void update_configuration(port_cfg_t * cfg);

    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_stateless(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
            | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual CFlowStatParser *get_flow_stat_parser();
    int add_del_eth_filter(CPhyEthIF * _if, bool is_add, uint16_t ethertype);
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);
};

class CTRexExtendedDriverBase40G : public CTRexExtendedDriverBase10G {
public:
    CTRexExtendedDriverBase40G(){
        // Since we support only 128 counters per if, it is OK to configure here 4 statically.
        // If we want to support more counters in case of card having less interfaces, we
        // Will have to identify the number of interfaces dynamically.
        m_if_per_card = 4;
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        // disabling flow control on 40G using DPDK API causes the interface to malfunction
        return new DpdkTRexPortAttr(port_id, false, false);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase40G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }
    virtual void update_configuration(port_cfg_t * cfg);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int add_del_rx_flow_stat_rule(uint8_t port_id, enum rte_filter_op op, uint16_t l3_proto
                                          , uint8_t l4_proto, uint8_t ipv6_next_h, uint16_t id);
    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }

    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual int wait_for_stable_link();
    virtual bool hw_rx_stat_supported(){return true;}
    virtual int verify_fw_ver(int i);
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);

private:
    virtual void add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type, uint8_t ttl
                               , uint16_t ip_id, uint8_t l4_proto, int queue, uint16_t stat_idx);
    virtual int add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);

    virtual bool drop_packets_incase_of_linkdown() {
        return (true);
    }

private:
    uint8_t m_if_per_card;
};

class CTRexExtendedDriverBaseVIC : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBaseVIC(){
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        return new DpdkTRexPortAttr(port_id, false, false);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBaseVIC() );
    }

    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }
    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }


    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }

    void clear_extended_stats(CPhyEthIF * _if);

    void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);


    virtual int get_min_sample_rate(void){
        return (RX_CHECK_MIX_SAMPLE_RATE);
    }

    virtual int verify_fw_ver(int i);

    virtual void update_configuration(port_cfg_t * cfg);

    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);

private:

    virtual void add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type, uint16_t id
                               , uint8_t l4_proto, uint8_t tos, int queue);
    virtual int add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);

};


class CTRexExtendedDriverBaseMlnx5G : public CTRexExtendedDriverBase10G {
public:
    CTRexExtendedDriverBaseMlnx5G(){
    }

    TRexPortAttr * create_port_attr(uint8_t port_id) {
        // disabling flow control on 40G using DPDK API causes the interface to malfunction
        return new DpdkTRexPortAttr(port_id, false, false);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBaseMlnx5G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }

    virtual void update_configuration(port_cfg_t * cfg);

    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual bool is_hardware_filter_is_supported(){
        return (true);
    }

    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual int get_rx_stat_capabilities() {
        return TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_PAYLOAD;
    }
    virtual int wait_for_stable_link();
    // disabling flow control on 40G using DPDK API causes the interface to malfunction
    virtual bool flow_control_disable_supported(){return false;}
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);

    virtual uint16_t enable_rss_drop_workaround(void) {
        return (5);
    }

private:
    virtual void add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type, uint16_t ip_id, uint8_t l4_proto
                               , int queue);
    virtual int add_del_rx_filter_rules(CPhyEthIF * _if, bool set_on);
};

typedef CTRexExtendedDriverBase * (*create_object_t) (void);


class CTRexExtendedDriverRec {
public:
    std::string         m_driver_name;
    create_object_t     m_constructor;
};

class CTRexExtendedDriverDb {
public:

    const std::string & get_driver_name() {
        return m_driver_name;
    }

    bool is_driver_exists(std::string name);



    void set_driver_name(std::string name){
        m_driver_was_set=true;
        m_driver_name=name;
        printf(" set driver name %s \n",name.c_str());
        m_drv=create_driver(m_driver_name);
        assert(m_drv);
    }

    CTRexExtendedDriverBase * get_drv(){
        if (!m_driver_was_set) {
            printf(" ERROR too early to use this object !\n");
            printf(" need to set the right driver \n");
            assert(0);
        }
        assert(m_drv);
        return (m_drv);
    }

public:

    static CTRexExtendedDriverDb * Ins();

private:
    CTRexExtendedDriverBase * create_driver(std::string name);

    CTRexExtendedDriverDb(){
        register_driver(std::string("rte_ixgbe_pmd"),CTRexExtendedDriverBase10G::create);
        register_driver(std::string("rte_igb_pmd"),CTRexExtendedDriverBase1G::create);
        register_driver(std::string("rte_i40e_pmd"),CTRexExtendedDriverBase40G::create);
        register_driver(std::string("rte_enic_pmd"),CTRexExtendedDriverBaseVIC::create);
        register_driver(std::string("librte_pmd_mlx5"),CTRexExtendedDriverBaseMlnx5G::create);


        /* virtual devices */
        register_driver(std::string("rte_em_pmd"),CTRexExtendedDriverBaseE1000::create);
        register_driver(std::string("rte_vmxnet3_pmd"),CTRexExtendedDriverBase1GVm::create);
        register_driver(std::string("rte_virtio_pmd"),CTRexExtendedDriverBase1GVm::create);




        m_driver_was_set=false;
        m_drv=0;
        m_driver_name="";
    }
    void register_driver(std::string name,create_object_t func);
    static CTRexExtendedDriverDb * m_ins;
    bool        m_driver_was_set;
    std::string m_driver_name;
    CTRexExtendedDriverBase * m_drv;
    std::vector <CTRexExtendedDriverRec*>     m_list;

};

CTRexExtendedDriverDb * CTRexExtendedDriverDb::m_ins;


void CTRexExtendedDriverDb::register_driver(std::string name,
                                            create_object_t func){
    CTRexExtendedDriverRec * rec;
    rec = new CTRexExtendedDriverRec();
    rec->m_driver_name=name;
    rec->m_constructor=func;
    m_list.push_back(rec);
}


bool CTRexExtendedDriverDb::is_driver_exists(std::string name){
    int i;
    for (i=0; i<(int)m_list.size(); i++) {
        if (m_list[i]->m_driver_name == name) {
            return (true);
        }
    }
    return (false);
}


CTRexExtendedDriverBase * CTRexExtendedDriverDb::create_driver(std::string name){
    int i;
    for (i=0; i<(int)m_list.size(); i++) {
        if (m_list[i]->m_driver_name == name) {
            return ( m_list[i]->m_constructor() );
        }
    }
    return( (CTRexExtendedDriverBase *)0);
}



CTRexExtendedDriverDb * CTRexExtendedDriverDb::Ins(){
    if (!m_ins) {
        m_ins = new CTRexExtendedDriverDb();
    }
    return (m_ins);
}

static CTRexExtendedDriverBase *  get_ex_drv(){

    return ( CTRexExtendedDriverDb::Ins()->get_drv());
}

static inline int get_min_sample_rate(void){
    return ( get_ex_drv()->get_min_sample_rate());
}

// cores =0==1,1*2,2,3,4,5,6
// An enum for all the option types
enum { OPT_HELP,
       OPT_MODE_BATCH,
       OPT_MODE_INTERACTIVE,
       OPT_NODE_DUMP,
       OPT_DUMP_INTERFACES,
       OPT_UT,
       OPT_CORES,
       OPT_SINGLE_CORE,
       OPT_FLIP_CLIENT_SERVER,
       OPT_FLOW_FLIP_CLIENT_SERVER,
       OPT_FLOW_FLIP_CLIENT_SERVER_SIDE,
       OPT_RATE_MULT,
       OPT_DURATION,
       OPT_PLATFORM_FACTOR,
       OPT_PUB_DISABLE,
       OPT_LIMT_NUM_OF_PORTS,
       OPT_PLAT_CFG_FILE,
       OPT_MBUF_FACTOR,
       OPT_LATENCY,
       OPT_NO_CLEAN_FLOW_CLOSE,
       OPT_LATENCY_MASK,
       OPT_ONLY_LATENCY,
       OPT_LATENCY_PREVIEW ,
       OPT_WAIT_BEFORE_TRAFFIC,
       OPT_PCAP,
       OPT_RX_CHECK,
       OPT_IO_MODE,
       OPT_IPV6,
       OPT_LEARN,
       OPT_LEARN_MODE,
       OPT_LEARN_VERIFY,
       OPT_L_PKT_MODE,
       OPT_NO_FLOW_CONTROL,
       OPT_VLAN,
       OPT_RX_CHECK_HOPS,
       OPT_CLIENT_CFG_FILE,
       OPT_NO_KEYBOARD_INPUT,
       OPT_VIRT_ONE_TX_RX_QUEUE,
       OPT_PREFIX,
       OPT_SEND_DEBUG_PKT,
       OPT_NO_WATCHDOG,
       OPT_ALLOW_COREDUMP,
       OPT_CHECKSUM_OFFLOAD,
       OPT_CLOSE,
       OPT_ARP_REF_PER,
};

/* these are the argument types:
   SO_NONE --    no argument needed
   SO_REQ_SEP -- single required argument
   SO_MULTI --   multiple arguments needed
*/
static CSimpleOpt::SOption parser_options[] =
    {
        { OPT_HELP,                   "-?",                SO_NONE   },
        { OPT_HELP,                   "-h",                SO_NONE   },
        { OPT_HELP,                   "--help",            SO_NONE   },
        { OPT_UT,                     "--ut",              SO_NONE   },
        { OPT_MODE_BATCH,             "-f",                SO_REQ_SEP},
        { OPT_MODE_INTERACTIVE,       "-i",                SO_NONE   },
        { OPT_PLAT_CFG_FILE,          "--cfg",             SO_REQ_SEP},
        { OPT_SINGLE_CORE,            "-s",                SO_NONE  },
        { OPT_FLIP_CLIENT_SERVER,"--flip",SO_NONE  },
        { OPT_FLOW_FLIP_CLIENT_SERVER,"-p",SO_NONE  },
        { OPT_FLOW_FLIP_CLIENT_SERVER_SIDE,"-e",SO_NONE  },
        { OPT_NO_CLEAN_FLOW_CLOSE,"--nc",SO_NONE  },
        { OPT_LIMT_NUM_OF_PORTS,"--limit-ports", SO_REQ_SEP },
        { OPT_CORES     , "-c",         SO_REQ_SEP },
        { OPT_NODE_DUMP , "-v",         SO_REQ_SEP },
        { OPT_DUMP_INTERFACES , "--dump-interfaces",         SO_MULTI },
        { OPT_LATENCY , "-l",         SO_REQ_SEP },
        { OPT_DURATION     , "-d",  SO_REQ_SEP },
        { OPT_PLATFORM_FACTOR     , "-pm",  SO_REQ_SEP },
        { OPT_PUB_DISABLE     , "-pubd",  SO_NONE },
        { OPT_RATE_MULT     , "-m",  SO_REQ_SEP },
        { OPT_LATENCY_MASK     , "--lm",  SO_REQ_SEP },
        { OPT_ONLY_LATENCY, "--lo",  SO_NONE  },
        { OPT_LATENCY_PREVIEW ,       "-k",   SO_REQ_SEP   },
        { OPT_WAIT_BEFORE_TRAFFIC ,   "-w",   SO_REQ_SEP   },
        { OPT_PCAP,       "--pcap",       SO_NONE   },
        { OPT_RX_CHECK,   "--rx-check",  SO_REQ_SEP },
        { OPT_IO_MODE,   "--iom",  SO_REQ_SEP },
        { OPT_RX_CHECK_HOPS, "--hops", SO_REQ_SEP },
        { OPT_IPV6,       "--ipv6",       SO_NONE   },
        { OPT_LEARN, "--learn",       SO_NONE   },
        { OPT_LEARN_MODE, "--learn-mode",       SO_REQ_SEP   },
        { OPT_LEARN_VERIFY, "--learn-verify",       SO_NONE   },
        { OPT_L_PKT_MODE, "--l-pkt-mode",       SO_REQ_SEP   },
        { OPT_NO_FLOW_CONTROL, "--no-flow-control-change",       SO_NONE   },
        { OPT_VLAN,       "--vlan",       SO_NONE   },
        { OPT_CLIENT_CFG_FILE, "--client_cfg", SO_REQ_SEP },
        { OPT_CLIENT_CFG_FILE, "--client-cfg", SO_REQ_SEP },
        { OPT_NO_KEYBOARD_INPUT ,"--no-key", SO_NONE   },
        { OPT_VIRT_ONE_TX_RX_QUEUE, "--vm-sim", SO_NONE },
        { OPT_PREFIX, "--prefix", SO_REQ_SEP },
        { OPT_SEND_DEBUG_PKT, "--send-debug-pkt", SO_REQ_SEP },
        { OPT_MBUF_FACTOR     , "--mbuf-factor",  SO_REQ_SEP },
        { OPT_NO_WATCHDOG ,     "--no-watchdog",  SO_NONE  },
        { OPT_ALLOW_COREDUMP ,  "--allow-coredump",  SO_NONE  },
        { OPT_CHECKSUM_OFFLOAD, "--checksum-offload", SO_NONE },
        { OPT_CLOSE, "--close-at-end", SO_NONE },
        { OPT_ARP_REF_PER, "--arp-refresh-period", SO_REQ_SEP },
        SO_END_OF_OPTIONS
    };

static int usage(){

    printf(" Usage: t-rex-64 [mode] <options>\n\n");
    printf(" mode is one of:\n");
    printf("   -f <file> : YAML file with traffic template configuration (Will run TRex in 'stateful' mode)\n");
    printf("   -i        : Run TRex in 'stateless' mode\n");
    printf("\n");

    printf(" Available options are:\n");
    printf(" --allow-coredump           : Allow creation of core dump \n");
    printf(" --arp-refresh-period       : Period in seconds between sending of gratuitous ARP for our addresses. Value of 0 means 'never send' \n");
    printf(" -c <num>>                  : Number of hardware threads to allocate for each port pair. Overrides the 'c' argument from config file \n");
    printf(" --cfg <file>               : Use file as TRex config file instead of the default /etc/trex_cfg.yaml \n");
    printf(" --checksum-offload         : Enable IP, TCP and UDP tx checksum offloading, using DPDK. This requires all used interfaces to support this \n");
    printf(" --client_cfg <file>        : YAML file describing clients configuration \n");
    printf(" --close-at-end             : Call rte_eth_dev_stop and close at exit. Calling these functions caused link down issues in older versions, \n");
    printf("                               so we do not call them by default for now. Leaving this as option in case someone thinks it is helpful for him \n");
    printf("                               This it temporary option. Will be removed in the future \n");
    printf(" -d                         : Duration of the test in sec (default is 3600). Look also at --nc \n");
    printf(" -e                         : Like -p but src/dst IP will be chosen according to the port (i.e. on client port send all packets with client src and server dest, and vice versa on server port \n");
    printf(" --flip                     : Each flow will be sent both from client to server and server to client. This can acheive better port utilization when flow traffic is asymmetric \n");
    printf(" --hops <hops>              : If rx check is enabled, the hop number can be assigned. See manual for details \n");
    printf(" --iom  <mode>              : IO mode  for server output [0- silent, 1- normal , 2- short] \n");
    printf(" --ipv6                     : Work in ipv6 mode \n");
    printf(" -k  <num>                  : Run 'warm up' traffic for num seconds before starting the test. \n");
    printf(" -l <rate>                  : In parallel to the test, run latency check, sending packets at rate/sec from each interface \n");
    printf("    Rate of zero means no latency check \n");
    printf(" --learn (deprecated). Replaced by --learn-mode. To get older behaviour, use --learn-mode 2 \n");
    printf(" --learn-mode [1-3]         : Work in NAT environments, learn the dynamic NAT translation and ALG \n");
    printf("      1    Use TCP ACK in first SYN to pass NAT translation information. Will work only for TCP streams. Initial SYN packet must be first packet in stream \n");
    printf("      2    Add special IP option to pass NAT translation information. Will not work on certain firewalls if they drop packets with IP options \n");
    printf("      3    Like 1, but without support for sequence number randomization in server->clien direction. Performance (flow/second) better than 1 \n");
    printf(" --learn-verify             : Test the NAT translation mechanism. Should be used when there is no NAT in the setup \n");
    printf(" --limit-ports              : Limit number of ports used. Must be even number (TRex always uses port pairs) \n");
    printf(" --lm                       : Hex mask of cores that should send traffic \n");
    printf("    For example: Value of 0x5 will cause only ports 0 and 2 to send traffic \n");
    printf(" --lo                       : Only run latency test \n");
    printf(" --l-pkt-mode <0-3>         : Set mode for sending latency packets \n");
    printf("      0 (default)    send SCTP packets  \n");
    printf("      1              Send ICMP request packets  \n");
    printf("      2              Send ICMP requests from client side, and response from server side (for working with firewall) \n");
    printf("      3              Send ICMP requests with sequence ID 0 from both sides \n");
    printf(" -m <num>                   : Rate multiplier.  Multiply basic rate of templates by this number \n");
    printf(" --mbuf-factor              : Factor for packet memory \n");
    printf(" --nc                       : If set, will not wait for all flows to be closed, before terminating - see manual for more information \n");
    printf(" --no-flow-control-change   : By default TRex disables flow-control. If this option is given, it does not touch it \n");
    printf(" --no-key                   : Daemon mode, don't get input from keyboard \n");
    printf(" --no-watchdog              : Disable watchdog \n");
    printf(" -p                         : Send all flow packets from the same interface (choosed randomly between client ad server ports) without changing their src/dst IP \n");
    printf(" -pm                        : Platform factor. If you have splitter in the setup, you can multiply the total results by this factor \n");
    printf("    e.g --pm 2.0 will multiply all the results bps in this factor \n");
    printf(" --prefix <nam>             : For running multi TRex instances on the same machine. Each instance should have different name \n");
    printf(" -pubd                      : Disable monitors publishers \n");
    printf(" --rx-check  <rate>         : Enable rx check. TRex will sample flows at 1/rate and check order, latency and more \n");
    printf(" -s                         : Single core. Run only one data path core. For debug \n");
    printf(" --send-debug-pkt <proto>   : Do not run traffic generator. Just send debug packet and dump receive queues \n");
    printf("    Supported protocols are 1 for icmp, 2 for UDP, 3 for TCP, 4 for ARP, 5 for 9K UDP \n");
    printf(" -v <verbosity level>       : The higher the value, print more debug information \n");
    printf(" --vlan                     : Relevant only for stateless mode with Intel 82599 10G NIC \n");
    printf("                              When configuring flow stat and latency per stream rules, assume all streams uses VLAN \n");
    printf(" --vm-sim                   : Simulate vm with driver of one input queue and one output queue \n");
    printf(" -w  <num>                  : Wait num seconds between init of interfaces and sending traffic, default is 1 \n");
    printf("\n");
    printf(" Examples: ");
    printf(" basic trex run for 20 sec and multiplier of 10 \n");
    printf("  t-rex-64 -f cap2/dns.yaml -m 10 -d 20 \n");
    printf("\n\n");
    printf(" Copyright (c) 2015-2016 Cisco Systems, Inc.    \n");
    printf("                                                                  \n");
    printf(" Licensed under the Apache License, Version 2.0 (the 'License') \n");
    printf(" you may not use this file except in compliance with the License. \n");
    printf(" You may obtain a copy of the License at                          \n");
    printf("                                                                  \n");
    printf("    http://www.apache.org/licenses/LICENSE-2.0                    \n");
    printf("                                                                  \n");
    printf(" Unless required by applicable law or agreed to in writing, software \n");
    printf(" distributed under the License is distributed on an \"AS IS\" BASIS,   \n");
    printf(" WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. \n");
    printf(" See the License for the specific language governing permissions and      \n");
    printf(" limitations under the License.                                           \n");
    printf(" \n");
    printf(" Open Source Components / Libraries \n");
    printf(" DPDK       (BSD)       \n");
    printf(" YAML-CPP   (BSD)       \n");
    printf(" JSONCPP    (MIT)       \n");
    printf(" \n");
    printf(" Open Source Binaries \n");
    printf(" ZMQ        (LGPL v3plus) \n");
    printf(" \n");
    printf(" Version : %s   \n",VERSION_BUILD_NUM);
    printf(" DPDK version : %s   \n",rte_version());
    printf(" User    : %s   \n",VERSION_USER);
    printf(" Date    : %s , %s \n",get_build_date(),get_build_time());
    printf(" Uuid    : %s    \n",VERSION_UIID);
    printf(" Git SHA : %s    \n",VERSION_GIT_SHA);
    return (0);
}


int gtest_main(int argc, char **argv) ;

static void parse_err(const std::string &msg) {
    std::cout << "\nArgument Parsing Error: \n\n" << "*** "<< msg << "\n\n";
    exit(-1);
}

static int parse_options(int argc, char *argv[], CParserOption* po, bool first_time ) {
    CSimpleOpt args(argc, argv, parser_options);

    bool latency_was_set=false;
    (void)latency_was_set;
    char ** rgpszArg = NULL;
    bool opt_vlan_was_set = false;

    int a=0;
    int node_dump=0;

    po->preview.setFileWrite(true);
    po->preview.setRealTime(true);
    uint32_t tmp_data;

    po->m_run_mode = CParserOption::RUN_MODE_INVALID;

    while ( args.Next() ){
        if (args.LastError() == SO_SUCCESS) {
            switch (args.OptionId()) {

            case OPT_UT :
                parse_err("Supported only in simulation");
                break;

            case OPT_HELP:
                usage();
                return -1;

            case OPT_MODE_BATCH:
                if (po->m_run_mode != CParserOption::RUN_MODE_INVALID) {
                    parse_err("Please specify single run mode");
                }
                po->m_run_mode = CParserOption::RUN_MODE_BATCH;
                po->cfg_file = args.OptionArg();
                break;

            case OPT_MODE_INTERACTIVE:
                if (po->m_run_mode != CParserOption::RUN_MODE_INVALID) {
                    parse_err("Please specify single run mode");
                }
                po->m_run_mode = CParserOption::RUN_MODE_INTERACTIVE;
                break;

            case OPT_NO_KEYBOARD_INPUT  :
                po->preview.set_no_keyboard(true);
                break;

            case OPT_CLIENT_CFG_FILE :
                po->client_cfg_file = args.OptionArg();
                break;

            case OPT_PLAT_CFG_FILE :
                po->platform_cfg_file = args.OptionArg();
                break;

            case OPT_SINGLE_CORE :
                po->preview.setSingleCore(true);
                break;

            case OPT_IPV6:
                po->preview.set_ipv6_mode_enable(true);
                break;


            case OPT_LEARN :
                po->m_learn_mode = CParserOption::LEARN_MODE_IP_OPTION;
                break;

            case OPT_LEARN_MODE :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                if (! po->is_valid_opt_val(tmp_data, CParserOption::LEARN_MODE_DISABLED, CParserOption::LEARN_MODE_MAX, "--learn-mode")) {
                    exit(-1);
                }
                po->m_learn_mode = (uint8_t)tmp_data;
                break;

            case OPT_LEARN_VERIFY :
                // must configure learn_mode for learn verify to work. If different learn mode will be given later, it will be set instead.
                if (po->m_learn_mode == 0) {
                    po->m_learn_mode = CParserOption::LEARN_MODE_IP_OPTION;
                }
                po->preview.set_learn_and_verify_mode_enable(true);
                break;

            case OPT_L_PKT_MODE :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                if (! po->is_valid_opt_val(tmp_data, 0, L_PKT_SUBMODE_0_SEQ, "--l-pkt-mode")) {
                    exit(-1);
                }
                po->m_l_pkt_mode=(uint8_t)tmp_data;
                break;

            case OPT_NO_FLOW_CONTROL:
                po->preview.set_disable_flow_control_setting(true);
                break;
            case OPT_VLAN:
                opt_vlan_was_set = true;
                break;
            case OPT_LIMT_NUM_OF_PORTS :
                po->m_expected_portd =atoi(args.OptionArg());
                break;
            case  OPT_CORES  :
                po->preview.setCores(atoi(args.OptionArg()));
                break;
            case OPT_FLIP_CLIENT_SERVER :
                po->preview.setClientServerFlip(true);
                break;
            case OPT_NO_CLEAN_FLOW_CLOSE :
                po->preview.setNoCleanFlowClose(true);
                break;
            case OPT_FLOW_FLIP_CLIENT_SERVER :
                po->preview.setClientServerFlowFlip(true);
                break;
            case OPT_FLOW_FLIP_CLIENT_SERVER_SIDE:
                po->preview.setClientServerFlowFlipAddr(true);
                break;
            case OPT_NODE_DUMP:
                a=atoi(args.OptionArg());
                node_dump=1;
                po->preview.setFileWrite(false);
                break;
            case OPT_DUMP_INTERFACES:
                if (first_time) {
                    rgpszArg = args.MultiArg(1);
                    while (rgpszArg != NULL) {
                        po->dump_interfaces.push_back(rgpszArg[0]);
                        rgpszArg = args.MultiArg(1);
                    }
                }
                if (po->m_run_mode != CParserOption::RUN_MODE_INVALID) {
                    parse_err("Please specify single run mode (-i for stateless, or -f <file> for stateful");
                }
                po->m_run_mode = CParserOption::RUN_MODE_DUMP_INFO;
                break;
            case OPT_MBUF_FACTOR:
                sscanf(args.OptionArg(),"%f", &po->m_mbuf_factor);
                break;
            case OPT_RATE_MULT :
                sscanf(args.OptionArg(),"%f", &po->m_factor);
                break;
            case OPT_DURATION :
                sscanf(args.OptionArg(),"%f", &po->m_duration);
                break;
            case OPT_PUB_DISABLE:
                po->preview.set_zmq_publish_enable(false);
                break;
            case OPT_PLATFORM_FACTOR:
                sscanf(args.OptionArg(),"%f", &po->m_platform_factor);
                break;
            case OPT_LATENCY :
                latency_was_set=true;
                sscanf(args.OptionArg(),"%d", &po->m_latency_rate);
                break;
            case OPT_LATENCY_MASK :
                sscanf(args.OptionArg(),"%x", &po->m_latency_mask);
                break;
            case OPT_ONLY_LATENCY :
                po->preview.setOnlyLatency(true);
                break;
            case OPT_NO_WATCHDOG :
                po->preview.setWDDisable(true);
                break;
            case OPT_ALLOW_COREDUMP :
                po->preview.setCoreDumpEnable(true);
                break;
            case  OPT_LATENCY_PREVIEW :
                sscanf(args.OptionArg(),"%d", &po->m_latency_prev);
                break;
            case  OPT_WAIT_BEFORE_TRAFFIC :
                sscanf(args.OptionArg(),"%d", &po->m_wait_before_traffic);
                break;
            case OPT_PCAP:
                po->preview.set_pcap_mode_enable(true);
                break;
            case OPT_RX_CHECK :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_rx_check_sample=(uint16_t)tmp_data;
                po->preview.set_rx_check_enable(true);
                break;
            case OPT_RX_CHECK_HOPS :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_rx_check_hops = (uint16_t)tmp_data;
                break;
            case OPT_IO_MODE :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_io_mode=(uint16_t)tmp_data;
                break;

            case OPT_VIRT_ONE_TX_RX_QUEUE:
                po->preview.set_vm_one_queue_enable(true);
                break;

            case OPT_PREFIX:
                po->prefix = args.OptionArg();
                break;

            case OPT_SEND_DEBUG_PKT:
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_debug_pkt_proto = (uint8_t)tmp_data;
                break;

            case OPT_CHECKSUM_OFFLOAD:
                po->preview.setChecksumOffloadEnable(true);
                break;

            case OPT_CLOSE:
                po->preview.setCloseEnable(true);
                break;
            case  OPT_ARP_REF_PER:
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_arp_ref_per=(uint16_t)tmp_data;
                break;

            default:
                usage();
                return -1;
                break;
            } // End of switch
        }// End of IF
        else {
            usage();
            return -1;
        }
    } // End of while


    if ((po->m_run_mode ==  CParserOption::RUN_MODE_INVALID) ) {
        parse_err("Please provide single run mode. -f <file> for stateful or -i for stateless (interactive)");
    }

    if (CGlobalInfo::is_learn_mode() && po->preview.get_ipv6_mode_enable()) {
        parse_err("--learn mode is not supported with --ipv6, beacuse there is no such thing as NAT66 (ipv6 to ipv6 translation) \n" \
                  "If you think it is important, please open a defect or write to TRex mailing list\n");
    }

    if (po->preview.get_is_rx_check_enable() ||  po->is_latency_enabled() || CGlobalInfo::is_learn_mode()
        || (CGlobalInfo::m_options.m_arp_ref_per != 0) || get_vm_one_queue_enable()) {
        po->set_rx_enabled();
    }

    if ( node_dump ){
        po->preview.setVMode(a);
    }

    /* if we have a platform factor we need to devided by it so we can still work with normalized yaml profile  */
    po->m_factor = po->m_factor/po->m_platform_factor;

    uint32_t cores=po->preview.getCores();
    if ( cores > ((BP_MAX_CORES)/2-1) ) {
        fprintf(stderr, " Error: maximum supported core number is: %d \n",((BP_MAX_CORES)/2-1));
        return -1;
    }


    if ( first_time ){
        /* only first time read the configuration file */
        if ( po->platform_cfg_file.length() >0  ) {
            if ( node_dump ){
                printf("Using configuration file %s \n",po->platform_cfg_file.c_str());
            }
            global_platform_cfg_info.load_from_yaml_file(po->platform_cfg_file);
            if ( node_dump ){
                global_platform_cfg_info.Dump(stdout);
            }
        }else{
            if ( utl_is_file_exists("/etc/trex_cfg.yaml") ){
                if ( node_dump ){
                    printf("Using configuration file /etc/trex_cfg.yaml \n");
                }
                global_platform_cfg_info.load_from_yaml_file("/etc/trex_cfg.yaml");
                if ( node_dump ){
                    global_platform_cfg_info.Dump(stdout);
                }
            }
        }
    }

    if ( get_is_stateless() ) {
        if ( opt_vlan_was_set ) {
            po->preview.set_vlan_mode_enable(true);
        }
        if (CGlobalInfo::m_options.client_cfg_file != "") {
            parse_err("Client config file is not supported with interactive (stateless) mode ");
        }
        if ( po->m_duration ) {
            parse_err("Duration is not supported with interactive (stateless) mode ");
        }

        if ( po->preview.get_is_rx_check_enable() ) {
            parse_err("Rx check is not supported with interactive (stateless) mode ");
        }

        if  ( (po->is_latency_enabled()) || (po->preview.getOnlyLatency()) ){
            parse_err("Latency check is not supported with interactive (stateless) mode ");
        }

        if ( po->preview.getSingleCore() ){
            parse_err("Single core is not supported with interactive (stateless) mode ");
        }

    }
    else {
        if ( !po->m_duration ) {
            po->m_duration = 3600.0;
        }
    }
    return 0;
}

static int parse_options_wrapper(int argc, char *argv[], CParserOption* po, bool first_time ) {
    // copy, as arg parser sometimes changes the argv
    char ** argv_copy = (char **) malloc(sizeof(char *) * argc);
    for(int i=0; i<argc; i++) {
        argv_copy[i] = strdup(argv[i]);
    }
    int ret = parse_options(argc, argv_copy, po, first_time);

    // free
    for(int i=0; i<argc; i++) {
        free(argv_copy[i]);
    }
    free(argv_copy);
    return ret;
}

int main_test(int argc , char * argv[]);


#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define TX_WTHRESH_1G 1  /**< Default values of TX write-back threshold reg. */
#define TX_PTHRESH_1G 1 /**< Default values of TX prefetch threshold reg. */


struct port_cfg_t {
public:
    port_cfg_t(){
        memset(&m_port_conf,0,sizeof(m_port_conf));
        memset(&m_rx_conf,0,sizeof(m_rx_conf));
        memset(&m_tx_conf,0,sizeof(m_tx_conf));
        memset(&m_rx_drop_conf,0,sizeof(m_rx_drop_conf));

        m_rx_conf.rx_thresh.pthresh = RX_PTHRESH;
        m_rx_conf.rx_thresh.hthresh = RX_HTHRESH;
        m_rx_conf.rx_thresh.wthresh = RX_WTHRESH;
        m_rx_conf.rx_free_thresh =32;

        m_rx_drop_conf.rx_thresh.pthresh = 0;
        m_rx_drop_conf.rx_thresh.hthresh = 0;
        m_rx_drop_conf.rx_thresh.wthresh = 0;
        m_rx_drop_conf.rx_free_thresh =32;
        m_rx_drop_conf.rx_drop_en=1;

        m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
        m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
        m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;

        m_port_conf.rxmode.jumbo_frame=1;
        m_port_conf.rxmode.max_rx_pkt_len =9*1024+22;
        m_port_conf.rxmode.hw_strip_crc=1;
    }



    inline void update_var(void){
        get_ex_drv()->update_configuration(this);
    }

    inline void update_global_config_fdir(void){
        get_ex_drv()->update_global_config_fdir(this);
    }

    /* enable FDIR */
    inline void update_global_config_fdir_10g(void){
        m_port_conf.fdir_conf.mode=RTE_FDIR_MODE_PERFECT_MAC_VLAN;
        m_port_conf.fdir_conf.pballoc=RTE_FDIR_PBALLOC_64K;
        m_port_conf.fdir_conf.status=RTE_FDIR_NO_REPORT_STATUS;
        /* Offset of flexbytes field in RX packets (in 16-bit word units). */
        /* Note: divide by 2 to convert byte offset to word offset */
        if (get_is_stateless()) {
            m_port_conf.fdir_conf.flexbytes_offset = (14+4)/2;
            /* Increment offset 4 bytes for the case where we add VLAN */
            if (  CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) {
                m_port_conf.fdir_conf.flexbytes_offset += (4/2);
            }
        } else {
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ) {
                m_port_conf.fdir_conf.flexbytes_offset = (14+6)/2;
            } else {
                m_port_conf.fdir_conf.flexbytes_offset = (14+8)/2;
            }

            /* Increment offset 4 bytes for the case where we add VLAN */
            if (  CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) {
                m_port_conf.fdir_conf.flexbytes_offset += (4/2);
            }
        }
        m_port_conf.fdir_conf.drop_queue=1;
    }

    inline void update_global_config_fdir_40g(void){
        m_port_conf.fdir_conf.mode=RTE_FDIR_MODE_PERFECT;
        m_port_conf.fdir_conf.pballoc=RTE_FDIR_PBALLOC_64K;
        m_port_conf.fdir_conf.status=RTE_FDIR_NO_REPORT_STATUS;
    }

    struct rte_eth_conf     m_port_conf;
    struct rte_eth_rxconf   m_rx_conf;
    struct rte_eth_rxconf   m_rx_drop_conf;
    struct rte_eth_txconf   m_tx_conf;
};


/* this object is per core / per port / per queue
   each core will have 2 ports to send to


   port0                                port1

   0,1,2,3,..15 out queue ( per core )       0,1,2,3,..15 out queue ( per core )

*/


typedef struct cnt_name_ {
    uint32_t offset;
    char * name;
}cnt_name_t ;

#define MY_REG(a) {a,(char *)#a}

void CPhyEthIFStats::Clear() {
    ipackets = 0;
    ibytes = 0;
    f_ipackets = 0;
    f_ibytes = 0;
    opackets = 0;
    obytes = 0;
    ierrors = 0;
    oerrors = 0;
    imcasts = 0;
    rx_nombuf = 0;
    memset(&m_prev_stats, 0, sizeof(m_prev_stats));
    memset(m_rx_per_flow_pkts, 0, sizeof(m_rx_per_flow_pkts));
    memset(m_rx_per_flow_bytes, 0, sizeof(m_rx_per_flow_bytes));
}

// dump all counters (even ones that equal 0)
void CPhyEthIFStats::DumpAll(FILE *fd) {
#define DP_A4(f) printf(" %-40s : %llu \n",#f, (unsigned long long)f)
#define DP_A(f) if (f) printf(" %-40s : %llu \n",#f, (unsigned long long)f)
    DP_A4(opackets);
    DP_A4(obytes);
    DP_A4(ipackets);
    DP_A4(ibytes);
    DP_A(ierrors);
    DP_A(oerrors);
}

// dump all non zero counters
void CPhyEthIFStats::Dump(FILE *fd) {
    DP_A(opackets);
    DP_A(obytes);
    DP_A(f_ipackets);
    DP_A(f_ibytes);
    DP_A(ipackets);
    DP_A(ibytes);
    DP_A(ierrors);
    DP_A(oerrors);
    DP_A(imcasts);
    DP_A(rx_nombuf);
}

void CPhyEthIgnoreStats::dump(FILE *fd) {
    DP_A4(opackets);
    DP_A4(obytes);
    DP_A4(ipackets);
    DP_A4(ibytes);
    DP_A4(m_tx_arp);
    DP_A4(m_rx_arp);
}

// Clear the RX queue of an interface, dropping all packets
void CPhyEthIF::flush_rx_queue(void){

    rte_mbuf_t * rx_pkts[32];
    int j=0;
    uint16_t cnt=0;

    while (true) {
        j++;
        cnt = rx_burst(m_rx_queue,rx_pkts,32);
        if ( cnt ) {
            int i;
            for (i=0; i<(int)cnt;i++) {
                rte_mbuf_t * m=rx_pkts[i];
                /*printf("rx--\n");
                  rte_pktmbuf_dump(stdout,m, rte_pktmbuf_pkt_len(m));*/
                rte_pktmbuf_free(m);
            }
        }
        if ( ((cnt==0) && (j>10)) || (j>15) ) {
            break;
        }
    }
    if (cnt>0) {
        printf(" Warning can't flush rx-queue for port %d \n",(int)get_port_id());
    }
}


void CPhyEthIF::dump_stats_extended(FILE *fd){

    cnt_name_t reg[]={
        MY_REG(IXGBE_GPTC), /* total packet */
        MY_REG(IXGBE_GOTCL), /* total bytes */
        MY_REG(IXGBE_GOTCH),

        MY_REG(IXGBE_GPRC),
        MY_REG(IXGBE_GORCL),
        MY_REG(IXGBE_GORCH),



        MY_REG(IXGBE_RXNFGPC),
        MY_REG(IXGBE_RXNFGBCL),
        MY_REG(IXGBE_RXNFGBCH),
        MY_REG(IXGBE_RXDGPC  ),
        MY_REG(IXGBE_RXDGBCL ),
        MY_REG(IXGBE_RXDGBCH  ),
        MY_REG(IXGBE_RXDDGPC ),
        MY_REG(IXGBE_RXDDGBCL ),
        MY_REG(IXGBE_RXDDGBCH  ),
        MY_REG(IXGBE_RXLPBKGPC ),
        MY_REG(IXGBE_RXLPBKGBCL),
        MY_REG(IXGBE_RXLPBKGBCH ),
        MY_REG(IXGBE_RXDLPBKGPC ),
        MY_REG(IXGBE_RXDLPBKGBCL),
        MY_REG(IXGBE_RXDLPBKGBCH ),
        MY_REG(IXGBE_TXDGPC      ),
        MY_REG(IXGBE_TXDGBCL     ),
        MY_REG(IXGBE_TXDGBCH     ),
        MY_REG(IXGBE_FDIRUSTAT ),
        MY_REG(IXGBE_FDIRFSTAT ),
        MY_REG(IXGBE_FDIRMATCH ),
        MY_REG(IXGBE_FDIRMISS )

    };
    fprintf (fd," extended counters \n");
    int i;
    for (i=0; i<sizeof(reg)/sizeof(reg[0]); i++) {
        cnt_name_t *lp=&reg[i];
        uint32_t c=pci_reg_read(lp->offset);
        // xl710 bug. Counter values are -559038737 when they should be 0
        if (c && c != -559038737 ) {
            fprintf (fd," %s  : %d \n",lp->name,c);
        }
    }
}

int CPhyEthIF::get_rx_stat_capabilities() {
    return get_ex_drv()->get_rx_stat_capabilities();
}



void CPhyEthIF::configure(uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue,
                          const struct rte_eth_conf *eth_conf){
    int ret;
    ret = rte_eth_dev_configure(m_port_id,
                                nb_rx_queue,
                                nb_tx_queue,
                                eth_conf);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: "
                 "err=%d, port=%u\n",
                 ret, m_port_id);

    /* get device info */
    rte_eth_dev_info_get(m_port_id, &m_dev_info);

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
        /* check if the device supports TCP and UDP checksum offloading */
        if ((m_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) == 0) {
            rte_exit(EXIT_FAILURE, "Device does not support UDP checksum offload: "
                     "port=%u\n",
                     m_port_id);
        }
        if ((m_dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0) {
            rte_exit(EXIT_FAILURE, "Device does not support TCP checksum offload: "
                     "port=%u\n",
                     m_port_id);
        }
    }
}


/*

  rx-queue 0 - default- all traffic not goint to queue 1
  will be drop as queue is disable


  rx-queue 1 - Latency measurement packets will go here

  pci_reg_write(IXGBE_L34T_IMIR(0),(1<<21));

*/

void CPhyEthIF::configure_rx_duplicate_rules(){

    if ( get_is_rx_filter_enable() ){

        if ( get_ex_drv()->is_hardware_filter_is_supported()==false ){
            printf(" ERROR this feature is not supported with current hardware \n");
            exit(1);
        }
        get_ex_drv()->configure_rx_filter_rules(this);
    }
}


void CPhyEthIF::stop_rx_drop_queue() {
    // In debug mode, we want to see all packets. Don't want to disable any queue.
    if ( get_vm_one_queue_enable() || (CGlobalInfo::m_options.m_debug_pkt_proto != 0)) {
        return;
    }
    if ( CGlobalInfo::m_options.is_rx_enabled() ) {
        if ( (!get_ex_drv()->is_hardware_support_drop_queue())  ) {
            printf(" ERROR latency feature is not supported with current hardware  \n");
            exit(1);
        }
    }
    get_ex_drv()->stop_queue(this, MAIN_DPDK_DATA_Q);
}


void CPhyEthIF::rx_queue_setup(uint16_t rx_queue_id,
                               uint16_t nb_rx_desc,
                               unsigned int socket_id,
                               const struct rte_eth_rxconf *rx_conf,
                               struct rte_mempool *mb_pool){

    int ret = rte_eth_rx_queue_setup(m_port_id , rx_queue_id,
                                     nb_rx_desc,
                                     socket_id,
                                     rx_conf,
                                     mb_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: "
                 "err=%d, port=%u\n",
                 ret, m_port_id);
}



void CPhyEthIF::tx_queue_setup(uint16_t tx_queue_id,
                               uint16_t nb_tx_desc,
                               unsigned int socket_id,
                               const struct rte_eth_txconf *tx_conf){

    int ret = rte_eth_tx_queue_setup( m_port_id,
                                      tx_queue_id,
                                      nb_tx_desc,
                                      socket_id,
                                      tx_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
                 "err=%d, port=%u queue=%u\n",
                 ret, m_port_id, tx_queue_id);

}

void CPhyEthIF::stop(){
    if (CGlobalInfo::m_options.preview.getCloseEnable()) {
        rte_eth_dev_stop(m_port_id);
        rte_eth_dev_close(m_port_id);
    }
}

void CPhyEthIF::start(){

    get_ex_drv()->clear_extended_stats(this);

    int ret;

    m_bw_tx.reset();
    m_bw_rx.reset();

    m_stats.Clear();
    int i;
    for (i=0;i<10; i++ ) {
        ret = rte_eth_dev_start(m_port_id);
        if (ret==0) {
            return;
        }
        delay(1000);
    }
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start: "
                 "err=%d, port=%u\n",
                 ret, m_port_id);

}

// Disabling flow control on interface
void CPhyEthIF::disable_flow_control(){
    int ret;
    // see trex-64 issue with loopback on the same NIC
    struct rte_eth_fc_conf fc_conf;
    memset(&fc_conf,0,sizeof(fc_conf));
    fc_conf.mode=RTE_FC_NONE;
    fc_conf.autoneg=1;
    fc_conf.pause_time=100;
    int i;
    for (i=0; i<5; i++) {
        ret=rte_eth_dev_flow_ctrl_set(m_port_id,&fc_conf);
        if (ret==0) {
            break;
        }
        delay(1000);
    }
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_flow_ctrl_set: "
                 "err=%d, port=%u\n probably link is down. Please check your link activity, or skip flow-control disabling, using: --no-flow-control-change option\n",
                 ret, m_port_id);
}

/*
Get user frienly devices description from saved env. var
Changes certain attributes based on description
*/
void DpdkTRexPortAttr::update_description(){
    struct rte_pci_addr pci_addr;
    char pci[16];
    char * envvar;
    std::string pci_envvar_name;
    pci_addr = rte_eth_devices[m_port_id].pci_dev->addr;
    snprintf(pci, sizeof(pci), "%04x:%02x:%02x.%d", pci_addr.domain, pci_addr.bus, pci_addr.devid, pci_addr.function);
    intf_info_st.pci_addr = pci;
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
        printf("port %d desc: %s\n", m_port_id, intf_info_st.description.c_str());
    }
}

int DpdkTRexPortAttr::set_led(bool on){
    if (on) {
        return rte_eth_led_on(m_port_id);
    }else{
        return rte_eth_led_off(m_port_id);
    }
}

int DpdkTRexPortAttr::get_flow_ctrl(int &mode) {
    int ret = rte_eth_dev_flow_ctrl_get(m_port_id, &fc_conf_tmp);
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
    int ret = rte_eth_dev_flow_ctrl_get(m_port_id, &fc_conf_tmp);
    if (ret) {
        return ret;
    }
    fc_conf_tmp.mode = (enum rte_eth_fc_mode) mode;
    return rte_eth_dev_flow_ctrl_set(m_port_id, &fc_conf_tmp);
}

void DpdkTRexPortAttr::reset_xstats() {
    rte_eth_xstats_reset(m_port_id);
}

int DpdkTRexPortAttr::get_xstats_values(xstats_values_t &xstats_values) {
    int size = rte_eth_xstats_get(m_port_id, NULL, 0);
    if (size < 0) {
        return size;
    }
    xstats_values_tmp.resize(size);
    xstats_values.resize(size);
    size = rte_eth_xstats_get(m_port_id, xstats_values_tmp.data(), size);
    if (size < 0) {
        return size;
    }
    for (int i=0; i<size; i++) {
        xstats_values[xstats_values_tmp[i].id] = xstats_values_tmp[i].value;
    }
    return 0;
}

int DpdkTRexPortAttr::get_xstats_names(xstats_names_t &xstats_names){
    int size = rte_eth_xstats_get_names(m_port_id, NULL, 0);
    if (size < 0) {
        return size;
    }
    xstats_names_tmp.resize(size);
    xstats_names.resize(size);
    size = rte_eth_xstats_get_names(m_port_id, xstats_names_tmp.data(), size);
    if (size < 0) {
        return size;
    }
    for (int i=0; i<size; i++) {
        xstats_names[i] = xstats_names_tmp[i].name;
    }
    return 0;
}

void DpdkTRexPortAttr::dump_link(FILE *fd){
    fprintf(fd,"port : %d \n",(int)m_port_id);
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

void DpdkTRexPortAttr::update_device_info(){
    rte_eth_dev_info_get(m_port_id, &dev_info);
}

void DpdkTRexPortAttr::get_supported_speeds(supp_speeds_t &supp_speeds){
    uint32_t speed_capa = dev_info.speed_capa;
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
    rte_eth_link_get(m_port_id, &m_link);
}

bool DpdkTRexPortAttr::update_link_status_nowait(){
    rte_eth_link new_link;
    bool changed = false;
    rte_eth_link_get_nowait(m_port_id, &new_link);

    if (new_link.link_speed != m_link.link_speed ||
                new_link.link_duplex != m_link.link_duplex ||
                    new_link.link_autoneg != m_link.link_autoneg ||
                        new_link.link_status != m_link.link_status) {
        changed = true;

        /* in case of link status change - notify the dest object */
        if (new_link.link_status != m_link.link_status) {
            get_dest().on_link_down();
        }
    }

    m_link = new_link;
    return changed;
}

int DpdkTRexPortAttr::add_mac(char * mac){
    struct ether_addr mac_addr;
    for (int i=0; i<6;i++) {
        mac_addr.addr_bytes[i] =mac[i];
    }
    return rte_eth_dev_mac_addr_add(m_port_id, &mac_addr,0);
}

int DpdkTRexPortAttr::set_promiscuous(bool enable){
    if (enable) {
        rte_eth_promiscuous_enable(m_port_id);
    }else{
        rte_eth_promiscuous_disable(m_port_id);
    }
    return 0;
}

int DpdkTRexPortAttr::set_link_up(bool up){
    if (up) {
        return rte_eth_dev_set_link_up(m_port_id);
    }else{
        return rte_eth_dev_set_link_down(m_port_id);
    }
}

bool DpdkTRexPortAttr::get_promiscuous(){
    int ret=rte_eth_promiscuous_get(m_port_id);
    if (ret<0) {
        rte_exit(EXIT_FAILURE, "rte_eth_promiscuous_get: "
                 "err=%d, port=%u\n",
                 ret, m_port_id);

    }
    return ( ret?true:false);
}


void DpdkTRexPortAttr::get_hw_src_mac(struct ether_addr *mac_addr){
    rte_eth_macaddr_get(m_port_id , mac_addr);
}

int CPhyEthIF::dump_fdir_global_stats(FILE *fd) {
    return get_ex_drv()->dump_fdir_global_stats(this, fd);
}

void dump_hw_state(FILE *fd,struct ixgbe_hw_stats *hs ){

#define DP_A1(f) if (hs->f) fprintf(fd," %-40s : %llu \n",#f, (unsigned long long)hs->f)
#define DP_A2(f,m) for (i=0;i<m; i++) { if (hs->f[i]) fprintf(fd," %-40s[%d] : %llu \n",#f,i, (unsigned long long)hs->f[i]); }
    int i;

    //for (i=0;i<8; i++) { if (hs->mpc[i]) fprintf(fd," %-40s[%d] : %llu \n","mpc",i,hs->mpc[i]); }
    DP_A2(mpc,8);
    DP_A1(crcerrs);
    DP_A1(illerrc);
    //DP_A1(errbc);
    DP_A1(mspdc);
    DP_A1(mpctotal);
    DP_A1(mlfc);
    DP_A1(mrfc);
    DP_A1(rlec);
    //DP_A1(lxontxc);
    //DP_A1(lxonrxc);
    //DP_A1(lxofftxc);
    //DP_A1(lxoffrxc);
    //DP_A2(pxontxc,8);
    //DP_A2(pxonrxc,8);
    //DP_A2(pxofftxc,8);
    //DP_A2(pxoffrxc,8);

    //DP_A1(prc64);
    //DP_A1(prc127);
    //DP_A1(prc255);
    // DP_A1(prc511);
    //DP_A1(prc1023);
    //DP_A1(prc1522);

    DP_A1(gprc);
    DP_A1(bprc);
    DP_A1(mprc);
    DP_A1(gptc);
    DP_A1(gorc);
    DP_A1(gotc);
    DP_A2(rnbc,8);
    DP_A1(ruc);
    DP_A1(rfc);
    DP_A1(roc);
    DP_A1(rjc);
    DP_A1(mngprc);
    DP_A1(mngpdc);
    DP_A1(mngptc);
    DP_A1(tor);
    DP_A1(tpr);
    DP_A1(tpt);
    DP_A1(ptc64);
    DP_A1(ptc127);
    DP_A1(ptc255);
    DP_A1(ptc511);
    DP_A1(ptc1023);
    DP_A1(ptc1522);
    DP_A1(mptc);
    DP_A1(bptc);
    DP_A1(xec);
    DP_A2(qprc,16);
    DP_A2(qptc,16);
    DP_A2(qbrc,16);
    DP_A2(qbtc,16);
    DP_A2(qprdc,16);
    DP_A2(pxon2offc,8);
    DP_A1(fdirustat_add);
    DP_A1(fdirustat_remove);
    DP_A1(fdirfstat_fadd);
    DP_A1(fdirfstat_fremove);
    DP_A1(fdirmatch);
    DP_A1(fdirmiss);
    DP_A1(fccrc);
    DP_A1(fclast);
    DP_A1(fcoerpdc);
    DP_A1(fcoeprc);
    DP_A1(fcoeptc);
    DP_A1(fcoedwrc);
    DP_A1(fcoedwtc);
    DP_A1(fcoe_noddp);
    DP_A1(fcoe_noddp_ext_buff);
    DP_A1(ldpcec);
    DP_A1(pcrc8ec);
    DP_A1(b2ospc);
    DP_A1(b2ogprc);
    DP_A1(o2bgptc);
    DP_A1(o2bspc);
}

void CPhyEthIF::set_ignore_stats_base(CPreTestStats &pre_stats) {
    // reading m_stats, so drivers saving prev in m_stats will be updated.
    // Actually, we want m_stats to be cleared
    get_ex_drv()->get_extended_stats(this, &m_stats);

    m_ignore_stats.ipackets = m_stats.ipackets;
    m_ignore_stats.ibytes = m_stats.ibytes;
    m_ignore_stats.opackets = m_stats.opackets;
    m_ignore_stats.obytes = m_stats.obytes;
    m_stats.ipackets = 0;
    m_stats.opackets = 0;
    m_stats.ibytes = 0;
    m_stats.obytes = 0;

    m_ignore_stats.m_tx_arp = pre_stats.m_tx_arp;
    m_ignore_stats.m_rx_arp = pre_stats.m_rx_arp;

    if (CGlobalInfo::m_options.preview.getVMode() >= 3) {
        fprintf(stdout, "Pre test statistics for port %d\n", get_port_id());
        m_ignore_stats.dump(stdout);
    }
}

void CPhyEthIF::dump_stats(FILE *fd){

    update_counters();

    fprintf(fd,"port : %d \n",(int)m_port_id);
    fprintf(fd,"------------\n");
    m_stats.DumpAll(fd);
    //m_stats.Dump(fd);
    printf (" Tx : %.1fMb/sec  \n",m_last_tx_rate);
    //printf (" Rx : %.1fMb/sec  \n",m_last_rx_rate);
}

void CPhyEthIF::stats_clear(){
    rte_eth_stats_reset(m_port_id);
    m_stats.Clear();
}

class CCorePerPort  {
public:
    CCorePerPort (){
        m_tx_queue_id=0;
        m_len=0;
        int i;
        for (i=0; i<MAX_PKT_BURST; i++) {
            m_table[i]=0;
        }
        m_port=0;
    }
    uint8_t                 m_tx_queue_id;
    uint8_t                 m_tx_queue_id_lat; // q id for tx of latency pkts
    uint16_t                m_len;
    rte_mbuf_t *            m_table[MAX_PKT_BURST];
    CPhyEthIF  *            m_port;
};


#define MAX_MBUF_CACHE 100


/* per core/gbe queue port for trasmitt */
class CCoreEthIF : public CVirtualIF {
public:
    enum {
     INVALID_Q_ID = 255
    };

public:

    CCoreEthIF(){
        m_mbuf_cache=0;
    }

    bool Create(uint8_t             core_id,
                uint8_t            tx_client_queue_id,
                CPhyEthIF  *        tx_client_port,
                uint8_t            tx_server_queue_id,
                CPhyEthIF  *        tx_server_port,
                uint8_t             tx_q_id_lat);
    void Delete();

    virtual int open_file(std::string file_name){
        return (0);
    }

    virtual int close_file(void){
        return (flush_tx_queue());
    }
    __attribute__ ((noinline)) int send_node_flow_stat(rte_mbuf *m, CGenNodeStateless * node_sl
                                                       , CCorePerPort *  lp_port
                                                       , CVirtualIFPerSideStats  * lp_stats, bool is_const);
    virtual int send_node(CGenNode * node);
    virtual void send_one_pkt(pkt_dir_t dir, rte_mbuf_t *m);
    virtual int flush_tx_queue(void);
    __attribute__ ((noinline)) void handle_slowpath_features(CGenNode *node, rte_mbuf_t *m, uint8_t *p, pkt_dir_t dir);

    void apply_client_cfg(const ClientCfgBase *cfg, rte_mbuf_t *m, pkt_dir_t dir, uint8_t *p);

    bool process_rx_pkt(pkt_dir_t   dir,rte_mbuf_t * m);

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p);

    virtual pkt_dir_t port_id_to_dir(uint8_t port_id);
    void GetCoreCounters(CVirtualIFPerSideStats *stats);
    void DumpCoreStats(FILE *fd);
    void DumpIfStats(FILE *fd);
    static void DumpIfCfgHeader(FILE *fd);
    void DumpIfCfg(FILE *fd);

    socket_id_t get_socket_id(){
        return ( CGlobalInfo::m_socket.port_to_socket( m_ports[0].m_port->get_port_id() ) );
    }

    const CCorePerPort * get_ports() {
        return m_ports;
    }

protected:

    int send_burst(CCorePerPort * lp_port,
                   uint16_t len,
                   CVirtualIFPerSideStats  * lp_stats);
    int send_pkt(CCorePerPort * lp_port,
                 rte_mbuf_t *m,
                 CVirtualIFPerSideStats  * lp_stats);
    int send_pkt_lat(CCorePerPort * lp_port,
                 rte_mbuf_t *m,
                 CVirtualIFPerSideStats  * lp_stats);

    void add_vlan(rte_mbuf_t *m, uint16_t vlan_id);

protected:
    uint8_t      m_core_id;
    uint16_t     m_mbuf_cache;
    CCorePerPort m_ports[CS_NUM]; /* each core has 2 tx queues 1. client side and server side */
    CNodeRing *  m_ring_to_rx;

} __rte_cache_aligned; ;

class CCoreEthIFStateless : public CCoreEthIF {
public:
    virtual int send_node_flow_stat(rte_mbuf *m, CGenNodeStateless * node_sl, CCorePerPort *  lp_port
                                    , CVirtualIFPerSideStats  * lp_stats, bool is_const);
    virtual int send_node(CGenNode * node);
protected:
    int handle_slow_path_node(CGenNode *node);
    int send_pcap_node(CGenNodePCAP *pcap_node);
};

bool CCoreEthIF::Create(uint8_t             core_id,
                        uint8_t             tx_client_queue_id,
                        CPhyEthIF  *        tx_client_port,
                        uint8_t             tx_server_queue_id,
                        CPhyEthIF  *        tx_server_port,
                        uint8_t tx_q_id_lat ) {
    m_ports[CLIENT_SIDE].m_tx_queue_id = tx_client_queue_id;
    m_ports[CLIENT_SIDE].m_port        = tx_client_port;
    m_ports[CLIENT_SIDE].m_tx_queue_id_lat = tx_q_id_lat;
    m_ports[SERVER_SIDE].m_tx_queue_id = tx_server_queue_id;
    m_ports[SERVER_SIDE].m_port        = tx_server_port;
    m_ports[SERVER_SIDE].m_tx_queue_id_lat = tx_q_id_lat;
    m_core_id = core_id;

    CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();
    m_ring_to_rx = rx_dp->getRingDpToCp(core_id-1);
    assert( m_ring_to_rx);
    return (true);
}

int CCoreEthIF::flush_tx_queue(void){
    /* flush both sides */
    pkt_dir_t dir;
    for (dir = CLIENT_SIDE; dir < CS_NUM; dir++) {
        CCorePerPort * lp_port = &m_ports[dir];
        CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
        if ( likely(lp_port->m_len > 0) ) {
            send_burst(lp_port, lp_port->m_len, lp_stats);
            lp_port->m_len = 0;
        }
    }

    return 0;
}

void CCoreEthIF::GetCoreCounters(CVirtualIFPerSideStats *stats){
    stats->Clear();
    pkt_dir_t   dir ;
    for (dir=CLIENT_SIDE; dir<CS_NUM; dir++) {
        stats->Add(&m_stats[dir]);
    }
}

void CCoreEthIF::DumpCoreStats(FILE *fd){
    fprintf (fd,"------------------------ \n");
    fprintf (fd," per core stats core id : %d  \n",m_core_id);
    fprintf (fd,"------------------------ \n");

    CVirtualIFPerSideStats stats;
    GetCoreCounters(&stats);
    stats.Dump(stdout);
}

void CCoreEthIF::DumpIfCfgHeader(FILE *fd){
    fprintf (fd," core, c-port, c-queue, s-port, s-queue, lat-queue\n");
    fprintf (fd," ------------------------------------------\n");
}

void CCoreEthIF::DumpIfCfg(FILE *fd){
    fprintf (fd," %d   %6u %6u  %6u  %6u %6u  \n",m_core_id,
             m_ports[CLIENT_SIDE].m_port->get_port_id(),
             m_ports[CLIENT_SIDE].m_tx_queue_id,
             m_ports[SERVER_SIDE].m_port->get_port_id(),
             m_ports[SERVER_SIDE].m_tx_queue_id,
             m_ports[SERVER_SIDE].m_tx_queue_id_lat
             );
}


void CCoreEthIF::DumpIfStats(FILE *fd){

    fprintf (fd,"------------------------ \n");
    fprintf (fd," per core per if stats id : %d  \n",m_core_id);
    fprintf (fd,"------------------------ \n");

    const char * t[]={"client","server"};
    pkt_dir_t   dir ;
    for (dir=CLIENT_SIDE; dir<CS_NUM; dir++) {
        CCorePerPort * lp=&m_ports[dir];
        CVirtualIFPerSideStats * lpstats = &m_stats[dir];
        fprintf (fd," port %d, queue id :%d  - %s \n",lp->m_port->get_port_id(),lp->m_tx_queue_id,t[dir] );
        fprintf (fd," ---------------------------- \n");
        lpstats->Dump(fd);
    }
}

#define DELAY_IF_NEEDED

int CCoreEthIF::send_burst(CCorePerPort * lp_port,
                           uint16_t len,
                           CVirtualIFPerSideStats  * lp_stats){

    uint16_t ret = lp_port->m_port->tx_burst(lp_port->m_tx_queue_id,lp_port->m_table,len);
#ifdef DELAY_IF_NEEDED
    while ( unlikely( ret<len ) ){
        rte_delay_us(1);
        lp_stats->m_tx_queue_full += 1;
        uint16_t ret1=lp_port->m_port->tx_burst(lp_port->m_tx_queue_id,
                                                &lp_port->m_table[ret],
                                                len-ret);
        ret+=ret1;
    }
#else
    /* CPU has burst of packets larger than TX can send. Need to drop packets */
    if ( unlikely(ret < len) ) {
        lp_stats->m_tx_drop += (len-ret);
        uint16_t i;
        for (i=ret; i<len;i++) {
            rte_mbuf_t * m=lp_port->m_table[i];
            rte_pktmbuf_free(m);
        }
    }
#endif

    return (0);
}


int CCoreEthIF::send_pkt(CCorePerPort * lp_port,
                         rte_mbuf_t      *m,
                         CVirtualIFPerSideStats  * lp_stats
                         ){

    uint16_t len = lp_port->m_len;
    lp_port->m_table[len]=m;
    len++;
    /* enough pkts to be sent */
    if (unlikely(len == MAX_PKT_BURST)) {
        send_burst(lp_port, MAX_PKT_BURST,lp_stats);
        len = 0;
    }
    lp_port->m_len = len;

    return (0);
}

int CCoreEthIF::send_pkt_lat(CCorePerPort *lp_port, rte_mbuf_t *m, CVirtualIFPerSideStats *lp_stats) {
    // We allow sending only from first core of each port. This is serious internal bug otherwise.
    assert(lp_port->m_tx_queue_id_lat != INVALID_Q_ID);

    int ret = lp_port->m_port->tx_burst(lp_port->m_tx_queue_id_lat, &m, 1);

#ifdef DELAY_IF_NEEDED
    while ( unlikely( ret != 1 ) ){
        rte_delay_us(1);
        lp_stats->m_tx_queue_full += 1;
        ret = lp_port->m_port->tx_burst(lp_port->m_tx_queue_id_lat, &m, 1);
    }

#else
    if ( unlikely( ret != 1 ) ) {
        lp_stats->m_tx_drop ++;
        rte_pktmbuf_free(m);
        return 0;
    }

#endif

    return ret;
}

void CCoreEthIF::send_one_pkt(pkt_dir_t       dir,
                              rte_mbuf_t      *m){
    CCorePerPort *  lp_port=&m_ports[dir];
    CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
    send_pkt(lp_port,m,lp_stats);
    /* flush */
    send_burst(lp_port,lp_port->m_len,lp_stats);
    lp_port->m_len = 0;
}

int CCoreEthIFStateless::send_node_flow_stat(rte_mbuf *m, CGenNodeStateless * node_sl, CCorePerPort *  lp_port
                                             , CVirtualIFPerSideStats  * lp_stats, bool is_const) {
    // Defining this makes 10% percent packet loss. 1% packet reorder.
# ifdef ERR_CNTRS_TEST
    static int temp=1;
    temp++;
#endif

    uint16_t hw_id = node_sl->get_stat_hw_id();
    rte_mbuf *mi;
    struct flow_stat_payload_header *fsp_head = NULL;

    if (hw_id >= MAX_FLOW_STATS) {
        // payload rule hw_ids are in the range right above ip id rules
        uint16_t hw_id_payload = hw_id - MAX_FLOW_STATS;
        if (hw_id_payload > max_stat_hw_id_seen_payload) {
            max_stat_hw_id_seen_payload = hw_id_payload;
        }

        mi = node_sl->alloc_flow_stat_mbuf(m, fsp_head, is_const);
        fsp_head->seq = lp_stats->m_lat_data[hw_id_payload].get_seq_num();
        fsp_head->hw_id = hw_id_payload;
        fsp_head->flow_seq = lp_stats->m_lat_data[hw_id_payload].get_flow_seq();
        fsp_head->magic = FLOW_STAT_PAYLOAD_MAGIC;

        lp_stats->m_lat_data[hw_id_payload].inc_seq_num();
#ifdef ERR_CNTRS_TEST
        if (temp % 10 == 0) {
            fsp_head->seq = lp_stats->m_lat_data[hw_id_payload].inc_seq_num();
        }
        if ((temp - 1) % 100 == 0) {
            fsp_head->seq = lp_stats->m_lat_data[hw_id_payload].get_seq_num() - 4;
        }
#endif
    } else {
        // ip id rule
        if (hw_id > max_stat_hw_id_seen) {
            max_stat_hw_id_seen = hw_id;
        }
        mi = m;
    }
    tx_per_flow_t *lp_s = &lp_stats->m_tx_per_flow[hw_id];
    lp_s->add_pkts(1);
    lp_s->add_bytes(mi->pkt_len + 4); // We add 4 because of ethernet CRC

    if (hw_id >= MAX_FLOW_STATS) {
        fsp_head->time_stamp = os_get_hr_tick_64();
        send_pkt_lat(lp_port, mi, lp_stats);
    } else {
        send_pkt(lp_port, mi, lp_stats);
    }
    return 0;
}

int CCoreEthIFStateless::send_node(CGenNode * no) {
    /* if a node is marked as slow path - single IF to redirect it to slow path */
    if (no->get_is_slow_path()) {
        return handle_slow_path_node(no);
    }

    CGenNodeStateless * node_sl=(CGenNodeStateless *) no;

    /* check that we have mbuf  */
    rte_mbuf_t *    m;

    pkt_dir_t dir=(pkt_dir_t)node_sl->get_mbuf_cache_dir();
    CCorePerPort *  lp_port=&m_ports[dir];
    CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
    if ( likely(node_sl->is_cache_mbuf_array()) ) {
        m=node_sl->cache_mbuf_array_get_cur();
        rte_pktmbuf_refcnt_update(m,1);
    }else{
        m=node_sl->get_cache_mbuf();

        if (m) {
            /* cache case */
            rte_pktmbuf_refcnt_update(m,1);
        }else{
            m=node_sl->alloc_node_with_vm();
            assert(m);
        }
    }

    if (unlikely(node_sl->is_stat_needed())) {
        if ( unlikely(node_sl->is_cache_mbuf_array()) ) {
            // No support for latency + cache. If user asks for cache on latency stream, we change cache to 0.
            // assert here just to make sure.
            assert(1);
        }
        return send_node_flow_stat(m, node_sl, lp_port, lp_stats, (node_sl->get_cache_mbuf()) ? true : false);
    } else {
        send_pkt(lp_port,m,lp_stats);
    }

    return (0);
};

int CCoreEthIFStateless::send_pcap_node(CGenNodePCAP *pcap_node) {
    rte_mbuf_t *m = pcap_node->get_pkt();
    if (!m) {
        return (-1);
    }

    pkt_dir_t dir = (pkt_dir_t)pcap_node->get_mbuf_dir();
    CCorePerPort *lp_port=&m_ports[dir];
    CVirtualIFPerSideStats *lp_stats = &m_stats[dir];

    send_pkt(lp_port, m, lp_stats);

    return (0);
}

/**
 * slow path code goes here
 *
 */
int CCoreEthIFStateless::handle_slow_path_node(CGenNode * no) {

    if (no->m_type == CGenNode::PCAP_PKT) {
        return send_pcap_node((CGenNodePCAP *)no);
    }

    return (-1);
}

void CCoreEthIF::apply_client_cfg(const ClientCfgBase *cfg, rte_mbuf_t *m, pkt_dir_t dir, uint8_t *p) {

    assert(cfg);

    /* take the right direction config */
    const ClientCfgDirBase &cfg_dir = ( (dir == CLIENT_SIDE) ? cfg->m_initiator : cfg->m_responder);

    /* dst mac */
    if (cfg_dir.has_dst_mac_addr()) {
        memcpy(p, cfg_dir.get_dst_mac_addr(), 6);
    }

    /* src mac */
    if (cfg_dir.has_src_mac_addr()) {
        memcpy(p + 6, cfg_dir.get_src_mac_addr(), 6);
    }

    /* VLAN */
    if (cfg_dir.has_vlan()) {
        add_vlan(m, cfg_dir.get_vlan());
    }
}


void CCoreEthIF::add_vlan(rte_mbuf_t *m, uint16_t vlan_id) {
    m->ol_flags = PKT_TX_VLAN_PKT;
    m->l2_len   = 14;
    m->vlan_tci = vlan_id;
}

/**
 * slow path features goes here (avoid multiple IFs)
 *
 */
void CCoreEthIF::handle_slowpath_features(CGenNode *node, rte_mbuf_t *m, uint8_t *p, pkt_dir_t dir) {


    /* MAC ovverride */
    if ( unlikely( CGlobalInfo::m_options.preview.get_mac_ip_overide_enable() ) ) {
        /* client side */
        if ( node->is_initiator_pkt() ) {
            *((uint32_t*)(p+6)) = PKT_NTOHL(node->m_src_ip);
        }
    }

    /* flag is faster than checking the node pointer (another cacheline) */
    if ( unlikely(CGlobalInfo::m_options.preview.get_is_client_cfg_enable() ) ) {
        apply_client_cfg(node->m_client_cfg, m, dir, p);
    }

}

int CCoreEthIF::send_node(CGenNode * node) {

    if ( unlikely( node->get_cache_mbuf() !=NULL ) ) {
        pkt_dir_t       dir;
        rte_mbuf_t *    m=node->get_cache_mbuf();
        dir=(pkt_dir_t)node->get_mbuf_cache_dir();
        CCorePerPort *  lp_port=&m_ports[dir];
        CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
        rte_pktmbuf_refcnt_update(m,1);
        send_pkt(lp_port,m,lp_stats);
        return (0);
    }


    CFlowPktInfo *  lp=node->m_pkt_info;
    rte_mbuf_t *    m=lp->generate_new_mbuf(node);

    pkt_dir_t       dir;
    bool            single_port;

    dir         = node->cur_interface_dir();
    single_port = node->get_is_all_flow_from_same_dir() ;


    if ( unlikely( CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) ){
        /* which vlan to choose 0 or 1*/
        uint8_t vlan_port = (node->m_src_ip &1);
        uint16_t vlan_id  = CGlobalInfo::m_options.m_vlan_port[vlan_port];

        if (likely( vlan_id >0 ) ) {
            dir = dir ^ vlan_port;
        }else{
            /* both from the same dir but with VLAN0 */
            vlan_id = CGlobalInfo::m_options.m_vlan_port[0];
            dir = dir ^ 0;
        }

        add_vlan(m, vlan_id);
    }

    CCorePerPort *lp_port = &m_ports[dir];
    CVirtualIFPerSideStats *lp_stats = &m_stats[dir];

    if (unlikely(m==0)) {
        lp_stats->m_tx_alloc_error++;
        return(0);
    }

    /* update mac addr dest/src 12 bytes */
    uint8_t *p   = rte_pktmbuf_mtod(m, uint8_t*);
    uint8_t p_id = lp_port->m_port->get_port_id();

    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);

     /* when slowpath features are on */
    if ( unlikely( CGlobalInfo::m_options.preview.get_is_slowpath_features_on() ) ) {
        handle_slowpath_features(node, m, p, dir);
    }


    if ( unlikely( node->is_rx_check_enabled() ) ) {
        lp_stats->m_tx_rx_check_pkt++;
        lp->do_generate_new_mbuf_rxcheck(m, node, single_port);
        lp_stats->m_template.inc_template( node->get_template_id( ));
    }else{
        // cache only if it is not sample as this is more complex mbuf struct
        if ( unlikely( node->can_cache_mbuf() ) ) {
            if ( !CGlobalInfo::m_options.preview.isMbufCacheDisabled() ){
                m_mbuf_cache++;
                if (m_mbuf_cache < MAX_MBUF_CACHE) {
                    /* limit the number of object to cache */
                    node->set_mbuf_cache_dir( dir);
                    node->set_cache_mbuf(m);
                    rte_pktmbuf_refcnt_update(m,1);
                }
            }
        }
    }

    /*printf("send packet -- \n");
      rte_pktmbuf_dump(stdout,m, rte_pktmbuf_pkt_len(m));*/

    /* send the packet */
    send_pkt(lp_port,m,lp_stats);
    return (0);
}


int CCoreEthIF::update_mac_addr_from_global_cfg(pkt_dir_t  dir, uint8_t * p){
    assert(p);
    assert(dir<2);

    CCorePerPort *  lp_port=&m_ports[dir];
    uint8_t p_id=lp_port->m_port->get_port_id();
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);
    return (0);
}

pkt_dir_t
CCoreEthIF::port_id_to_dir(uint8_t port_id) {

    for (pkt_dir_t dir = 0; dir < CS_NUM; dir++) {
        if (m_ports[dir].m_port->get_port_id() == port_id) {
            return dir;
        }
    }

    return (CS_INVALID);
}

class CLatencyHWPort : public CPortLatencyHWBase {
public:
    void Create(CPhyEthIF  * p,
                uint8_t tx_queue,
                uint8_t rx_queue){
        m_port=p;
        m_tx_queue_id=tx_queue;
        m_rx_queue_id=rx_queue;
    }

    virtual int tx(rte_mbuf_t * m){
        rte_mbuf_t * tx_pkts[2];
        tx_pkts[0]=m;
        if ( likely( CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) ){
            /* vlan mode is the default */
            /* set the vlan */
            m->ol_flags = PKT_TX_VLAN_PKT;
            m->vlan_tci =CGlobalInfo::m_options.m_vlan_port[0];
            m->l2_len   =14;
        }
        uint16_t res=m_port->tx_burst(m_tx_queue_id,tx_pkts,1);
        if ( res == 0 ) {
            rte_pktmbuf_free(m);
            //printf(" queue is full for latency packet !!\n");
            return (-1);

        }
#if 0
        fprintf(stdout," ==> %f.03 send packet ..\n",now_sec());
        uint8_t *p1=rte_pktmbuf_mtod(m, uint8_t*);
        uint16_t pkt_size1=rte_pktmbuf_pkt_len(m);
        utl_DumpBuffer(stdout,p1,pkt_size1,0);
#endif

        return (0);
    }
    virtual rte_mbuf_t * rx(){
        rte_mbuf_t * rx_pkts[1];
        uint16_t cnt=m_port->rx_burst(m_rx_queue_id,rx_pkts,1);
        if (cnt) {
            return (rx_pkts[0]);
        }else{
            return (0);
        }
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts,
                              uint16_t nb_pkts){
        uint16_t cnt=m_port->rx_burst(m_rx_queue_id,rx_pkts,nb_pkts);
        return (cnt);
    }


private:
    CPhyEthIF  * m_port;
    uint8_t      m_tx_queue_id ;
    uint8_t      m_rx_queue_id;
};


class CLatencyVmPort : public CPortLatencyHWBase {
public:
    void Create(uint8_t port_index,CNodeRing * ring,
                CLatencyManager * mgr, CPhyEthIF  * p) {
        m_dir        = (port_index%2);
        m_ring_to_dp = ring;
        m_mgr        = mgr;
        m_port = p;
    }

    virtual int tx(rte_mbuf_t * m){
        if ( likely( CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) ){
            /* vlan mode is the default */
            /* set the vlan */
            m->ol_flags = PKT_TX_VLAN_PKT;
            m->vlan_tci =CGlobalInfo::m_options.m_vlan_port[0];
            m->l2_len   =14;
        }

        /* allocate node */
        CGenNodeLatencyPktInfo * node=(CGenNodeLatencyPktInfo * )CGlobalInfo::create_node();
        if ( node ) {
            node->m_msg_type = CGenNodeMsgBase::LATENCY_PKT;
            node->m_dir      = m_dir;
            node->m_pkt      = m;
            node->m_latency_offset = m_mgr->get_latency_header_offset();

            if ( m_ring_to_dp->Enqueue((CGenNode*)node) ==0 ){
                return (0);
            }
        }
        return (-1);
    }

    virtual rte_mbuf_t * rx() {
        rte_mbuf_t * rx_pkts[1];
        uint16_t cnt = m_port->rx_burst(0, rx_pkts, 1);
        if (cnt) {
            return (rx_pkts[0]);
        } else {
            return (0);
        }
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        uint16_t cnt = m_port->rx_burst(0, rx_pkts, nb_pkts);
        return (cnt);
    }

private:
    CPhyEthIF  * m_port;
    uint8_t                          m_dir;
    CNodeRing *                      m_ring_to_dp;   /* ring dp -> latency thread */
    CLatencyManager *                m_mgr;
};



class CPerPortStats {
public:
    uint64_t opackets;
    uint64_t obytes;
    uint64_t ipackets;
    uint64_t ibytes;
    uint64_t ierrors;
    uint64_t oerrors;
    tx_per_flow_t m_tx_per_flow[MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD];
    tx_per_flow_t m_prev_tx_per_flow[MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD];

    float     m_total_tx_bps;
    float     m_total_tx_pps;

    float     m_total_rx_bps;
    float     m_total_rx_pps;

    float     m_cpu_util;
    bool      m_link_up = true;
    bool      m_link_was_down = false;
};

class CGlobalStats {
public:
    enum DumpFormat {
        dmpSTANDARD,
        dmpTABLE
    };

    uint64_t  m_total_tx_pkts;
    uint64_t  m_total_rx_pkts;
    uint64_t  m_total_tx_bytes;
    uint64_t  m_total_rx_bytes;

    uint64_t  m_total_alloc_error;
    uint64_t  m_total_queue_full;
    uint64_t  m_total_queue_drop;

    uint64_t  m_total_clients;
    uint64_t  m_total_servers;
    uint64_t  m_active_sockets;

    uint64_t  m_total_nat_time_out;
    uint64_t  m_total_nat_time_out_wait_ack;
    uint64_t  m_total_nat_no_fid  ;
    uint64_t  m_total_nat_active  ;
    uint64_t  m_total_nat_syn_wait;
    uint64_t  m_total_nat_open    ;
    uint64_t  m_total_nat_learn_error    ;

    CPerTxthreadTemplateInfo m_template;

    float     m_socket_util;

    float m_platform_factor;
    float m_tx_bps;
    float m_rx_bps;
    float m_tx_pps;
    float m_rx_pps;
    float m_tx_cps;
    float m_tx_expected_cps;
    float m_tx_expected_pps;
    float m_tx_expected_bps;
    float m_rx_drop_bps;
    float m_active_flows;
    float m_open_flows;
    float m_cpu_util;
    float m_cpu_util_raw;
    float m_rx_cpu_util;
    float m_bw_per_core;
    uint8_t m_threads;

    uint32_t      m_num_of_ports;
    CPerPortStats m_port[TREX_MAX_PORTS];
public:
    void Dump(FILE *fd,DumpFormat mode);
    void DumpAllPorts(FILE *fd);
    void dump_json(std::string & json, bool baseline);
private:
    std::string get_field(const char *name, float &f);
    std::string get_field(const char *name, uint64_t &f);
    std::string get_field_port(int port, const char *name, float &f);
    std::string get_field_port(int port, const char *name, uint64_t &f);

};

std::string CGlobalStats::get_field(const char *name, float &f){
    char buff[200];
    if(f <= -10.0 or f >= 10.0)
        snprintf(buff, sizeof(buff), "\"%s\":%.1f,",name,f);
    else
        snprintf(buff, sizeof(buff), "\"%s\":%.3e,",name,f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field(const char *name, uint64_t &f){
    char buff[200];
    snprintf(buff,  sizeof(buff), "\"%s\":%llu,", name, (unsigned long long)f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field_port(int port, const char *name, float &f){
    char buff[200];
    if(f <= -10.0 or f >= 10.0)
        snprintf(buff,  sizeof(buff), "\"%s-%d\":%.1f,", name, port, f);
    else
        snprintf(buff, sizeof(buff), "\"%s-%d\":%.3e,", name, port, f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field_port(int port, const char *name, uint64_t &f){
    char buff[200];
    snprintf(buff, sizeof(buff), "\"%s-%d\":%llu,",name, port, (unsigned long long)f);
    return (std::string(buff));
}


void CGlobalStats::dump_json(std::string & json, bool baseline){
    /* refactor this to JSON */

    json="{\"name\":\"trex-global\",\"type\":0,";
    if (baseline) {
        json += "\"baseline\": true,";
    }

    json +="\"data\":{";

    char ts_buff[200];
    snprintf(ts_buff , sizeof(ts_buff), "\"ts\":{\"value\":%lu, \"freq\":%lu},", os_get_hr_tick_64(), os_get_hr_freq());
    json+= std::string(ts_buff);

#define GET_FIELD(f) get_field(#f, f)
#define GET_FIELD_PORT(p,f) get_field_port(p, #f, lp->f)

    json+=GET_FIELD(m_cpu_util);
    json+=GET_FIELD(m_cpu_util_raw);
    json+=GET_FIELD(m_bw_per_core);
    json+=GET_FIELD(m_rx_cpu_util);
    json+=GET_FIELD(m_platform_factor);
    json+=GET_FIELD(m_tx_bps);
    json+=GET_FIELD(m_rx_bps);
    json+=GET_FIELD(m_tx_pps);
    json+=GET_FIELD(m_rx_pps);
    json+=GET_FIELD(m_tx_cps);
    json+=GET_FIELD(m_tx_expected_cps);
    json+=GET_FIELD(m_tx_expected_pps);
    json+=GET_FIELD(m_tx_expected_bps);
    json+=GET_FIELD(m_total_alloc_error);
    json+=GET_FIELD(m_total_queue_full);
    json+=GET_FIELD(m_total_queue_drop);
    json+=GET_FIELD(m_rx_drop_bps);
    json+=GET_FIELD(m_active_flows);
    json+=GET_FIELD(m_open_flows);

    json+=GET_FIELD(m_total_tx_pkts);
    json+=GET_FIELD(m_total_rx_pkts);
    json+=GET_FIELD(m_total_tx_bytes);
    json+=GET_FIELD(m_total_rx_bytes);

    json+=GET_FIELD(m_total_clients);
    json+=GET_FIELD(m_total_servers);
    json+=GET_FIELD(m_active_sockets);
    json+=GET_FIELD(m_socket_util);

    json+=GET_FIELD(m_total_nat_time_out);
    json+=GET_FIELD(m_total_nat_time_out_wait_ack);
    json+=GET_FIELD(m_total_nat_no_fid );
    json+=GET_FIELD(m_total_nat_active );
    json+=GET_FIELD(m_total_nat_syn_wait);
    json+=GET_FIELD(m_total_nat_open   );
    json+=GET_FIELD(m_total_nat_learn_error);

    int i;
    for (i=0; i<(int)m_num_of_ports; i++) {
        CPerPortStats * lp=&m_port[i];
        json+=GET_FIELD_PORT(i,opackets) ;
        json+=GET_FIELD_PORT(i,obytes)   ;
        json+=GET_FIELD_PORT(i,ipackets) ;
        json+=GET_FIELD_PORT(i,ibytes)   ;
        json+=GET_FIELD_PORT(i,ierrors)  ;
        json+=GET_FIELD_PORT(i,oerrors)  ;
        json+=GET_FIELD_PORT(i,m_total_tx_bps);
        json+=GET_FIELD_PORT(i,m_total_tx_pps);
        json+=GET_FIELD_PORT(i,m_total_rx_bps);
        json+=GET_FIELD_PORT(i,m_total_rx_pps);
        json+=GET_FIELD_PORT(i,m_cpu_util);
    }
    json+=m_template.dump_as_json("template");
    json+="\"unknown\":0}}"  ;
}

void CGlobalStats::DumpAllPorts(FILE *fd){

    //fprintf (fd," Total-Tx-Pkts   : %s  \n",double_to_human_str((double)m_total_tx_pkts,"pkts",KBYE_1000).c_str());
    //fprintf (fd," Total-Rx-Pkts   : %s  \n",double_to_human_str((double)m_total_rx_pkts,"pkts",KBYE_1000).c_str());

    //fprintf (fd," Total-Tx-Bytes  : %s  \n",double_to_human_str((double)m_total_tx_bytes,"bytes",KBYE_1000).c_str());
    //fprintf (fd," Total-Rx-Bytes  : %s  \n",double_to_human_str((double)m_total_rx_bytes,"bytes",KBYE_1000).c_str());



    fprintf (fd," Cpu Utilization : %2.1f  %%  %2.1f Gb/core \n",m_cpu_util,m_bw_per_core);
    fprintf (fd," Platform_factor : %2.1f  \n",m_platform_factor);
    fprintf (fd," Total-Tx        : %s  ",double_to_human_str(m_tx_bps,"bps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," NAT time out    : %8llu", (unsigned long long)m_total_nat_time_out);
        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
            fprintf (fd," (%llu in wait for syn+ack)\n", (unsigned long long)m_total_nat_time_out_wait_ack);
        } else {
            fprintf (fd, "\n");
        }
    }else{
        fprintf (fd,"\n");
    }


    fprintf (fd," Total-Rx        : %s  ",double_to_human_str(m_rx_bps,"bps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," NAT aged flow id: %8llu \n", (unsigned long long)m_total_nat_no_fid);
    }else{
        fprintf (fd,"\n");
    }

    fprintf (fd," Total-PPS       : %s  ",double_to_human_str(m_tx_pps,"pps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Total NAT active: %8llu", (unsigned long long)m_total_nat_active);
        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
            fprintf (fd," (%llu waiting for syn)\n", (unsigned long long)m_total_nat_syn_wait);
        } else {
            fprintf (fd, "\n");
        }
    }else{
        fprintf (fd,"\n");
    }

    fprintf (fd," Total-CPS       : %s  ",double_to_human_str(m_tx_cps,"cps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Total NAT opened: %8llu \n", (unsigned long long)m_total_nat_open);
    }else{
        fprintf (fd,"\n");
    }
    fprintf (fd,"\n");
    fprintf (fd," Expected-PPS    : %s  ",double_to_human_str(m_tx_expected_pps,"pps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_verify_mode() ) {
        fprintf (fd," NAT learn errors: %8llu \n", (unsigned long long)m_total_nat_learn_error);
    }else{
        fprintf (fd,"\n");
    }
    fprintf (fd," Expected-CPS    : %s  \n",double_to_human_str(m_tx_expected_cps,"cps",KBYE_1000).c_str());
    fprintf (fd," Expected-BPS    : %s  \n",double_to_human_str(m_tx_expected_bps,"bps",KBYE_1000).c_str());
    fprintf (fd,"\n");
    fprintf (fd," Active-flows    : %8llu  Clients : %8llu   Socket-util : %3.4f %%    \n",
             (unsigned long long)m_active_flows,
             (unsigned long long)m_total_clients,
             m_socket_util);
    fprintf (fd," Open-flows      : %8llu  Servers : %8llu   Socket : %8llu Socket/Clients :  %.1f \n",
             (unsigned long long)m_open_flows,
             (unsigned long long)m_total_servers,
             (unsigned long long)m_active_sockets,
             (float)m_active_sockets/(float)m_total_clients);

    if (m_total_alloc_error) {
        fprintf (fd," Total_alloc_err  : %llu         \n", (unsigned long long)m_total_alloc_error);
    }
    if ( m_total_queue_full ){
        fprintf (fd," Total_queue_full : %llu         \n", (unsigned long long)m_total_queue_full);
    }
    if (m_total_queue_drop) {
        fprintf (fd," Total_queue_drop : %llu         \n", (unsigned long long)m_total_queue_drop);
    }

    //m_template.Dump(fd);

    fprintf (fd," drop-rate       : %s   \n",double_to_human_str(m_rx_drop_bps,"bps",KBYE_1000).c_str() );
}


void CGlobalStats::Dump(FILE *fd,DumpFormat mode){
    int i;
    int port_to_show=m_num_of_ports;
    if (port_to_show>4) {
        port_to_show=4;
        fprintf (fd," per port - limited to 4   \n");
    }


    if ( mode== dmpSTANDARD ){
        fprintf (fd," --------------- \n");
        for (i=0; i<(int)port_to_show; i++) {
            CPerPortStats * lp=&m_port[i];
            fprintf(fd,"port : %d ",(int)i);
            if ( ! lp->m_link_up ) {
                fprintf(fd," (link DOWN)");
            }
            fprintf(fd,"\n------------\n");
#define GS_DP_A4(f) fprintf(fd," %-40s : %llu \n",#f, (unsigned long long)lp->f)
#define GS_DP_A(f) if (lp->f) fprintf(fd," %-40s : %llu \n",#f, (unsigned long long)lp->f)
            GS_DP_A4(opackets);
            GS_DP_A4(obytes);
            GS_DP_A4(ipackets);
            GS_DP_A4(ibytes);
            GS_DP_A(ierrors);
            GS_DP_A(oerrors);
            fprintf (fd," Tx : %s  \n",double_to_human_str((double)lp->m_total_tx_bps,"bps",KBYE_1000).c_str());
        }
    }else{
        fprintf(fd," %10s ","ports");
        for (i=0; i<(int)port_to_show; i++) {
            CPerPortStats * lp=&m_port[i];
            if ( lp->m_link_up ) {
                fprintf(fd,"| %15d ",i);
            } else {
                std::string port_with_state = "(link DOWN) " + std::to_string(i);
                fprintf(fd,"| %15s ",port_with_state.c_str());
            }
        }
        fprintf(fd,"\n");
        fprintf(fd," -----------------------------------------------------------------------------------------\n");
        std::string names[]={"opackets","obytes","ipackets","ibytes","ierrors","oerrors","Tx Bw"
        };
        for (i=0; i<7; i++) {
            fprintf(fd," %10s ",names[i].c_str());
            int j=0;
            for (j=0; j<port_to_show;j++) {
                CPerPortStats * lp=&m_port[j];
                uint64_t cnt;
                switch (i) {
                case 0:
                    cnt=lp->opackets;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 1:
                    cnt=lp->obytes;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 2:
                    cnt=lp->ipackets;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 3:
                    cnt=lp->ibytes;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 4:
                    cnt=lp->ierrors;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 5:
                    cnt=lp->oerrors;
                    fprintf(fd,"| %15lu ",cnt);

                    break;
                case 6:
                    fprintf(fd,"| %15s ",double_to_human_str((double)lp->m_total_tx_bps,"bps",KBYE_1000).c_str());
                    break;
                default:
                    cnt=0xffffff;
                }
            } /* ports */
            fprintf(fd, "\n");
        }/* fields*/
    }


}

class CGlobalTRex  {

public:

    /**
     * different types of shutdown causes
     */
    typedef enum {
        SHUTDOWN_NONE,
        SHUTDOWN_TEST_ENDED,
        SHUTDOWN_CTRL_C,
        SHUTDOWN_SIGINT,
        SHUTDOWN_SIGTERM,
        SHUTDOWN_RPC_REQ
    } shutdown_rc_e;


    CGlobalTRex (){
        m_max_ports=4;
        m_max_cores=1;
        m_cores_to_dual_ports=0;
        m_max_queues_per_port=0;
        m_fl_was_init=false;
        m_expected_pps=0.0;
        m_expected_cps=0.0;
        m_expected_bps=0.0;
        m_trex_stateless = NULL;
        m_mark_for_shutdown = SHUTDOWN_NONE;
    }

    bool Create();
    void Delete();
    int  ixgbe_prob_init();
    int  cores_prob_init();
    int  queues_prob_init();
    int  ixgbe_start();
    int  ixgbe_rx_queue_flush();
    void ixgbe_configure_mg();
    void rx_sl_configure();
    bool is_all_links_are_up(bool dump=false);
    void pre_test();

    /**
     * mark for shutdown
     * on the next check - the control plane will
     * call shutdown()
     */
    void mark_for_shutdown(shutdown_rc_e rc) {

        if (is_marked_for_shutdown()) {
            return;
        }

        m_mark_for_shutdown = rc;
    }

private:
    void register_signals();

    /* try to stop all datapath cores and RX core */
    void try_stop_all_cores();
    /* send message to all dp cores */
    int  send_message_all_dp(TrexStatelessCpToDpMsgBase *msg);
    int  send_message_to_rx(TrexStatelessCpToRxMsgBase *msg);
    void check_for_dp_message_from_core(int thread_id);

    bool is_marked_for_shutdown() const {
        return (m_mark_for_shutdown != SHUTDOWN_NONE);
    }

    /**
     * shutdown sequence
     *
     */
    void shutdown();

public:
    void check_for_dp_messages();
    int start_master_statefull();
    int start_master_stateless();
    int run_in_core(virtual_thread_id_t virt_core_id);
    int core_for_rx(){
        if ( (! get_is_rx_thread_enabled()) ) {
            return -1;
        }else{
            return m_max_cores - 1;
        }
    }
    int run_in_rx_core();
    int run_in_master();

    void handle_fast_path();
    void handle_slow_path();

    int stop_master();
    /* return the minimum number of dp cores needed to support the active ports
       this is for c==1 or  m_cores_mul==1
    */
    int get_base_num_cores(){
        return (m_max_ports>>1);
    }

    int get_cores_tx(){
        /* 0 - master
           num_of_cores -
           last for latency */
        if ( (! get_is_rx_thread_enabled()) ) {
            return (m_max_cores - 1 );
        } else {
            return (m_max_cores - BP_MASTER_AND_LATENCY );
        }
    }

private:
    bool is_all_cores_finished();

public:

    void publish_async_data(bool sync_now, bool baseline = false);
    void publish_async_barrier(uint32_t key);
    void publish_async_port_attr_changed(uint8_t port_id);

    void dump_stats(FILE *fd,
                    CGlobalStats::DumpFormat format);
    void dump_template_info(std::string & json);
    bool sanity_check();
    void update_stats(void);
    tx_per_flow_t get_flow_tx_stats(uint8_t port, uint16_t hw_id);
    tx_per_flow_t clear_flow_tx_stats(uint8_t port, uint16_t index, bool is_lat);
    void get_stats(CGlobalStats & stats);
    float get_cpu_util_per_interface(uint8_t port_id);
    void dump_post_test_stats(FILE *fd);
    void dump_config(FILE *fd);
    void dump_links_status(FILE *fd);

    bool lookup_port_by_mac(const uint8_t *mac, uint8_t &port_id);
    
public:
    port_cfg_t  m_port_cfg;
    uint32_t    m_max_ports;    /* active number of ports supported options are  2,4,8,10,12  */
    uint32_t    m_max_cores;    /* current number of cores , include master and latency  ==> ( master)1+c*(m_max_ports>>1)+1( latency )  */
    uint32_t    m_cores_mul;    /* how cores multipler given  c=4 ==> m_cores_mul */
    uint32_t    m_max_queues_per_port; // Number of TX queues per port
    uint32_t    m_cores_to_dual_ports; /* number of TX cores allocated for each port pair */
    uint16_t    m_rx_core_tx_q_id; /* TX q used by rx core */
    // statistic
    CPPSMeasure  m_cps;
    float        m_expected_pps;
    float        m_expected_cps;
    float        m_expected_bps;//bps
    float        m_last_total_cps;

    CPhyEthIF   m_ports[TREX_MAX_PORTS];
    CCoreEthIF          m_cores_vif_sf[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserved - stateful */
    CCoreEthIFStateless m_cores_vif_sl[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserved - stateless*/
    CCoreEthIF *        m_cores_vif[BP_MAX_CORES];
    CParserOption m_po ;
    CFlowGenList  m_fl;
    bool          m_fl_was_init;
    volatile uint8_t       m_signal[BP_MAX_CORES] __rte_cache_aligned ; // Signal to main core when DP thread finished
    volatile bool m_sl_rx_running; // Signal main core when RX thread finished
    CLatencyManager     m_mg; // statefull RX core
    CRxCoreStateless    m_rx_sl; // stateless RX core
    CTrexGlobalIoMode   m_io_modes;
    CTRexExtendedDriverBase * m_drv;

private:
    CLatencyHWPort      m_latency_vports[TREX_MAX_PORTS];    /* read hardware driver */
    CLatencyVmPort      m_latency_vm_vports[TREX_MAX_PORTS]; /* vm driver */
    CLatencyPktInfo     m_latency_pkt;
    TrexPublisher       m_zmq_publisher;
    CGlobalStats        m_stats;
    uint32_t            m_stats_cnt;
    std::mutex          m_cp_lock;

    TrexMonitor         m_monitor;

    shutdown_rc_e       m_mark_for_shutdown;

public:
    TrexStateless       *m_trex_stateless;

};

// Before starting, send gratuitous ARP on our addresses, and try to resolve dst MAC addresses.
void CGlobalTRex::pre_test() {
    CPretest pretest(m_max_ports);
    bool resolve_needed = false;
    uint8_t empty_mac[ETHER_ADDR_LEN] = {0,0,0,0,0,0};
    bool need_grat_arp[TREX_MAX_PORTS];

    if (CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {
        std::vector<ClientCfgCompactEntry *> conf;
        m_fl.get_client_cfg_ip_list(conf);

        // If we got src MAC for port in global config, take it, otherwise use src MAC from DPDK
        uint8_t port_macs[m_max_ports][ETHER_ADDR_LEN];
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            memcpy(port_macs[port_id], CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.src, ETHER_ADDR_LEN);
        }

        for (std::vector<ClientCfgCompactEntry *>::iterator it = conf.begin(); it != conf.end(); it++) {
            uint8_t port = (*it)->get_port();
            uint16_t vlan = (*it)->get_vlan();
            uint32_t count = (*it)->get_count();
            uint32_t dst_ip = (*it)->get_dst_ip();
            uint32_t src_ip = (*it)->get_src_ip();

            for (int i = 0; i < count; i++) {
                //??? handle ipv6;
                if ((*it)->is_ipv4()) {
                    pretest.add_next_hop(port, dst_ip + i, vlan);
                }
            }
            if (!src_ip) {
                src_ip = CGlobalInfo::m_options.m_ip_cfg[port].get_ip();
                if (!src_ip) {
                    fprintf(stderr, "No matching src ip for port: %d ip:%s vlan: %d\n"
                            , port, ip_to_str(dst_ip).c_str(), vlan);
                    fprintf(stderr, "You must specify src_ip in client config file or in TRex config file\n");
                    exit(1);
                }
            }
            pretest.add_ip(port, src_ip, vlan, port_macs[port]);
            COneIPv4Info ipv4(src_ip, vlan, port_macs[port], port);
            m_mg.add_grat_arp_src(ipv4);

            delete *it;
        }
        if ( CGlobalInfo::m_options.preview.getVMode() > 1) {
            fprintf(stdout, "*******Pretest for client cfg********\n");
            pretest.dump(stdout);
            }
    } else {
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            if (! memcmp( CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, empty_mac, ETHER_ADDR_LEN)) {
                resolve_needed = true;
            } else {
                resolve_needed = false;
            }
            
            need_grat_arp[port_id] = CGlobalInfo::m_options.m_ip_cfg[port_id].get_ip() != 0;
            
            pretest.add_ip(port_id, CGlobalInfo::m_options.m_ip_cfg[port_id].get_ip()
                           , CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan()
                           , CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.src);

            if (resolve_needed) {
                pretest.add_next_hop(port_id, CGlobalInfo::m_options.m_ip_cfg[port_id].get_def_gw()
                                     , CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan());
            }
        }
    }

    for (int port_id = 0; port_id < m_max_ports; port_id++) {
        CPhyEthIF *pif = &m_ports[port_id];
        // Configure port to send all packets to software
        CTRexExtendedDriverDb::Ins()->get_drv()->set_rcv_all(pif, true);
    }


    pretest.send_grat_arp_all();
    bool ret;
    int count = 0;
    bool resolve_failed = false;
    do {
        ret = pretest.resolve_all();
        count++;
    } while ((ret != true) && (count < 10));
    if (ret != true) {
        resolve_failed = true;
    }

    if ( CGlobalInfo::m_options.preview.getVMode() > 1) {
        fprintf(stdout, "*******Pretest after resolving ********\n");
        pretest.dump(stdout);
    }

    if (CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {
        CManyIPInfo pretest_result;
        pretest.get_results(pretest_result);
        if (resolve_failed) {
            fprintf(stderr, "Resolution of following IPs failed. Exiting.\n");
            for (const COneIPInfo *ip=pretest_result.get_next(); ip != NULL;
                   ip = pretest_result.get_next()) {
                if (ip->resolve_needed()) {
                    ip->dump(stderr, "  ");
                }
            }
            exit(1);
        }
        m_fl.set_client_config_resolved_macs(pretest_result);
        if ( CGlobalInfo::m_options.preview.getVMode() > 1) {
            m_fl.dump_client_config(stdout);
        }

        bool port_found[TREX_MAX_PORTS];
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            port_found[port_id] = false;
        }
        // If client config enabled, we don't resolve MACs from trex_cfg.yaml. For latency (-l)
        // We need to able to send packets from RX core, so need to configure MAC/vlan for each port.
        for (const COneIPInfo *ip=pretest_result.get_next(); ip != NULL; ip = pretest_result.get_next()) {
            // Use first MAC/vlan we see on each port
            uint8_t port_id = ip->get_port();
            uint16_t vlan = ip->get_vlan();
            if ( ! port_found[port_id]) {
                port_found[port_id] = true;
                ip->get_mac(CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest);
                CGlobalInfo::m_options.m_ip_cfg[port_id].set_vlan(vlan);
            }
        }
    } else {
        uint8_t mac[ETHER_ADDR_LEN];
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            if (! memcmp(CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, empty_mac, ETHER_ADDR_LEN)) {
                // we don't have dest MAC. Get it from what we resolved.
                uint32_t ip = CGlobalInfo::m_options.m_ip_cfg[port_id].get_def_gw();
                uint16_t vlan = CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan();

                if (!pretest.get_mac(port_id, ip, vlan, mac)) {
                    fprintf(stderr, "Failed resolving dest MAC for default gateway:%d.%d.%d.%d on port %d\n"
                            , (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port_id);

                    if (get_is_stateless()) {
                        continue;
                    } else {
                        exit(1);
                    }
                }



                memcpy(CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, mac, ETHER_ADDR_LEN);
                // if port is connected in loopback, no need to send gratuitous ARP. It will only confuse our ingress counters.
                if (need_grat_arp[port_id] && (! pretest.is_loopback(port_id))) {
                    COneIPv4Info ipv4(CGlobalInfo::m_options.m_ip_cfg[port_id].get_ip()
                                      , CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan()
                                      , CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.src
                                      , port_id);
                    m_mg.add_grat_arp_src(ipv4);
                }
            }

            // update statistics baseline, so we can ignore what happened in pre test phase
            CPhyEthIF *pif = &m_ports[port_id];
            CPreTestStats pre_stats = pretest.get_stats(port_id);
            pif->set_ignore_stats_base(pre_stats);

            // Configure port back to normal mode. Only relevant packets handled by software.
            CTRexExtendedDriverDb::Ins()->get_drv()->set_rcv_all(pif, false);

           }
        }
    
    /* for stateless only - set port mode */
    if (get_is_stateless()) {
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            uint32_t src_ipv4 = CGlobalInfo::m_options.m_ip_cfg[port_id].get_ip();
            uint32_t dg = CGlobalInfo::m_options.m_ip_cfg[port_id].get_def_gw();
            const uint8_t *dst_mac = CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest; 

            /* L3 mode */
            if (src_ipv4 && dg) {
                if (memcmp(dst_mac, empty_mac, 6) == 0) {
                    m_trex_stateless->get_port_by_id(port_id)->set_l3_mode(src_ipv4, dg);
                } else {
                    m_trex_stateless->get_port_by_id(port_id)->set_l3_mode(src_ipv4, dg, dst_mac);
                }

                /* L2 mode */
            } else {
                m_trex_stateless->get_port_by_id(port_id)->set_l2_mode(dst_mac);
            }
        }
    }

 
}

/**
 * check for a single core
 *
 * @author imarom (19-Nov-15)
 *
 * @param thread_id
 */
void
CGlobalTRex::check_for_dp_message_from_core(int thread_id) {

    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(thread_id);

    /* fast path check */
    if ( likely ( ring->isEmpty() ) ) {
        return;
    }

    while ( true ) {
        CGenNode * node = NULL;
        if (ring->Dequeue(node) != 0) {
            break;
        }
        assert(node);

        TrexStatelessDpToCpMsgBase * msg = (TrexStatelessDpToCpMsgBase *)node;
        msg->handle();
        delete msg;
    }

}

/**
 * check for messages that arrived from DP to CP
 *
 */
void
CGlobalTRex::check_for_dp_messages() {

    /* for all the cores - check for a new message */
    for (int i = 0; i < get_cores_tx(); i++) {
        check_for_dp_message_from_core(i);
    }
}

bool CGlobalTRex::is_all_links_are_up(bool dump){
    bool all_link_are=true;
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->get_port_attr()->update_link_status();
        if ( dump ){
            _if->dump_stats(stdout);
        }
        if ( _if->get_port_attr()->is_link_up() == false){
            all_link_are=false;
            break;
        }
    }
    return (all_link_are);
}

void CGlobalTRex::try_stop_all_cores(){

    TrexStatelessDpQuit * dp_msg= new TrexStatelessDpQuit();
    send_message_all_dp(dp_msg);
    delete dp_msg;

    if (get_is_stateless()) {
        TrexStatelessRxQuit * rx_msg= new TrexStatelessRxQuit();
        send_message_to_rx(rx_msg);
    }

    // no need to delete rx_msg. Deleted by receiver
    bool all_core_finished = false;
    int i;
    for (i=0; i<20; i++) {
        if ( is_all_cores_finished() ){
            all_core_finished =true;
            break;
        }
        delay(100);
    }
    if ( all_core_finished ){
        m_zmq_publisher.publish_event(TrexPublisher::EVENT_SERVER_STOPPED);
        printf(" All cores stopped !! \n");
    }else{
        printf(" ERROR one of the DP core is stucked !\n");
    }
}


int  CGlobalTRex::send_message_all_dp(TrexStatelessCpToDpMsgBase *msg){

    int max_threads=(int)CMsgIns::Ins()->getCpDp()->get_num_threads();
    int i;

    for (i=0; i<max_threads; i++) {
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp((uint8_t)i);
        ring->Enqueue((CGenNode*)msg->clone());
    }
    return (0);
}

int CGlobalTRex::send_message_to_rx(TrexStatelessCpToRxMsgBase *msg) {
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->Enqueue((CGenNode *) msg);

    return (0);
}


int  CGlobalTRex::ixgbe_rx_queue_flush(){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->flush_rx_queue();
    }
    return (0);
}


// init stateful rx core
void CGlobalTRex::ixgbe_configure_mg(void) {
    int i;
    CLatencyManagerCfg mg_cfg;
    mg_cfg.m_max_ports = m_max_ports;

    uint32_t latency_rate=CGlobalInfo::m_options.m_latency_rate;

    if ( latency_rate ) {
        mg_cfg.m_cps = (double)latency_rate ;
    } else {
        // If RX core needed, we need something to make the scheduler running.
        // If nothing configured, send 1 CPS latency measurement packets.
        if (CGlobalInfo::m_options.m_arp_ref_per == 0) {
            mg_cfg.m_cps = 1.0;
        } else {
            mg_cfg.m_cps = 0;
        }
    }

    if ( get_vm_one_queue_enable() ) {
        /* vm mode, indirect queues  */
        for (i=0; i<m_max_ports; i++) {
            CPhyEthIF * _if = &m_ports[i];
            CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();

            uint8_t thread_id = (i>>1);

            CNodeRing * r = rx_dp->getRingCpToDp(thread_id);
            m_latency_vm_vports[i].Create((uint8_t)i, r, &m_mg, _if);

            mg_cfg.m_ports[i] =&m_latency_vm_vports[i];
        }

    }else{
        for (i=0; i<m_max_ports; i++) {
            CPhyEthIF * _if=&m_ports[i];
            _if->dump_stats(stdout);
            m_latency_vports[i].Create(_if, m_rx_core_tx_q_id, 1);

            mg_cfg.m_ports[i] =&m_latency_vports[i];
        }
    }


    m_mg.Create(&mg_cfg);
    m_mg.set_mask(CGlobalInfo::m_options.m_latency_mask);
}

// init m_rx_sl object for stateless rx core
void CGlobalTRex::rx_sl_configure(void) {
    CRxSlCfg rx_sl_cfg;
    int i;

    rx_sl_cfg.m_max_ports = m_max_ports;
    rx_sl_cfg.m_num_crc_fix_bytes = get_ex_drv()->get_num_crc_fix_bytes();

    if ( get_vm_one_queue_enable() ) {
        /* vm mode, indirect queues  */
        for (i=0; i < m_max_ports; i++) {
            CPhyEthIF * _if = &m_ports[i];
            CMessagingManager * rx_dp = CMsgIns::Ins()->getRxDp();
            uint8_t thread_id = (i >> 1);
            CNodeRing * r = rx_dp->getRingCpToDp(thread_id);
            m_latency_vm_vports[i].Create(i, r, &m_mg, _if);
            rx_sl_cfg.m_ports[i] = &m_latency_vm_vports[i];
        }
    } else {
        for (i = 0; i < m_max_ports; i++) {
            CPhyEthIF * _if = &m_ports[i];
            m_latency_vports[i].Create(_if, m_rx_core_tx_q_id, 1);
            rx_sl_cfg.m_ports[i] = &m_latency_vports[i];
        }
    }

    m_rx_sl.create(rx_sl_cfg);
}

int  CGlobalTRex::ixgbe_start(void){
    int i;
    for (i=0; i<m_max_ports; i++) {
        socket_id_t socket_id = CGlobalInfo::m_socket.port_to_socket((port_id_t)i);
        assert(CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);
        CPhyEthIF * _if=&m_ports[i];
        _if->Create((uint8_t)i);
        uint16_t rx_rss = get_ex_drv()->enable_rss_drop_workaround();

        if ( get_vm_one_queue_enable() ) {
            /* VMXNET3 does claim to support 16K but somehow does not work */
            /* reduce to 2000 */
            m_port_cfg.m_port_conf.rxmode.max_rx_pkt_len = 2000;
            /* In VM case, there is one tx q and one rx q */
            _if->configure(1, 1, &m_port_cfg.m_port_conf);
            // Only 1 rx queue, so use it for everything
            m_rx_core_tx_q_id = 0;
            _if->set_rx_queue(0);
            _if->rx_queue_setup(0, RTE_TEST_RX_DESC_VM_DEFAULT, socket_id, &m_port_cfg.m_rx_conf,
                                CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);
            // 1 TX queue in VM case
            _if->tx_queue_setup(0, RTE_TEST_TX_DESC_VM_DEFAULT, socket_id, &m_port_cfg.m_tx_conf);
        } else {
            // 2 rx queues.
            // TX queues: 1 for each core handling the port pair + 1 for latency pkts + 1 for use by RX core
            
            uint16_t rx_queues;

            if (rx_rss==0) {
                rx_queues=2;
            }else{
                rx_queues=rx_rss;
            }

            _if->configure(rx_queues, m_cores_to_dual_ports + 2, &m_port_cfg.m_port_conf);
            m_rx_core_tx_q_id = m_cores_to_dual_ports;

            if ( rx_rss ) {
                int j=0;
                for (j=0;j<rx_rss; j++) {
                        if (j==MAIN_DPDK_RX_Q){
                            continue;
                        }
                        /* drop queue */
                        _if->rx_queue_setup(j,
                                        RTE_TEST_RX_DESC_DEFAULT_MLX,
                                        socket_id,
                                        &m_port_cfg.m_rx_conf,
                                        CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);


                }
            }else{
                 // setup RX drop queue
                _if->rx_queue_setup(MAIN_DPDK_DATA_Q,
                                    RTE_TEST_RX_DESC_DEFAULT,
                                    socket_id,
                                    &m_port_cfg.m_rx_conf,
                                    CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);
                // setup RX filter queue
                _if->set_rx_queue(MAIN_DPDK_RX_Q);
            }

            _if->rx_queue_setup(MAIN_DPDK_RX_Q,
                                RTE_TEST_RX_LATENCY_DESC_DEFAULT,
                                socket_id,
                                &m_port_cfg.m_rx_conf,
                                CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_9k);

            for (int qid = 0; qid < m_max_queues_per_port; qid++) {
                _if->tx_queue_setup((uint16_t)qid,
                                    RTE_TEST_TX_DESC_DEFAULT ,
                                    socket_id,
                                    &m_port_cfg.m_tx_conf);
            }
        }

        if ( rx_rss ){
            _if->configure_rss_redirect_table(rx_rss,MAIN_DPDK_RX_Q);
        }

        _if->stats_clear();
        _if->start();
        _if->configure_rx_duplicate_rules();

        if ( ! get_vm_one_queue_enable()  && ! CGlobalInfo::m_options.preview.get_is_disable_flow_control_setting()
             && _if->get_port_attr()->is_fc_change_supported()) {
            _if->disable_flow_control();
        }

        _if->get_port_attr()->add_mac((char *)CGlobalInfo::m_options.get_src_mac_addr(i));

        fflush(stdout);
    }

    if ( !is_all_links_are_up()  ){
        /* wait for ports to be stable */
        get_ex_drv()->wait_for_stable_link();

        if ( !is_all_links_are_up(true) /*&& !get_is_stateless()*/ ){ // disable start with link down for now

            /* temporary solution for trex-192 issue, solve the case for X710/XL710, will work for both Statless and Stateful */
            if (  get_ex_drv()->drop_packets_incase_of_linkdown() ){
                printf(" WARNING : there is no link on one of the ports, driver support auto drop in case of link down - continue\n");
            }else{
                dump_links_status(stdout);
                rte_exit(EXIT_FAILURE, " One of the links is down \n");
            }
        }
    } else {
        get_ex_drv()->wait_after_link_up();
    }

    dump_links_status(stdout);

    ixgbe_rx_queue_flush();

    if (! get_is_stateless()) {
        ixgbe_configure_mg();
    } 


    /* core 0 - control
       core 1 - port 0-0,1-0,
       core 2 - port 2-0,3-0,
       core 3 - port 0-1,1-1,
       core 4 - port 2-1,3-1,

    */
    int port_offset=0;
    uint8_t lat_q_id;

    if ( get_vm_one_queue_enable() ) {
        lat_q_id = 0;
    } else {
        lat_q_id = get_cores_tx() / get_base_num_cores() + 1;
    }
    for (i=0; i<get_cores_tx(); i++) {
        int j=(i+1);
        int queue_id=((j-1)/get_base_num_cores() );   /* for the first min core queue 0 , then queue 1 etc */
        if ( get_is_stateless() ){
            m_cores_vif[j]=&m_cores_vif_sl[j];
        }else{
            m_cores_vif[j]=&m_cores_vif_sf[j];
        }
        m_cores_vif[j]->Create(j,
                               queue_id,
                               &m_ports[port_offset], /* 0,2*/
                               queue_id,
                               &m_ports[port_offset+1], /*1,3*/
                               lat_q_id);
        port_offset+=2;
        if (port_offset == m_max_ports) {
            port_offset = 0;
            // We want to allow sending latency packets only from first core handling a port
            lat_q_id = CCoreEthIF::INVALID_Q_ID;
        }
    }

    fprintf(stdout," -------------------------------\n");
    fprintf(stdout, "RX core uses TX queue number %d on all ports\n", m_rx_core_tx_q_id);
    CCoreEthIF::DumpIfCfgHeader(stdout);
    for (i=0; i<get_cores_tx(); i++) {
        m_cores_vif[i+1]->DumpIfCfg(stdout);
    }
    fprintf(stdout," -------------------------------\n");

    return (0);
}

static void trex_termination_handler(int signum);

void CGlobalTRex::register_signals() {
    struct sigaction action;

    /* handler */
    action.sa_handler = trex_termination_handler;

    /* blocked signals during handling */
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);

    /* no flags */
    action.sa_flags = 0;

    /* register */
    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

bool CGlobalTRex::Create(){
    CFlowsYamlInfo     pre_yaml_info;

    register_signals();

    m_stats_cnt =0;
    if (!get_is_stateless()) {
        pre_yaml_info.load_from_yaml_file(CGlobalInfo::m_options.cfg_file);
    }

    if ( !m_zmq_publisher.Create( CGlobalInfo::m_options.m_zmq_port,
                                  !CGlobalInfo::m_options.preview.get_zmq_publish_enable() ) ){
        return (false);
    }

    if ( pre_yaml_info.m_vlan_info.m_enable ){
        CGlobalInfo::m_options.preview.set_vlan_mode_enable(true);
    }
    /* End update pre flags */

    ixgbe_prob_init();
    cores_prob_init();
    queues_prob_init();

    /* allocate rings */
    assert( CMsgIns::Ins()->Create(get_cores_tx()) );

    if ( sizeof(CGenNodeNatInfo) != sizeof(CGenNode)  ) {
        printf("ERROR sizeof(CGenNodeNatInfo) %lu != sizeof(CGenNode) %lu must be the same size \n",sizeof(CGenNodeNatInfo),sizeof(CGenNode));
        assert(0);
    }

    if ( sizeof(CGenNodeLatencyPktInfo) != sizeof(CGenNode)  ) {
        printf("ERROR sizeof(CGenNodeLatencyPktInfo) %lu != sizeof(CGenNode) %lu must be the same size \n",sizeof(CGenNodeLatencyPktInfo),sizeof(CGenNode));
        assert(0);
    }

    /* allocate the memory */

    uint32_t rx_mbuf = 0 ;

    if ( get_vm_one_queue_enable() ) {
        rx_mbuf = (m_max_ports * RTE_TEST_RX_DESC_VM_DEFAULT);
    }else{
        rx_mbuf = (m_max_ports * (RTE_TEST_RX_LATENCY_DESC_DEFAULT+RTE_TEST_RX_DESC_DEFAULT));
    }

    CGlobalInfo::init_pools(rx_mbuf);
    ixgbe_start();
    dump_config(stdout);

    /* start stateless */
    if (get_is_stateless()) {

        TrexStatelessCfg cfg;

        TrexRpcServerConfig rpc_req_resp_cfg(TrexRpcServerConfig::RPC_PROT_TCP,
                                             global_platform_cfg_info.m_zmq_rpc_port,
                                             &m_cp_lock);

        cfg.m_port_count         = CGlobalInfo::m_options.m_expected_portd;
        cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
        cfg.m_rpc_server_verbose = false;
        cfg.m_platform_api       = new TrexDpdkPlatformApi();
        cfg.m_publisher          = &m_zmq_publisher;

        m_trex_stateless = new TrexStateless(cfg);
        
        rx_sl_configure();
    }

    return (true);

}
void CGlobalTRex::Delete(){

    m_zmq_publisher.Delete();
    m_fl.Delete();

    if (m_trex_stateless) {
        delete m_trex_stateless;
        m_trex_stateless = NULL;
    }
}



int  CGlobalTRex::ixgbe_prob_init(void){

    m_max_ports  = rte_eth_dev_count();
    if (m_max_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    printf(" Number of ports found: %d \n",m_max_ports);

    if ( m_max_ports %2 !=0 ) {
        rte_exit(EXIT_FAILURE, " Number of ports %d should be even, mask the one port in the configuration file  \n, ",
                 m_max_ports);
    }

    if ( CGlobalInfo::m_options.get_expected_ports() > TREX_MAX_PORTS ) {
        rte_exit(EXIT_FAILURE, " Maximum ports supported are %d, use the configuration file to set the expected number of ports   \n",TREX_MAX_PORTS);
    }

    if ( CGlobalInfo::m_options.get_expected_ports() > m_max_ports ){
        rte_exit(EXIT_FAILURE, " There are %d ports you expected more %d,use the configuration file to set the expected number of ports   \n",
                 m_max_ports,
                 CGlobalInfo::m_options.get_expected_ports());
    }
    if (CGlobalInfo::m_options.get_expected_ports() < m_max_ports ) {
        /* limit the number of ports */
        m_max_ports=CGlobalInfo::m_options.get_expected_ports();
    }
    assert(m_max_ports <= TREX_MAX_PORTS);

    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get((uint8_t) 0,&dev_info);

    if ( CGlobalInfo::m_options.preview.getVMode() > 0){
        printf("\n\n");
        printf("if_index : %d \n",dev_info.if_index);
        printf("driver name : %s \n",dev_info.driver_name);
        printf("min_rx_bufsize : %d \n",dev_info.min_rx_bufsize);
        printf("max_rx_pktlen  : %d \n",dev_info.max_rx_pktlen);
        printf("max_rx_queues  : %d \n",dev_info.max_rx_queues);
        printf("max_tx_queues  : %d \n",dev_info.max_tx_queues);
        printf("max_mac_addrs  : %d \n",dev_info.max_mac_addrs);

        printf("rx_offload_capa : %x \n",dev_info.rx_offload_capa);
        printf("tx_offload_capa : %x \n",dev_info.tx_offload_capa);
    }



    if ( !CTRexExtendedDriverDb::Ins()->is_driver_exists(dev_info.driver_name) ){
        printf(" Error: driver %s is not supported. Please consult the documentation for a list of supported drivers\n"
               ,dev_info.driver_name);
        exit(1);
    }

    int i;
    struct rte_eth_dev_info dev_info1;

    for (i=1; i<m_max_ports; i++) {
        rte_eth_dev_info_get((uint8_t) i,&dev_info1);
        if ( strcmp(dev_info1.driver_name,dev_info.driver_name)!=0) {
            printf(" ERROR all device should have the same type  %s != %s \n",dev_info1.driver_name,dev_info.driver_name);
            exit(1);
        }
    }

    CTRexExtendedDriverDb::Ins()->set_driver_name(dev_info.driver_name);
    m_drv = CTRexExtendedDriverDb::Ins()->get_drv();

    // check if firmware version is new enough
    for (i = 0; i < m_max_ports; i++) {
        if (m_drv->verify_fw_ver(i) < 0) {
            // error message printed by verify_fw_ver
            exit(1);
        }
    }

    m_port_cfg.update_var();

    if ( get_is_rx_filter_enable() ){
        m_port_cfg.update_global_config_fdir();
    }

    if ( get_vm_one_queue_enable() ) {
        /* verify that we have only one thread/core per dual- interface */
        if ( CGlobalInfo::m_options.preview.getCores()>1 ) {
            printf(" ERROR the number of cores should be 1 when the driver support only one tx queue and one rx queue \n");
            exit(1);
        }
    }
    return (0);
}

int  CGlobalTRex::cores_prob_init(){
    m_max_cores = rte_lcore_count();
    assert(m_max_cores>0);
    return (0);
}

int  CGlobalTRex::queues_prob_init(){

    if (m_max_cores < 2) {
        rte_exit(EXIT_FAILURE, "number of cores should be at least 2 \n");
    }

    assert((m_max_ports>>1) <= get_cores_tx() );

    m_cores_mul = CGlobalInfo::m_options.preview.getCores();

    m_cores_to_dual_ports  = m_cores_mul;

    /* core 0 - control
       -core 1 - port 0/1
       -core 2 - port 2/3
       -core 3 - port 0/1
       -core 4 - port 2/3

       m_cores_to_dual_ports = 2;
    */

    // One q for each core allowed to send on this port + 1 for latency q (Used in stateless) + 1 for RX core.
    m_max_queues_per_port  = m_cores_to_dual_ports + 2;

    if (m_max_queues_per_port > BP_MAX_TX_QUEUE) {
        rte_exit(EXIT_FAILURE,
                 "Error: Number of TX queues exceeds %d. Try running with lower -c <val> \n",BP_MAX_TX_QUEUE);
    }

    assert(m_max_queues_per_port>0);
    return (0);
}


void CGlobalTRex::dump_config(FILE *fd){
    fprintf(fd," number of ports         : %u \n",m_max_ports);
    fprintf(fd," max cores for 2 ports   : %u \n",m_cores_to_dual_ports);
    fprintf(fd," max queue per port      : %u \n",m_max_queues_per_port);
}


void CGlobalTRex::dump_links_status(FILE *fd){
    for (int i=0; i<m_max_ports; i++) {
        m_ports[i].get_port_attr()->update_link_status_nowait();
        m_ports[i].get_port_attr()->dump_link(fd);
    }
}

bool CGlobalTRex::lookup_port_by_mac(const uint8_t *mac, uint8_t &port_id) {
    for (int i = 0; i < m_max_ports; i++) {
        if (memcmp(m_ports[i].get_port_attr()->get_src_mac(), mac, 6) == 0) {
            port_id = i;
            return true;
        }
    }
    
    return false;
}

void CGlobalTRex::dump_post_test_stats(FILE *fd){
    uint64_t pkt_out=0;
    uint64_t pkt_out_bytes=0;
    uint64_t pkt_in_bytes=0;
    uint64_t pkt_in=0;
    uint64_t sw_pkt_out=0;
    uint64_t sw_pkt_out_err=0;
    uint64_t sw_pkt_out_bytes=0;
    uint64_t tx_arp = 0;
    uint64_t rx_arp = 0;

    int i;
    for (i=0; i<get_cores_tx(); i++) {
        CCoreEthIF * erf_vif = m_cores_vif[i+1];
        CVirtualIFPerSideStats stats;
        erf_vif->GetCoreCounters(&stats);
        sw_pkt_out     += stats.m_tx_pkt;
        sw_pkt_out_err += stats.m_tx_drop +stats.m_tx_queue_full +stats.m_tx_alloc_error ;
        sw_pkt_out_bytes +=stats.m_tx_bytes;
    }


    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        pkt_in  +=_if->get_stats().ipackets;
        pkt_in_bytes +=_if->get_stats().ibytes;
        pkt_out +=_if->get_stats().opackets;
        pkt_out_bytes +=_if->get_stats().obytes;
        tx_arp += _if->get_ignore_stats().get_tx_arp();
        rx_arp += _if->get_ignore_stats().get_rx_arp();
    }
    if ( CGlobalInfo::m_options.is_latency_enabled() ){
        sw_pkt_out += m_mg.get_total_pkt();
        sw_pkt_out_bytes +=m_mg.get_total_bytes();
    }


    fprintf (fd," summary stats \n");
    fprintf (fd," -------------- \n");

    if (pkt_in > pkt_out)
        {
            fprintf (fd, " Total-pkt-drop       : 0 pkts \n");
            if (pkt_in > pkt_out * 1.01)
                fprintf (fd, " Warning : number of rx packets exceeds 101%% of tx packets!\n");
        }
    else
        fprintf (fd, " Total-pkt-drop       : %llu pkts \n", (unsigned long long) (pkt_out - pkt_in));
    for (i=0; i<m_max_ports; i++) {
        if ( m_stats.m_port[i].m_link_was_down ) {
            fprintf (fd, " WARNING: Link was down at port %d during test (at least for some time)!\n", i);
        }
    }
    fprintf (fd," Total-tx-bytes       : %llu bytes \n", (unsigned long long)pkt_out_bytes);
    fprintf (fd," Total-tx-sw-bytes    : %llu bytes \n", (unsigned long long)sw_pkt_out_bytes);
    fprintf (fd," Total-rx-bytes       : %llu byte \n", (unsigned long long)pkt_in_bytes);

    fprintf (fd," \n");

    fprintf (fd," Total-tx-pkt         : %llu pkts \n", (unsigned long long)pkt_out);
    fprintf (fd," Total-rx-pkt         : %llu pkts \n", (unsigned long long)pkt_in);
    fprintf (fd," Total-sw-tx-pkt      : %llu pkts \n", (unsigned long long)sw_pkt_out);
    fprintf (fd," Total-sw-err         : %llu pkts \n", (unsigned long long)sw_pkt_out_err);
    fprintf (fd," Total ARP sent       : %llu pkts \n", (unsigned long long)tx_arp);
    fprintf (fd," Total ARP received   : %llu pkts \n", (unsigned long long)rx_arp);


    if ( CGlobalInfo::m_options.is_latency_enabled() ){
        fprintf (fd," maximum-latency   : %.0f usec \n",m_mg.get_max_latency());
        fprintf (fd," average-latency   : %.0f usec \n",m_mg.get_avr_latency());
        fprintf (fd," latency-any-error : %s  \n",m_mg.is_any_error()?"ERROR":"OK");
    }


}


void CGlobalTRex::update_stats(){

    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->update_counters();
    }
    uint64_t total_open_flows=0;


    CFlowGenListPerThread   * lpt;
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        total_open_flows +=   lpt->m_stats.m_total_open_flows ;
    }
    m_last_total_cps = m_cps.add(total_open_flows);

}

tx_per_flow_t CGlobalTRex::get_flow_tx_stats(uint8_t port, uint16_t index) {
    return m_stats.m_port[port].m_tx_per_flow[index] - m_stats.m_port[port].m_prev_tx_per_flow[index];
}

// read stats. Return read value, and clear.
tx_per_flow_t CGlobalTRex::clear_flow_tx_stats(uint8_t port, uint16_t index, bool is_lat) {
    uint8_t port0;
    CFlowGenListPerThread * lpt;
    tx_per_flow_t ret;

    m_stats.m_port[port].m_tx_per_flow[index].clear();

    for (int i=0; i < get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        port0 = lpt->getDualPortId() * 2;
        if ((port == port0) || (port == port0 + 1)) {
            m_stats.m_port[port].m_tx_per_flow[index] +=
                lpt->m_node_gen.m_v_if->m_stats[port - port0].m_tx_per_flow[index];
            if (is_lat)
                lpt->m_node_gen.m_v_if->m_stats[port - port0].m_lat_data[index - MAX_FLOW_STATS].reset();
        }
    }

    ret = m_stats.m_port[port].m_tx_per_flow[index] - m_stats.m_port[port].m_prev_tx_per_flow[index];

    // Since we return diff from prev, following "clears" the stats.
    m_stats.m_port[port].m_prev_tx_per_flow[index] = m_stats.m_port[port].m_tx_per_flow[index];

    return ret;
}

void CGlobalTRex::get_stats(CGlobalStats & stats){

    int i;
    float total_tx=0.0;
    float total_rx=0.0;
    float total_tx_pps=0.0;
    float total_rx_pps=0.0;

    stats.m_total_tx_pkts  = 0;
    stats.m_total_rx_pkts  = 0;
    stats.m_total_tx_bytes = 0;
    stats.m_total_rx_bytes = 0;
    stats.m_total_alloc_error=0;
    stats.m_total_queue_full=0;
    stats.m_total_queue_drop=0;


    stats.m_num_of_ports = m_max_ports;
    stats.m_cpu_util = m_fl.GetCpuUtil();
    stats.m_cpu_util_raw = m_fl.GetCpuUtilRaw();
    if (get_is_stateless()) {
        stats.m_rx_cpu_util = m_rx_sl.get_cpu_util();
    }
    stats.m_threads      = m_fl.m_threads_info.size();

    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        CPerPortStats * stp=&stats.m_port[i];

        CPhyEthIFStats & st =_if->get_stats();

        stp->opackets = st.opackets;
        stp->obytes   = st.obytes;
        stp->ipackets = st.ipackets;
        stp->ibytes   = st.ibytes;
        stp->ierrors  = st.ierrors;
        stp->oerrors  = st.oerrors;
        stp->m_total_tx_bps = _if->get_last_tx_rate()*_1Mb_DOUBLE;
        stp->m_total_tx_pps = _if->get_last_tx_pps_rate();
        stp->m_total_rx_bps = _if->get_last_rx_rate()*_1Mb_DOUBLE;
        stp->m_total_rx_pps = _if->get_last_rx_pps_rate();
        stp->m_link_up        = _if->get_port_attr()->is_link_up();
        stp->m_link_was_down |= ! _if->get_port_attr()->is_link_up();

        stats.m_total_tx_pkts  += st.opackets;
        stats.m_total_rx_pkts  += st.ipackets;
        stats.m_total_tx_bytes += st.obytes;
        stats.m_total_rx_bytes += st.ibytes;

        total_tx +=_if->get_last_tx_rate();
        total_rx +=_if->get_last_rx_rate();
        total_tx_pps +=_if->get_last_tx_pps_rate();
        total_rx_pps +=_if->get_last_rx_pps_rate();
        // IP ID rules
        for (uint16_t flow = 0; flow <= max_stat_hw_id_seen; flow++) {
            stats.m_port[i].m_tx_per_flow[flow].clear();
        }
        // payload rules
        for (uint16_t flow = MAX_FLOW_STATS; flow <= MAX_FLOW_STATS + max_stat_hw_id_seen_payload; flow++) {
            stats.m_port[i].m_tx_per_flow[flow].clear();
        }

        stp->m_cpu_util = get_cpu_util_per_interface(i);

    }

    uint64_t total_open_flows=0;
    uint64_t total_active_flows=0;

    uint64_t total_clients=0;
    uint64_t total_servers=0;
    uint64_t active_sockets=0;
    uint64_t total_sockets=0;


    uint64_t total_nat_time_out =0;
    uint64_t total_nat_time_out_wait_ack =0;
    uint64_t total_nat_no_fid   =0;
    uint64_t total_nat_active   =0;
    uint64_t total_nat_syn_wait = 0;
    uint64_t total_nat_open     =0;
    uint64_t total_nat_learn_error=0;

    CFlowGenListPerThread   * lpt;
    stats.m_template.Clear();
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        total_open_flows +=   lpt->m_stats.m_total_open_flows ;
        total_active_flows += (lpt->m_stats.m_total_open_flows-lpt->m_stats.m_total_close_flows) ;

        stats.m_total_alloc_error += lpt->m_node_gen.m_v_if->m_stats[0].m_tx_alloc_error+
            lpt->m_node_gen.m_v_if->m_stats[1].m_tx_alloc_error;
        stats.m_total_queue_full +=lpt->m_node_gen.m_v_if->m_stats[0].m_tx_queue_full+
            lpt->m_node_gen.m_v_if->m_stats[1].m_tx_queue_full;

        stats.m_total_queue_drop +=lpt->m_node_gen.m_v_if->m_stats[0].m_tx_drop+
            lpt->m_node_gen.m_v_if->m_stats[1].m_tx_drop;

        stats.m_template.Add(&lpt->m_node_gen.m_v_if->m_stats[0].m_template);
        stats.m_template.Add(&lpt->m_node_gen.m_v_if->m_stats[1].m_template);


        total_clients   += lpt->m_smart_gen.getTotalClients();
        total_servers   += lpt->m_smart_gen.getTotalServers();
        active_sockets  += lpt->m_smart_gen.ActiveSockets();
        total_sockets   += lpt->m_smart_gen.MaxSockets();

        total_nat_time_out +=lpt->m_stats.m_nat_flow_timeout;
        total_nat_time_out_wait_ack += lpt->m_stats.m_nat_flow_timeout_wait_ack;
        total_nat_no_fid   +=lpt->m_stats.m_nat_lookup_no_flow_id ;
        total_nat_active   +=lpt->m_stats.m_nat_lookup_add_flow_id - lpt->m_stats.m_nat_lookup_remove_flow_id;
        total_nat_syn_wait += lpt->m_stats.m_nat_lookup_add_flow_id - lpt->m_stats.m_nat_lookup_wait_ack_state;
        total_nat_open     +=lpt->m_stats.m_nat_lookup_add_flow_id;
        total_nat_learn_error   +=lpt->m_stats.m_nat_flow_learn_error;
        uint8_t port0 = lpt->getDualPortId() *2;
        // IP ID rules
        for (uint16_t flow = 0; flow <= max_stat_hw_id_seen; flow++) {
            stats.m_port[port0].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->m_stats[0].m_tx_per_flow[flow];
            stats.m_port[port0 + 1].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->m_stats[1].m_tx_per_flow[flow];
        }
        // payload rules
        for (uint16_t flow = MAX_FLOW_STATS; flow <= MAX_FLOW_STATS + max_stat_hw_id_seen_payload; flow++) {
            stats.m_port[port0].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->m_stats[0].m_tx_per_flow[flow];
            stats.m_port[port0 + 1].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->m_stats[1].m_tx_per_flow[flow];
        }

    }

    stats.m_total_nat_time_out = total_nat_time_out;
    stats.m_total_nat_time_out_wait_ack = total_nat_time_out_wait_ack;
    stats.m_total_nat_no_fid   = total_nat_no_fid;
    stats.m_total_nat_active   = total_nat_active;
    stats.m_total_nat_syn_wait = total_nat_syn_wait;
    stats.m_total_nat_open     = total_nat_open;
    stats.m_total_nat_learn_error     = total_nat_learn_error;

    stats.m_total_clients = total_clients;
    stats.m_total_servers = total_servers;
    stats.m_active_sockets = active_sockets;

    if (total_sockets != 0) {
        stats.m_socket_util =100.0*(double)active_sockets/(double)total_sockets;
    } else {
        stats.m_socket_util = 0;
    }



    float drop_rate=total_tx-total_rx;
    if ( (drop_rate<0.0)  || (drop_rate < 0.1*total_tx ) )  {
        drop_rate=0.0;
    }
    float pf =CGlobalInfo::m_options.m_platform_factor;
    stats.m_platform_factor = pf;

    stats.m_active_flows = total_active_flows*pf;
    stats.m_open_flows   = total_open_flows*pf;
    stats.m_rx_drop_bps   = drop_rate*pf *_1Mb_DOUBLE;

    stats.m_tx_bps        = total_tx*pf*_1Mb_DOUBLE;
    stats.m_rx_bps        = total_rx*pf*_1Mb_DOUBLE;
    stats.m_tx_pps        = total_tx_pps*pf;
    stats.m_rx_pps        = total_rx_pps*pf;
    stats.m_tx_cps        = m_last_total_cps*pf;
    if(stats.m_cpu_util < 0.0001)
        stats.m_bw_per_core = 0;
    else
        stats.m_bw_per_core   = 2*(stats.m_tx_bps/1e9)*100.0/(stats.m_cpu_util*stats.m_threads);

    stats.m_tx_expected_cps        = m_expected_cps*pf;
    stats.m_tx_expected_pps        = m_expected_pps*pf;
    stats.m_tx_expected_bps        = m_expected_bps*pf;
}

float
CGlobalTRex::get_cpu_util_per_interface(uint8_t port_id) {
    CPhyEthIF * _if = &m_ports[port_id];

    float    tmp = 0;
    uint8_t  cnt = 0;
    for (const auto &p : _if->get_core_list()) {
        uint8_t core_id = p.first;
        CFlowGenListPerThread *lp = m_fl.m_threads_info[core_id];
        if (lp->is_port_active(port_id)) {
            tmp += lp->m_cpu_cp_u.GetVal();
            cnt++;
        }
    }

    return ( (cnt > 0) ? (tmp / cnt) : 0);

}

bool CGlobalTRex::sanity_check(){

    CFlowGenListPerThread   * lpt;
    uint32_t errors=0;
    int i;
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        errors   += lpt->m_smart_gen.getErrorAllocationCounter();
    }

    if ( errors ) {
        printf(" ERRORs sockets allocation errors! \n");
        printf(" you should allocate more clients in the pool \n");
        return(true);
    }
    return ( false);
}


/* dump the template info */
void CGlobalTRex::dump_template_info(std::string & json){
    CFlowGenListPerThread   * lpt = m_fl.m_threads_info[0];
    CFlowsYamlInfo * yaml_info=&lpt->m_yaml_info;

    json="{\"name\":\"template_info\",\"type\":0,\"data\":[";
    int i;
    for (i=0; i<yaml_info->m_vec.size()-1; i++) {
        CFlowYamlInfo * r=&yaml_info->m_vec[i] ;
        json+="\""+ r->m_name+"\"";
        json+=",";
    }
    json+="\""+yaml_info->m_vec[i].m_name+"\"";
    json+="]}" ;
}

void CGlobalTRex::dump_stats(FILE *fd, CGlobalStats::DumpFormat format){

    update_stats();
    get_stats(m_stats);

    if (format==CGlobalStats::dmpTABLE) {
        if ( m_io_modes.m_g_mode == CTrexGlobalIoMode::gNORMAL ){
            switch (m_io_modes.m_pp_mode ){
            case CTrexGlobalIoMode::ppDISABLE:
                fprintf(fd,"\n+Per port stats disabled \n");
                break;
            case CTrexGlobalIoMode::ppTABLE:
                fprintf(fd,"\n-Per port stats table \n");
                m_stats.Dump(fd,CGlobalStats::dmpTABLE);
                break;
            case CTrexGlobalIoMode::ppSTANDARD:
                fprintf(fd,"\n-Per port stats - standard\n");
                m_stats.Dump(fd,CGlobalStats::dmpSTANDARD);
                break;
            };

            switch (m_io_modes.m_ap_mode ){
            case   CTrexGlobalIoMode::apDISABLE:
                fprintf(fd,"\n+Global stats disabled \n");
                break;
            case   CTrexGlobalIoMode::apENABLE:
                fprintf(fd,"\n-Global stats enabled \n");
                m_stats.DumpAllPorts(fd);
                break;
            };
        }
    }else{
        /* at exit , always need to dump it in standartd mode for scripts*/
        m_stats.Dump(fd,format);
        m_stats.DumpAllPorts(fd);
    }

}

void
CGlobalTRex::publish_async_data(bool sync_now, bool baseline) {
    std::string json;

    /* refactor to update, dump, and etc. */
    if (sync_now) {
        update_stats();
        get_stats(m_stats);
    }

    m_stats.dump_json(json, baseline);
    m_zmq_publisher.publish_json(json);

    /* generator json , all cores are the same just sample the first one */
    m_fl.m_threads_info[0]->m_node_gen.dump_json(json);
    m_zmq_publisher.publish_json(json);


    if ( !get_is_stateless() ){
        dump_template_info(json);
        m_zmq_publisher.publish_json(json);
    }

    if ( get_is_rx_check_mode() ) {
        m_mg.rx_check_dump_json(json );
        m_zmq_publisher.publish_json(json);
    }

    /* backward compatible */
    m_mg.dump_json(json );
    m_zmq_publisher.publish_json(json);

    /* more info */
    m_mg.dump_json_v2(json );
    m_zmq_publisher.publish_json(json);

    if (get_is_stateless()) {
        std::string stat_json;
        std::string latency_json;
        if (m_trex_stateless->m_rx_flow_stat.dump_json(stat_json, latency_json, baseline)) {
            m_zmq_publisher.publish_json(stat_json);
            m_zmq_publisher.publish_json(latency_json);
        }
    }
}

void
CGlobalTRex::publish_async_barrier(uint32_t key) {
    m_zmq_publisher.publish_barrier(key);
}

void
CGlobalTRex:: publish_async_port_attr_changed(uint8_t port_id) {
    Json::Value data;
    data["port_id"] = port_id;
    TRexPortAttr * _attr = m_ports[port_id].get_port_attr();

    _attr->to_json(data["attr"]);

    m_zmq_publisher.publish_event(TrexPublisher::EVENT_PORT_ATTR_CHANGED, data);
}

void
CGlobalTRex::handle_slow_path() {
    m_stats_cnt+=1;

    // update speed, link up/down etc.
    for (int i=0; i<m_max_ports; i++) {
        bool changed = m_ports[i].get_port_attr()->update_link_status_nowait();
        if (changed) {
            publish_async_port_attr_changed(i);
        }
    }

    if ( CGlobalInfo::m_options.preview.get_no_keyboard() ==false ) {
        if ( m_io_modes.handle_io_modes() ) {
            mark_for_shutdown(SHUTDOWN_CTRL_C);
            return;
        }
    }

    if ( sanity_check() ) {
        mark_for_shutdown(SHUTDOWN_TEST_ENDED);
        return;
    }

    if (m_io_modes.m_g_mode != CTrexGlobalIoMode::gDISABLE ) {
        fprintf(stdout,"\033[2J");
        fprintf(stdout,"\033[2H");

    } else {
        if ( m_io_modes.m_g_disable_first  ) {
            m_io_modes.m_g_disable_first=false;
            fprintf(stdout,"\033[2J");
            fprintf(stdout,"\033[2H");
            printf("clean !!!\n");
            fflush(stdout);
        }
    }


    if (m_io_modes.m_g_mode == CTrexGlobalIoMode::gHELP ) {
        m_io_modes.DumpHelp(stdout);
    }

    dump_stats(stdout,CGlobalStats::dmpTABLE);

    if (m_io_modes.m_g_mode == CTrexGlobalIoMode::gNORMAL ) {
        fprintf (stdout," current time    : %.1f sec  \n",now_sec());
        float d= CGlobalInfo::m_options.m_duration - now_sec();
        if (d<0) {
            d=0;

        }
        fprintf (stdout," test duration   : %.1f sec  \n",d);
    }

    if (m_io_modes.m_g_mode == CTrexGlobalIoMode::gMem) {

        if ( m_stats_cnt%4==0) {
            fprintf (stdout," %s \n",CGlobalInfo::dump_pool_as_json_str().c_str());
        }
    }


    if ( CGlobalInfo::m_options.is_rx_enabled() && (! get_is_stateless())) {
        m_mg.update();

        if ( m_io_modes.m_g_mode ==  CTrexGlobalIoMode::gNORMAL ) {
            if (CGlobalInfo::m_options.m_latency_rate != 0) {
                switch (m_io_modes.m_l_mode) {
                case CTrexGlobalIoMode::lDISABLE:
                    fprintf(stdout, "\n+Latency stats disabled \n");
                    break;
                case CTrexGlobalIoMode::lENABLE:
                    fprintf(stdout, "\n-Latency stats enabled \n");
                    m_mg.DumpShort(stdout);
                    break;
                case CTrexGlobalIoMode::lENABLE_Extended:
                    fprintf(stdout, "\n-Latency stats extended \n");
                    m_mg.Dump(stdout);
                    break;
                }
            }

            if ( get_is_rx_check_mode() ) {

                switch (m_io_modes.m_rc_mode) {
                case CTrexGlobalIoMode::rcDISABLE:
                    fprintf(stdout,"\n+Rx Check stats disabled \n");
                    break;
                case CTrexGlobalIoMode::rcENABLE:
                    fprintf(stdout,"\n-Rx Check stats enabled \n");
                    m_mg.DumpShortRxCheck(stdout);
                    break;
                case CTrexGlobalIoMode::rcENABLE_Extended:
                    fprintf(stdout,"\n-Rx Check stats enhanced \n");
                    m_mg.DumpRxCheck(stdout);
                    break;
                }
            }
        }
    }
    if ( m_io_modes.m_g_mode ==  CTrexGlobalIoMode::gNAT ) {
        if ( m_io_modes.m_nat_mode == CTrexGlobalIoMode::natENABLE ) {
            if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
                fprintf(stdout, "NAT flow table info\n");
                m_mg.dump_nat_flow_table(stdout);
            } else {
                fprintf(stdout, "\nThis is only relevant in --learn-mode %d\n", CParserOption::LEARN_MODE_TCP_ACK);
            }
        }
    }

    /* publish data */
    publish_async_data(false);
}


void
CGlobalTRex::handle_fast_path() {
    /* check from messages from DP */
    check_for_dp_messages();

    /* measure CPU utilization by sampling (we sample 1000 to get an accurate sampling) */
    for (int i = 0; i < 1000; i++) {
        m_fl.UpdateFast();

        if (get_is_stateless()) {
            m_rx_sl.update_cpu_util();
        }else{
            m_mg.update_fast();
        }

        rte_pause();
    }


    if ( is_all_cores_finished() ) {
        mark_for_shutdown(SHUTDOWN_TEST_ENDED);
    }
}


/**
 * shutdown sequence
 *
 */
void CGlobalTRex::shutdown() {
    std::stringstream ss;
    ss << " *** TRex is shutting down - cause: '";

    switch (m_mark_for_shutdown) {

    case SHUTDOWN_TEST_ENDED:
        ss << "test has ended'";
        break;

    case SHUTDOWN_CTRL_C:
        ss << "CTRL + C detected'";
        break;

    case SHUTDOWN_SIGINT:
        ss << "received signal SIGINT'";
        break;

    case SHUTDOWN_SIGTERM:
        ss << "received signal SIGTERM'";
        break;

    case SHUTDOWN_RPC_REQ:
        ss << "server received RPC 'shutdown' request'";
        break;

    default:
        assert(0);
    }

    /* report */
    std::cout << ss.str() << "\n";

    /* first stop the WD */
    TrexWatchDog::getInstance().stop();

    /* stateless shutdown */
    if (get_is_stateless()) {
        m_trex_stateless->shutdown();
    }

    if (!is_all_cores_finished()) {
        try_stop_all_cores();
    }

    m_mg.stop();

    delay(1000);

    /* shutdown drivers */
    for (int i = 0; i < m_max_ports; i++) {
        m_ports[i].stop();
    }

    if (m_mark_for_shutdown != SHUTDOWN_TEST_ENDED) {
        /* we should stop latency and exit to stop agents */
        Delete();
        utl_termio_reset();
        exit(-1);
    }
}


int CGlobalTRex::run_in_master() {

    //rte_thread_setname(pthread_self(), "TRex Control");

    if ( get_is_stateless() ) {
        m_trex_stateless->launch_control_plane();
    }

    /* exception and scope safe */
    std::unique_lock<std::mutex> cp_lock(m_cp_lock);

    uint32_t slow_path_counter = 0;

    const int FASTPATH_DELAY_MS = 10;
    const int SLOWPATH_DELAY_MS = 500;

    m_monitor.create("master", 2);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    TrexWatchDog::getInstance().start();

    while (!is_marked_for_shutdown()) {

        /* fast path */
        handle_fast_path();

        /* slow path */
        if (slow_path_counter >= SLOWPATH_DELAY_MS) {
            handle_slow_path();
            slow_path_counter = 0;
        }


        cp_lock.unlock();
        delay(FASTPATH_DELAY_MS);
        slow_path_counter += FASTPATH_DELAY_MS;
        cp_lock.lock();

        m_monitor.tickle();
    }

    /* on exit release the lock */
    cp_lock.unlock();

    /* shutdown everything gracefully */
    shutdown();

    return (0);
}



int CGlobalTRex::run_in_rx_core(void){

    rte_thread_setname(pthread_self(), "TRex RX");

    if (get_is_stateless()) {
        m_sl_rx_running = true;
        m_rx_sl.start();
        m_sl_rx_running = false;
    } else {
        if ( CGlobalInfo::m_options.is_rx_enabled() ){
            m_sl_rx_running = false;
            m_mg.start(0, true);
        }
    }

    return (0);
}

int CGlobalTRex::run_in_core(virtual_thread_id_t virt_core_id){
    std::stringstream ss;

    ss << "Trex DP core " << int(virt_core_id);
    rte_thread_setname(pthread_self(), ss.str().c_str());

    CPreviewMode *lp=&CGlobalInfo::m_options.preview;
    if ( lp->getSingleCore() &&
         (virt_core_id==2 ) &&
         (lp-> getCores() ==1) ){
        printf(" bypass this core \n");
        m_signal[virt_core_id]=1;
        return (0);
    }


    assert(m_fl_was_init);
    CFlowGenListPerThread   * lpt;

    lpt = m_fl.m_threads_info[virt_core_id-1];

    /* register a watchdog handle on current core */
    lpt->m_monitor.create(ss.str(), 1);
    TrexWatchDog::getInstance().register_monitor(&lpt->m_monitor);

    if (get_is_stateless()) {
        lpt->start_stateless_daemon(*lp);
    }else{
        lpt->start_generate_stateful(CGlobalInfo::m_options.out_file,*lp);
    }

    /* done - remove this from the watchdog (we might wait on join for a long time) */
    lpt->m_monitor.disable();

    m_signal[virt_core_id]=1;
    return (0);
}


int CGlobalTRex::stop_master(){

    delay(1000);
    fprintf(stdout," ==================\n");
    fprintf(stdout," interface sum \n");
    fprintf(stdout," ==================\n");
    dump_stats(stdout,CGlobalStats::dmpSTANDARD);
    fprintf(stdout," ==================\n");
    fprintf(stdout," \n\n");

    fprintf(stdout," ==================\n");
    fprintf(stdout," interface sum \n");
    fprintf(stdout," ==================\n");

    CFlowGenListPerThread   * lpt;
    uint64_t total_tx_rx_check=0;

    int i;
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        CCoreEthIF * erf_vif = m_cores_vif[i+1];

        erf_vif->DumpCoreStats(stdout);
        erf_vif->DumpIfStats(stdout);
        total_tx_rx_check+=erf_vif->m_stats[CLIENT_SIDE].m_tx_rx_check_pkt+
            erf_vif->m_stats[SERVER_SIDE].m_tx_rx_check_pkt;
    }

    fprintf(stdout," ==================\n");
    fprintf(stdout," generators \n");
    fprintf(stdout," ==================\n");
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        lpt->m_node_gen.DumpHist(stdout);
        lpt->DumpStats(stdout);
    }
    if ( CGlobalInfo::m_options.is_latency_enabled() ){
        fprintf(stdout," ==================\n");
        fprintf(stdout," latency \n");
        fprintf(stdout," ==================\n");
        m_mg.DumpShort(stdout);
        m_mg.Dump(stdout);
        m_mg.DumpShortRxCheck(stdout);
        m_mg.DumpRxCheck(stdout);
        m_mg.DumpRxCheckVerification(stdout,total_tx_rx_check);
    }

    dump_stats(stdout,CGlobalStats::dmpSTANDARD);
    dump_post_test_stats(stdout);

    return (0);
}

bool CGlobalTRex::is_all_cores_finished() {
    int i;
    for (i=0; i<get_cores_tx(); i++) {
        if ( m_signal[i+1]==0){
            return false;
        }
    }
    if (m_sl_rx_running)
        return false;

    return true;
}


int CGlobalTRex::start_master_stateless(){
    int i;
    for (i=0; i<BP_MAX_CORES; i++) {
        m_signal[i]=0;
    }
    m_fl.Create();
    m_expected_pps = 0;
    m_expected_cps = 0;
    m_expected_bps = 0;

    m_fl.generate_p_thread_info(get_cores_tx());
    CFlowGenListPerThread   * lpt;

    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        CVirtualIF * erf_vif = m_cores_vif[i+1];
        lpt->set_vif(erf_vif);
        lpt->m_node_gen.m_socket_id =m_cores_vif[i+1]->get_socket_id();
    }
    m_fl_was_init=true;

    return (0);
}

int CGlobalTRex::start_master_statefull() {
    int i;
    for (i=0; i<BP_MAX_CORES; i++) {
        m_signal[i]=0;
    }

    m_fl.Create();
    m_fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,get_cores_tx());

    /* client config */
    if (CGlobalInfo::m_options.client_cfg_file != "") {
        try {
            m_fl.load_client_config_file(CGlobalInfo::m_options.client_cfg_file);
        } catch (const std::runtime_error &e) {
            std::cout << "\n*** " << e.what() << "\n\n";
            exit(-1);
        }
        CGlobalInfo::m_options.preview.set_client_cfg_enable(true);
        m_fl.set_client_config_tuple_gen_info(&m_fl.m_yaml_info.m_tuple_gen);
        pre_test();
    }

    /* verify options */
    try {
        CGlobalInfo::m_options.verify();
    } catch (const std::runtime_error &e) {
        std::cout << "\n*** " << e.what() << "\n\n";
        exit(-1);
    }

    m_expected_pps = m_fl.get_total_pps();
    m_expected_cps = 1000.0*m_fl.get_total_kcps();
    m_expected_bps = m_fl.get_total_tx_bps();
    if ( m_fl.get_total_repeat_flows() > 2000) {
        /* disable flows cache */
        CGlobalInfo::m_options.preview.setDisableMbufCache(true);
    }

    CTupleGenYamlInfo * tg=&m_fl.m_yaml_info.m_tuple_gen;

    m_mg.set_ip( tg->m_client_pool[0].get_ip_start(),
                 tg->m_server_pool[0].get_ip_start(),
                 tg->m_client_pool[0].getDualMask()
                 );

    if (  CGlobalInfo::m_options.preview.getVMode() >0 ) {
        m_fl.DumpCsv(stdout);
        for (i=0; i<100; i++) {
            fprintf(stdout,"\n");
        }
        fflush(stdout);
    }

    m_fl.generate_p_thread_info(get_cores_tx());
    CFlowGenListPerThread   * lpt;

    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        //CNullIF * erf_vif = new CNullIF();
        CVirtualIF * erf_vif = m_cores_vif[i+1];
        lpt->set_vif(erf_vif);
        /* socket id */
        lpt->m_node_gen.m_socket_id =m_cores_vif[i+1]->get_socket_id();

    }
    m_fl_was_init=true;

    return (0);
}


////////////////////////////////////////////
static CGlobalTRex g_trex;


void CPhyEthIF::configure_rss_redirect_table(uint16_t numer_of_queues,
                                             uint16_t skip_queue){

                                                                                                                            
     struct rte_eth_dev_info dev_info;                                                                                 
                                                                                                                       
     rte_eth_dev_info_get(m_port_id,&dev_info); 
     assert(dev_info.reta_size>0);

     int reta_conf_size = 
          std::max(1, dev_info.reta_size / RTE_RETA_GROUP_SIZE); 

     struct rte_eth_rss_reta_entry64 reta_conf[reta_conf_size];  

     rte_eth_dev_rss_reta_query(m_port_id,&reta_conf[0],dev_info.reta_size);                                              

     int i,j;
     
     for (j=0; j<reta_conf_size; j++) {
         uint16_t skip=0;
         reta_conf[j].mask = ~0ULL; 
         for (i=0; i<RTE_RETA_GROUP_SIZE; i++) {
             uint16_t q;
             while (true) {
                 q=(i+skip)%numer_of_queues;
                 if (q!=skip_queue) {
                     break;
                 }
                 skip+=1;
             }
             reta_conf[j].reta[i]=q;
           //  printf(" %d %d %d \n",j,i,q);
         }
     }
     rte_eth_dev_rss_reta_update(m_port_id,&reta_conf[0],dev_info.reta_size);                                             

     rte_eth_dev_rss_reta_query(m_port_id,&reta_conf[0],dev_info.reta_size);                                              

     #if 0
     /* verification */
     for (j=0; j<reta_conf_size; j++) {
         for (i=0; i<RTE_RETA_GROUP_SIZE; i++) {
             printf(" R  %d %d %d \n",j,i,reta_conf[j].reta[i]);
         }
     }
     #endif

}


void CPhyEthIF::update_counters() {
    get_ex_drv()->get_extended_stats(this, &m_stats);
    CRXCoreIgnoreStat ign_stats;
    
    if (get_is_stateless()) {
        g_trex.m_rx_sl.get_ignore_stats(m_port_id, ign_stats, true);
    } else {
        g_trex.m_mg.get_ignore_stats(m_port_id, ign_stats, true);
    }
    
    m_stats.obytes -= ign_stats.get_tx_bytes();
    m_stats.opackets -= ign_stats.get_tx_pkts();
    m_ignore_stats.opackets += ign_stats.get_tx_pkts();
    m_ignore_stats.obytes += ign_stats.get_tx_bytes();
    m_ignore_stats.m_tx_arp += ign_stats.get_tx_arp();

    m_last_tx_rate      =  m_bw_tx.add(m_stats.obytes);
    m_last_rx_rate      =  m_bw_rx.add(m_stats.ibytes);
    m_last_tx_pps       =  m_pps_tx.add(m_stats.opackets);
    m_last_rx_pps       =  m_pps_rx.add(m_stats.ipackets);
}

bool CPhyEthIF::Create(uint8_t portid) {
    m_port_id      = portid;
    m_last_rx_rate = 0.0;
    m_last_tx_rate = 0.0;
    m_last_tx_pps  = 0.0;
    m_port_attr    = g_trex.m_drv->create_port_attr(portid);

    /* set src MAC addr */
    uint8_t empty_mac[ETHER_ADDR_LEN] = {0,0,0,0,0,0};
    if (! memcmp( CGlobalInfo::m_options.m_mac_addr[m_port_id].u.m_mac.src, empty_mac, ETHER_ADDR_LEN)) {
        rte_eth_macaddr_get(m_port_id,
                            (struct ether_addr *)&CGlobalInfo::m_options.m_mac_addr[m_port_id].u.m_mac.src);
    }

    return true;
}

const std::vector<std::pair<uint8_t, uint8_t>> &
CPhyEthIF::get_core_list() {

    /* lazy find */
    if (m_core_id_list.size() == 0) {

        for (uint8_t core_id = 0; core_id < g_trex.get_cores_tx(); core_id++) {

            /* iterate over all the directions*/
            for (uint8_t dir = 0 ; dir < CS_NUM; dir++) {
                if (g_trex.m_cores_vif[core_id + 1]->get_ports()[dir].m_port->get_port_id() == m_port_id) {
                    m_core_id_list.push_back(std::make_pair(core_id, dir));
                }
            }
        }
    }

    return m_core_id_list;

}

int CPhyEthIF::reset_hw_flow_stats() {
    if (get_ex_drv()->hw_rx_stat_supported()) {
        get_ex_drv()->reset_rx_stats(this, m_stats.m_fdir_prev_pkts, 0, MAX_FLOW_STATS);
    } else {
        g_trex.m_rx_sl.reset_rx_stats(get_port_id());
    }
    return 0;
}

// get/reset flow director counters
// return 0 if OK. -1 if operation not supported.
// rx_stats, tx_stats - arrays of len max - min + 1. Returning rx, tx updated absolute values.
// min, max - minimum, maximum counters range to get
// reset - If true, need to reset counter value after reading
int CPhyEthIF::get_flow_stats(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset) {
    uint32_t diff_pkts[MAX_FLOW_STATS];
    uint32_t diff_bytes[MAX_FLOW_STATS];
    bool hw_rx_stat_supported = get_ex_drv()->hw_rx_stat_supported();

    if (hw_rx_stat_supported) {
        if (get_ex_drv()->get_rx_stats(this, diff_pkts, m_stats.m_fdir_prev_pkts
                                       , diff_bytes, m_stats.m_fdir_prev_bytes, min, max) < 0) {
            return -1;
        }
    } else {
        g_trex.m_rx_sl.get_rx_stats(get_port_id(), rx_stats, min, max, reset, TrexPlatformApi::IF_STAT_IPV4_ID);
    }

    for (int i = min; i <= max; i++) {
        if ( reset ) {
            // return value so far, and reset
            if (hw_rx_stat_supported) {
                if (rx_stats != NULL) {
                    rx_stats[i - min].set_pkts(m_stats.m_rx_per_flow_pkts[i] + diff_pkts[i]);
                    rx_stats[i - min].set_bytes(m_stats.m_rx_per_flow_bytes[i] + diff_bytes[i]);
                }
                m_stats.m_rx_per_flow_pkts[i] = 0;
                m_stats.m_rx_per_flow_bytes[i] = 0;
                get_ex_drv()->reset_rx_stats(this, &m_stats.m_fdir_prev_pkts[i], i, 1);

            }
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.clear_flow_tx_stats(m_port_id, i, false);
            }
        } else {
            if (hw_rx_stat_supported) {
                m_stats.m_rx_per_flow_pkts[i] += diff_pkts[i];
                m_stats.m_rx_per_flow_bytes[i] += diff_bytes[i];
                if (rx_stats != NULL) {
                    rx_stats[i - min].set_pkts(m_stats.m_rx_per_flow_pkts[i]);
                    rx_stats[i - min].set_bytes(m_stats.m_rx_per_flow_bytes[i]);
                }
            }
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.get_flow_tx_stats(m_port_id, i);
            }
        }
    }

    return 0;
}

int CPhyEthIF::get_flow_stats_payload(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset) {
    g_trex.m_rx_sl.get_rx_stats(get_port_id(), rx_stats, min, max, reset, TrexPlatformApi::IF_STAT_PAYLOAD);
    for (int i = min; i <= max; i++) {
        if ( reset ) {
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.clear_flow_tx_stats(m_port_id, i + MAX_FLOW_STATS, true);
            }
        } else {
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.get_flow_tx_stats(m_port_id, i + MAX_FLOW_STATS);
            }
        }
    }

    return 0;
}

// If needed, send packets to rx core for processing.
// This is relevant only in VM case, where we receive packets to the working DP core (only 1 DP core in this case)
bool CCoreEthIF::process_rx_pkt(pkt_dir_t dir, rte_mbuf_t * m) {
    CFlowStatParser parser;
    uint32_t ip_id;

    if (parser.parse(rte_pktmbuf_mtod(m, uint8_t*), rte_pktmbuf_pkt_len(m)) != 0) {
        return false;
    }
    bool send=false;

    // e1000 on ESXI hands us the packet with the ethernet FCS
    if (parser.get_pkt_size() < rte_pktmbuf_pkt_len(m)) {
        rte_pktmbuf_trim(m, rte_pktmbuf_pkt_len(m) - parser.get_pkt_size());
    }

    if ( get_is_stateless() ) {
        // In stateless RX, we only care about flow stat packets
        if ((parser.get_ip_id(ip_id) == 0) && ((ip_id & 0xff00) == IP_ID_RESERVE_BASE)) {
            send = true;
        }
    } else {
        CLatencyPktMode *c_l_pkt_mode = g_trex.m_mg.c_l_pkt_mode;
        bool is_lateancy_pkt =  c_l_pkt_mode->IsLatencyPkt((IPHeader *)parser.get_l4()) &
            CCPortLatency::IsLatencyPkt(parser.get_l4() + c_l_pkt_mode->l4_header_len());

        if (is_lateancy_pkt) {
            send = true;
        } else {
            if ( get_is_rx_filter_enable() ) {
                uint8_t max_ttl = 0xff - get_rx_check_hops();
                uint8_t pkt_ttl = parser.get_ttl();
                if ( (pkt_ttl==max_ttl) || (pkt_ttl==(max_ttl-1) ) ) {
                    send=true;
                }
            }
        }
    }


    if (send) {
        CGenNodeLatencyPktInfo * node=(CGenNodeLatencyPktInfo * )CGlobalInfo::create_node();
        if ( node ) {
            node->m_msg_type = CGenNodeMsgBase::LATENCY_PKT;
            node->m_dir      = dir;
            node->m_latency_offset = 0xdead;
            node->m_pkt      = m;
            if ( m_ring_to_rx->Enqueue((CGenNode*)node)==0 ){
            }else{
                CGlobalInfo::free_node((CGenNode *)node);
                send=false;
            }

#ifdef LATENCY_QUEUE_TRACE_
            printf("rx to cp --\n");
            rte_pktmbuf_dump(stdout,m, rte_pktmbuf_pkt_len(m));
#endif
        }else{
            send=false;
        }
    }
    return (send);
}

TrexStateless * get_stateless_obj() {
    return g_trex.m_trex_stateless;
}

CRxCoreStateless * get_rx_sl_core_obj() {
    return &g_trex.m_rx_sl;
}

static int latency_one_lcore(__attribute__((unused)) void *dummy)
{
    CPlatformSocketInfo * lpsock=&CGlobalInfo::m_socket;
    physical_thread_id_t  phy_id =rte_lcore_id();

    if ( lpsock->thread_phy_is_rx(phy_id) ) {
        g_trex.run_in_rx_core();
    }else{

        if ( lpsock->thread_phy_is_master( phy_id ) ) {
            g_trex.run_in_master();
            delay(1);
        }else{
            delay((uint32_t)(1000.0*CGlobalInfo::m_options.m_duration));
            /* this core has stopped */
            g_trex.m_signal[ lpsock->thread_phy_to_virt( phy_id ) ]=1;
        }
    }
    return 0;
}



static int slave_one_lcore(__attribute__((unused)) void *dummy)
{
    CPlatformSocketInfo * lpsock=&CGlobalInfo::m_socket;
    physical_thread_id_t  phy_id =rte_lcore_id();

    if ( lpsock->thread_phy_is_rx(phy_id) ) {
        g_trex.run_in_rx_core();
    }else{
        if ( lpsock->thread_phy_is_master( phy_id ) ) {
            g_trex.run_in_master();
            delay(1);
        }else{
            g_trex.run_in_core( lpsock->thread_phy_to_virt( phy_id ) );
        }
    }
    return 0;
}



uint32_t get_cores_mask(uint32_t cores,int offset){
    int i;

    uint32_t res=1;

    uint32_t mask=(1<<(offset+1));
    for (i=0; i<(cores-1); i++) {
        res |= mask ;
        mask = mask <<1;
    }
    return (res);
}


static char *g_exe_name;
const char *get_exe_name() {
    return g_exe_name;
}


int main(int argc , char * argv[]){
    g_exe_name = argv[0];

    return ( main_test(argc , argv));
}


int update_global_info_from_platform_file(){

    CPlatformYamlInfo *cg=&global_platform_cfg_info;

    CGlobalInfo::m_socket.Create(&cg->m_platform);


    if (!cg->m_info_exist) {
        /* nothing to do ! */
        return 0;
    }

    CGlobalInfo::m_options.prefix =cg->m_prefix;
    CGlobalInfo::m_options.preview.setCores(cg->m_thread_per_dual_if);

    if ( cg->m_port_limit_exist ){
        CGlobalInfo::m_options.m_expected_portd =cg->m_port_limit;
    }

    if ( cg->m_enable_zmq_pub_exist ){
        CGlobalInfo::m_options.preview.set_zmq_publish_enable(cg->m_enable_zmq_pub);
        CGlobalInfo::m_options.m_zmq_port = cg->m_zmq_pub_port;
    }
    if ( cg->m_telnet_exist ){
        CGlobalInfo::m_options.m_telnet_port = cg->m_telnet_port;
    }

    if ( cg->m_mac_info_exist ){
        int i;
        /* cop the file info */

        int port_size=cg->m_mac_info.size();

        if ( port_size > TREX_MAX_PORTS ){
            port_size = TREX_MAX_PORTS;
        }
        for (i=0; i<port_size; i++){
            cg->m_mac_info[i].copy_src(( char *)CGlobalInfo::m_options.m_mac_addr[i].u.m_mac.src)   ;
            cg->m_mac_info[i].copy_dest(( char *)CGlobalInfo::m_options.m_mac_addr[i].u.m_mac.dest)  ;
            CGlobalInfo::m_options.m_ip_cfg[i].set_def_gw(cg->m_mac_info[i].get_def_gw());
            CGlobalInfo::m_options.m_ip_cfg[i].set_ip(cg->m_mac_info[i].get_ip());
            CGlobalInfo::m_options.m_ip_cfg[i].set_mask(cg->m_mac_info[i].get_mask());
            CGlobalInfo::m_options.m_ip_cfg[i].set_vlan(cg->m_mac_info[i].get_vlan());
        }
    }

    /* mul by interface type */
    float mul=1.0;
    if (cg->m_port_bandwidth_gb<10) {
        cg->m_port_bandwidth_gb=10.0;
    }

    mul = mul*(float)cg->m_port_bandwidth_gb/10.0;
    mul= mul * (float)cg->m_port_limit/2.0;

    mul= mul * CGlobalInfo::m_options.m_mbuf_factor;


    CGlobalInfo::m_memory_cfg.set_pool_cache_size(RTE_MEMPOOL_CACHE_MAX_SIZE);

    CGlobalInfo::m_memory_cfg.set_number_of_dp_cors(
                                                    CGlobalInfo::m_options.get_number_of_dp_cores_needed() );

    CGlobalInfo::m_memory_cfg.set(cg->m_memory,mul);
    return (0);
}

extern "C" int eal_cpu_detected(unsigned lcore_id);
// return mask representing available cores
int core_mask_calc() {
    uint32_t mask = 0;
    int lcore_id;

    for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
        if (eal_cpu_detected(lcore_id)) {
            mask |= (1 << lcore_id);
        }
    }

    return mask;
}

// Return number of set bits in i
uint32_t num_set_bits(uint32_t i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

// sanity check if the cores we want to use really exist
int core_mask_sanity(uint32_t wanted_core_mask) {
    uint32_t calc_core_mask = core_mask_calc();
    uint32_t wanted_core_num, calc_core_num;

    wanted_core_num = num_set_bits(wanted_core_mask);
    calc_core_num = num_set_bits(calc_core_mask);

    if (calc_core_num == 1) {
        printf ("Error: You have only 1 core available. Minimum configuration requires 2 cores\n");
        printf("        If you are running on VM, consider adding more cores if possible\n");
        return -1;
    }
    if (wanted_core_num > calc_core_num) {
        printf("Error: You have %d threads available, but you asked for %d threads.\n", calc_core_num, wanted_core_num);
        printf("       Calculation is: -c <num>(%d) * dual ports (%d) + 1 master thread %s"
               , CGlobalInfo::m_options.preview.getCores(), CGlobalInfo::m_options.get_expected_dual_ports()
               , get_is_rx_thread_enabled() ? "+1 latency thread (because of -l flag)\n" : "\n");
        if (CGlobalInfo::m_options.preview.getCores() > 1)
            printf("       Maybe try smaller -c <num>.\n");
        printf("       If you are running on VM, consider adding more cores if possible\n");
        return -1;
    }

    if (wanted_core_mask != (wanted_core_mask & calc_core_mask)) {
        printf ("Serious error: Something is wrong with the hardware. Wanted core mask is %x. Existing core mask is %x\n", wanted_core_mask, calc_core_mask);
        return -1;
    }

    return 0;
}

int  update_dpdk_args(void){

    CPlatformSocketInfo * lpsock=&CGlobalInfo::m_socket;
    CParserOption * lpop= &CGlobalInfo::m_options;

    lpsock->set_rx_thread_is_enabled(get_is_rx_thread_enabled());
    lpsock->set_number_of_threads_per_ports(lpop->preview.getCores() );
    lpsock->set_number_of_dual_ports(lpop->get_expected_dual_ports());
    if ( !lpsock->sanity_check() ){
        printf(" ERROR in configuration file \n");
        return (-1);
    }

    if ( CGlobalInfo::m_options.preview.getVMode() > 0  ) {
        lpsock->dump(stdout);
    }

    snprintf(global_cores_str, sizeof(global_cores_str), "0x%llx" ,(unsigned long long)lpsock->get_cores_mask());
    if (core_mask_sanity(strtol(global_cores_str, NULL, 16)) < 0) {
        return -1;
    }

    /* set the DPDK options */
    global_dpdk_args_num = 0;

    global_dpdk_args[global_dpdk_args_num++]=(char *)"xx";
    global_dpdk_args[global_dpdk_args_num++]=(char *)"-c";
    global_dpdk_args[global_dpdk_args_num++]=(char *)global_cores_str;
    global_dpdk_args[global_dpdk_args_num++]=(char *)"-n";
    global_dpdk_args[global_dpdk_args_num++]=(char *)"4";

    if ( CGlobalInfo::m_options.preview.getVMode() == 0  ) {
        global_dpdk_args[global_dpdk_args_num++]=(char *)"--log-level";
        snprintf(global_loglevel_str, sizeof(global_loglevel_str), "%d", 4);
        global_dpdk_args[global_dpdk_args_num++]=(char *)global_loglevel_str;
    }else{
        global_dpdk_args[global_dpdk_args_num++]=(char *)"--log-level";
        snprintf(global_loglevel_str, sizeof(global_loglevel_str), "%d", CGlobalInfo::m_options.preview.getVMode()+1);
        global_dpdk_args[global_dpdk_args_num++]=(char *)global_loglevel_str;
    }

    global_dpdk_args[global_dpdk_args_num++] = (char *)"--master-lcore";

    snprintf(global_master_id_str, sizeof(global_master_id_str), "%u", lpsock->get_master_phy_id());
    global_dpdk_args[global_dpdk_args_num++] = global_master_id_str;

    /* add white list */
    if (lpop->m_run_mode == CParserOption::RUN_MODE_DUMP_INFO and lpop->dump_interfaces.size()) {
        for (int i=0; i<(int)lpop->dump_interfaces.size(); i++) {
            global_dpdk_args[global_dpdk_args_num++]=(char *)"-w";
            global_dpdk_args[global_dpdk_args_num++]=(char *)lpop->dump_interfaces[i].c_str();
        }
    }
    else {
        for (int i=0; i<(int)global_platform_cfg_info.m_if_list.size(); i++) {
            global_dpdk_args[global_dpdk_args_num++]=(char *)"-w";
            global_dpdk_args[global_dpdk_args_num++]=(char *)global_platform_cfg_info.m_if_list[i].c_str();
        }
    }



    if ( lpop->prefix.length()  ){
        global_dpdk_args[global_dpdk_args_num++]=(char *)"--file-prefix";
        snprintf(global_prefix_str, sizeof(global_prefix_str), "%s", lpop->prefix.c_str());
        global_dpdk_args[global_dpdk_args_num++]=(char *)global_prefix_str;
        global_dpdk_args[global_dpdk_args_num++]=(char *)"-m";
        if (global_platform_cfg_info.m_limit_memory.length()) {
            global_dpdk_args[global_dpdk_args_num++]=(char *)global_platform_cfg_info.m_limit_memory.c_str();
        }else{
            global_dpdk_args[global_dpdk_args_num++]=(char *)"1024";
        }
    }


    if ( CGlobalInfo::m_options.preview.getVMode() > 0  ) {
        printf("args \n");
        int i;
        for (i=0; i<global_dpdk_args_num; i++) {
            printf(" %s \n",global_dpdk_args[i]);
        }
    }
    return (0);
}


int sim_load_list_of_cap_files(CParserOption * op){

    CFlowGenList fl;
    fl.Create();
    fl.load_from_yaml(op->cfg_file,1);
    if ( op->preview.getVMode() >0 ) {
        fl.DumpCsv(stdout);
    }
    uint32_t start=    os_get_time_msec();

    CErfIF erf_vif;

    fl.generate_p_thread_info(1);
    CFlowGenListPerThread   * lpt;
    lpt=fl.m_threads_info[0];
    lpt->set_vif(&erf_vif);

    if ( (op->preview.getVMode() >1)  || op->preview.getFileWrite() ) {
        lpt->start_generate_stateful(op->out_file,op->preview);
    }

    lpt->m_node_gen.DumpHist(stdout);

    uint32_t stop=    os_get_time_msec();
    printf(" d time = %ul %ul \n",stop-start,os_get_time_freq());
    fl.Delete();
    return (0);
}

void dump_interfaces_info() {
    printf("Showing interfaces info.\n");
    uint8_t m_max_ports = rte_eth_dev_count();
    struct ether_addr mac_addr;
    char mac_str[ETHER_ADDR_FMT_SIZE];
    struct rte_pci_addr pci_addr;

    for (uint8_t port_id=0; port_id<m_max_ports; port_id++) {
        // PCI, MAC and Driver
        pci_addr = rte_eth_devices[port_id].pci_dev->addr;
        rte_eth_macaddr_get(port_id, &mac_addr);
        ether_format_addr(mac_str, sizeof mac_str, &mac_addr);
        printf("PCI: %04x:%02x:%02x.%d - MAC: %s - Driver: %s\n",
            pci_addr.domain, pci_addr.bus, pci_addr.devid, pci_addr.function, mac_str,
            rte_eth_devices[port_id].pci_dev->driver->name);
    }
}

int main_test(int argc , char * argv[]){


    utl_termio_init();

    int ret;
    unsigned lcore_id;
    printf("Starting  TRex %s please wait  ... \n",VERSION_BUILD_NUM);

    CGlobalInfo::m_options.preview.clean();

    if ( parse_options_wrapper(argc, argv, &CGlobalInfo::m_options,true ) != 0){
        exit(-1);
    }

    /* enable core dump if requested */
    if (CGlobalInfo::m_options.preview.getCoreDumpEnable()) {
        utl_set_coredump_size(-1);
    }
    else {
        utl_set_coredump_size(0);
    }


    update_global_info_from_platform_file();

    /* It is not a mistake. Give the user higher priorty over the configuration file */
    if (parse_options_wrapper(argc, argv, &CGlobalInfo::m_options ,false) != 0) {
        exit(-1);
    }


    if ( CGlobalInfo::m_options.preview.getVMode() > 0){
        CGlobalInfo::m_options.dump(stdout);
        CGlobalInfo::m_memory_cfg.Dump(stdout);
    }


    if (update_dpdk_args() < 0) {
        return -1;
    }

    CParserOption * po=&CGlobalInfo::m_options;


    if ( CGlobalInfo::m_options.preview.getVMode() == 0  ) {
        rte_set_log_level(1);

    }
    uid_t uid;
    uid = geteuid ();
    if ( uid != 0 ) {
        printf("ERROR you must run with superuser priviliges \n");
        printf("User id   : %d \n",uid);
        printf("try 'sudo' %s \n",argv[0]);
        return (-1);
    }

    /* set affinity to the master core as default */
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(CGlobalInfo::m_socket.get_master_phy_id(), &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

    ret = rte_eal_init(global_dpdk_args_num, (char **)global_dpdk_args);
    if (ret < 0){
        printf(" You might need to run ./trex-cfg  once  \n");
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    }
    if (CGlobalInfo::m_options.m_run_mode == CParserOption::RUN_MODE_DUMP_INFO) {
        dump_interfaces_info();
        exit(0);
    }
    reorder_dpdk_ports();
    time_init();

    /* check if we are in simulation mode */
    if ( CGlobalInfo::m_options.out_file != "" ){
        printf(" t-rex simulation mode into %s \n",CGlobalInfo::m_options.out_file.c_str());
        return ( sim_load_list_of_cap_files(&CGlobalInfo::m_options) );
    }

    if ( !g_trex.Create() ){
        exit(1);
    }

    if (po->preview.get_is_rx_check_enable() &&  (po->m_rx_check_sample< get_min_sample_rate()) ) {
        po->m_rx_check_sample = get_min_sample_rate();
        printf("Warning:rx check sample rate should not be lower than %d. Setting it to %d\n",get_min_sample_rate(),get_min_sample_rate());
    }

    /* set dump mode */
    g_trex.m_io_modes.set_mode((CTrexGlobalIoMode::CliDumpMode)CGlobalInfo::m_options.m_io_mode);

    /* disable WD if needed */
    bool wd_enable = (CGlobalInfo::m_options.preview.getWDDisable() ? false : true);
    TrexWatchDog::getInstance().init(wd_enable);

    g_trex.m_sl_rx_running = false;
    if ( get_is_stateless() ) {
        g_trex.start_master_stateless();

    }else{
        g_trex.start_master_statefull();
    }

    // For unit testing of HW rules and queues configuration. Just send some packets and exit.
    if (CGlobalInfo::m_options.m_debug_pkt_proto != 0) {
        CTrexDebug debug = CTrexDebug(g_trex.m_ports, g_trex.m_max_ports);
        int ret;

        if (CGlobalInfo::m_options.m_debug_pkt_proto == D_PKT_TYPE_HW_TOGGLE_TEST) {
            // Unit test: toggle many times between receive all and stateless/stateful modes,
            // to test resiliency of add/delete fdir filters
            printf("Starting receive all/normal mode toggle unit test\n");
            for (int i = 0; i < 100; i++) {
                for (int port_id = 0; port_id < g_trex.m_max_ports; port_id++) {
                    CPhyEthIF *pif = &g_trex.m_ports[port_id];
                    CTRexExtendedDriverDb::Ins()->get_drv()->set_rcv_all(pif, true);
                }
                ret = debug.test_send(D_PKT_TYPE_HW_VERIFY_RCV_ALL);
                if (ret != 0) {
                    printf("Iteration %d: Receive all mode failed\n", i);
                    exit(ret);
                }

                for (int port_id = 0; port_id < g_trex.m_max_ports; port_id++) {
                    CPhyEthIF *pif = &g_trex.m_ports[port_id];
                    CTRexExtendedDriverDb::Ins()->get_drv()->configure_rx_filter_rules(pif);
                }

                ret = debug.test_send(D_PKT_TYPE_HW_VERIFY);
                if (ret != 0) {
                    printf("Iteration %d: Normal mode failed\n", i);
                    exit(ret);
                }

                printf("Iteration %d OK\n", i);
            }
            exit(0);
        } else {
            if (CGlobalInfo::m_options.m_debug_pkt_proto == D_PKT_TYPE_HW_VERIFY_RCV_ALL) {
                for (int port_id = 0; port_id < g_trex.m_max_ports; port_id++) {
                    CPhyEthIF *pif = &g_trex.m_ports[port_id];
                    CTRexExtendedDriverDb::Ins()->get_drv()->set_rcv_all(pif, true);
                }
            }
            ret = debug.test_send(CGlobalInfo::m_options.m_debug_pkt_proto);
            exit(ret);
        }
    }

    // in case of client config, we already run pretest
    if (! CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {
        g_trex.pre_test();
    }

    // after doing all needed ARP resolution, we need to flush queues, and stop our drop queue
    g_trex.ixgbe_rx_queue_flush();
    for (int i = 0; i < g_trex.m_max_ports; i++) {
        CPhyEthIF *_if = &g_trex.m_ports[i];
        _if->stop_rx_drop_queue();
    }

    if ( CGlobalInfo::m_options.is_latency_enabled()
         && (CGlobalInfo::m_options.m_latency_prev > 0)) {
        uint32_t pkts = CGlobalInfo::m_options.m_latency_prev *
            CGlobalInfo::m_options.m_latency_rate;
        printf("Starting warm up phase for %d sec\n",CGlobalInfo::m_options.m_latency_prev);
        g_trex.m_mg.start(pkts, NULL);
        delay(CGlobalInfo::m_options.m_latency_prev* 1000);
        printf("Finished \n");
        g_trex.m_mg.reset();
    }

    if ( CGlobalInfo::m_options.preview.getOnlyLatency() ){
        rte_eal_mp_remote_launch(latency_one_lcore, NULL, CALL_MASTER);
        RTE_LCORE_FOREACH_SLAVE(lcore_id) {
            if (rte_eal_wait_lcore(lcore_id) < 0)
                return -1;
        }
        g_trex.stop_master();

        return (0);
    }

    if ( CGlobalInfo::m_options.preview.getSingleCore() ) {
        g_trex.run_in_core(1);
        g_trex.stop_master();
        return (0);
    }

    rte_eal_mp_remote_launch(slave_one_lcore, NULL, CALL_MASTER);
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0)
            return -1;
    }

    g_trex.stop_master();
    g_trex.Delete();
    utl_termio_reset();

    return (0);
}

void wait_x_sec(int sec) {
    int i;
    printf(" wait %d sec ", sec);
    fflush(stdout);
    for (i=0; i<sec; i++) {
        delay(1000);
        printf(".");
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
}

/*
Changes the order of rte_eth_devices array elements
to be consistent with our /etc/trex_cfg.yaml
*/
void reorder_dpdk_ports() {
    rte_eth_dev rte_eth_devices_temp[RTE_MAX_ETHPORTS];
    uint8_t m_port_map[RTE_MAX_ETHPORTS];
    struct rte_pci_addr addr;
    uint8_t port_id;

    // gather port relation information and save current array to temp
    for (int i=0; i<(int)global_platform_cfg_info.m_if_list.size(); i++) {
        memcpy(&rte_eth_devices_temp[i], &rte_eth_devices[i], sizeof rte_eth_devices[i]);
        if (eal_parse_pci_BDF(global_platform_cfg_info.m_if_list[i].c_str(), &addr) != 0 && eal_parse_pci_DomBDF(global_platform_cfg_info.m_if_list[i].c_str(), &addr) != 0) {
            printf("Failed mapping TRex port id to DPDK id: %d\n", i);
            exit(1);
        }
        rte_eth_dev_get_port_by_addr(&addr, &port_id);
        m_port_map[port_id] = i;
        // print the relation in verbose mode
        if ( CGlobalInfo::m_options.preview.getVMode() > 0){
            printf("TRex cfg port id: %d <-> DPDK port id: %d\n", i, port_id);
        }
    }

    // actual reorder
    for (int i=0; i<(int)global_platform_cfg_info.m_if_list.size(); i++) {
        memcpy(&rte_eth_devices[m_port_map[i]], &rte_eth_devices_temp[i], sizeof rte_eth_devices_temp[i]);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// driver section
//////////////////////////////////////////////////////////////////////////////////////////////
int CTRexExtendedDriverBase::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    uint8_t port_id=_if->get_rte_port_id();
    return (rte_eth_dev_rx_queue_stop(port_id, q_num));
}

int CTRexExtendedDriverBase::wait_for_stable_link() {
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
    return 0;
}

void CTRexExtendedDriverBase::wait_after_link_up() {
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
}

CFlowStatParser *CTRexExtendedDriverBase::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser();
    assert (parser);
    return parser;
}

// in 1G we need to wait if links became ready to soon
void CTRexExtendedDriverBase1G::wait_after_link_up(){
    wait_x_sec(6 + CGlobalInfo::m_options.m_wait_before_traffic);
}

int CTRexExtendedDriverBase1G::wait_for_stable_link(){
    wait_x_sec(9 + CGlobalInfo::m_options.m_wait_before_traffic);
    return(0);
}

void CTRexExtendedDriverBase1G::update_configuration(port_cfg_t * cfg){

    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
}

void CTRexExtendedDriverBase1G::update_global_config_fdir(port_cfg_t * cfg){
    // Configuration is done in configure_rx_filter_rules by writing to registers
}

#define E1000_RXDCTL_QUEUE_ENABLE	0x02000000
// e1000 driver does not support the generic stop/start queue API, so we need to implement ourselves
int CTRexExtendedDriverBase1G::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    uint32_t reg_val = _if->pci_reg_read( E1000_RXDCTL(q_num));
    reg_val &= ~E1000_RXDCTL_QUEUE_ENABLE;
    _if->pci_reg_write( E1000_RXDCTL(q_num), reg_val);
    return 0;
}

int CTRexExtendedDriverBase1G::configure_rx_filter_rules(CPhyEthIF * _if){
    if ( get_is_stateless() ) {
        return configure_rx_filter_rules_stateless(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }

    return 0;
}

int CTRexExtendedDriverBase1G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    uint16_t hops = get_rx_check_hops();
    uint16_t v4_hops = (hops << 8)&0xff00;
    uint8_t protocol;

    if (CGlobalInfo::m_options.m_l_pkt_mode == 0) {
        protocol = IPPROTO_SCTP;
    } else {
        protocol = IPPROTO_ICMP;
    }
    /* enable filter to pass packet to rx queue 1 */
    _if->pci_reg_write( E1000_IMIR(0), 0x00020000);
    _if->pci_reg_write( E1000_IMIREXT(0), 0x00081000);
    _if->pci_reg_write( E1000_TTQF(0),   protocol
                        | 0x00008100 /* enable */
                        | 0xE0010000 /* RX queue is 1 */
                        );


    /* 16  :   12 MAC , (2)0x0800,2      | DW0 , DW1
       6 bytes , TTL , PROTO     | DW2=0 , DW3=0x0000FF06
    */
    int i;
    // IPv4: bytes being compared are {TTL, Protocol}
    uint16_t ff_rules_v4[6]={
        (uint16_t)(0xFF06 - v4_hops),
        (uint16_t)(0xFE11 - v4_hops),
        (uint16_t)(0xFF11 - v4_hops),
        (uint16_t)(0xFE06 - v4_hops),
        (uint16_t)(0xFF01 - v4_hops),
        (uint16_t)(0xFE01 - v4_hops),
    }  ;
    // IPv6: bytes being compared are {NextHdr, HopLimit}
    uint16_t ff_rules_v6[2]={
        (uint16_t)(0x3CFF - hops),
        (uint16_t)(0x3CFE - hops),
    }  ;
    uint16_t *ff_rules;
    uint16_t num_rules;
    uint32_t mask=0;
    int  rule_id;

    if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
        ff_rules = &ff_rules_v6[0];
        num_rules = sizeof(ff_rules_v6)/sizeof(ff_rules_v6[0]);
    }else{
        ff_rules = &ff_rules_v4[0];
        num_rules = sizeof(ff_rules_v4)/sizeof(ff_rules_v4[0]);
    }

    clear_rx_filter_rules(_if);

    uint8_t len = 24;
    for (rule_id=0; rule_id<num_rules; rule_id++ ) {
        /* clear rule all */
        for (i=0; i<0xff; i+=4) {
            _if->pci_reg_write( (E1000_FHFT(rule_id)+i) , 0);
        }

        if (  CGlobalInfo::m_options.preview.get_vlan_mode_enable() ){
            len += 8;
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
                // IPv6 VLAN: NextHdr/HopLimit offset = 0x18
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+0) , PKT_NTOHS(ff_rules[rule_id]) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+8) , 0x03); /* MASK */
            }else{
                // IPv4 VLAN: TTL/Protocol offset = 0x1A
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+0) , (PKT_NTOHS(ff_rules[rule_id])<<16) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+8) , 0x0C); /* MASK */
            }
        }else{
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
                // IPv6: NextHdr/HopLimit offset = 0x14
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+4) , PKT_NTOHS(ff_rules[rule_id]) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+8) , 0x30); /* MASK */
            }else{
                // IPv4: TTL/Protocol offset = 0x16
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+4) , (PKT_NTOHS(ff_rules[rule_id])<<16) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+8) , 0xC0); /* MASK */
            }
        }

        // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
        _if->pci_reg_write( (E1000_FHFT(rule_id)+0xFC) , (1<<16) | (1<<8)  | len);

        mask |=(1<<rule_id);
    }

    /* enable all rules */
    _if->pci_reg_write(E1000_WUFC, (mask<<16) | (1<<14) );

    return (0);
}

// Sadly, DPDK has no support for i350 filters, so we need to implement by writing to registers.
int CTRexExtendedDriverBase1G::configure_rx_filter_rules_stateless(CPhyEthIF * _if) {
    /* enable filter to pass packet to rx queue 1 */
    _if->pci_reg_write( E1000_IMIR(0), 0x00020000);
    _if->pci_reg_write( E1000_IMIREXT(0), 0x00081000);

    uint8_t len = 24;
    uint32_t mask = 0;
    int rule_id;

    clear_rx_filter_rules(_if);

    rule_id = 0;
    mask |= 0x1 << rule_id;
    // filter for byte 18 of packet (msb of IP ID) should equal ff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)) ,  0x00ff0000);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x04); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000008);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    // same as 0, but with vlan. type should be vlan. Inside vlan, should be IP with lsb of IP ID equals 0xff
    rule_id = 1;
    mask |= 0x1 << rule_id;
    // filter for byte 22 of packet (msb of IP ID) should equal ff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 4) ,  0x00ff0000);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x40 | 0x03); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate VLAN.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000081);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // + bytes 16 + 17 (vlan type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) ) ,  0x00000008);
    // Was written together with IP ID filter
    // _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x03); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    rule_id = 2;
    mask |= 0x1 << rule_id;
    // ipv6 flow stat
    // filter for byte 16 of packet (part of flow label) should equal 0xff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)) ,  0x000000ff);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x01); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate IPv6.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x0000dd86);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    rule_id = 3;
    mask |= 0x1 << rule_id;
    // same as 2, with vlan. Type is vlan. Inside vlan, IPv6 with flow label second bits 4-11 equals 0xff
    // filter for byte 20 of packet (part of flow label) should equal 0xff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 4) ,  0x000000ff);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x10 | 0x03); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate VLAN.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000081);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // + bytes 16 + 17 (vlan type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) ) ,  0x0000dd86);
    // Was written together with flow label filter
    // _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x03); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    /* enable rules */
    _if->pci_reg_write(E1000_WUFC, (mask << 16) | (1 << 14) );

    return (0);
}

// clear registers of rules
void CTRexExtendedDriverBase1G::clear_rx_filter_rules(CPhyEthIF * _if) {
    for (int rule_id = 0 ; rule_id < 8; rule_id++) {
        for (int i = 0; i < 0xff; i += 4) {
            _if->pci_reg_write( (E1000_FHFT(rule_id) + i) , 0);
        }
    }
}

int CTRexExtendedDriverBase1G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    // byte 12 equals 08 - for IPv4 and ARP
    //                86 - For IPv6
    //                81 - For VLAN
    //                88 - For MPLS
    uint8_t eth_types[] = {0x08, 0x86, 0x81, 0x88};
    uint32_t mask = 0;

    clear_rx_filter_rules(_if);

    if (set_on) {
        for (int rule_id = 0; rule_id < sizeof(eth_types); rule_id++) {
            mask |= 0x1 << rule_id;
            // Filter for byte 12 of packet
            _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x000000 | eth_types[rule_id]);
            _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x10); /* MASK */
            // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1, len = 24
            _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | 24);
        }
    } else {
        configure_rx_filter_rules(_if);
    }

    return 0;
}

void CTRexExtendedDriverBase1G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){

    stats->ipackets     +=  _if->pci_reg_read(E1000_GPRC) ;

    stats->ibytes       +=  (_if->pci_reg_read(E1000_GORCL) );
    stats->ibytes       +=  (((uint64_t)_if->pci_reg_read(E1000_GORCH))<<32);


    stats->opackets     +=  _if->pci_reg_read(E1000_GPTC);
    stats->obytes       +=  _if->pci_reg_read(E1000_GOTCL) ;
    stats->obytes       +=  ( (((uint64_t)_if->pci_reg_read(IXGBE_GOTCH))<<32) );

    stats->f_ipackets   +=  0;
    stats->f_ibytes     += 0;


    stats->ierrors      +=  ( _if->pci_reg_read(E1000_RNBC) +
                              _if->pci_reg_read(E1000_CRCERRS) +
                              _if->pci_reg_read(E1000_ALGNERRC ) +
                              _if->pci_reg_read(E1000_SYMERRS ) +
                              _if->pci_reg_read(E1000_RXERRC ) +

                              _if->pci_reg_read(E1000_ROC)+
                              _if->pci_reg_read(E1000_RUC)+
                              _if->pci_reg_read(E1000_RJC) +

                              _if->pci_reg_read(E1000_XONRXC)+
                              _if->pci_reg_read(E1000_XONTXC)+
                              _if->pci_reg_read(E1000_XOFFRXC)+
                              _if->pci_reg_read(E1000_XOFFTXC)+
                              _if->pci_reg_read(E1000_FCRUC)
                              );

    stats->oerrors      +=  0;
    stats->imcasts      =  0;
    stats->rx_nombuf    =  0;
}

void CTRexExtendedDriverBase1G::clear_extended_stats(CPhyEthIF * _if){
}

#if 0
int CTRexExtendedDriverBase1G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                            ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    uint32_t port_id = _if->get_port_id();
    return g_trex.m_rx_sl.get_rx_stats(port_id, pkts, prev_pkts, bytes, prev_bytes, min, max);
}
#endif

void CTRexExtendedDriverBase10G::clear_extended_stats(CPhyEthIF * _if){
    _if->pci_reg_read(IXGBE_RXNFGPC);
}

void CTRexExtendedDriverBase10G::update_global_config_fdir(port_cfg_t * cfg){
    cfg->update_global_config_fdir_10g();
}

void CTRexExtendedDriverBase10G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules(CPhyEthIF * _if) {
    set_rcv_all(_if, false);
    if ( get_is_stateless() ) {
        return configure_rx_filter_rules_stateless(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }

    return 0;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules_stateless(CPhyEthIF * _if) {
    uint8_t port_id = _if->get_rte_port_id();
    int  ip_id_lsb;

    // 0..MAX_FLOW_STATS-1 is for rules using ip_id.
    // MAX_FLOW_STATS rule is for the payload rules. Meaning counter value is in the payload
    for (ip_id_lsb = 0; ip_id_lsb <= MAX_FLOW_STATS; ip_id_lsb++ ) {
        struct rte_eth_fdir_filter fdir_filter;
        int res = 0;

        memset(&fdir_filter,0,sizeof(fdir_filter));
        fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
        fdir_filter.soft_id = ip_id_lsb; // We can use the ip_id_lsb also as filter soft_id
        fdir_filter.input.flow_ext.flexbytes[0] = 0xff;
        fdir_filter.input.flow_ext.flexbytes[1] = ip_id_lsb;
        fdir_filter.action.rx_queue = 1;
        fdir_filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
        fdir_filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
        res = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_ADD, &fdir_filter);

        if (res != 0) {
            rte_exit(EXIT_FAILURE, "Error: rte_eth_dev_filter_ctrl in configure_rx_filter_rules_stateless: %d\n",res);
        }
    }

    return 0;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    uint8_t port_id=_if->get_rte_port_id();
    uint16_t hops = get_rx_check_hops();
    uint16_t v4_hops = (hops << 8)&0xff00;

    /* enable rule 0 SCTP -> queue 1 for latency  */
    /* 1<<21 means that queue 1 is for SCTP */
    _if->pci_reg_write(IXGBE_L34T_IMIR(0),(1<<21));
    _if->pci_reg_write(IXGBE_FTQF(0),
                       IXGBE_FTQF_PROTOCOL_SCTP|
                       (IXGBE_FTQF_PRIORITY_MASK<<IXGBE_FTQF_PRIORITY_SHIFT)|
                       ((0x0f)<<IXGBE_FTQF_5TUPLE_MASK_SHIFT)|IXGBE_FTQF_QUEUE_ENABLE);

    // IPv4: bytes being compared are {TTL, Protocol}
    uint16_t ff_rules_v4[6]={
        (uint16_t)(0xFF11 - v4_hops),
        (uint16_t)(0xFE11 - v4_hops),
        (uint16_t)(0xFF06 - v4_hops),
        (uint16_t)(0xFE06 - v4_hops),
        (uint16_t)(0xFF01 - v4_hops),
        (uint16_t)(0xFE01 - v4_hops),
    };
    // IPv6: bytes being compared are {NextHdr, HopLimit}
    uint16_t ff_rules_v6[6]={
        (uint16_t)(0x3CFF - hops),
        (uint16_t)(0x3CFE - hops),
    };

    uint16_t *ff_rules;
    uint16_t num_rules;
    int  rule_id;

    if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
        ff_rules = &ff_rules_v6[0];
        num_rules = sizeof(ff_rules_v6)/sizeof(ff_rules_v6[0]);
    }else{
        ff_rules = &ff_rules_v4[0];
        num_rules = sizeof(ff_rules_v4)/sizeof(ff_rules_v4[0]);
    }

    for (rule_id=0; rule_id<num_rules; rule_id++ ) {
        struct rte_eth_fdir_filter fdir_filter;
        uint16_t ff_rule = ff_rules[rule_id];
        int res = 0;

        memset(&fdir_filter,0,sizeof(fdir_filter));
        /* TOS/PROTO */
        if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
            fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
        }else{
            fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
        }
        fdir_filter.soft_id = rule_id;

        fdir_filter.input.flow_ext.flexbytes[0] = (ff_rule >> 8) & 0xff;
        fdir_filter.input.flow_ext.flexbytes[1] = ff_rule & 0xff;
        fdir_filter.action.rx_queue = 1;
        fdir_filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
        fdir_filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
        res = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_ADD, &fdir_filter);

        if (res != 0) {
            rte_exit(EXIT_FAILURE, "Error: rte_eth_dev_filter_ctrl in configure_rx_filter_rules_statefull: %d\n",res);
        }
    }
    return (0);
}

int CTRexExtendedDriverBase10G::add_del_eth_filter(CPhyEthIF * _if, bool is_add, uint16_t ethertype) {
    int res = 0;
    uint8_t port_id=_if->get_rte_port_id();
    struct rte_eth_ethertype_filter filter;
    enum rte_filter_op op;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = ethertype;
    res = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_ETHERTYPE, RTE_ETH_FILTER_GET, &filter);

    if (is_add && (res >= 0))
        return 0;
    if ((! is_add) && (res == -ENOENT))
        return 0;

    if (is_add) {
        op = RTE_ETH_FILTER_ADD;
    } else {
        op = RTE_ETH_FILTER_DELETE;
    }

    filter.queue = 1;
    res = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_ETHERTYPE, op, &filter);
    if (res != 0) {
        printf("Error: %s L2 filter for ethertype 0x%04x returned %d\n", is_add ? "Adding":"Deleting", ethertype, res);
        exit(1);
    }
    return 0;
}

int CTRexExtendedDriverBase10G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    int res = 0;
    res = add_del_eth_filter(_if, set_on, ETHER_TYPE_ARP);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_IPv4);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_IPv6);

    return res;
}

void CTRexExtendedDriverBase10G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){

    int i;
    uint64_t t=0;

    if ( !get_is_stateless() ) {

        for (i=0; i<8;i++) {
            t+=_if->pci_reg_read(IXGBE_MPC(i));
        }
    }

    stats->ipackets     +=  _if->pci_reg_read(IXGBE_GPRC) ;

    stats->ibytes       +=  (_if->pci_reg_read(IXGBE_GORCL) +(((uint64_t)_if->pci_reg_read(IXGBE_GORCH))<<32));



    stats->opackets     +=  _if->pci_reg_read(IXGBE_GPTC);
    stats->obytes       +=  (_if->pci_reg_read(IXGBE_GOTCL) +(((uint64_t)_if->pci_reg_read(IXGBE_GOTCH))<<32));

    stats->f_ipackets   +=  _if->pci_reg_read(IXGBE_RXDGPC);
    stats->f_ibytes     += (_if->pci_reg_read(IXGBE_RXDGBCL) +(((uint64_t)_if->pci_reg_read(IXGBE_RXDGBCH))<<32));


    stats->ierrors      +=  ( _if->pci_reg_read(IXGBE_RLEC) +
                              _if->pci_reg_read(IXGBE_ERRBC) +
                              _if->pci_reg_read(IXGBE_CRCERRS) +
                              _if->pci_reg_read(IXGBE_ILLERRC ) +
                              _if->pci_reg_read(IXGBE_ROC)+
                              _if->pci_reg_read(IXGBE_RUC)+t);

    stats->oerrors      +=  0;
    stats->imcasts      =  0;
    stats->rx_nombuf    =  0;

}

int CTRexExtendedDriverBase10G::wait_for_stable_link(){
    wait_x_sec(1 + CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverBase10G::get_flow_stat_parser() {
    CFlowStatParser *parser = new C82599Parser(CGlobalInfo::m_options.preview.get_vlan_mode_enable() ? true:false);
    assert (parser);
    return parser;
}

void CTRexExtendedDriverBase40G::clear_extended_stats(CPhyEthIF * _if){
    rte_eth_stats_reset(_if->get_port_id());
}


void CTRexExtendedDriverBase40G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->update_global_config_fdir_40g();
}

// What is the type of the rule the respective hw_id counter counts.
struct fdir_hw_id_params_t {
    uint16_t rule_type;
    uint8_t l4_proto;
};

static struct fdir_hw_id_params_t fdir_hw_id_rule_params[512];

/* Add rule to send packets with protocol 'type', and ttl 'ttl' to rx queue 1 */
// ttl is used in statefull mode, and ip_id in stateless. We configure the driver registers so that only one of them applies.
// So, the rule will apply if packet has either the correct ttl or IP ID, depending if we are in statfull or stateless.
void CTRexExtendedDriverBase40G::add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type, uint8_t ttl
                                               , uint16_t ip_id, uint8_t l4_proto, int queue, uint16_t stat_idx) {
    int ret=rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);
    static int filter_soft_id = 0;

    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, port_id);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

#if 0
    printf("40g::%s rules: port:%d type:%d ttl:%d ip_id:%x l4:%d q:%d hw index:%d\n"
           , (op == RTE_ETH_FILTER_ADD) ?  "add" : "del"
           , port_id, type, ttl, ip_id, l4_proto, queue, stat_idx);
#endif

    filter.action.rx_queue = queue;
    filter.action.behavior =RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status =RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.action.stat_count_index = stat_idx;
    filter.soft_id = filter_soft_id++;
    filter.input.flow_type = type;

    if (op == RTE_ETH_FILTER_ADD) {
        fdir_hw_id_rule_params[stat_idx].rule_type = type;
        fdir_hw_id_rule_params[stat_idx].l4_proto = l4_proto;
    }

    switch (type) {
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
        filter.input.flow.ip4_flow.ttl=ttl;
        filter.input.flow.ip4_flow.ip_id = ip_id;
        if (l4_proto != 0)
            filter.input.flow.ip4_flow.proto = l4_proto;
        break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
        filter.input.flow.ipv6_flow.hop_limits=ttl;
        filter.input.flow.ipv6_flow.flow_label = ip_id;
        filter.input.flow.ipv6_flow.proto = l4_proto;
        break;
    }

    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, op, (void*)&filter);
    if ( ret != 0 ) {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl: err=%d, port=%u\n",
                 ret, port_id);
    }
}

int CTRexExtendedDriverBase40G::add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type) {
    int ret;
    struct rte_eth_ethertype_filter filter;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = eth_type;
    filter.flags = 0;
    filter.queue = MAIN_DPDK_RX_Q;
    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_ETHERTYPE, op, (void *) &filter);

    return ret;
}

extern "C" int rte_eth_fdir_stats_reset(uint8_t port_id, uint32_t *stats, uint32_t start, uint32_t len);

// type - rule type. Currently we only support rules in IP ID.
// proto - Packet protocol: UDP or TCP
// id - Counter id in HW. We assume it is in the range 0..MAX_FLOW_STATS
int CTRexExtendedDriverBase40G::add_del_rx_flow_stat_rule(uint8_t port_id, enum rte_filter_op op, uint16_t l3_proto
                                                          , uint8_t l4_proto, uint8_t ipv6_next_h, uint16_t id) {
    uint32_t rule_id = (port_id % m_if_per_card) * MAX_FLOW_STATS + id;
    uint16_t rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
    uint8_t next_proto;

    if (l3_proto == EthernetHeader::Protocol::IP) {
        next_proto = l4_proto;
        switch(l4_proto) {
        case IPPROTO_TCP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_TCP;
            break;
        case IPPROTO_UDP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_UDP;
            break;
        default:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
            break;
        }
    } else {
        // IPv6
        next_proto = ipv6_next_h;
        switch(l4_proto) {
        case IPPROTO_TCP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_TCP;
            break;
        case IPPROTO_UDP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_UDP;
            break;
        default:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
            break;
        }
    }

    add_del_rules(op, port_id, rte_type, 0, IP_ID_RESERVE_BASE + id, next_proto, MAIN_DPDK_DATA_Q, rule_id);
    return 0;
}

int CTRexExtendedDriverBase40G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    uint32_t port_id = _if->get_port_id();
    uint16_t hops = get_rx_check_hops();
    int i;

    rte_eth_fdir_stats_reset(port_id, NULL, 0, 1);
    for (i = 0; i < 2; i++) {
        uint8_t ttl = TTL_RESERVE_DUPLICATE - i - hops;
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        /* Rules for latency measurement packets */
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, ttl, 0, IPPROTO_ICMP, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_SCTP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
    }
    return 0;
}

const uint32_t FDIR_TEMP_HW_ID = 511;
const uint32_t FDIR_PAYLOAD_RULES_HW_ID = 510;
extern const uint32_t FLOW_STAT_PAYLOAD_IP_ID;
int CTRexExtendedDriverBase40G::configure_rx_filter_rules(CPhyEthIF * _if) {
    uint32_t port_id = _if->get_port_id();

    if (get_is_stateless()) {
        i40e_trex_fdir_reg_init(port_id, I40E_TREX_INIT_STL);

        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, IPPROTO_ICMP, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);

        rte_eth_fdir_stats_reset(_if->get_port_id(), NULL, FDIR_TEMP_HW_ID, 1);
        return 0; // Other rules are configured dynamically in stateless
    } else {
        i40e_trex_fdir_reg_init(port_id, I40E_TREX_INIT_STF);
        return configure_rx_filter_rules_statefull(_if);
    }
}

void CTRexExtendedDriverBase40G::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
    uint32_t port_id = _if->get_port_id();
    uint32_t rule_id = (port_id % m_if_per_card) * MAX_FLOW_STATS + min;

    // Since flow dir counters are not wrapped around as promised in the data sheet, but rather get stuck at 0xffffffff
    // we reset the HW value
    rte_eth_fdir_stats_reset(port_id, NULL, rule_id, len);

    for (int i =0; i < len; i++) {
        stats[i] = 0;
    }
}

// instead of adding this to rte_ethdev.h
extern "C" int rte_eth_fdir_stats_get(uint8_t port_id, uint32_t *stats, uint32_t start, uint32_t len);
// we read every 0.5 second. We want to catch the counter when it approach the maximum (where it will stuck,
// and we will start losing packets).
const uint32_t X710_FDIR_RESET_THRESHOLD = 0xffffffff - 1000000000/8/64*40;

// get rx stats on _if, between min and max
// prev_pkts should be the previous values read from the hardware.
//            Getting changed to be equal to current HW values.
// pkts return the diff between prev_pkts and current hw values
// bytes and prev_bytes are not used. X710 fdir filters do not support byte count.
int CTRexExtendedDriverBase40G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    uint32_t hw_stats[MAX_FLOW_STATS];
    uint32_t port_id = _if->get_port_id();
    uint32_t start = (port_id % m_if_per_card) * MAX_FLOW_STATS + min;
    uint32_t len = max - min + 1;
    uint32_t loop_start = min;

    rte_eth_fdir_stats_get(port_id, hw_stats, start, len);
    for (int i = loop_start; i <  loop_start + len; i++) {
        if (unlikely(hw_stats[i - min] > X710_FDIR_RESET_THRESHOLD)) {
            // When x710 fdir counters reach max of 32 bits (4G), the get stuck. To handle this, we temporarily
            // move to temp counter, reset the counter in danger, and go back to using it.
            // see trex-199 for more details
            uint32_t counter, temp_count;
            uint32_t hw_id = start - min + i;

            add_del_rules( RTE_ETH_FILTER_ADD, port_id, fdir_hw_id_rule_params[hw_id].rule_type, 0
                           , IP_ID_RESERVE_BASE + i, fdir_hw_id_rule_params[hw_id].l4_proto, MAIN_DPDK_DATA_Q
                           , FDIR_TEMP_HW_ID);
            delay(100);
            rte_eth_fdir_stats_reset(port_id, &counter, hw_id, 1);
            add_del_rules( RTE_ETH_FILTER_ADD, port_id, fdir_hw_id_rule_params[hw_id].rule_type, 0
                           , IP_ID_RESERVE_BASE + i, fdir_hw_id_rule_params[hw_id].l4_proto, MAIN_DPDK_DATA_Q, hw_id);
            delay(100);
            rte_eth_fdir_stats_reset(port_id, &temp_count, FDIR_TEMP_HW_ID, 1);
            pkts[i] = counter + temp_count - prev_pkts[i];
            prev_pkts[i] = 0;
        } else {
            pkts[i] = hw_stats[i - min] - prev_pkts[i];
            prev_pkts[i] = hw_stats[i - min];
        }
        bytes[i] = 0;
    }

    return 0;
}

// if fd != NULL, dump fdir stats of _if
// return num of filters
int CTRexExtendedDriverBase40G::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
    uint32_t port_id = _if->get_port_id();
    struct rte_eth_fdir_stats stat;
    int ret;

    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_STATS, (void*)&stat);
    if (ret == 0) {
        if (fd)
            fprintf(fd, "Num filters on guarant poll:%d, best effort poll:%d\n", stat.guarant_cnt, stat.best_cnt);
        return (stat.guarant_cnt + stat.best_cnt);
    } else {
        if (fd)
            fprintf(fd, "Failed reading fdir statistics\n");
        return -1;
    }
}

void CTRexExtendedDriverBase40G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    struct rte_eth_stats stats1;
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;
    rte_eth_stats_get(_if->get_port_id(), &stats1);

    stats->ipackets += stats1.ipackets - prev_stats->ipackets;
    stats->ibytes   += stats1.ibytes - prev_stats->ibytes;
    stats->opackets += stats1.opackets - prev_stats->opackets;
    stats->obytes   += stats1.obytes - prev_stats->obytes
        + (stats1.opackets << 2) - (prev_stats->opackets << 2);
    stats->f_ipackets += 0;
    stats->f_ibytes   += 0;
    stats->ierrors    += stats1.imissed + stats1.ierrors + stats1.rx_nombuf
        - prev_stats->imissed - prev_stats->ierrors - prev_stats->rx_nombuf;
    stats->oerrors    += stats1.oerrors - prev_stats->oerrors;
    stats->imcasts    += 0;
    stats->rx_nombuf  += stats1.rx_nombuf - prev_stats->rx_nombuf;

    prev_stats->ipackets = stats1.ipackets;
    prev_stats->ibytes = stats1.ibytes;
    prev_stats->opackets = stats1.opackets;
    prev_stats->obytes = stats1.obytes;
    prev_stats->imissed = stats1.imissed;
    prev_stats->oerrors = stats1.oerrors;
    prev_stats->ierrors = stats1.ierrors;
    prev_stats->rx_nombuf = stats1.rx_nombuf;
}

int CTRexExtendedDriverBase40G::wait_for_stable_link(){
    wait_x_sec(1 + CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}

extern "C" int rte_eth_get_fw_ver(int port, uint32_t *ver);

int CTRexExtendedDriverBase40G::verify_fw_ver(int port_id) {
    uint32_t version;
    int ret;

    ret = rte_eth_get_fw_ver(port_id, &version);

    if (ret == 0) {
        if (CGlobalInfo::m_options.preview.getVMode() >= 1) {
            printf("port %d: FW ver %02d.%02d.%02d\n", port_id, ((version >> 12) & 0xf), ((version >> 4) & 0xff)
                   ,(version & 0xf));
        }

        if ((((version >> 12) & 0xf) < 5)  || ((((version >> 12) & 0xf) == 5) && ((version >> 4 & 0xff) == 0)
                                               && ((version & 0xf) < 4))) {
            printf("Error: In this TRex version, X710 firmware must be at least 05.00.04\n");
            printf("  Please refer to %s for upgrade instructions\n",
                   "https://trex-tgn.cisco.com/trex/doc/trex_manual.html#_firmware_update_to_xl710_x710");
            exit(1);
        }
    }

    return ret;
}

CFlowStatParser *CTRexExtendedDriverBase40G::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser();
    assert (parser);
    return parser;
}

int CTRexExtendedDriverBase40G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    uint32_t port_id = _if->get_port_id();
    enum rte_filter_op op = set_on ? RTE_ETH_FILTER_ADD : RTE_ETH_FILTER_DELETE;

    add_del_eth_type_rule(port_id, op, EthernetHeader::Protocol::ARP);

    if (set_on) {
        i40e_trex_fdir_reg_init(port_id, I40E_TREX_INIT_RCV_ALL);
    }

    // In order to receive packets, we also need to configure rules for each type.
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 10, 0, 0, MAIN_DPDK_RX_Q, 0);

    if (! set_on) {
        configure_rx_filter_rules(_if);
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/* MLX5 */

void CTRexExtendedDriverBaseMlnx5G::clear_extended_stats(CPhyEthIF * _if){
    rte_eth_stats_reset(_if->get_port_id());
}

void CTRexExtendedDriverBaseMlnx5G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->update_global_config_fdir_40g();
    /* update mask */
    cfg->m_port_conf.fdir_conf.mask.ipv4_mask.proto=0xff;
    cfg->m_port_conf.fdir_conf.mask.ipv4_mask.tos=0x01;
    cfg->m_port_conf.fdir_conf.mask.ipv6_mask.proto=0xff;
    cfg->m_port_conf.fdir_conf.mask.ipv6_mask.tc=0x01;

    /* enable RSS */
    cfg->m_port_conf.rxmode.mq_mode =ETH_MQ_RX_RSS;
    cfg->m_port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP;

}

/*
   In case of MLX5 driver, the rule is not really added according to givern parameters.
   ip_id == 1 means add rule on TOS (or traffic_class) field.
   ip_id == 2 means add rule to receive all packets.
 */
void CTRexExtendedDriverBaseMlnx5G::add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type,
                                                  uint16_t ip_id, uint8_t l4_proto, int queue) {
    int ret = rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);
    static int filter_soft_id = 0;

    if ( ret != 0 ) {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported err=%d, port=%u \n", ret, port_id);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

#if 0
    printf("MLNX add_del_rules::%s rules: port:%d type:%d ip_id:%x l4:%d q:%d\n"
           , (op == RTE_ETH_FILTER_ADD) ?  "add" : "del"
           , port_id, type, ip_id, l4_proto, queue);
#endif

    filter.action.rx_queue = queue;
    filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.soft_id = filter_soft_id++;
    filter.input.flow_type = type;

    switch (type) {
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
        filter.input.flow.ip4_flow.ip_id = ip_id;
        if (l4_proto != 0)
            filter.input.flow.ip4_flow.proto = l4_proto;
        break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
        filter.input.flow.ipv6_flow.flow_label = ip_id;
        filter.input.flow.ipv6_flow.proto = l4_proto;
        break;
    }

    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, op, (void*)&filter);
    if ( ret != 0 ) {
        if (((op == RTE_ETH_FILTER_ADD) && (ret == EEXIST)) || ((op == RTE_ETH_FILTER_DELETE) && (ret == ENOENT)))
            return;

        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl: err=%d, port=%u\n",
                 ret, port_id);
    }
}

int CTRexExtendedDriverBaseMlnx5G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    uint8_t port_id=_if->get_rte_port_id();

    if (set_on) {
        add_del_rx_filter_rules(_if, false);
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 2, 17, MAIN_DPDK_RX_Q);
    } else {
        add_del_rules(RTE_ETH_FILTER_DELETE, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 2, 17, MAIN_DPDK_RX_Q);
        add_del_rx_filter_rules(_if, true);
    }

    return 0;

}

int CTRexExtendedDriverBaseMlnx5G::add_del_rx_filter_rules(CPhyEthIF * _if, bool set_on) {
    uint32_t port_id = _if->get_port_id();
    enum rte_filter_op op;

    if (set_on) {
        op = RTE_ETH_FILTER_ADD;
    } else {
        op = RTE_ETH_FILTER_DELETE;
    }

    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 1, 17, MAIN_DPDK_RX_Q);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 1, 6, MAIN_DPDK_RX_Q);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 1, 1, MAIN_DPDK_RX_Q);  /*ICMP*/
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 1, 132, MAIN_DPDK_RX_Q);  /*SCTP*/
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17, MAIN_DPDK_RX_Q);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 1, 6, MAIN_DPDK_RX_Q);
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 1, MAIN_DPDK_RX_Q);  /*ICMP*/
    add_del_rules(op, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 132, MAIN_DPDK_RX_Q);  /*SCTP*/

    return 0;
}

int CTRexExtendedDriverBaseMlnx5G::configure_rx_filter_rules(CPhyEthIF * _if) {
    set_rcv_all(_if, false);
    return add_del_rx_filter_rules(_if, true);
}

void CTRexExtendedDriverBaseMlnx5G::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
    for (int i =0; i < len; i++) {
        stats[i] = 0;
    }
}

int CTRexExtendedDriverBaseMlnx5G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    /* not supported yet */
    return 0;
}

int CTRexExtendedDriverBaseMlnx5G::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
    uint32_t port_id = _if->get_port_id();
    struct rte_eth_fdir_stats stat;
    int ret;

    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_STATS, (void*)&stat);
    if (ret == 0) {
        if (fd)
            fprintf(fd, "Num filters on guarant poll:%d, best effort poll:%d\n", stat.guarant_cnt, stat.best_cnt);
        return (stat.guarant_cnt + stat.best_cnt);
    } else {
        if (fd)
            fprintf(fd, "Failed reading fdir statistics\n");
        return -1;
    }
}

void CTRexExtendedDriverBaseMlnx5G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){

    struct rte_eth_stats stats1;
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;
    rte_eth_stats_get(_if->get_port_id(), &stats1);

    stats->ipackets += stats1.ipackets - prev_stats->ipackets;
    stats->ibytes   += stats1.ibytes - prev_stats->ibytes +
        + (stats1.ipackets << 2) - (prev_stats->ipackets << 2);
    stats->opackets += stats1.opackets - prev_stats->opackets;
    stats->obytes   += stats1.obytes - prev_stats->obytes
        + (stats1.opackets << 2) - (prev_stats->opackets << 2);
    stats->f_ipackets += 0;
    stats->f_ibytes   += 0;
    stats->ierrors    += stats1.imissed + stats1.ierrors + stats1.rx_nombuf
        - prev_stats->imissed - prev_stats->ierrors - prev_stats->rx_nombuf;
    stats->oerrors    += stats1.oerrors - prev_stats->oerrors;
    stats->imcasts    += 0;
    stats->rx_nombuf  += stats1.rx_nombuf - prev_stats->rx_nombuf;

    prev_stats->ipackets = stats1.ipackets;
    prev_stats->ibytes = stats1.ibytes;
    prev_stats->opackets = stats1.opackets;
    prev_stats->obytes = stats1.obytes;
    prev_stats->imissed = stats1.imissed;
    prev_stats->oerrors = stats1.oerrors;
    prev_stats->ierrors = stats1.ierrors;
    prev_stats->rx_nombuf = stats1.rx_nombuf;
}

int CTRexExtendedDriverBaseMlnx5G::wait_for_stable_link(){
    delay(20);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverBaseMlnx5G::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser();
    assert (parser);
    return parser;
}

//////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/* VIC */

void CTRexExtendedDriverBaseVIC::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.rxmode.max_rx_pkt_len =9*1000-10;
    cfg->m_port_conf.fdir_conf.mask.ipv4_mask.tos = 0x01;
    cfg->m_port_conf.fdir_conf.mask.ipv6_mask.tc  = 0x01;
}

void CTRexExtendedDriverBaseVIC::add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type
                                               , uint16_t id, uint8_t l4_proto, uint8_t tos, int queue) {
    int ret=rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);

    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, port_id);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

#if 0
    printf("VIC add_del_rules::%s rules: port:%d type:%d id:%d l4:%d tod:%d, q:%d\n"
           , (op == RTE_ETH_FILTER_ADD) ?  "add" : "del"
           , port_id, type, id, l4_proto, tos, queue);
#endif

    filter.action.rx_queue = queue;
    filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.soft_id = id;
    filter.input.flow_type = type;

    switch (type) {
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
        filter.input.flow.ip4_flow.tos = tos;
        filter.input.flow.ip4_flow.proto = l4_proto;
        break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
        filter.input.flow.ipv6_flow.tc = tos;
        filter.input.flow.ipv6_flow.proto = l4_proto;
        break;
    }

    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR, op, (void*)&filter);
    if ( ret != 0 ) {
        if (((op == RTE_ETH_FILTER_ADD) && (ret == -EEXIST)) || ((op == RTE_ETH_FILTER_DELETE) && (ret == -ENOENT)))
            return;

        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl: err=%d, port=%u\n",
                 ret, port_id);
    }
}

int CTRexExtendedDriverBaseVIC::add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type) {
    int ret;
    struct rte_eth_ethertype_filter filter;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = eth_type;
    filter.flags = 0;
    filter.queue = MAIN_DPDK_RX_Q;
    ret = rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_ETHERTYPE, op, (void *) &filter);

    return ret;
}

int CTRexExtendedDriverBaseVIC::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    uint32_t port_id = _if->get_port_id();

    set_rcv_all(_if, false);

    // Rules to direct all IP packets with tos lsb bit 1 to RX Q.
    // IPv4
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 1, 17, 0x1, MAIN_DPDK_RX_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 1, 6,  0x1, MAIN_DPDK_RX_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_SCTP, 1, 132,  0x1, MAIN_DPDK_RX_Q); /*SCTP*/
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 1, 1,  0x1, MAIN_DPDK_RX_Q);  /*ICMP*/
    // Ipv6
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 6,  0x1, MAIN_DPDK_RX_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17,  0x1, MAIN_DPDK_RX_Q);

    // Because of some issue with VIC firmware, IPv6 UDP and ICMP go by default to q 1, so we
    // need these rules to make them go to q 0.
    // rule appply to all packets with 0 on tos lsb.
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 6,  0, MAIN_DPDK_DATA_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17,  0, MAIN_DPDK_DATA_Q);

    return 0;
}


int CTRexExtendedDriverBaseVIC::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    uint8_t port_id = _if->get_rte_port_id();

    // soft ID 100 tells VIC driver to add rule for all ether types.
    // Added with highest priority (implicitly in the driver), so if it exists, it applies before all other rules
    if (set_on) {
        add_del_rules(RTE_ETH_FILTER_ADD, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 100, 30, 0, MAIN_DPDK_RX_Q);
    } else {
        add_del_rules(RTE_ETH_FILTER_DELETE, port_id, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 100, 30, 0, MAIN_DPDK_RX_Q);
    }

    return 0;

}

void CTRexExtendedDriverBaseVIC::clear_extended_stats(CPhyEthIF * _if){
    rte_eth_stats_reset(_if->get_port_id());
}

void CTRexExtendedDriverBaseVIC::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    struct rte_eth_stats stats1;
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;
    rte_eth_stats_get(_if->get_port_id(), &stats1);

    stats->ipackets += stats1.ipackets - prev_stats->ipackets;
    stats->ibytes   += stats1.ibytes - prev_stats->ibytes
         - ((stats1.ipackets << 2) - (prev_stats->ipackets << 2));
    stats->opackets += stats1.opackets - prev_stats->opackets;
    stats->obytes   += stats1.obytes - prev_stats->obytes;
    stats->f_ipackets += 0;
    stats->f_ibytes   += 0;
    stats->ierrors    += stats1.imissed + stats1.ierrors + stats1.rx_nombuf
        - prev_stats->imissed - prev_stats->ierrors - prev_stats->rx_nombuf;
    stats->oerrors    += stats1.oerrors - prev_stats->oerrors;
    stats->imcasts    += 0;
    stats->rx_nombuf  += stats1.rx_nombuf - prev_stats->rx_nombuf;

    prev_stats->ipackets = stats1.ipackets;
    prev_stats->ibytes = stats1.ibytes;
    prev_stats->opackets = stats1.opackets;
    prev_stats->obytes = stats1.obytes;
    prev_stats->imissed = stats1.imissed;
    prev_stats->oerrors = stats1.oerrors;
    prev_stats->ierrors = stats1.ierrors;
    prev_stats->rx_nombuf = stats1.rx_nombuf;
}

int CTRexExtendedDriverBaseVIC::verify_fw_ver(int port_id) {

    struct rte_eth_fdir_info fdir_info;

    if ( rte_eth_dev_filter_ctrl(port_id,RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_INFO,(void *)&fdir_info) == 0 ){
        if ( fdir_info.flow_types_mask[0] & (1<< RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) ) {
           /* support new features */
            if (CGlobalInfo::m_options.preview.getVMode() >= 1) {
                printf("VIC port %d: FW support advanced filtering \n", port_id);
            }
            return (0);
        }
    }

    printf("Error: VIC firmware should upgrade to support advanced filtering \n");
    printf("  Please refer to %s for upgrade instructions\n",
           "https://trex-tgn.cisco.com/trex/doc/trex_manual.html");
    exit(1);
}

int CTRexExtendedDriverBaseVIC::configure_rx_filter_rules(CPhyEthIF * _if) {

    if (get_is_stateless()) {
        /* both stateless and stateful work in the same way, might changed in the future TOS */
        return configure_rx_filter_rules_statefull(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }
}

void CTRexExtendedDriverBaseVIC::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
}

int CTRexExtendedDriverBaseVIC::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    printf(" NOT supported yet \n");
    return 0;
}

// if fd != NULL, dump fdir stats of _if
// return num of filters
int CTRexExtendedDriverBaseVIC::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
 //printf(" NOT supported yet \n");
 return (0);
}

CFlowStatParser *CTRexExtendedDriverBaseVIC::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser();
    assert (parser);
    return parser;
}


/////////////////////////////////////////////////////////////////////////////////////


void CTRexExtendedDriverBase1GVm::update_configuration(port_cfg_t * cfg){
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get((uint8_t) 0,&dev_info);

    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
    cfg->m_tx_conf.txq_flags=dev_info.default_txconf.txq_flags;

}


int CTRexExtendedDriverBase1GVm::configure_rx_filter_rules(CPhyEthIF * _if){
    return (0);
}

void CTRexExtendedDriverBase1GVm::clear_extended_stats(CPhyEthIF * _if){

    rte_eth_stats_reset(_if->get_port_id());

}

int CTRexExtendedDriverBase1GVm::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    return (0);
}

void CTRexExtendedDriverBase1GVm::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    struct rte_eth_stats stats1;
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;
    rte_eth_stats_get(_if->get_port_id(), &stats1);

    stats->ipackets   += stats1.ipackets - prev_stats->ipackets;
    stats->ibytes     += stats1.ibytes - prev_stats->ibytes;
    stats->opackets   += stats1.opackets - prev_stats->opackets;
    stats->obytes     += stats1.obytes - prev_stats->obytes;
    stats->f_ipackets += 0;
    stats->f_ibytes   += 0;
    stats->ierrors    += stats1.imissed + stats1.ierrors + stats1.rx_nombuf
        - prev_stats->imissed - prev_stats->ierrors - prev_stats->rx_nombuf;
    stats->oerrors    += stats1.oerrors - prev_stats->oerrors;
    stats->imcasts    += 0;
    stats->rx_nombuf  += stats1.rx_nombuf - prev_stats->rx_nombuf;

    prev_stats->ipackets = stats1.ipackets;
    prev_stats->ibytes = stats1.ibytes;
    prev_stats->opackets = stats1.opackets;
    prev_stats->obytes = stats1.obytes;
    prev_stats->imissed = stats1.imissed;
    prev_stats->oerrors = stats1.oerrors;
    prev_stats->ierrors = stats1.ierrors;
    prev_stats->rx_nombuf = stats1.rx_nombuf;
}

int CTRexExtendedDriverBase1GVm::wait_for_stable_link(){
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}



/**
 * convert chain of mbuf to one big mbuf
 *
 * @param m
 *
 * @return
 */
struct rte_mbuf *  rte_mbuf_convert_to_one_seg(struct rte_mbuf *m){
    unsigned int len;
    struct rte_mbuf * r;
    struct rte_mbuf * old_m;
    old_m=m;

    len=rte_pktmbuf_pkt_len(m);
    /* allocate one big mbuf*/
    r = CGlobalInfo::pktmbuf_alloc(0,len);
    assert(r);
    if (r==0) {
        rte_pktmbuf_free(m);
        return(r);
    }
    char *p=rte_pktmbuf_append(r,len);

    while ( m ) {
        len = m->data_len;
        assert(len);
        memcpy(p,(char *)m->buf_addr, len);
        p+=len;
        m = m->next;
    }
    rte_pktmbuf_free(old_m);
    return(r);
}

/**
 * handle a signal for termination
 *
 * @author imarom (7/27/2016)
 *
 * @param signum
 */
static void trex_termination_handler(int signum) {
    std::stringstream ss;

    /* be sure that this was given on the main process */
    assert(rte_eal_process_type() == RTE_PROC_PRIMARY);

    switch (signum) {
    case SIGINT:
        g_trex.mark_for_shutdown(CGlobalTRex::SHUTDOWN_SIGINT);
        break;

    case SIGTERM:
        g_trex.mark_for_shutdown(CGlobalTRex::SHUTDOWN_SIGTERM);
        break;

    default:
        assert(0);
    }

}

/***********************************************************
 * platfrom API object
 * TODO: REMOVE THIS TO A SEPERATE FILE
 *
 **********************************************************/
int TrexDpdkPlatformApi::get_xstats_values(uint8_t port_id, xstats_values_t &xstats_values) const {
    return g_trex.m_ports[port_id].get_port_attr()->get_xstats_values(xstats_values);
}

int TrexDpdkPlatformApi::get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names) const {
    return g_trex.m_ports[port_id].get_port_attr()->get_xstats_names(xstats_names);
}


void TrexDpdkPlatformApi::get_port_num(uint8_t &port_num) const {
    port_num = g_trex.m_max_ports;
}

void
TrexDpdkPlatformApi::get_global_stats(TrexPlatformGlobalStats &stats) const {
    CGlobalStats trex_stats;
    g_trex.get_stats(trex_stats);

    stats.m_stats.m_cpu_util = trex_stats.m_cpu_util;
    if (get_is_stateless()) {
        stats.m_stats.m_rx_cpu_util = trex_stats.m_rx_cpu_util;
    }

    stats.m_stats.m_tx_bps             = trex_stats.m_tx_bps;
    stats.m_stats.m_tx_pps             = trex_stats.m_tx_pps;
    stats.m_stats.m_total_tx_pkts      = trex_stats.m_total_tx_pkts;
    stats.m_stats.m_total_tx_bytes     = trex_stats.m_total_tx_bytes;

    stats.m_stats.m_rx_bps             = trex_stats.m_rx_bps;
    stats.m_stats.m_rx_pps             = /*trex_stats.m_rx_pps*/ 0; /* missing */
    stats.m_stats.m_total_rx_pkts      = trex_stats.m_total_rx_pkts;
    stats.m_stats.m_total_rx_bytes     = trex_stats.m_total_rx_bytes;
}

void
TrexDpdkPlatformApi::get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const {

}

uint8_t
TrexDpdkPlatformApi::get_dp_core_count() const {
    return CGlobalInfo::m_options.get_number_of_dp_cores_needed();
}


void
TrexDpdkPlatformApi::port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {

    CPhyEthIF *lpt = &g_trex.m_ports[port_id];

    /* copy data from the interface */
    cores_id_list = lpt->get_core_list();
}


void
TrexDpdkPlatformApi::get_interface_info(uint8_t interface_id, intf_info_st &info) const {
    struct ether_addr rte_mac_addr;

    info.driver_name = CTRexExtendedDriverDb::Ins()->get_driver_name();
    info.has_crc     = CTRexExtendedDriverDb::Ins()->get_drv()->has_crc_added();

    /* mac INFO */

    /* hardware */
    g_trex.m_ports[interface_id].get_port_attr()->get_hw_src_mac(&rte_mac_addr);
    assert(ETHER_ADDR_LEN == 6);

    memcpy(info.hw_macaddr, rte_mac_addr.addr_bytes, 6);

    info.numa_node =  g_trex.m_ports[interface_id].m_dev_info.pci_dev->numa_node;
    struct rte_pci_addr *loc = &g_trex.m_ports[interface_id].m_dev_info.pci_dev->addr;

    char pci_addr[50];
    snprintf(pci_addr, sizeof(pci_addr), PCI_PRI_FMT, loc->domain, loc->bus, loc->devid, loc->function);
    info.pci_addr = pci_addr;

}

void
TrexDpdkPlatformApi::publish_async_data_now(uint32_t key, bool baseline) const {
    g_trex.publish_async_data(true, baseline);
    g_trex.publish_async_barrier(key);
}

void
TrexDpdkPlatformApi::publish_async_port_attr_changed(uint8_t port_id) const {
    g_trex.publish_async_port_attr_changed(port_id);
}

void
TrexDpdkPlatformApi::get_interface_stat_info(uint8_t interface_id, uint16_t &num_counters, uint16_t &capabilities) const {
    num_counters = CTRexExtendedDriverDb::Ins()->get_drv()->get_stat_counters_num();
    capabilities = CTRexExtendedDriverDb::Ins()->get_drv()->get_rx_stat_capabilities();
}

int TrexDpdkPlatformApi::get_flow_stats(uint8 port_id, void *rx_stats, void *tx_stats, int min, int max, bool reset
                                        , TrexPlatformApi::driver_stat_cap_e type) const {
    if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
        return g_trex.m_ports[port_id].get_flow_stats_payload((rx_per_flow_t *)rx_stats, (tx_per_flow_t *)tx_stats
                                                              , min, max, reset);
    } else {
        return g_trex.m_ports[port_id].get_flow_stats((rx_per_flow_t *)rx_stats, (tx_per_flow_t *)tx_stats
                                                      , min, max, reset);
    }
}

int TrexDpdkPlatformApi::get_rfc2544_info(void *rfc2544_info, int min, int max, bool reset) const {
    return g_trex.m_rx_sl.get_rfc2544_info((rfc2544_info_t *)rfc2544_info, min, max, reset);
}

int TrexDpdkPlatformApi::get_rx_err_cntrs(void *rx_err_cntrs) const {
    return g_trex.m_rx_sl.get_rx_err_cntrs((CRxCoreErrCntrs *)rx_err_cntrs);
}

int TrexDpdkPlatformApi::reset_hw_flow_stats(uint8_t port_id) const {
    return g_trex.m_ports[port_id].reset_hw_flow_stats();
}

int TrexDpdkPlatformApi::add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                               , uint8_t ipv6_next_h, uint16_t id) const {
    return CTRexExtendedDriverDb::Ins()->get_drv()
        ->add_del_rx_flow_stat_rule(port_id, RTE_ETH_FILTER_ADD, l3_type, l4_proto, ipv6_next_h, id);
}

int TrexDpdkPlatformApi::del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                               , uint8_t ipv6_next_h, uint16_t id) const {
    return CTRexExtendedDriverDb::Ins()->get_drv()
        ->add_del_rx_flow_stat_rule(port_id, RTE_ETH_FILTER_DELETE, l3_type, l4_proto, ipv6_next_h, id);
}

void TrexDpdkPlatformApi::flush_dp_messages() const {
    g_trex.check_for_dp_messages();
}

int TrexDpdkPlatformApi::get_active_pgids(flow_stat_active_t &result) const {
    return g_trex.m_trex_stateless->m_rx_flow_stat.get_active_pgids(result);
}

int TrexDpdkPlatformApi::get_cpu_util_full(cpu_util_full_t &cpu_util_full) const {
    uint8_t p1;
    uint8_t p2;

    cpu_util_full.resize((int)g_trex.m_fl.m_threads_info.size());
    for (int thread_id=0; thread_id<(int)g_trex.m_fl.m_threads_info.size(); thread_id++) {

        /* history */
        CFlowGenListPerThread *lp = g_trex.m_fl.m_threads_info[thread_id];
        cpu_vct_st &per_cpu = cpu_util_full[thread_id];
        lp->m_cpu_cp_u.GetHistory(per_cpu);


        /* active ports */
        lp->get_port_ids(p1, p2);
        per_cpu.m_port1 = (lp->is_port_active(p1) ? p1 : -1);
        per_cpu.m_port2 = (lp->is_port_active(p2) ? p2 : -1);

    }
    return 0;
}

int TrexDpdkPlatformApi::get_mbuf_util(Json::Value &mbuf_pool) const {
    CGlobalInfo::dump_pool_as_json(mbuf_pool);
    return 0;
}

CFlowStatParser *TrexDpdkPlatformApi::get_flow_stat_parser() const {
    return CTRexExtendedDriverDb::Ins()->get_drv()->get_flow_stat_parser();
}

TRexPortAttr *TrexDpdkPlatformApi::getPortAttrObj(uint8_t port_id) const {
    return g_trex.m_ports[port_id].get_port_attr();
}


int DpdkTRexPortAttr::set_rx_filter_mode(rx_filter_mode_e rx_filter_mode) {

    if (rx_filter_mode == m_rx_filter_mode) {
        return (0);
    }

    CPhyEthIF *_if = &g_trex.m_ports[m_port_id];
    bool recv_all = (rx_filter_mode == RX_FILTER_MODE_ALL);
    int rc = CTRexExtendedDriverDb::Ins()->get_drv()->set_rcv_all(_if, recv_all);
    if (rc != 0) {
        return (rc);
    }

    m_rx_filter_mode = rx_filter_mode;

    return (0);
}

bool DpdkTRexPortAttr::is_loopback() const {
    uint8_t port_id;
    return g_trex.lookup_port_by_mac(m_dest.get_dest_mac(), port_id);
}

/**
 * marks the control plane for a total server shutdown
 *
 * @author imarom (7/27/2016)
 */
void TrexDpdkPlatformApi::mark_for_shutdown() const {
    g_trex.mark_for_shutdown(CGlobalTRex::SHUTDOWN_RPC_REQ);
}

