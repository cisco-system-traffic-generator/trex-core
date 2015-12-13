/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

// DPDK c++ issue 
#define UINT8_MAX 255
#define UINT16_MAX 0xFFFF
// DPDK c++ issue 

#include <pwd.h>
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
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_random.h>
#include "bp_sim.h"
#include "latency.h"
#include "os_time.h"
#include <common/arg/SimpleGlob.h>
#include <common/arg/SimpleOpt.h>
#include <common/basic_utils.h>

#include <stateless/cp/trex_stateless.h>
#include <stateless/dp/trex_stream_node.h>
#include <publisher/trex_publisher.h>
#include <stateless/messaging/trex_stateless_messaging.h>

#include <../linux_dpdk/version.h>

extern "C" {
  #include <dpdk_lib18/librte_pmd_ixgbe/ixgbe/ixgbe_type.h>
}
#include <dpdk_lib18/librte_pmd_e1000/e1000/e1000_regs.h>
#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "global_io_mode.h"
#include "utl_term_io.h"
#include "msg_manager.h"
#include "platform_cfg.h"
#include "latency.h"

#include <internal_api/trex_platform_api.h>

#define RX_CHECK_MIX_SAMPLE_RATE 8
#define RX_CHECK_MIX_SAMPLE_RATE_1G 2


#define SOCKET0         0

#define BP_MAX_PKT      32
#define MAX_PKT_BURST   32


#define BP_MAX_PORTS (MAX_LATENCY_PORTS)
#define BP_MAX_CORES 32
#define BP_MAX_TX_QUEUE 16
#define BP_MASTER_AND_LATENCY 2

#define RTE_TEST_RX_DESC_DEFAULT 64
#define RTE_TEST_RX_LATENCY_DESC_DEFAULT (1*1024)

#define RTE_TEST_RX_DESC_VM_DEFAULT 512
#define RTE_TEST_TX_DESC_VM_DEFAULT 512

typedef struct rte_mbuf * (*rte_mbuf_convert_to_one_seg_t)(struct rte_mbuf *m);
struct rte_mbuf *  rte_mbuf_convert_to_one_seg(struct rte_mbuf *m);
extern "C" int vmxnet3_xmit_set_callback(rte_mbuf_convert_to_one_seg_t cb);



#define RTE_TEST_TX_DESC_DEFAULT 512
#define RTE_TEST_RX_DESC_DROP    0

static inline int get_vm_one_queue_enable(){
    return (CGlobalInfo::m_options.preview.get_vm_one_queue_enable() ?1:0);
}

static inline int get_is_latency_thread_enable(){
    return (CGlobalInfo::m_options.is_latency_enabled() ?1:0);
}

struct port_cfg_t;
class  CPhyEthIF;
class CPhyEthIFStats ;

class CTRexExtendedDriverBase {
public:

    virtual TrexPlatformApi::driver_speed_e get_driver_speed() = 0;

    virtual int get_min_sample_rate(void)=0;
    virtual void update_configuration(port_cfg_t * cfg)=0;
    virtual void update_global_config_fdir(port_cfg_t * cfg)=0;

    virtual bool is_hardware_filter_is_supported(){
        return(false);
    }
    virtual int configure_rx_filter_rules(CPhyEthIF * _if)=0;

    virtual bool is_hardware_support_drop_queue(){
        return(false);
    }

    virtual int configure_drop_queue(CPhyEthIF * _if)=0;
    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats)=0;
    virtual void clear_extended_stats(CPhyEthIF * _if)=0;
    virtual int  wait_for_stable_link()=0;
};


class CTRexExtendedDriverBase1G : public CTRexExtendedDriverBase {

public:
    CTRexExtendedDriverBase1G(){
    }

    TrexPlatformApi::driver_speed_e get_driver_speed() {
        return TrexPlatformApi::SPEED_1G;
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

    virtual int configure_rx_filter_rules(CPhyEthIF * _if);

    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }

    virtual int configure_drop_queue(CPhyEthIF * _if);

    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);

    virtual void clear_extended_stats(CPhyEthIF * _if);

    virtual int wait_for_stable_link();
};

class CTRexExtendedDriverBase1GVm : public CTRexExtendedDriverBase {

public:
    CTRexExtendedDriverBase1GVm(){
        /* we are working in mode that we have 1 queue for rx and one queue for tx*/
        CGlobalInfo::m_options.preview.set_vm_one_queue_enable(true);
    }

    TrexPlatformApi::driver_speed_e get_driver_speed() {
        return TrexPlatformApi::SPEED_1G;
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

    virtual int configure_drop_queue(CPhyEthIF * _if);


    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);

    virtual void clear_extended_stats(CPhyEthIF * _if);

    virtual int wait_for_stable_link();
};


class CTRexExtendedDriverBase10G : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBase10G(){
    }

    TrexPlatformApi::driver_speed_e get_driver_speed() {
        return TrexPlatformApi::SPEED_10G;
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

    virtual bool is_hardware_support_drop_queue(){
        return(true);
    }
    virtual int configure_drop_queue(CPhyEthIF * _if);

    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
};

class CTRexExtendedDriverBase40G : public CTRexExtendedDriverBase10G {
public:
    CTRexExtendedDriverBase40G(){
    }

    TrexPlatformApi::driver_speed_e get_driver_speed() {
        return TrexPlatformApi::SPEED_40G;
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase40G() );
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
    virtual int configure_drop_queue(CPhyEthIF * _if);



    virtual void get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
private:
    void add_rules(CPhyEthIF * _if,
                   enum rte_eth_flow_type type,
                   uint8_t ttl);
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

        /* virtual devices */
        register_driver(std::string("rte_em_pmd"),CTRexExtendedDriverBase1GVm::create);
        register_driver(std::string("rte_vmxnet3_pmd"),CTRexExtendedDriverBase1GVm::create);
        register_driver(std::string("rte_virtio_pmd"),CTRexExtendedDriverBase1GVm::create);
        register_driver(std::string("rte_enic_pmd"),CTRexExtendedDriverBase1GVm::create);
        



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

#define MAX_DPDK_ARGS 40
static CPlatformYamlInfo global_platform_cfg_info;
static int global_dpdk_args_num ;
static char * global_dpdk_args[MAX_DPDK_ARGS];
static char global_cores_str[100];
static char global_prefix_str[100];
static char global_loglevel_str[20];

// cores =0==1,1*2,2,3,4,5,6
// An enum for all the option types
enum { OPT_HELP, 
    OPT_MODE_BATCH, 
    OPT_MODE_INTERACTIVE,
    OPT_NODE_DUMP,  
    OPT_UT,
    OPT_FILE_OUT,
    OPT_REAL_TIME,
    OPT_CORES,
    OPT_SINGLE_CORE,
    OPT_FLIP_CLIENT_SERVER,
    OPT_FLOW_FLIP_CLIENT_SERVER,
    OPT_FLOW_FLIP_CLIENT_SERVER_SIDE,
    OPT_BW_FACTOR,
    OPT_DURATION,
    OPT_PLATFORM_FACTOR,
    OPT_PUB_DISABLE,
    OPT_LIMT_NUM_OF_PORTS,
    OPT_PLAT_CFG_FILE,


    OPT_LATENCY,
    OPT_NO_CLEAN_FLOW_CLOSE,
    OPT_LATENCY_MASK,
    OPT_ONLY_LATENCY,
    OPT_1G_MODE,
    OPT_LATENCY_PREVIEW ,
    OPT_PCAP,
	OPT_RX_CHECK,
    OPT_IO_MODE,
    OPT_IPV6,
    OPT_LEARN,
    OPT_LEARN_VERIFY,
    OPT_L_PKT_MODE,
    OPT_NO_FLOW_CONTROL,
    OPT_RX_CHECK_HOPS,
	OPT_MAC_FILE,
    OPT_NO_KEYBOARD_INPUT,
	OPT_VLAN,
    OPT_VIRT_ONE_TX_RX_QUEUE,
    OPT_PREFIX,
    OPT_MAC_SPLIT

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
    { OPT_REAL_TIME ,             "-r",                SO_NONE  },
    { OPT_SINGLE_CORE,            "-s",                SO_NONE  },
    { OPT_FILE_OUT,               "-o" ,               SO_REQ_SEP},
    { OPT_FLIP_CLIENT_SERVER,"--flip",SO_NONE  },
    { OPT_FLOW_FLIP_CLIENT_SERVER,"-p",SO_NONE  },
    { OPT_FLOW_FLIP_CLIENT_SERVER_SIDE,"-e",SO_NONE  },

    { OPT_NO_CLEAN_FLOW_CLOSE,"--nc",SO_NONE  },

    { OPT_LIMT_NUM_OF_PORTS,"--limit-ports", SO_REQ_SEP },
    { OPT_CORES     , "-c",         SO_REQ_SEP },
    { OPT_NODE_DUMP , "-v",         SO_REQ_SEP },
    { OPT_LATENCY , "-l",         SO_REQ_SEP },

    { OPT_DURATION     , "-d",  SO_REQ_SEP },
    { OPT_PLATFORM_FACTOR     , "-pm",  SO_REQ_SEP },

    { OPT_PUB_DISABLE     , "-pubd",  SO_NONE },


    { OPT_BW_FACTOR     , "-m",  SO_REQ_SEP },
    { OPT_LATENCY_MASK     , "--lm",  SO_REQ_SEP },
    { OPT_ONLY_LATENCY, "--lo",  SO_NONE  },

    { OPT_1G_MODE,       "-1g",   SO_NONE   },
    { OPT_LATENCY_PREVIEW ,       "-k",   SO_REQ_SEP   },
    { OPT_PCAP,       "--pcap",       SO_NONE   },
	{ OPT_RX_CHECK,   "--rx-check",  SO_REQ_SEP },
    { OPT_IO_MODE,   "--iom",  SO_REQ_SEP },      
    { OPT_RX_CHECK_HOPS, "--hops", SO_REQ_SEP },
    { OPT_IPV6,       "--ipv6",       SO_NONE   },
    { OPT_LEARN, "--learn",       SO_NONE   },
    { OPT_LEARN_VERIFY, "--learn-verify",       SO_NONE   },
    { OPT_L_PKT_MODE, "--l-pkt-mode",       SO_REQ_SEP   },
    { OPT_NO_FLOW_CONTROL, "--no-flow-control",       SO_NONE   },
    { OPT_VLAN,       "--vlan",       SO_NONE   },
    { OPT_MAC_FILE, "--mac", SO_REQ_SEP }, 
    { OPT_NO_KEYBOARD_INPUT ,"--no-key", SO_NONE   },
    { OPT_VIRT_ONE_TX_RX_QUEUE, "--vm-sim", SO_NONE }, 
    { OPT_PREFIX, "--prefix", SO_REQ_SEP }, 
    { OPT_MAC_SPLIT, "--mac-spread", SO_REQ_SEP },

    SO_END_OF_OPTIONS
};




static int usage(){

    printf(" Usage: t-rex-64 [MODE] [OPTION] -f cfg.yaml -c cores   \n");
    printf(" \n");
    printf(" \n");
    
    printf(" mode \n\n");
    printf(" -f [file]                  : YAML file  with template configuration \n");
    printf(" -i                         : launch TRex in interactive mode (RPC server)\n");
    printf(" \n\n");

    printf(" options \n\n");

    printf(" --mac [file]               : YAML file with <client ip, mac addr> configuration \n");
    printf(" \n\n");
    printf(" -c [number of threads]     : default is 1. number of threads to allocate for each dual ports. \n");
    printf("  \n");
    printf(" -s                         : run only one data path core. for debug\n");
    printf("  \n");
    printf(" --flip                     : flow will be sent from client->server and server->client for maximum throughput \n");
    printf("  \n");
    printf(" -p                         : flow-flip , send all flow packets from the same interface base of client ip \n");
    printf(" -e                         : like -p but comply to the generator rules  \n");

    printf("  \n");
    printf(" -l [pkt/sec]               : run latency daemon in this rate  \n");
    printf("    e.g -l 1000 run 1000 pkt/sec from each interface , zero mean to disable latency check  \n");
    printf(" --lm                         : latency mask  \n");
    printf("    0x1 only port 0 will send traffic  \n");
    printf(" --lo                         :only latency test   \n");

    printf("  \n");

    printf(" --limit-ports              : limit number of ports, must be even e.g. 2,4  \n");
    printf("  \n");
    printf(" --nc                       : If set, will not wait for all the flows to be closed, terminate faster- see manual for more information   \n");
    printf("  \n");
    printf(" -d                         : duration of the test in sec. look for --nc  \n");
    printf("  \n");
    printf(" -pm                        : platform factor ,in case you have splitter in the setup you can multiply the total results in this factor  \n");
    printf("    e.g --pm 2.0 will multiply all the results bps in this factor   \n");
    printf("  \n");
    printf(" -pubd                      : disable monitors publishers  \n");

    printf(" -m                         : factor of bandwidth \n");
    printf("  \n");
    printf(" -k  [sec]                  : run latency test before starting the test. it will wait for x sec sending packet and x sec after that  \n");
    printf("  \n");

    printf(" --cfg [platform_yaml]      : load and configure platform using this file see example in cfg/cfg_examplexx.yaml file  \n");
    printf("                              this file is used to configure/mask interfaces cores affinity and mac addr  \n");
    printf("                              you can copy this file to /etc/trex_cfg.yaml   \n");
    printf("  \n");

    printf(" --ipv6                     : work in ipv6 mode\n");

    printf(" --learn                    : Work in NAT environments, learn the dynamic NAT translation and ALG  \n");
    printf(" --learn-verify             : Learn the translation, but intended for verification of the mechanism in cases that NAT does not exist \n");
    printf("  \n");
    printf(" --l-pkt-mode [0-3]         : Set mode for sending latency packets.\n");
    printf("      0 (default)    send SCTP packets  \n");
    printf("      1              Send ICMP request packets  \n");
    printf("      2              Send ICMP requests from client side, and response from server side (for working with firewall) \n");
    printf("      3              Send ICMP requests with sequence ID 0 from both sides \n");
    printf(" -v  [1-3]                  :  verbose mode ( works only on the debug image ! )  \n");
    printf("      1    show only stats  \n");
    printf("      2    run preview do not write to file  \n");
    printf("      3    run preview write stats file  \n");
    printf("  Note in case of verbose mode you don't need to add the output file \n");
    printf("   \n");
    printf("  Warning : This program can generate huge-files (TB ) watch out! try this only on local drive \n");
    printf(" \n");
	printf("  \n");
	printf(" --rx-check  [sample]       :  enable rx check thread, using this thread we sample flows 1/sample and check order,latency and more  \n");
	printf("                              this feature consume another thread  \n");
    printf("  \n");
    printf(" --hops [hops]              :  If rx check is enabled, the hop number can be assigned. The default number of hops is 1\n");
    printf(" --iom  [mode]              :  io mode for interactive mode [0- silent, 1- normal , 2- short]   \n");
    printf("                              this feature consume another thread  \n");
    printf("  \n");
    printf(" --no-key                   : daemon mode, don't get input from keyboard \n");
    printf(" --no-flow-control          : In default TRex disables flow-control using this flag it does not touch it \n");
    printf(" --prefix                   : for multi trex, each instance should have a different name \n");
    printf(" --mac-spread               : Spread the destination mac-order by this factor. e.g 2 will generate the traffic to 2 devices DEST-MAC ,DEST-MAC+1  \n");
    printf("                             maximum is up to 128 devices   \n");
    
    
    printf("\n simulation mode : \n");
    printf(" Using this mode you can generate the traffic into a pcap file and learn how trex works \n");
    printf(" With this version you must be SUDO to use this mode ( I know this is not normal )  \n");
    printf(" you can use the Linux CEL version of t-rex to do it without super user   \n");
    printf("  \n");
    printf(" -o [capfile_name]  simulate trex into pcap file  \n");
    printf(" --pcap             export the file in pcap mode \n");
    printf(" bp-sim-64 -d 10 -f cfg.yaml  -o my.pcap --pcap  # export 10 sec of what Trex will do on real-time to a file my.pcap \n");
    printf(" --vm-sim               : simulate vm with driver of one input queue and one output queue \n");
    printf("  \n");
    printf(" Examples: ");
    printf(" basic trex run for 10 sec and multiplier of x10 \n");
    printf("  #>t-rex-64 -f cfg.yaml  -m 10 -d 10 \n");
    printf("  \n ");

    printf("  preview show csv stats \n");
    printf("  #>t-rex-64 -c 1 -f cfg.yaml -v 1 -p -m 10 -d 10 --nc -l 1000\n");
    printf("  \n ");

    printf("  5)   ! \n");
    printf("  #>t-rex-64 -f cfg.yaml -c 1 --flip \n");

    printf("\n");
    printf("\n");
    printf(" Copyright (c) 2015-2015 Cisco Systems, Inc.    \n");
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

            case OPT_MAC_FILE :
                po->mac_file = args.OptionArg();
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

            case OPT_VLAN:
                po->preview.set_vlan_mode_enable(true);
                break;

            case OPT_LEARN :
                po->preview.set_lean_mode_enable(true);
                break;

            case OPT_LEARN_VERIFY :
                po->preview.set_lean_mode_enable(true);
                po->preview.set_lean_and_verify_mode_enable(true);
                break;

            case OPT_L_PKT_MODE :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_l_pkt_mode=(uint8_t)tmp_data;
                break;

            case OPT_REAL_TIME  :
                printf(" warning -r is deprecated, real time is not needed any more , it is the default \n");
                po->preview.setRealTime(true);
                break;

            case OPT_NO_FLOW_CONTROL:
                po->preview.set_disable_flow_control_setting(true);
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
            case OPT_FILE_OUT:
                po->out_file = args.OptionArg();
                break;
            case OPT_NODE_DUMP:
                a=atoi(args.OptionArg());
                node_dump=1;
                po->preview.setFileWrite(false);
                break;
            case OPT_BW_FACTOR :
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
            case OPT_1G_MODE :
                po->preview.set_1g_mode(true);
                break;

            case  OPT_LATENCY_PREVIEW :
                sscanf(args.OptionArg(),"%d", &po->m_latency_prev);
                break;

            case OPT_PCAP:
                po->preview.set_pcap_mode_enable(true);
                break;

            case OPT_RX_CHECK :
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_rx_check_sampe=(uint16_t)tmp_data;
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

            case OPT_MAC_SPLIT:
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_mac_splitter = (uint8_t)tmp_data;
                po->preview.set_mac_ip_features_enable(true);
                po->preview.setDestMacSplit(true);
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
        parse_err("Please provide single run mode (e.g. batch or interactive)");
     }

    if ( po->m_mac_splitter > 128 ){
        std::stringstream ss;
        ss << "maximum mac spreading is 128 you set it to: " << po->m_mac_splitter;
        parse_err(ss.str());
    }

    if ( po->preview.get_learn_mode_enable()  ){
        if  ( po->preview.get_ipv6_mode_enable() ){
            parse_err("--learn mode is not supported with --ipv6, beacuse there is not such thing NAT66 ( ipv6-ipv6) \n" \
                      "if you think it is important,open a defect \n");
        }
        if ( po->is_latency_disabled() ){
            /* set latency thread */
            po->m_latency_rate =1000;
        }
    }

    if (po->preview.get_is_rx_check_enable() &&  ( po->is_latency_disabled() ) ) {
        printf(" rx check must be enabled with latency check. try adding '-l 1000'   \n");
        return -1;
    }

    if ( node_dump ){
        po->preview.setVMode(a);
    }

    /* if we have a platform factor we need to devided by it so we can still work with normalized yaml profile  */
    po->m_factor = po->m_factor/po->m_platform_factor;

    uint32_t cores=po->preview.getCores();
    if ( cores > ((BP_MAX_CORES)/2-1) ) {
        printf(" ERROR maximum supported cores are : %d \n",((BP_MAX_CORES)/2-1));
        return -1;
    }


    if ( first_time ){
        /* only first time read the configuration file */
        if ( po->platform_cfg_file.length() >0  ) {
            if ( node_dump ){
               printf("load platform configuration file from %s \n",po->platform_cfg_file.c_str());
            }
            global_platform_cfg_info.load_from_yaml_file(po->platform_cfg_file);
            if ( node_dump ){
                global_platform_cfg_info.Dump(stdout);
            }
        }else{
            if ( utl_is_file_exists("/etc/trex_cfg.yaml") ){
                printf("found configuration file at /etc/trex_cfg.yaml \n");
                global_platform_cfg_info.load_from_yaml_file("/etc/trex_cfg.yaml");
                if ( node_dump ){
                    global_platform_cfg_info.Dump(stdout);
                }
            }
        }
    }

    if ( get_is_stateless() ) {
        if ( po->preview.get_is_rx_check_enable() ) {
            parse_err("Rx check is not supported with interactive mode ");
        }

        if  ( (! po->is_latency_disabled()) || (po->preview.getOnlyLatency()) ){
            parse_err("Latecny check is not supported with interactive mode ");
        }

        if ( po->preview.getSingleCore() ){
            parse_err("single core is not supported with interactive mode ");
        }

    }
    return 0;
}


int main_test(int argc , char * argv[]);


//static const char * default_argv[] = {"xx","-c", "0x7", "-n","2","-b","0000:0b:01.01"};
//static int argv_num = 7;
                                             


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
        memset(&m_port_conf,0,sizeof(rte_eth_conf));
        memset(&m_rx_conf,0,sizeof(rte_eth_rxconf));
        memset(&m_tx_conf,0,sizeof(rte_eth_rxconf));
        memset(&m_rx_drop_conf,0,sizeof(rte_eth_rxconf));
        

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
        m_port_conf.rxmode.max_rx_pkt_len =2000;
        m_port_conf.rxmode.hw_strip_crc=1;
    }



	inline void update_var(void){
        get_ex_drv()->update_configuration(this);
    }

    inline void update_global_config_fdir(void){
        get_ex_drv()->update_global_config_fdir(this);
    }

	/* enable FDIR */
	inline void update_global_config_fdir_10g_1g(void){
		m_port_conf.fdir_conf.mode=RTE_FDIR_MODE_PERFECT;
		m_port_conf.fdir_conf.pballoc=RTE_FDIR_PBALLOC_64K;
		m_port_conf.fdir_conf.status=RTE_FDIR_NO_REPORT_STATUS; 
		/* Offset of flexbytes field in RX packets (in 16-bit word units). */
		/* Note: divide by 2 to convert byte offset to word offset */
		if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
			m_port_conf.fdir_conf.flexbytes_offset=(14+6)/2;
		}else{
			m_port_conf.fdir_conf.flexbytes_offset=(14+8)/2;
		}
                        
		/* Increment offset 4 bytes for the case where we add VLAN */
		if (  CGlobalInfo::m_options.preview.get_vlan_mode_enable() ){
	        	m_port_conf.fdir_conf.flexbytes_offset+=(4/2);
		}
		m_port_conf.fdir_conf.drop_queue=1;
	}

    inline void update_global_config_fdir_40g(void){
        m_port_conf.fdir_conf.mode=RTE_FDIR_MODE_PERFECT;
        m_port_conf.fdir_conf.pballoc=RTE_FDIR_PBALLOC_64K;
        m_port_conf.fdir_conf.status=RTE_FDIR_NO_REPORT_STATUS; 
        /* Offset of flexbytes field in RX packets (in 16-bit word units). */
        /* Note: divide by 2 to convert byte offset to word offset */
        #if 0
        if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
            m_port_conf.fdir_conf.flexbytes_offset=(14+6)/2;
        }else{
            m_port_conf.fdir_conf.flexbytes_offset=(14+8)/2;
        }

        /* Increment offset 4 bytes for the case where we add VLAN */
        if (  CGlobalInfo::m_options.preview.get_vlan_mode_enable() ){
                m_port_conf.fdir_conf.flexbytes_offset+=(4/2);
        }
        #endif

    // TBD Flow Director does not work with XL710 yet we need to understand why 
    #if 0
        struct rte_eth_fdir_flex_conf * lp = &m_port_conf.fdir_conf.flex_conf;

        //lp->nb_flexmasks=1;
        //lp->flex_mask[0].flow_type=RTE_ETH_FLOW_TYPE_SCTPV4;
        //memset(lp->flex_mask[0].mask,0xff,RTE_ETH_FDIR_MAX_FLEXLEN);

        lp->nb_payloads=1;
        lp->flex_set[0].type = RTE_ETH_L3_PAYLOAD; 
        lp->flex_set[0].src_offset[0]=8;

        //m_port_conf.fdir_conf.drop_queue=1;
    #endif
    }

    struct rte_eth_conf     m_port_conf;
    struct rte_eth_rxconf   m_rx_conf;
    struct rte_eth_rxconf   m_rx_drop_conf;
    struct rte_eth_txconf   m_tx_conf;
};


/* this object is per core / per port / per queue 
   each core will have 2 ports to send too  


       port0                                port1

 0,1,2,3,..15 out queue ( per core )       0,1,2,3,..15 out queue ( per core )

*/


typedef struct cnt_name_ {
    uint32_t offset;
    char * name;
}cnt_name_t ;

#define MY_REG(a) {a,(char *)#a}


class CPhyEthIFStats {

public:
    uint64_t ipackets;  /**< Total number of successfully received packets. */
    uint64_t ibytes;    /**< Total number of successfully received bytes. */

    uint64_t f_ipackets;  /**< Total number of successfully received packets - filter SCTP*/
    uint64_t f_ibytes;    /**< Total number of successfully received bytes. - filter SCTP */

    uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
    uint64_t obytes;    /**< Total number of successfully transmitted bytes. */

    uint64_t ierrors;   /**< Total number of erroneous received packets. */
    uint64_t oerrors;   /**< Total number of failed transmitted packets. */
    uint64_t imcasts;   /**< Total number of multicast received packets. */
    uint64_t rx_nombuf; /**< Total number of RX mbuf allocation failures. */


public:
    void Clear();
    void Dump(FILE *fd);
    void DumpAll(FILE *fd);
};

void CPhyEthIFStats::Clear(){

    ipackets =0;
    ibytes =0   ; 

    f_ipackets=0;
    f_ibytes=0;    

    opackets=0;
    obytes=0;    

    ierrors=0;  
    oerrors=0;   
    imcasts=0;   
    rx_nombuf=0; 
}


void CPhyEthIFStats::DumpAll(FILE *fd){

    #define DP_A4(f) printf(" %-40s : %llu \n",#f, (unsigned long long)f)
    #define DP_A(f) if (f) printf(" %-40s : %llu \n",#f, (unsigned long long)f)
    DP_A4(opackets);  
    DP_A4(obytes);      
    DP_A4(ipackets);  
    DP_A4(ibytes);      
    DP_A(ierrors);     
    DP_A(oerrors);     

}


void CPhyEthIFStats::Dump(FILE *fd){

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



class CPhyEthIF  {
public:
    CPhyEthIF (){
        m_port_id=0;
        m_rx_queue=0;
    }
    bool Create(uint8_t portid){
        m_port_id      = portid;
        m_last_rx_rate = 0.0;
        m_last_tx_rate = 0.0;
        m_last_tx_pps  = 0.0;
        return (true);
    }
    void Delete();

    void set_rx_queue(uint8_t rx_queue){
        m_rx_queue=rx_queue;
    }


    void configure(uint16_t nb_rx_queue,
				  uint16_t nb_tx_queue,
				  const struct rte_eth_conf *eth_conf);

    void macaddr_get(struct ether_addr *mac_addr);

    void get_stats(CPhyEthIFStats *stats);

    void get_stats_1g(CPhyEthIFStats *stats);


    void rx_queue_setup(uint16_t rx_queue_id,
                        uint16_t nb_rx_desc, 
                        unsigned int socket_id,
                        const struct rte_eth_rxconf *rx_conf,
                        struct rte_mempool *mb_pool);

    void tx_queue_setup(uint16_t tx_queue_id,
                        uint16_t nb_tx_desc, 
                        unsigned int socket_id,
                        const struct rte_eth_txconf *tx_conf);

    void configure_rx_drop_queue();

    void configure_rx_duplicate_rules();

    void start();

    void stop();

    void update_link_status();

    bool is_link_up(){
        return (m_link.link_status?true:false);
    }

    void dump_link(FILE *fd);

    void disable_flow_control();

    void set_promiscuous(bool enable);

    void add_mac(char * mac);


    bool get_promiscuous();

    void dump_stats(FILE *fd);

    void update_counters();


    void stats_clear();

    uint8_t             get_port_id(){
            return (m_port_id);
    }

    float get_last_tx_rate(){
        return (m_last_tx_rate);
    }

    float get_last_rx_rate(){
        return (m_last_rx_rate);
    }

    float get_last_tx_pps_rate(){
        return (m_last_tx_pps);
    }

    float get_last_rx_pps_rate(){
        return (m_last_rx_pps);
    }

    CPhyEthIFStats     & get_stats(){
              return ( m_stats );
    }

    void flush_rx_queue(void);

public:

    inline uint16_t  tx_burst(uint16_t queue_id,
                                struct rte_mbuf **tx_pkts, 
                                uint16_t nb_pkts);

    inline uint16_t  rx_burst(uint16_t queue_id,
                                struct rte_mbuf **rx_pkts, 
                                uint16_t nb_pkts);


    inline uint32_t pci_reg_read(uint32_t reg_off){
    	void *reg_addr;
    	uint32_t reg_v;
    	reg_addr = (void *)((char *)m_dev_info.pci_dev->mem_resource[0].addr +
    			    reg_off);
        reg_v = *((volatile uint32_t *)reg_addr);
        return rte_le_to_cpu_32(reg_v);
    }


    inline void pci_reg_write(uint32_t reg_off, 
                              uint32_t reg_v){
    	void *reg_addr;
    
    	reg_addr = (void *)((char *)m_dev_info.pci_dev->mem_resource[0].addr +
    			    reg_off);
    	*((volatile uint32_t *)reg_addr) = rte_cpu_to_le_32(reg_v);
    }

    void dump_stats_extended(FILE *fd);

    uint8_t                  get_rte_port_id(void){
                    return ( m_port_id );
    }
private:
    uint8_t                  m_port_id;
    uint8_t                  m_rx_queue;
    struct rte_eth_link      m_link;
    uint64_t                 m_sw_try_tx_pkt;
    uint64_t                 m_sw_tx_drop_pkt;
    CBwMeasure               m_bw_tx;
    CBwMeasure               m_bw_rx;
    CPPSMeasure              m_pps_tx;
    CPPSMeasure              m_pps_rx;

    CPhyEthIFStats           m_stats;

    float                    m_last_tx_rate;
    float                    m_last_rx_rate;
    float                    m_last_tx_pps;
    float                    m_last_rx_pps;
public:
    struct rte_eth_dev_info  m_dev_info;   
};


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
    fprintf (fd," externded counter \n");
    int i;
    for (i=0; i<sizeof(reg)/sizeof(reg[0]); i++) {
        cnt_name_t *lp=&reg[i];
		uint32_t c=pci_reg_read(lp->offset);
		if (c) {
			fprintf (fd," %s  : %d \n",lp->name,c);
		}
    }
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


void CPhyEthIF::configure_rx_drop_queue(){

    if ( get_vm_one_queue_enable() ) {
        return;
    }
    if ( CGlobalInfo::m_options.is_latency_disabled()==false ) {
        if ( (!get_ex_drv()->is_hardware_support_drop_queue())  ) {
            printf(" ERROR latency feature is not supported with current hardware  \n");
            exit(1);
        }
    }
    get_ex_drv()->configure_drop_queue(this);
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
    rte_eth_dev_stop(m_port_id);
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

void CPhyEthIF::disable_flow_control(){
       if ( get_vm_one_queue_enable()  ){
           return;
       }
       int ret;
       if ( !CGlobalInfo::m_options.preview.get_is_disable_flow_control_setting()  ){
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
                  "err=%d, port=%u\n probably link is down please check you link activity or enable flow-control using this CLI flag --no-flow-control  \n",
                ret, m_port_id);
    }
}



void CPhyEthIF::dump_link(FILE *fd){
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

void CPhyEthIF::update_link_status(){
    rte_eth_link_get(m_port_id, &m_link);
}

void CPhyEthIF::add_mac(char * mac){
    struct ether_addr mac_addr;
    int i=0;
    for (i=0; i<6;i++) {
        mac_addr.addr_bytes[i] =mac[i];
    }
    rte_eth_dev_mac_addr_add(m_port_id, &mac_addr,0);
}

void CPhyEthIF::set_promiscuous(bool enable){
    if (enable) {
        rte_eth_promiscuous_enable(m_port_id);
    }else{
       rte_eth_promiscuous_disable(m_port_id);
    }
}

bool CPhyEthIF::get_promiscuous(){
    int ret=rte_eth_promiscuous_get(m_port_id);
    if (ret<0) {
        rte_exit(EXIT_FAILURE, "rte_eth_promiscuous_get: "
                "err=%d, port=%u\n",
                 ret, m_port_id);

    }
    return ( ret?true:false);
}


void CPhyEthIF::macaddr_get(struct ether_addr *mac_addr){
       rte_eth_macaddr_get(m_port_id , mac_addr);
}

void CPhyEthIF::get_stats_1g(CPhyEthIFStats *stats){ 

   stats->ipackets     +=  pci_reg_read(E1000_GPRC) ;

   stats->ibytes       +=  (pci_reg_read(E1000_GORCL) );
   stats->ibytes       +=  (((uint64_t)pci_reg_read(E1000_GORCH))<<32);
                        

   stats->opackets     +=  pci_reg_read(E1000_GPTC);
   stats->obytes       +=  pci_reg_read(E1000_GOTCL) ;
   stats->obytes       +=  ( (((uint64_t)pci_reg_read(IXGBE_GOTCH))<<32) );

   stats->f_ipackets   +=  0;
   stats->f_ibytes     += 0;


   stats->ierrors      +=  ( pci_reg_read(E1000_RNBC) +
                             pci_reg_read(E1000_CRCERRS) + 
                             pci_reg_read(E1000_ALGNERRC ) +
                             pci_reg_read(E1000_SYMERRS ) +
                             pci_reg_read(E1000_RXERRC ) +

                             pci_reg_read(E1000_ROC)+
                             pci_reg_read(E1000_RUC)+ 
                             pci_reg_read(E1000_RJC) +

                             pci_reg_read(E1000_XONRXC)+   
                            pci_reg_read(E1000_XONTXC)+
                            pci_reg_read(E1000_XOFFRXC)+
                            pci_reg_read(E1000_XOFFTXC)+
                            pci_reg_read(E1000_FCRUC)
                             );

   stats->oerrors      +=  0;
   stats->imcasts      =  0;
   stats->rx_nombuf    =  0;

   m_last_tx_rate      =  m_bw_tx.add(stats->obytes);
   m_last_rx_rate      =  m_bw_rx.add(stats->ibytes);
   m_last_tx_pps       =  m_pps_tx.add(stats->opackets);
   m_last_rx_pps       =  m_pps_rx.add(stats->ipackets);

}

void CPhyEthIF::get_stats(CPhyEthIFStats *stats){ 

   get_ex_drv()->get_extended_stats(this,stats);

   m_last_tx_rate      =  m_bw_tx.add(stats->obytes);
   m_last_rx_rate      =  m_bw_rx.add(stats->ibytes);
   m_last_tx_pps       =  m_pps_tx.add(stats->opackets);
   m_last_rx_pps       =  m_pps_rx.add(stats->ipackets);
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
    DP_A2(qprc,16)
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


void CPhyEthIF::update_counters(){ 
    get_stats(&m_stats);
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

inline uint16_t  CPhyEthIF::tx_burst(uint16_t queue_id,
                                       struct rte_mbuf **tx_pkts, 
                                       uint16_t nb_pkts){
    uint16_t ret = rte_eth_tx_burst(m_port_id, queue_id, tx_pkts, nb_pkts);
    return (ret);
}


inline uint16_t  CPhyEthIF::rx_burst(uint16_t queue_id,
                                struct rte_mbuf **rx_pkts, 
                                uint16_t nb_pkts){
   return (rte_eth_rx_burst(m_port_id, queue_id,
                                     rx_pkts, nb_pkts));

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
    uint16_t                m_tx_queue_id;
    uint16_t                m_len;
    rte_mbuf_t *            m_table[MAX_PKT_BURST];
    CPhyEthIF  *            m_port;  
};


#define MAX_MBUF_CACHE 100


/* per core/gbe queue port for trasmitt */
class CCoreEthIF : public CVirtualIF {

public:

    CCoreEthIF(){
        m_mbuf_cache=0;
    }

public:
    bool Create(uint8_t             core_id,
                uint16_t            tx_client_queue_id,
                CPhyEthIF  *        tx_client_port, 

                uint16_t            tx_server_queue_id,
                CPhyEthIF  *        tx_server_port);
    void Delete();

    virtual int open_file(std::string file_name){
        return (0);
    }

    virtual int close_file(void){
        return (flush_tx_queue());
    }

    virtual int send_node(CGenNode * node);
    virtual void send_one_pkt(pkt_dir_t       dir, rte_mbuf_t      *m);

    virtual int flush_tx_queue(void);

    __attribute__ ((noinline)) void flush_rx_queue();
    __attribute__ ((noinline)) void update_mac_addr(CGenNode * node,uint8_t *p);

    bool process_rx_pkt(pkt_dir_t   dir,rte_mbuf_t * m);

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, rte_mbuf_t      *m);

    virtual pkt_dir_t port_id_to_dir(uint8_t port_id);

public:
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



protected:
    uint8_t      m_core_id;
    uint16_t     m_mbuf_cache; 
    CCorePerPort m_ports[CS_NUM]; /* each core has 2 tx queues 1. client side and server side */
    CNodeRing *  m_ring_to_rx;

} __rte_cache_aligned; ;

class CCoreEthIFStateless : public CCoreEthIF {
public:
    virtual int send_node(CGenNode * node);
};

bool CCoreEthIF::Create(uint8_t             core_id,
                        uint16_t            tx_client_queue_id,
                        CPhyEthIF  *        tx_client_port, 

                        uint16_t            tx_server_queue_id,
                        CPhyEthIF  *        tx_server_port){
    m_ports[CLIENT_SIDE].m_tx_queue_id = tx_client_queue_id;
    m_ports[CLIENT_SIDE].m_port        = tx_client_port;

    m_ports[SERVER_SIDE].m_tx_queue_id = tx_server_queue_id;
    m_ports[SERVER_SIDE].m_port        = tx_server_port;
    m_core_id = core_id;

    CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();
    m_ring_to_rx = rx_dp->getRingDpToCp(core_id-1);
    assert( m_ring_to_rx);
    return (true);
}

void CCoreEthIF::flush_rx_queue(void){
    pkt_dir_t   dir ;
    bool is_latency=get_is_latency_thread_enable();
    for (dir=CLIENT_SIDE; dir<CS_NUM; dir++) {
        CCorePerPort * lp_port=&m_ports[dir];
        CPhyEthIF * lp=lp_port->m_port;

        rte_mbuf_t * rx_pkts[32];
        int j=0;

        while (true) {
            j++;
            uint16_t cnt =lp->rx_burst(0,rx_pkts,32);
            if ( cnt ) {
                int i;
                for (i=0; i<(int)cnt;i++) {
                    rte_mbuf_t * m=rx_pkts[i];
                    if ( is_latency ){
                        if (!process_rx_pkt(dir,m)){
                            rte_pktmbuf_free(m);
                        }
                    }else{
                        rte_pktmbuf_free(m);
                    }
                }
            }
            if ((cnt<5) || j>10 ) {
                break;
            }
        }
    }
}

int CCoreEthIF::flush_tx_queue(void){
    /* flush both sides */
    pkt_dir_t   dir ;
    for (dir=CLIENT_SIDE; dir<CS_NUM; dir++) {
        CCorePerPort * lp_port=&m_ports[dir];
        CVirtualIFPerSideStats  * lp_stats= &m_stats[dir];
        if ( likely(lp_port->m_len > 0) ) {
            send_burst(lp_port,lp_port->m_len,lp_stats);
             lp_port->m_len = 0;
        }
    }

    if ( unlikely( get_vm_one_queue_enable() ) ){
        /* try drain the rx packets */
        flush_rx_queue();
    }
    return (0);
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
    fprintf (fd," core ,  c-port, c-queue , s-port, s-queue \n");
    fprintf (fd," ------------------------------------------\n");
}

void CCoreEthIF::DumpIfCfg(FILE *fd){
    fprintf (fd," %d,   %u , %u , %u , %u  \n",m_core_id,
             m_ports[CLIENT_SIDE].m_port->get_port_id(),
             m_ports[CLIENT_SIDE].m_tx_queue_id,
             m_ports[SERVER_SIDE].m_port->get_port_id(),
             m_ports[SERVER_SIDE].m_tx_queue_id
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
        //rte_pause();
        //rte_pause();
        lp_stats->m_tx_queue_full += 1;
        uint16_t ret1=lp_port->m_port->tx_burst(lp_port->m_tx_queue_id,
                                        &lp_port->m_table[ret],
                                        len-ret);
        ret+=ret1;
    }
    #endif

    /* CPU has burst of packets , more that TX can send need to drop them !!*/
    if ( unlikely(ret < len) ) {
        lp_stats->m_tx_drop += (len-ret);
        uint16_t i;
        for (i=ret; i<len;i++) {
            rte_mbuf_t * m=lp_port->m_table[i];
            rte_pktmbuf_free(m);
        }
    }

    return (0);
}
                         

int CCoreEthIF::send_pkt(CCorePerPort * lp_port,
                         rte_mbuf_t      *m,
                         CVirtualIFPerSideStats  * lp_stats
                         ){

    lp_stats->m_tx_pkt   +=1;
    lp_stats->m_tx_bytes += (rte_pktmbuf_pkt_len(m)+4);

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



void CCoreEthIF::send_one_pkt(pkt_dir_t       dir, 
                              rte_mbuf_t      *m){
    CCorePerPort *  lp_port=&m_ports[dir];
    CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
    send_pkt(lp_port,m,lp_stats);
    /* flush */
    send_burst(lp_port,lp_port->m_len,lp_stats);
    lp_port->m_len = 0;
}


void CCoreEthIF::update_mac_addr(CGenNode * node,uint8_t *p){

    if ( CGlobalInfo::m_options.preview.getDestMacSplit() ) {
        p[5]+= (node->m_src_ip % CGlobalInfo::m_options.m_mac_splitter);
    }

    if ( unlikely( CGlobalInfo::m_options.preview.get_mac_ip_mapping_enable() ) ) {
        /* mac mapping file is configured
         */
        if (node->m_src_mac.inused==INUSED) {
            memcpy(p+6, &node->m_src_mac.mac, sizeof(uint8_t)*6);
        }
    } else if ( unlikely( CGlobalInfo::m_options.preview.get_mac_ip_overide_enable() ) ){
        /* client side */
        if ( node->is_initiator_pkt() ){
            *((uint32_t*)(p+6))=PKT_NTOHL(node->m_src_ip);
        }
    }
}



int CCoreEthIFStateless::send_node(CGenNode * no){
    CGenNodeStateless * node_sl=(CGenNodeStateless *) no;

    /* check that we have mbuf  */
    rte_mbuf_t *    m=node_sl->get_cache_mbuf();
    assert( m );
    pkt_dir_t dir=(pkt_dir_t)node_sl->get_mbuf_cache_dir();
    CCorePerPort *  lp_port=&m_ports[dir];
    CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];
    rte_pktmbuf_refcnt_update(m,1);
    send_pkt(lp_port,m,lp_stats);
    return (0);
};



int CCoreEthIF::send_node(CGenNode * node){

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
    bool single_port;

    dir = node->cur_interface_dir();
    single_port = node->get_is_all_flow_from_same_dir() ;

    if ( unlikely( CGlobalInfo::m_options.preview.get_vlan_mode_enable() ) ){
        /* which vlan to choose 0 or 1*/
        uint8_t vlan_port = (node->m_src_ip &1);

        /* set the vlan */
        m->ol_flags = PKT_TX_VLAN_PKT;
		m->l2_len   =14;
		uint16_t vlan_id = CGlobalInfo::m_options.m_vlan_port[vlan_port];


		if (likely( vlan_id >0 ) ) {
			m->vlan_tci = vlan_id;
			dir = dir ^ vlan_port;
		}else{
			/* both from the same dir but with VLAN0 */
			m->vlan_tci = CGlobalInfo::m_options.m_vlan_port[0];
			dir = dir ^ 0;
		}
    }

    CCorePerPort *  lp_port=&m_ports[dir];
    CVirtualIFPerSideStats  * lp_stats = &m_stats[dir];

    if (unlikely(m==0)) {
        lp_stats->m_tx_alloc_error++;
        return(0);
    }
    
    /* update mac addr dest/src 12 bytes */
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    uint8_t p_id=lp_port->m_port->get_port_id();


    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);

    /* if customer enables both mac_file and get_mac_ip_overide, 
     * we will apply mac_file.
     */
    if ( unlikely(CGlobalInfo::m_options.preview.get_mac_ip_features_enable() ) ) {
        update_mac_addr(node,p);
    }

	if ( unlikely( node->is_rx_check_enabled() ) ) {
        lp_stats->m_tx_rx_check_pkt++;
        lp->do_generate_new_mbuf_rxcheck(m,node,dir,single_port);
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


int CCoreEthIF::update_mac_addr_from_global_cfg(pkt_dir_t  dir, 
                                rte_mbuf_t      *m){
    assert(m);
    assert(dir<2);
    CCorePerPort *  lp_port=&m_ports[dir];
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
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
                        CLatencyManager * mgr){
        m_dir        = (port_index%2);
        m_ring_to_dp = ring;
        m_mgr        = mgr;
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

    virtual rte_mbuf_t * rx(){
            return (0);
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, 
                               uint16_t nb_pkts){
        return (0);
    }


private:
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

    float     m_total_tx_bps;
    float     m_total_tx_pps;

    float     m_total_rx_bps;
    float     m_total_rx_pps;
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
    uint64_t  m_total_nat_no_fid  ;
    uint64_t  m_total_nat_active  ;
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
    uint8_t m_threads;

    uint32_t      m_num_of_ports;
    CPerPortStats m_port[BP_MAX_PORTS];
public:
    void Dump(FILE *fd,DumpFormat mode);
    void DumpAllPorts(FILE *fd);
    void dump_json(std::string & json);
private:
    std::string get_field(std::string name,float &f);
    std::string get_field(std::string name,uint64_t &f);
    std::string get_field_port(int port,std::string name,float &f);
    std::string get_field_port(int port,std::string name,uint64_t &f);

};

std::string CGlobalStats::get_field(std::string name,float &f){
    char buff[200];
    sprintf(buff,"\"%s\":%.1f,",name.c_str(),f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field(std::string name,uint64_t &f){
    char buff[200];
    sprintf(buff,"\"%s\":%llu,",name.c_str(), (unsigned long long)f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field_port(int port,std::string name,float &f){
    char buff[200];
    sprintf(buff,"\"%s-%d\":%.1f,",name.c_str(),port,f);
    return (std::string(buff));
}

std::string CGlobalStats::get_field_port(int port,std::string name,uint64_t &f){
    char buff[200];
    sprintf(buff,"\"%s-%d\":%llu,",name.c_str(),port, (unsigned long long)f);
    return (std::string(buff));
}


void CGlobalStats::dump_json(std::string & json){
    json="{\"name\":\"trex-global\",\"type\":0,\"data\":{";

    #define GET_FIELD(f) get_field(std::string(#f),f)
    #define GET_FIELD_PORT(p,f) get_field_port(p,std::string(#f),lp->f)

    json+=GET_FIELD(m_cpu_util);
    json+=GET_FIELD(m_platform_factor);
    json+=GET_FIELD(m_tx_bps);
    json+=GET_FIELD(m_rx_bps);
    json+=GET_FIELD(m_tx_pps);
    json+=GET_FIELD(m_rx_pps);
    json+=GET_FIELD(m_tx_cps);
    json+=GET_FIELD(m_tx_expected_cps);
    json+=GET_FIELD(m_tx_expected_pps);
    json+=GET_FIELD(m_tx_expected_bps);
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
    json+=GET_FIELD(m_total_nat_no_fid );
    json+=GET_FIELD(m_total_nat_active );
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
    }
    json+=m_template.dump_as_json("template");
    json+="\"unknown\":0}}"  ;
}

void CGlobalStats::DumpAllPorts(FILE *fd){

    //fprintf (fd," Total-Tx-Pkts   : %s  \n",double_to_human_str((double)m_total_tx_pkts,"pkts",KBYE_1000).c_str());
    //fprintf (fd," Total-Rx-Pkts   : %s  \n",double_to_human_str((double)m_total_rx_pkts,"pkts",KBYE_1000).c_str());

    //fprintf (fd," Total-Tx-Bytes  : %s  \n",double_to_human_str((double)m_total_tx_bytes,"bytes",KBYE_1000).c_str());
    //fprintf (fd," Total-Rx-Bytes  : %s  \n",double_to_human_str((double)m_total_rx_bytes,"bytes",KBYE_1000).c_str());


    
    fprintf (fd," Cpu Utilization : %2.1f  %%  %2.1f Gb/core \n",m_cpu_util,(2*(m_tx_bps/1e9)*100.0/(m_cpu_util*m_threads)));
    fprintf (fd," Platform_factor : %2.1f  \n",m_platform_factor);
    fprintf (fd," Total-Tx        : %s  ",double_to_human_str(m_tx_bps,"bps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Nat_time_out    : %8llu \n", (unsigned long long)m_total_nat_time_out);
    }else{
        fprintf (fd,"\n");
    }


    fprintf (fd," Total-Rx        : %s  ",double_to_human_str(m_rx_bps,"bps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Nat_no_fid      : %8llu \n", (unsigned long long)m_total_nat_no_fid);
    }else{
        fprintf (fd,"\n");
    }

    fprintf (fd," Total-PPS       : %s  ",double_to_human_str(m_tx_pps,"pps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Total_nat_active: %8llu \n", (unsigned long long)m_total_nat_active);
    }else{
        fprintf (fd,"\n");
    }

    fprintf (fd," Total-CPS       : %s  ",double_to_human_str(m_tx_cps,"cps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_mode() ) {
        fprintf (fd," Total_nat_open  : %8llu \n", (unsigned long long)m_total_nat_open);
    }else{
        fprintf (fd,"\n");
    }
    fprintf (fd,"\n");
    fprintf (fd," Expected-PPS    : %s  ",double_to_human_str(m_tx_expected_pps,"pps",KBYE_1000).c_str());
    if ( CGlobalInfo::is_learn_verify_mode() ) {
        fprintf (fd," Nat_learn_errors: %8llu \n", (unsigned long long)m_total_nat_learn_error);
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
            fprintf(fd,"port : %d \n",(int)i);
            fprintf(fd,"------------\n");
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
                fprintf(fd,"| %15d ",i);
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







struct CGlobalTRex  {

public:
    CGlobalTRex (){
       m_max_ports=4;
       m_max_cores=1;
       m_cores_to_dual_ports=0;
       m_max_queues_per_port=0;
       m_test =NULL;
       m_fl_was_init=false;
       m_expected_pps=0.0;                
       m_expected_cps=0.0;
       m_expected_bps=0.0;
       m_trex_stateless = NULL;
    }
public:

    bool Create();
    void Delete();

    int  ixgbe_prob_init();
    int  cores_prob_init();
    int  queues_prob_init();
    int  ixgbe_start();
    int  ixgbe_rx_queue_flush();
    int  ixgbe_configure_mg();


    bool is_all_links_are_up(bool dump=false);
    int  set_promisc_all(bool enable);

    int  reset_counters();

public:

private:
    /* try to stop all datapath cores */
    void try_stop_all_dp();
    /* send message to all dp cores */
    int  send_message_all_dp(TrexStatelessCpToDpMsgBase *msg);

    void check_for_dp_message_from_core(int thread_id);
    void check_for_dp_messages();

public:

    int start_send_master();
    int start_master_stateless();

    int run_in_core(virtual_thread_id_t virt_core_id);
    int stop_core(virtual_thread_id_t virt_core_id);

    int core_for_latency(){
        if ( (!get_is_latency_thread_enable()) ){
            return (-1);
        }else{
                return ( m_max_cores - 1 );
            }

    }

    int run_in_laterncy_core();

    int run_in_master();
    int stop_master();


    /* return the minimum number of dp cores need to support the active ports 
       this is for c==1 or  m_cores_mul==1
    */
    int get_base_num_cores(){
        return (m_max_ports>>1);
    }

    int get_cores_tx(){
        /* 0 - master 
           num_of_cores - 

                                                    
           last for latency */
        if ( (!get_is_latency_thread_enable()) ){
            return (m_max_cores - 1 );
        }else{
            return (m_max_cores - BP_MASTER_AND_LATENCY );
        }
    }




public:
    int test_send();



    int rcv_send(int port,int queue_id);
    int rcv_send_all(int queue_id);

private:
    bool is_all_cores_finished();

    int test_send_pkts(uint16_t queue_id,
                          int pkt,
                          int port);


    int create_pkt(uint8_t *pkt,int pkt_size);
    int create_udp_pkt();
    int create_icmp_pkt();



public:
    void dump_stats(FILE *fd,
                    std::string & json,CGlobalStats::DumpFormat format);

    void dump_template_info(std::string & json);

    bool sanity_check();

    void update_stats(void);
    void get_stats(CGlobalStats & stats);


    void dump_post_test_stats(FILE *fd);

    void dump_config(FILE *fd);

public:
    port_cfg_t  m_port_cfg;

    /*
       exaple1 :  
           req=4 ,m_max_ports =4   ,c=1 , l=1
       
       ==>    
       m_max_cores = 4/2+1+1 =4;
       m_cores_mul = 1
       

    */

    uint32_t    m_max_ports;    /* active number of ports supported options are  2,4,8,10,12  */
    uint32_t    m_max_cores;    /* current number of cores , include master and latency  ==> ( master)1+c*(m_max_ports>>1)+1( latency )  */
    uint32_t    m_cores_mul;    /* how cores multipler given  c=4 ==> m_cores_mul */

    uint32_t    m_max_queues_per_port;
    uint32_t    m_cores_to_dual_ports; /* number of ports that will handle dual ports */
    uint16_t    m_latency_tx_queue_id;

    // statistic 
    CPPSMeasure  m_cps;
    float        m_expected_pps;                
    float        m_expected_cps;                
    float        m_expected_bps;//bps           
    float        m_last_total_cps;



    CPhyEthIF   m_ports[BP_MAX_PORTS];
    CCoreEthIF          m_cores_vif_sf[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserve - stateful */
    CCoreEthIFStateless m_cores_vif_sl[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserve - stateless*/
    CCoreEthIF *        m_cores_vif[BP_MAX_CORES];


    CParserOption m_po ;
    CFlowGenList  m_fl;
    bool          m_fl_was_init;

    volatile uint8_t       m_signal[BP_MAX_CORES] __rte_cache_aligned ;

    CLatencyManager     m_mg;
    CTrexGlobalIoMode   m_io_modes;

private:

private:
    rte_mbuf_t *        m_test;
    uint64_t            m_test_drop;

    CLatencyHWPort      m_latency_vports[BP_MAX_PORTS];    /* read hardware driver */
    CLatencyVmPort      m_latency_vm_vports[BP_MAX_PORTS]; /* vm driver */

    CLatencyPktInfo     m_latency_pkt;
    TrexPublisher       m_zmq_publisher;

public:
    TrexStateless       *m_trex_stateless;
};



int  CGlobalTRex::rcv_send(int port,int queue_id){

    CPhyEthIF * lp=&m_ports[port];
    rte_mbuf_t * rx_pkts[32];
    printf(" test rx port:%d queue:%d \n",port,queue_id);
    printf(" --------------\n");
    uint16_t cnt=lp->rx_burst(queue_id,rx_pkts,32);

    int i;
    for (i=0; i<(int)cnt;i++) {
        rte_mbuf_t * m=rx_pkts[i];
        int pkt_size=rte_pktmbuf_pkt_len(m);
        char *p=rte_pktmbuf_mtod(m, char*);
        utl_DumpBuffer(stdout,p,pkt_size,0);
        rte_pktmbuf_free(m);
    }
    return (0);
}

int  CGlobalTRex::rcv_send_all(int queue_id){
    int i;
    for (i=0; i<m_max_ports; i++) {
        rcv_send(i,queue_id);
    }
    return (0);
}




int CGlobalTRex::test_send(){
    int i;

    //set_promisc_all(true);
    create_udp_pkt();

	CRx_check_header rx_check_header;
    (void)rx_check_header;

	rx_check_header.m_time_stamp=0x1234567;
	rx_check_header.m_option_type=RX_CHECK_V4_OPT_TYPE;
	rx_check_header.m_option_len=RX_CHECK_V4_OPT_LEN;
	rx_check_header.m_magic=2;
	rx_check_header.m_pkt_id=7;
	rx_check_header.m_flow_id=9;
	rx_check_header.m_flags=11;


    assert(m_test);
    for (i=0; i<1; i++) {
        //test_send_pkts(0,1,0);
        //test_send_pkts(m_latency_tx_queue_id,12,0);
        //test_send_pkts(m_latency_tx_queue_id,1,1);
        //test_send_pkts(m_latency_tx_queue_id,1,2);
        //test_send_pkts(m_latency_tx_queue_id,1,3);
        test_send_pkts(0,1,0);
        test_send_pkts(0,2,1);

        /*delay(1000);
        fprintf(stdout," --------------------------------\n");
        fprintf(stdout," after sending to port %d \n",i);
        fprintf(stdout," --------------------------------\n");
        dump_stats(stdout);
        fprintf(stdout," --------------------------------\n");*/
    }
    //test_send_pkts(m_latency_tx_queue_id,1,1);
    //test_send_pkts(m_latency_tx_queue_id,1,2);
    //test_send_pkts(m_latency_tx_queue_id,1,3);


    printf(" ---------\n");
    printf(" rx queue 0 \n");
    printf(" ---------\n");
    rcv_send_all(0);
    printf("\n\n");

    printf(" ---------\n");
    printf(" rx queue 1 \n");
    printf(" ---------\n");
    rcv_send_all(1);
    printf(" ---------\n");

    delay(1000);

   #if 1
    int j=0;
   for (j=0; j<m_max_ports; j++) {
        CPhyEthIF * lp=&m_ports[j];
		printf(" port : %d \n",j);
		printf(" ----------\n");

		lp->update_counters();
		lp->get_stats().Dump(stdout);
        lp->dump_stats_extended(stdout);
    }
  /*for (j=0; j<4; j++) {
       CPhyEthIF * lp=&m_ports[j];
       lp->dump_stats_extended(stdout);
   }*/
   #endif

    fprintf(stdout," drop : %llu \n", (unsigned long long)m_test_drop);
    return (0);
}



const uint8_t udp_pkt[]={ 
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x08,0x00,

    0x45,0x00,0x00,0x81,
    0xaf,0x7e,0x00,0x00,
    0x12,0x11,0xd9,0x23,
    0x01,0x01,0x01,0x01,
    0x3d,0xad,0x72,0x1b,

    0x11,0x11,
    0x11,0x11,

    0x00,0x6d,
	0x00,0x00,

    0x64,0x31,0x3a,0x61,
    0x64,0x32,0x3a,0x69,0x64,
    0x32,0x30,0x3a,0xd0,0x0e,
    0xa1,0x4b,0x7b,0xbd,0xbd,
    0x16,0xc6,0xdb,0xc4,0xbb,0x43,
    0xf9,0x4b,0x51,0x68,0x33,0x72,
    0x20,0x39,0x3a,0x69,0x6e,0x66,0x6f,
    0x5f,0x68,0x61,0x73,0x68,0x32,0x30,0x3a,0xee,0xc6,0xa3,
    0xd3,0x13,0xa8,0x43,0x06,0x03,0xd8,0x9e,0x3f,0x67,0x6f,
    0xe7,0x0a,0xfd,0x18,0x13,0x8d,0x65,0x31,0x3a,0x71,0x39,
    0x3a,0x67,0x65,0x74,0x5f,0x70,0x65,0x65,0x72,0x73,0x31,
    0x3a,0x74,0x38,0x3a,0x3d,0xeb,0x0c,0xbf,0x0d,0x6a,0x0d,
    0xa5,0x31,0x3a,0x79,0x31,0x3a,0x71,0x65,0x87,0xa6,0x7d,
    0xe7
};


const uint8_t icmp_pkt1[]={
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x08,0x00,

    0x45,0x02,0x00,0x30,
    0x00,0x00,0x40,0x00,
    0xaa,0x01,0xbd,0x04,
    0x9b,0xe6,0x18,0x9b, //SIP
    0xcb,0xff,0xfc,0xc2, //DIP

    0x08, 0x00, 
    0x01, 0x02,  //checksum
    0xaa, 0xbb,  // id
    0x00, 0x00,  // Sequence number

    0x11,0x22,0x33,0x44, // magic 
    0x00,0x00,0x00,0x00, //64 bit counter
    0x00,0x00,0x00,0x00,
    0x00,0x01,0xa0,0x00, //seq
    0x00,0x00,0x00,0x00,

}; 




int CGlobalTRex::create_pkt(uint8_t *pkt,int pkt_size){
    rte_mempool_t * mp= CGlobalInfo::m_mem_pool[0].m_big_mbuf_pool ;

    rte_mbuf_t * m=rte_pktmbuf_alloc(mp);
    if ( unlikely(m==0) )  {
        printf("ERROR no packets \n");
        return (0);
    }
    char *p=rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* set pkt data */
    memcpy(p,pkt,pkt_size);
    //m->ol_flags = PKT_TX_VLAN_PKT;
    //m->pkt.vlan_tci =200;

    m_test = m;

    return (0);
}

int CGlobalTRex::create_udp_pkt(){
    return (create_pkt((uint8_t*)udp_pkt,sizeof(udp_pkt)));
}

int CGlobalTRex::create_icmp_pkt(){
    return (create_pkt((uint8_t*)icmp_pkt1,sizeof(icmp_pkt1)));
}


/* test by sending 10 packets ...*/
int CGlobalTRex::test_send_pkts(uint16_t queue_id,
                                      int pkt,
                                      int port){

    CPhyEthIF * lp=&m_ports[port];
    rte_mbuf_t * tx_pkts[32];
    if (pkt >32 ) {
        pkt =32;
    }

    int i;
    for (i=0; i<pkt; i++) {
        rte_mbuf_refcnt_update(m_test,1);
        tx_pkts[i]=m_test;
    }
    uint16_t res=lp->tx_burst(queue_id,tx_pkts,pkt);
    if ((pkt-res)>0) {
        m_test_drop+=(pkt-res);
    }
    return (0);
}





int  CGlobalTRex::set_promisc_all(bool enable){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->set_promiscuous(enable);
    }

    return (0);
}



int  CGlobalTRex::reset_counters(){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->stats_clear();
    }

    return (0);
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
        _if->update_link_status();
        if ( dump ){
            _if->dump_stats(stdout);
        }
        if ( _if->is_link_up() == false){
            all_link_are=false;
            break;
        }
    }
    return (all_link_are);
}


void CGlobalTRex::try_stop_all_dp(){

    TrexStatelessDpQuit * msg= new TrexStatelessDpQuit();
    send_message_all_dp(msg);
    delete msg;
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


int  CGlobalTRex::ixgbe_rx_queue_flush(){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=&m_ports[i];
        _if->flush_rx_queue();
    }
    return (0);
}


int  CGlobalTRex::ixgbe_configure_mg(void){
    int i;
    CLatencyManagerCfg mg_cfg;
    mg_cfg.m_max_ports = m_max_ports;

    uint32_t latency_rate=CGlobalInfo::m_options.m_latency_rate;

    if ( latency_rate ) {
        mg_cfg.m_cps = (double)latency_rate ;
    }else{
        mg_cfg.m_cps = 100.0;
    }

    if ( get_vm_one_queue_enable() ) {
        /* vm mode, indirect queues  */
        for (i=0; i<m_max_ports; i++) {

            CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();

            uint8_t thread_id = (i>>1); 

            CNodeRing * r = rx_dp->getRingCpToDp(thread_id);
            m_latency_vm_vports[i].Create((uint8_t)i,r,&m_mg);

            mg_cfg.m_ports[i] =&m_latency_vm_vports[i];
        }

    }else{
        for (i=0; i<m_max_ports; i++) {
            CPhyEthIF * _if=&m_ports[i];
            _if->dump_stats(stdout);
            m_latency_vports[i].Create(_if,m_latency_tx_queue_id,1);

            mg_cfg.m_ports[i] =&m_latency_vports[i];
        }
    }


    m_mg.Create(&mg_cfg);
    m_mg.set_mask(CGlobalInfo::m_options.m_latency_mask);

    return (0);
}


int  CGlobalTRex::ixgbe_start(void){
    int i;
    for (i=0; i<m_max_ports; i++) {

        CPhyEthIF * _if=&m_ports[i];
        _if->Create((uint8_t)i);
        /* last TX queue if for latency check */
        if ( get_vm_one_queue_enable() ) {
            /* one tx one rx */
            _if->configure(1,
                           1,
                           &m_port_cfg.m_port_conf);

            /* will not be used */
            m_latency_tx_queue_id= m_cores_to_dual_ports;

            socket_id_t socket_id = CGlobalInfo::m_socket.port_to_socket((port_id_t)i);
            assert(CGlobalInfo::m_mem_pool[socket_id].m_big_mbuf_pool);



            _if->set_rx_queue(0);
            _if->rx_queue_setup(0,
                                RTE_TEST_RX_DESC_VM_DEFAULT,
                                socket_id, 
                                &m_port_cfg.m_rx_conf,
                                CGlobalInfo::m_mem_pool[socket_id].m_big_mbuf_pool);

            int qid;
            for ( qid=0; qid<(m_max_queues_per_port); qid++) {
                _if->tx_queue_setup((uint16_t)qid,
                                    RTE_TEST_TX_DESC_VM_DEFAULT ,
                                    socket_id, 
                                    &m_port_cfg.m_tx_conf);

            }

        }else{
            _if->configure(2,
                          m_cores_to_dual_ports+1,
                          &m_port_cfg.m_port_conf);

            /* the latency queue for latency measurement packets */
            m_latency_tx_queue_id= m_cores_to_dual_ports;

            socket_id_t socket_id = CGlobalInfo::m_socket.port_to_socket((port_id_t)i);
            assert(CGlobalInfo::m_mem_pool[socket_id].m_big_mbuf_pool);


            /* drop queue */
            _if->rx_queue_setup(0,
                                RTE_TEST_RX_DESC_DEFAULT,
                                socket_id, 
                                &m_port_cfg.m_rx_conf,
                                CGlobalInfo::m_mem_pool[socket_id].m_big_mbuf_pool);


            /* set the filter queue */
            _if->set_rx_queue(1);
            /* latency measurement ring is 1 */
            _if->rx_queue_setup(1,
                                RTE_TEST_RX_LATENCY_DESC_DEFAULT,
                                socket_id, 
                                &m_port_cfg.m_rx_conf,
                                CGlobalInfo::m_mem_pool[socket_id].m_big_mbuf_pool);

            int qid;
            for ( qid=0; qid<(m_max_queues_per_port+1); qid++) {
                _if->tx_queue_setup((uint16_t)qid,
                                    RTE_TEST_TX_DESC_DEFAULT ,
                                    socket_id, 
                                    &m_port_cfg.m_tx_conf);

            }

        }


        _if->stats_clear();

        _if->start();
        _if->configure_rx_drop_queue();
        _if->configure_rx_duplicate_rules();

        _if->disable_flow_control();

        _if->update_link_status();

        _if->dump_link(stdout);
        
        _if->add_mac((char *)CGlobalInfo::m_options.get_src_mac_addr(i));

        fflush(stdout);
    }

    if ( !is_all_links_are_up()  ){
        /* wait for ports to be stable */
        get_ex_drv()->wait_for_stable_link();

        if ( !is_all_links_are_up(true) ){
            rte_exit(EXIT_FAILURE, " "
                    " one of the link is down \n");
        }
    }

    ixgbe_rx_queue_flush();


    ixgbe_configure_mg();


    /* core 0 - control
       core 1 - port 0-0,1-0,
       core 2 - port 2-0,3-0, 
       core 3 - port 0-1,1-1, 
       core 4 - port 2-1,3-1, 

    */
    int port_offset=0;
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
                              &m_ports[port_offset+1] /*1,3*/
                              );
        port_offset+=2;
        if (port_offset == m_max_ports) {
            port_offset = 0;
        }    
     }

    fprintf(stdout," -------------------------------\n");
    CCoreEthIF::DumpIfCfgHeader(stdout);
    for (i=0; i<get_cores_tx(); i++) {
        m_cores_vif[i+1]->DumpIfCfg(stdout);
    }
    fprintf(stdout," -------------------------------\n");

    return (0);
}


bool CGlobalTRex::Create(){
    CFlowsYamlInfo     pre_yaml_info;

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

       TrexRpcServerConfig rpc_req_resp_cfg(TrexRpcServerConfig::RPC_PROT_TCP, global_platform_cfg_info.m_zmq_rpc_port);

       cfg.m_port_count         = CGlobalInfo::m_options.m_expected_portd;
       cfg.m_rpc_req_resp_cfg   = &rpc_req_resp_cfg;
       cfg.m_rpc_async_cfg      = NULL;
       cfg.m_rpc_server_verbose = false;
       cfg.m_platform_api       = new TrexDpdkPlatformApi();
       cfg.m_publisher          = &m_zmq_publisher;

       m_trex_stateless = new TrexStateless(cfg);
   }

   return (true);

}
void CGlobalTRex::Delete(){
    m_zmq_publisher.Delete();
}



int  CGlobalTRex::ixgbe_prob_init(void){

	m_max_ports  = rte_eth_dev_count();
	if (m_max_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    printf(" number of ports founded : %d \n",m_max_ports);



    if ( CGlobalInfo::m_options.get_expected_ports() >BP_MAX_PORTS ){
        rte_exit(EXIT_FAILURE, " maximum ports supported are %d, use the configuration file to set the expected number of ports   \n",BP_MAX_PORTS);
    }

    if ( CGlobalInfo::m_options.get_expected_ports() > m_max_ports ){
        rte_exit(EXIT_FAILURE, " there are %d ports you expected more %d,use the configuration file to set the expected number of ports   \n",
                 m_max_ports,
                 CGlobalInfo::m_options.get_expected_ports());
    }
    if (CGlobalInfo::m_options.get_expected_ports() < m_max_ports ) {
        /* limit the number of ports */
        m_max_ports=CGlobalInfo::m_options.get_expected_ports();
    }
    assert(m_max_ports <= BP_MAX_PORTS);

    if ( m_max_ports %2 !=0 ) {
        rte_exit(EXIT_FAILURE, " numbe of ports %d should be even, mask the one port in the configuration file  \n, ",
                 m_max_ports);

    }

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
        printf(" ERROR driver name  %s is not supported \n",dev_info.driver_name);
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

    /* register driver callback to convert mseg to signle seg */
    if (strcmp(dev_info.driver_name,"rte_vmxnet3_pmd")==0 ) {
        vmxnet3_xmit_set_callback(rte_mbuf_convert_to_one_seg);
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
          rte_exit(EXIT_FAILURE, "number of cores should be at least 3 \n");
    }

    if ( !( (m_max_ports  == 4) || (m_max_ports  == 2) || (m_max_ports  == 8) || (m_max_ports  == 6)  ) ){
        rte_exit(EXIT_FAILURE, "supported number of ports are 2-8 you have %d \n",m_max_ports);
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
    
    /* number of queue - 1 per core for dual ports*/
    m_max_queues_per_port  = m_cores_to_dual_ports;

    if (m_max_queues_per_port > BP_MAX_TX_QUEUE) {
        rte_exit(EXIT_FAILURE, 
         "maximum number of queue should be maximum %d  \n",BP_MAX_TX_QUEUE);
    }
    
    assert(m_max_queues_per_port>0);
    return (0);
}


void CGlobalTRex::dump_config(FILE *fd){
    fprintf(fd," number of ports         : %u \n",m_max_ports);
    fprintf(fd," max cores for 2 ports   : %u \n",m_cores_to_dual_ports);
    fprintf(fd," max queue per port      : %u \n",m_max_queues_per_port);
}



void CGlobalTRex::dump_post_test_stats(FILE *fd){
    uint64_t pkt_out=0;
    uint64_t pkt_out_bytes=0;
    uint64_t pkt_in_bytes=0;
    uint64_t pkt_in=0;
    uint64_t sw_pkt_out=0;
    uint64_t sw_pkt_out_err=0;
    uint64_t sw_pkt_out_bytes=0;

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
    }
    if ( !CGlobalInfo::m_options.is_latency_disabled() ){
        sw_pkt_out += m_mg.get_total_pkt();
        sw_pkt_out_bytes +=m_mg.get_total_bytes();
    }


    fprintf (fd," summary stats \n");
    fprintf (fd," -------------- \n");

    fprintf (fd," Total-pkt-drop       : %llu pkts \n",(unsigned long long)(pkt_out-pkt_in));
    fprintf (fd," Total-tx-bytes       : %llu bytes \n", (unsigned long long)pkt_out_bytes);
    fprintf (fd," Total-tx-sw-bytes    : %llu bytes \n", (unsigned long long)sw_pkt_out_bytes);
    fprintf (fd," Total-rx-bytes       : %llu byte \n", (unsigned long long)pkt_in_bytes);

    fprintf (fd," \n");

    fprintf (fd," Total-tx-pkt         : %llu pkts \n", (unsigned long long)pkt_out);
    fprintf (fd," Total-rx-pkt         : %llu pkts \n", (unsigned long long)pkt_in);
    fprintf (fd," Total-sw-tx-pkt      : %llu pkts \n", (unsigned long long)sw_pkt_out);
    fprintf (fd," Total-sw-err         : %llu pkts \n", (unsigned long long)sw_pkt_out_err);


    if ( !CGlobalInfo::m_options.is_latency_disabled() ){
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
    m_fl.Update();

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

        stats.m_total_tx_pkts  += st.opackets; 
        stats.m_total_rx_pkts  += st.ipackets;
        stats.m_total_tx_bytes += st.obytes;
        stats.m_total_rx_bytes += st.ibytes;

        total_tx +=_if->get_last_tx_rate();
        total_rx +=_if->get_last_rx_rate();
        total_tx_pps +=_if->get_last_tx_pps_rate();
        total_rx_pps +=_if->get_last_rx_pps_rate();

    }

    uint64_t total_open_flows=0;
    uint64_t total_active_flows=0;

    uint64_t total_clients=0;
    uint64_t total_servers=0;
    uint64_t active_sockets=0;
    uint64_t total_sockets=0;


    uint64_t total_nat_time_out =0;
    uint64_t total_nat_no_fid   =0;
    uint64_t total_nat_active   =0;
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

        stats.m_total_queue_drop =lpt->m_node_gen.m_v_if->m_stats[0].m_tx_drop+
                               lpt->m_node_gen.m_v_if->m_stats[1].m_tx_drop;

        stats.m_template.Add(&lpt->m_node_gen.m_v_if->m_stats[0].m_template);
        stats.m_template.Add(&lpt->m_node_gen.m_v_if->m_stats[1].m_template);


        total_clients   += lpt->m_smart_gen.getTotalClients();
        total_servers   += lpt->m_smart_gen.getTotalServers();
        active_sockets  += lpt->m_smart_gen.ActiveSockets();
        total_sockets   += lpt->m_smart_gen.MaxSockets();

        total_nat_time_out +=lpt->m_stats.m_nat_flow_timeout;
        total_nat_no_fid   +=lpt->m_stats.m_nat_lookup_no_flow_id ;
        total_nat_active   +=lpt->m_stats.m_nat_lookup_add_flow_id - lpt->m_stats.m_nat_lookup_remove_flow_id;
        total_nat_open     +=lpt->m_stats.m_nat_lookup_add_flow_id;
        total_nat_learn_error   +=lpt->m_stats.m_nat_flow_learn_error;
    }

    stats.m_total_nat_time_out = total_nat_time_out;
    stats.m_total_nat_no_fid   = total_nat_no_fid;
    stats.m_total_nat_active   = total_nat_active;
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

    stats.m_tx_expected_cps        = m_expected_cps*pf;
    stats.m_tx_expected_pps        = m_expected_pps*pf;
    stats.m_tx_expected_bps        = m_expected_bps*pf;
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

void CGlobalTRex::dump_stats(FILE *fd,std::string & json,
                                CGlobalStats::DumpFormat format){
    CGlobalStats  stats;
    update_stats();
    get_stats(stats);
    if (format==CGlobalStats::dmpTABLE) {
        if ( m_io_modes.m_g_mode == CTrexGlobalIoMode::gNORMAL ){
            switch (m_io_modes.m_pp_mode ){
                case CTrexGlobalIoMode::ppDISABLE:
                    fprintf(fd,"\n+Per port stats disabled \n");
                    break;
                case CTrexGlobalIoMode::ppTABLE:
                    fprintf(fd,"\n-Per port stats table \n");
                    stats.Dump(fd,CGlobalStats::dmpTABLE);
                    break;
                case CTrexGlobalIoMode::ppSTANDARD:
                    fprintf(fd,"\n-Per port stats - standard\n");
                    stats.Dump(fd,CGlobalStats::dmpSTANDARD);
                    break;
            };

            switch (m_io_modes.m_ap_mode ){
                case   CTrexGlobalIoMode::apDISABLE:
                    fprintf(fd,"\n+Global stats disabled \n");
                    break;
                case   CTrexGlobalIoMode::apENABLE:
                    fprintf(fd,"\n-Global stats enabled \n");
                    stats.DumpAllPorts(fd);
                    break;
            };
        }
    }else{
        /* at exit , always need to dump it in standartd mode for scripts*/
        stats.Dump(fd,format);
        stats.DumpAllPorts(fd);
    }
    stats.dump_json(json);
}


int CGlobalTRex::run_in_master(){

    std::string json;
    bool was_stopped=false;

    if ( get_is_stateless() ) {
        m_trex_stateless->launch_control_plane();
    }

    while ( true ) {

        if ( CGlobalInfo::m_options.preview.get_no_keyboard() ==false ){
            if ( m_io_modes.handle_io_modes() ){
                was_stopped=true;
                break;
            }
        }

        if ( sanity_check() ){
            printf(" Test was stopped \n");
            was_stopped=true;
            break;
        }
        if (m_io_modes.m_g_mode != CTrexGlobalIoMode::gDISABLE ) {
            fprintf(stdout,"\033[2J");
            fprintf(stdout,"\033[2H");

        }else{
            if ( m_io_modes.m_g_disable_first  ){
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

        dump_stats(stdout,json,CGlobalStats::dmpTABLE);

        if (m_io_modes.m_g_mode == CTrexGlobalIoMode::gNORMAL ) {
    		fprintf (stdout," current time    : %.1f sec  \n",now_sec());
    		float d= CGlobalInfo::m_options.m_duration - now_sec();
    		if (d<0) {
    			d=0;
    
    		}
    		fprintf (stdout," test duration   : %.1f sec  \n",d);
        }

        m_zmq_publisher.publish_json(json);

        /* generator json , all cores are the same just sample the first one */
        m_fl.m_threads_info[0]->m_node_gen.dump_json(json);
        m_zmq_publisher.publish_json(json);

        if ( !get_is_stateless() ){
            dump_template_info(json);
            m_zmq_publisher.publish_json(json);
        }

        if ( !CGlobalInfo::m_options.is_latency_disabled() ){
            m_mg.update();

            if ( m_io_modes.m_g_mode ==  CTrexGlobalIoMode::gNORMAL ){
                switch (m_io_modes.m_l_mode) {
                case CTrexGlobalIoMode::lDISABLE:
                    fprintf(stdout,"\n+Latency stats disabled \n");
                    break;
                case CTrexGlobalIoMode::lENABLE:
                    fprintf(stdout,"\n-Latency stats enabled \n");
                    m_mg.DumpShort(stdout);
                    break;
                case CTrexGlobalIoMode::lENABLE_Extended:
                    fprintf(stdout,"\n-Latency stats extended \n");
                    m_mg.Dump(stdout);
                    break;
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

                m_mg.rx_check_dump_json(json );
                m_zmq_publisher.publish_json(json);

              }/* ex checked */

            }

            /* backward compatible */
            m_mg.dump_json(json );
            m_zmq_publisher.publish_json(json);

            /* more info */
            m_mg.dump_json_v2(json );
            m_zmq_publisher.publish_json(json);

        }

        /* stateless info */
        m_trex_stateless->generate_publish_snapshot(json);
        m_zmq_publisher.publish_json(json);

        /* check from messages from DP */
        check_for_dp_messages();

        delay(500);

        if ( is_all_cores_finished() ) {
            break;
        }
    }

    if (!is_all_cores_finished()) {
        /* probably CLTR-C */
        try_stop_all_dp();
    }

    m_mg.stop();
    delay(1000);
    if ( was_stopped ){
        /* we should stop latency and exit to stop agents */
        exit(-1);
    }
    return (0);
}



int CGlobalTRex::run_in_laterncy_core(void){
    if ( !CGlobalInfo::m_options.is_latency_disabled() ){
        m_mg.start(0);
    }
    return (0);
}


int CGlobalTRex::stop_core(virtual_thread_id_t virt_core_id){
    m_signal[virt_core_id]=1;
    return (0);
}

int CGlobalTRex::run_in_core(virtual_thread_id_t virt_core_id){

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

    if (get_is_stateless()) {
        lpt->start_stateless_daemon(*lp);
    }else{
        lpt->start_generate_stateful(CGlobalInfo::m_options.out_file,*lp);
    }

    m_signal[virt_core_id]=1;
    return (0);
}


int CGlobalTRex::stop_master(){

    delay(1000);
    std::string json;
    fprintf(stdout," ==================\n");
    fprintf(stdout," interface sum \n");
    fprintf(stdout," ==================\n");
    dump_stats(stdout,json,CGlobalStats::dmpSTANDARD);
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
    if ( !CGlobalInfo::m_options.is_latency_disabled() ){
        fprintf(stdout," ==================\n");
        fprintf(stdout," latency \n");
        fprintf(stdout," ==================\n");
        m_mg.DumpShort(stdout);
        m_mg.Dump(stdout);
        m_mg.DumpShortRxCheck(stdout);
        m_mg.DumpRxCheck(stdout);
        m_mg.DumpRxCheckVerification(stdout,total_tx_rx_check);
    }
    
    dump_stats(stdout,json,CGlobalStats::dmpSTANDARD);
    dump_post_test_stats(stdout);
    m_fl.Delete();

    return (0);
}

bool CGlobalTRex::is_all_cores_finished(){
    int i;
    for (i=0; i<get_cores_tx(); i++) {
        if ( m_signal[i+1]==0){
            return (false);
        }
    }
    return (true);
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




int CGlobalTRex::start_send_master(){
    int i;
    for (i=0; i<BP_MAX_CORES; i++) {
        m_signal[i]=0;
    }

    m_fl.Create();
    m_fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,get_cores_tx());
    if (CGlobalInfo::m_options.mac_file != "") {
        CGlobalInfo::m_options.preview.set_mac_ip_mapping_enable(true);
        m_fl.load_from_mac_file(CGlobalInfo::m_options.mac_file);
        m_fl.m_mac_info.set_configured(true);
    } else {
        m_fl.m_mac_info.set_configured(false);
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

bool CCoreEthIF::process_rx_pkt(pkt_dir_t   dir,
                                rte_mbuf_t * m){

    CSimplePacketParser parser(m);
    if ( !parser.Parse()  ){
        return false;
    }
    bool send=false;
    CLatencyPktMode *c_l_pkt_mode = g_trex.m_mg.c_l_pkt_mode;
    bool is_lateancy_pkt =  c_l_pkt_mode->IsLatencyPkt(parser.m_ipv4) & parser.IsLatencyPkt(parser.m_l4 + c_l_pkt_mode->l4_header_len());

    if (is_lateancy_pkt){
        send=true;
    }else{
        if ( get_is_rx_filter_enable() ){
            uint8_t max_ttl = 0xff - get_rx_check_hops();
            uint8_t pkt_ttl = parser.getTTl();
            if ( (pkt_ttl==max_ttl) || (pkt_ttl==(max_ttl-1) ) ) {
               send=true;
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

static int latency_one_lcore(__attribute__((unused)) void *dummy)
{
    CPlatformSocketInfo * lpsock=&CGlobalInfo::m_socket;
    physical_thread_id_t  phy_id =rte_lcore_id();


    if ( lpsock->thread_phy_is_latency( phy_id )  ){
        g_trex.run_in_laterncy_core();
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


    if ( lpsock->thread_phy_is_latency( phy_id )  ){
        g_trex.run_in_laterncy_core();
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




int main(int argc , char * argv[]){

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

        if ( port_size > BP_MAX_PORTS ){
            port_size = BP_MAX_PORTS;
        }
        for (i=0; i<port_size; i++){
            cg->m_mac_info[i].copy_src(( char *)CGlobalInfo::m_options.m_mac_addr[i].u.m_mac.src)   ;
            cg->m_mac_info[i].copy_dest(( char *)CGlobalInfo::m_options.m_mac_addr[i].u.m_mac.dest)  ;
        }
    }

    /* mul by interface type */
    float mul=1.0;
    if (cg->m_port_bandwidth_gb<10) {
        cg->m_port_bandwidth_gb=10.0;
    }

    mul = mul*(float)cg->m_port_bandwidth_gb/10.0;
    mul= mul * (float)cg->m_port_limit/2.0;

    CGlobalInfo::m_memory_cfg.set(cg->m_memory,mul);
    CGlobalInfo::m_memory_cfg.set_number_of_dp_cors(
    CGlobalInfo::m_options.get_number_of_dp_cores_needed() );
    return (0);
}


int  update_dpdk_args(void){

    CPlatformSocketInfo * lpsock=&CGlobalInfo::m_socket;
    CParserOption * lpop= &CGlobalInfo::m_options;

    lpsock->set_latency_thread_is_enabled(get_is_latency_thread_enable());
    lpsock->set_number_of_threads_per_ports(lpop->preview.getCores() );
    lpsock->set_number_of_dual_ports(lpop->get_expected_dual_ports());
    if ( !lpsock->sanity_check() ){
        printf(" ERROR in configuration file \n");
        return (-1);
    }

    if ( CGlobalInfo::m_options.preview.getVMode() > 0  ) {
        lpsock->dump(stdout);
    }


    sprintf(global_cores_str,"0x%llx",(unsigned long long)lpsock->get_cores_mask());

    /* set the DPDK options */
    global_dpdk_args_num =7;

    global_dpdk_args[0]=(char *)"xx";
    global_dpdk_args[1]=(char *)"-c";
    global_dpdk_args[2]=(char *)global_cores_str;
    global_dpdk_args[3]=(char *)"-n";
    global_dpdk_args[4]=(char *)"4";

    if ( CGlobalInfo::m_options.preview.getVMode() == 0  ) {
        global_dpdk_args[5]=(char *)"--log-level";
        sprintf(global_loglevel_str,"%d",1);
        global_dpdk_args[6]=(char *)global_loglevel_str;
    }else{
        global_dpdk_args[5]=(char *)"--log-level";
        sprintf(global_loglevel_str,"%d",CGlobalInfo::m_options.preview.getVMode()+1);
        global_dpdk_args[6]=(char *)global_loglevel_str;
    }

    global_dpdk_args_num = 7;

    /* add white list */
    for (int i=0; i<(int)global_platform_cfg_info.m_if_list.size(); i++) {
        global_dpdk_args[global_dpdk_args_num++]=(char *)"-w";
        global_dpdk_args[global_dpdk_args_num++]=(char *)global_platform_cfg_info.m_if_list[i].c_str();
    }



    if ( lpop->prefix.length()  ){
        global_dpdk_args[global_dpdk_args_num++]=(char *)"--file-prefix";
        sprintf(global_prefix_str,"%s",lpop->prefix.c_str());
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


int main_test(int argc , char * argv[]){

    utl_termio_init();

    int ret;
    unsigned lcore_id;
    printf("Starting  TRex %s please wait  ... \n",VERSION_BUILD_NUM);

    CGlobalInfo::m_options.preview.clean();

    if ( parse_options(argc, argv, &CGlobalInfo::m_options,true ) != 0){
        exit(-1);
    }

    update_global_info_from_platform_file();

    /* it is not a mistake , give the user higher priorty over the configuration file */
    parse_options(argc, argv, &CGlobalInfo::m_options ,false);

    
    if ( CGlobalInfo::m_options.preview.getVMode() > 0){
        CGlobalInfo::m_options.dump(stdout);
        CGlobalInfo::m_memory_cfg.Dump(stdout);
    }

    update_dpdk_args();

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



    ret = rte_eal_init(global_dpdk_args_num, (char **)global_dpdk_args);
    if (ret < 0){
        printf(" You might need to run ./trex-cfg  once  \n");
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    }

    time_init();
    
        /* check if we are in simulation mode */
    if ( CGlobalInfo::m_options.out_file != "" ){
        printf(" t-rex simulation mode into %s \n",CGlobalInfo::m_options.out_file.c_str());
        return ( sim_load_list_of_cap_files(&CGlobalInfo::m_options) );
    }

    if ( !g_trex.Create() ){
        exit(1);
    }

    if (po->preview.get_is_rx_check_enable() &&  (po->m_rx_check_sampe< get_min_sample_rate()) ) {
        po->m_rx_check_sampe = get_min_sample_rate();
        printf("Warning rx check sample rate should be lower than %d setting it to %d\n",get_min_sample_rate(),get_min_sample_rate());
    }

    /* set dump mode */
    g_trex.m_io_modes.set_mode((CTrexGlobalIoMode::CliDumpMode)CGlobalInfo::m_options.m_io_mode);

    if ( !CGlobalInfo::m_options.is_latency_disabled() 
         && (CGlobalInfo::m_options.m_latency_prev>0)  ){
        uint32_t pkts = CGlobalInfo::m_options.m_latency_prev*
            CGlobalInfo::m_options.m_latency_rate;
        printf("Start prev latency check- for %d sec \n",CGlobalInfo::m_options.m_latency_prev);
        g_trex.m_mg.start(pkts);
        delay(CGlobalInfo::m_options.m_latency_prev* 1000);
        printf("Finished \n");
        g_trex.m_mg.reset();
        g_trex.reset_counters();
    }

    if ( get_is_stateless() ) {
        g_trex.start_master_stateless();

    }else{
        g_trex.start_send_master();
    }


	/* TBD_FDIR */
	#if 0
	printf(" test_send \n");
	g_trex.test_send();
    while (1) {
           delay(10000);
    }
	#endif

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


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// driver section 
//////////////////////////////////////////////////////////////////////////////////////////////

int CTRexExtendedDriverBase1G::wait_for_stable_link(){
        int i; 
        printf(" wait 10 sec ");
        fflush(stdout);
        for (i=0; i<10; i++) {
            delay(1000);
            printf(".");
            fflush(stdout);
        }
        printf("\n");
        fflush(stdout);
        return(0);
}

int CTRexExtendedDriverBase1G::configure_drop_queue(CPhyEthIF * _if){
    uint8_t protocol;
    if (CGlobalInfo::m_options.m_l_pkt_mode == 0) {
        protocol = IPPROTO_SCTP;
    } else {
        protocol = IPPROTO_ICMP;
    }

    _if->pci_reg_write( E1000_RXDCTL(0) , 0);

    /* enable filter to pass packet to rx queue 1 */
    _if->pci_reg_write( E1000_IMIR(0), 0x00020000);
    _if->pci_reg_write( E1000_IMIREXT(0), 0x00081000);
    _if->pci_reg_write( E1000_TTQF(0),   protocol
                                  | 0x00008100 /* enable */
                                  | 0xE0010000 /* RX queue is 1 */
                                    );
    return (0);
}

void CTRexExtendedDriverBase1G::update_configuration(port_cfg_t * cfg){

        cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
        cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
        cfg->m_tx_conf.tx_thresh.wthresh = 0;
}

void CTRexExtendedDriverBase1G::update_global_config_fdir(port_cfg_t * cfg){
    cfg->update_global_config_fdir_10g_1g();
}

int CTRexExtendedDriverBase1G::configure_rx_filter_rules(CPhyEthIF * _if){

    uint16_t hops = get_rx_check_hops();
    uint16_t v4_hops = (hops << 8)&0xff00; 

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



void CTRexExtendedDriverBase10G::clear_extended_stats(CPhyEthIF * _if){
    _if->pci_reg_read(IXGBE_RXNFGPC);
}

void CTRexExtendedDriverBase10G::update_global_config_fdir(port_cfg_t * cfg){
    cfg->update_global_config_fdir_10g_1g();
}

void CTRexExtendedDriverBase10G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules(CPhyEthIF * _if){
        /* 10Gb/sec 82599 */
        uint8_t port_id=_if->get_rte_port_id();

        uint16_t hops = get_rx_check_hops();
        uint16_t v4_hops = (hops << 8)&0xff00; 


      /* set the mask only for flex-data */
        rte_fdir_masks fdir_mask;
        memset(&fdir_mask,0,sizeof(rte_fdir_masks));
        fdir_mask.flexbytes=1;
        //fdir_mask.dst_port_mask=0xffff; /* enable of 
        int res;
        res=rte_eth_dev_fdir_set_masks(port_id,&fdir_mask);
        if (res!=0) {
             rte_exit(EXIT_FAILURE, " ERROR rte_eth_dev_fdir_set_masks : %d \n",res);
        }


        // IPv4: bytes being compared are {TTL, Protocol}
        uint16_t ff_rules_v4[6]={
            (uint16_t)(0xFF11 - v4_hops),
            (uint16_t)(0xFE11 - v4_hops),
            (uint16_t)(0xFF06 - v4_hops),
            (uint16_t)(0xFE06 - v4_hops),
            (uint16_t)(0xFF01 - v4_hops),
            (uint16_t)(0xFE01 - v4_hops),
        }  ;
        // IPv6: bytes being compared are {NextHdr, HopLimit}
        uint16_t ff_rules_v6[6]={
            (uint16_t)(0x3CFF - hops),
            (uint16_t)(0x3CFE - hops),
            (uint16_t)(0x3CFF - hops),
            (uint16_t)(0x3CFE - hops),
            (uint16_t)(0x3CFF - hops),
            (uint16_t)(0x3CFE - hops),
        }  ;
        const rte_l4type ff_rules_type[6]={
            RTE_FDIR_L4TYPE_UDP,
            RTE_FDIR_L4TYPE_UDP,
            RTE_FDIR_L4TYPE_TCP,
            RTE_FDIR_L4TYPE_TCP,
            RTE_FDIR_L4TYPE_NONE,
            RTE_FDIR_L4TYPE_NONE
        }  ;

        uint16_t *ff_rules;
        uint16_t num_rules;
        int  rule_id;

        assert (sizeof(ff_rules_v4) == sizeof(ff_rules_v6));
        num_rules = sizeof(ff_rules_v4)/sizeof(ff_rules_v4[0]);
        if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
            ff_rules = &ff_rules_v6[0];
        }else{
            ff_rules = &ff_rules_v4[0];
        }

        for (rule_id=0; rule_id<num_rules; rule_id++ ) {
    
            rte_fdir_filter fdir_filter;
            uint16_t ff_rule = ff_rules[rule_id];
            memset(&fdir_filter,0,sizeof(rte_fdir_filter));
            /* TOS/PROTO */
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
                fdir_filter.iptype = RTE_FDIR_IPTYPE_IPV6;
            }else{
                fdir_filter.iptype = RTE_FDIR_IPTYPE_IPV4;
            }
            fdir_filter.flex_bytes = PKT_NTOHS(ff_rule);
            fdir_filter.l4type = ff_rules_type[rule_id];
    
            res=rte_eth_dev_fdir_add_perfect_filter(port_id,
                                                    &fdir_filter,
                                                    rule_id, 1,0);
            if (res!=0) {
                 rte_exit(EXIT_FAILURE, " ERROR rte_eth_dev_fdir_add_perfect_filter : %d\n",res);
            }
        }
        return (0);
}

int CTRexExtendedDriverBase10G::configure_drop_queue(CPhyEthIF * _if){
    /* enable rule 0 SCTP -> queue 1 for latency  */
    /* 1<<21 means that queue 1 is for SCTP */
    _if->pci_reg_write(IXGBE_L34T_IMIR(0),(1<<21));

    _if->pci_reg_write(IXGBE_FTQF(0),
                      IXGBE_FTQF_PROTOCOL_SCTP|
                      (IXGBE_FTQF_PRIORITY_MASK<<IXGBE_FTQF_PRIORITY_SHIFT)|
                      ((0x0f)<<IXGBE_FTQF_5TUPLE_MASK_SHIFT)|IXGBE_FTQF_QUEUE_ENABLE);

    /* disable queue zero - default all traffic will go to here and will be dropped */
    _if->pci_reg_write( IXGBE_RXDCTL(0) , 0);
    return (0);
}

void CTRexExtendedDriverBase10G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){ 

   int i;
   uint64_t t=0;
    for (i=0; i<8;i++) {
        t+=_if->pci_reg_read(IXGBE_MPC(i));
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
    delay(2000);
    return (0);
}

////////////////////////////////////////////////////////////////////////////////


void CTRexExtendedDriverBase40G::clear_extended_stats(CPhyEthIF * _if){

    rte_eth_stats_reset(_if->get_port_id());

}

void CTRexExtendedDriverBase40G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->update_global_config_fdir_40g();
}


/* Add rule to send packets with protocol 'type', and ttl 'ttl' to rx queue 1 */
void CTRexExtendedDriverBase40G::add_rules(CPhyEthIF * _if,
                                           enum rte_eth_flow_type type,
                                           uint8_t ttl){
    uint8_t port_id = _if->get_port_id();
    int ret=rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);

    if (  ret !=0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                "err=%d, port=%u \n",
              ret, port_id);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

    filter.action.rx_queue =1;
    filter.action.behavior =RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status =RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.soft_id=0;
    
    filter.input.flow_type = type;
    filter.input.ttl=ttl;

    if (type == RTE_ETH_FLOW_TYPE_IPV4_OTHER) {
        filter.input.flow.ip4_flow.l4_proto = IPPROTO_ICMP; // In this case we want filter for icmp packets
    }

    /* We want to place latency packets in queue 1 */
    ret=rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR,
                RTE_ETH_FILTER_ADD, (void*)&filter);

    if (  ret !=0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl"
                "err=%d, port=%u \n",
              ret, port_id);
    }
}


int CTRexExtendedDriverBase40G::configure_rx_filter_rules(CPhyEthIF * _if){
    uint16_t hops = get_rx_check_hops();
    int i;
    for (i=0; i<2; i++) {
        uint8_t ttl=0xff-i-hops;
        add_rules(_if,RTE_ETH_FLOW_TYPE_UDPV4,ttl);
        add_rules(_if,RTE_ETH_FLOW_TYPE_TCPV4,ttl);
        add_rules(_if,RTE_ETH_FLOW_TYPE_UDPV6,ttl);
        add_rules(_if,RTE_ETH_FLOW_TYPE_TCPV6,ttl);
    }

    return (0);
}


int CTRexExtendedDriverBase40G::configure_drop_queue(CPhyEthIF * _if){

    /* Configure queue for latency packets */
    add_rules(_if,RTE_ETH_FLOW_TYPE_IPV4_OTHER,255);
    add_rules(_if,RTE_ETH_FLOW_TYPE_SCTPV4,255);
    return (0);
}

void CTRexExtendedDriverBase40G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){ 

    struct rte_eth_stats stats1;
    rte_eth_stats_get(_if->get_port_id(), &stats1);


   stats->ipackets     =  stats1.ipackets;
   stats->ibytes       =  stats1.ibytes; 

   stats->opackets     =  stats1.opackets;
   stats->obytes       =  stats1.obytes;

   stats->f_ipackets   = 0;
   stats->f_ibytes     = 0;


   stats->ierrors      =  stats1.ierrors + stats1.imissed + stats1.ibadcrc +
                               stats1.ibadlen      +
                               stats1.ierrors      +
                               stats1.oerrors      +
                               stats1.imcasts      +
                               stats1.rx_nombuf    +
                               stats1.tx_pause_xon +
                               stats1.rx_pause_xon +
                               stats1.tx_pause_xoff+
                               stats1.rx_pause_xoff ;


   stats->oerrors      =  stats1.oerrors;;
   stats->imcasts      =  0;
   stats->rx_nombuf    =  stats1.rx_nombuf;

}

int CTRexExtendedDriverBase40G::wait_for_stable_link(){
    delay(2000);
    return (0);
}

/////////////////////////////////////////////////////////////////////


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

int CTRexExtendedDriverBase1GVm::configure_drop_queue(CPhyEthIF * _if){


    return (0);
}

void CTRexExtendedDriverBase1GVm::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){ 

    struct rte_eth_stats stats1;
    rte_eth_stats_get(_if->get_port_id(), &stats1);


   stats->ipackets     =  stats1.ipackets;
   stats->ibytes       =  stats1.ibytes; 

   stats->opackets     =  stats1.opackets;
   stats->obytes       =  stats1.obytes;

   stats->f_ipackets   = 0;
   stats->f_ibytes     = 0;


   stats->ierrors      =  stats1.ierrors + stats1.imissed + stats1.ibadcrc +
                               stats1.ibadlen      +
                               stats1.ierrors      +
                               stats1.oerrors      +
                               stats1.imcasts      +
                               stats1.rx_nombuf    +
                               stats1.tx_pause_xon +
                               stats1.rx_pause_xon +
                               stats1.tx_pause_xoff+
                               stats1.rx_pause_xoff ;


   stats->oerrors      =  stats1.oerrors;;
   stats->imcasts      =  0;
   stats->rx_nombuf    =  stats1.rx_nombuf;

}

int CTRexExtendedDriverBase1GVm::wait_for_stable_link(){
    delay(10);
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


/***********************************************************
 * platfrom API object 
 * TODO: REMOVE THIS TO A SEPERATE FILE 
 * 
 **********************************************************/
void
TrexDpdkPlatformApi::get_global_stats(TrexPlatformGlobalStats &stats) const {
    CGlobalStats trex_stats;
    g_trex.get_stats(trex_stats);

    stats.m_stats.m_cpu_util = trex_stats.m_cpu_util;

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
    return CGlobalInfo::m_options.preview.getCores();
}


void
TrexDpdkPlatformApi::port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {

    cores_id_list.clear();

    /* iterate over all DP cores */
    for (uint8_t core_id = 0; core_id < g_trex.get_cores_tx(); core_id++) {

        /* iterate over all the directions*/
        for (uint8_t dir = 0 ; dir < CS_NUM; dir++) {
            if (g_trex.m_cores_vif[core_id + 1]->get_ports()[dir].m_port->get_port_id() == port_id) {
                cores_id_list.push_back(std::make_pair(core_id, dir));
            }
        }
    }
}

void
TrexDpdkPlatformApi::get_interface_info(uint8_t interface_id,
                                        std::string &driver_name,
                                        driver_speed_e &speed) const {

    driver_name = CTRexExtendedDriverDb::Ins()->get_driver_name();
    speed = CTRexExtendedDriverDb::Ins()->get_drv()->get_driver_speed();
}
