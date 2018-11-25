/*
  Hanoh Haim
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
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include <rte_config.h>
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
#include <rte_bus_pci.h>

#include "stt_cp.h"

#include "bp_sim.h"
#include "os_time.h"
#include "common/arg/SimpleGlob.h"
#include "common/arg/SimpleOpt.h"
#include "common/basic_utils.h"
#include "utl_sync_barrier.h"
#include "trex_build_info.h"

extern "C" {
#include "dpdk/drivers/net/ixgbe/base/ixgbe_type.h"
}

#include "trex_messaging.h"
#include "trex_rx_core.h"

/* stateless */
#include "stl/trex_stl.h"
#include "stl/trex_stl_stream_node.h"

/* stateful */
#include "stf/trex_stf.h"

/* ASTF */
#include "astf/trex_astf.h"
#include "astf_batch/trex_astf_batch.h"

#include "publisher/trex_publisher.h"
#include "../linux_dpdk/version.h"

#include "dpdk_funcs.h"
#include "global_io_mode.h"
#include "utl_term_io.h"
#include "msg_manager.h"
#include "platform_cfg.h"
#include "pre_test.h"
#include "stateful_rx_core.h"
#include "debug.h"
#include "pkt_gen.h"
#include "trex_port_attr.h"
#include "trex_driver_base.h"
#include "internal_api/trex_platform_api.h"
#include "main_dpdk.h"
#include "trex_watchdog.h"
#include "utl_port_map.h"
#include "astf/astf_db.h"
#include "utl_offloads.h"

#define MAX_PKT_BURST   32
#define BP_MAX_CORES 48
#define BP_MASTER_AND_LATENCY 2

void set_driver();
void reorder_dpdk_ports();

static inline int get_is_rx_thread_enabled() {
    return ((CGlobalInfo::m_options.is_rx_enabled() || get_is_interactive()) ?1:0);
}

#define MAX_DPDK_ARGS 50
static CPlatformYamlInfo global_platform_cfg_info;
static int g_dpdk_args_num ;
static char * g_dpdk_args[MAX_DPDK_ARGS];
static char g_cores_str[100];
static char g_socket_mem_str[200];
static char g_prefix_str[100];
static char g_loglevel_str[20];
static char g_master_id_str[10];
static char g_image_postfix[10];
static CPciPorts port_map;

#define TREX_NAME "_t-rex-64"

CTRexExtendedDriverDb * CTRexExtendedDriverDb::m_ins;

static inline int get_min_sample_rate(void){
    return ( get_ex_drv()->get_min_sample_rate());
}

// cores =0==1,1*2,2,3,4,5,6
// An enum for all the option types
enum { 
       /* no more before this */
       OPT_MIN,

       OPT_HELP,
       OPT_MODE_BATCH,
       OPT_MODE_INTERACTIVE,
       OPT_NODE_DUMP,
       OPT_DUMP_INTERFACES,
       OPT_UT,
       OPT_PROM, 
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
       OPT_NO_HW_FLOW_STAT,
       OPT_X710_RESET_THRESHOLD,
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
       OPT_CHECKSUM_OFFLOAD_DISABLE,
       OPT_TSO_OFFLOAD_DISABLE,
       OPT_LRO_OFFLOAD_DISABLE,
       OPT_CLOSE,
       OPT_ARP_REF_PER,
       OPT_NO_OFED_CHECK,
       OPT_NO_SCAPY_SERVER,
       OPT_ACTIVE_FLOW,
       OPT_RT,
       OPT_TCP_MODE,
       OPT_STL_MODE,
       OPT_MLX4_SO,
       OPT_MLX5_SO,
       OPT_NTACC_SO,
       OPT_ASTF_SERVR_ONLY,
       OPT_ASTF_CLIENT_MASK,
       OPT_ASTF_TUNABLE,
       OPT_NO_TERMIO,
       OPT_QUEUE_DROP,
       OPT_ASTF_EMUL_DEBUG, 
       OPT_SLEEPY_SCHEDULER,
    
       /* no more pass this */
       OPT_MAX

};

/* options hash - for first pass */
using OptHash = std::unordered_map<int, bool>;

/* these are the argument types:
   SO_NONE --    no argument needed
   SO_REQ_SEP -- single required argument
   SO_MULTI --   multiple arguments needed
*/
static CSimpleOpt::SOption parser_options[] =
    {
        { OPT_HELP,                   "-?",                SO_NONE    },
        { OPT_HELP,                   "-h",                SO_NONE    },
        { OPT_HELP,                   "--help",            SO_NONE    },
        { OPT_UT,                     "--ut",              SO_NONE    },
        { OPT_MODE_BATCH,             "-f",                SO_REQ_SEP },
        { OPT_PROM,                   "--prom",            SO_NONE },
        { OPT_MODE_INTERACTIVE,       "-i",                SO_NONE    },
        { OPT_PLAT_CFG_FILE,          "--cfg",             SO_REQ_SEP },
        { OPT_SINGLE_CORE,            "-s",                SO_NONE    },
        { OPT_FLIP_CLIENT_SERVER,     "--flip",            SO_NONE    },
        { OPT_FLOW_FLIP_CLIENT_SERVER,"-p",                SO_NONE    },
        { OPT_FLOW_FLIP_CLIENT_SERVER_SIDE, "-e",          SO_NONE    },
        { OPT_NO_CLEAN_FLOW_CLOSE,    "--nc",              SO_NONE    },
        { OPT_LIMT_NUM_OF_PORTS,      "--limit-ports",     SO_REQ_SEP },
        { OPT_CORES,                  "-c",                SO_REQ_SEP },
        { OPT_NODE_DUMP,              "-v",                SO_REQ_SEP },
        { OPT_DUMP_INTERFACES,        "--dump-interfaces", SO_MULTI   },
        { OPT_LATENCY,                "-l",                SO_REQ_SEP },
        { OPT_DURATION,               "-d",                SO_REQ_SEP },
        { OPT_PLATFORM_FACTOR,        "-pm",               SO_REQ_SEP },
        { OPT_PUB_DISABLE,            "-pubd",             SO_NONE    },
        { OPT_RATE_MULT,              "-m",                SO_REQ_SEP },
        { OPT_LATENCY_MASK,           "--lm",              SO_REQ_SEP },
        { OPT_ONLY_LATENCY,           "--lo",              SO_NONE    },
        { OPT_LATENCY_PREVIEW,        "-k",                SO_REQ_SEP },
        { OPT_WAIT_BEFORE_TRAFFIC,    "-w",                SO_REQ_SEP },
        { OPT_PCAP,                   "--pcap",            SO_NONE    },
        { OPT_RX_CHECK,               "--rx-check",        SO_REQ_SEP },
        { OPT_IO_MODE,                "--iom",             SO_REQ_SEP },
        { OPT_RX_CHECK_HOPS,          "--hops",            SO_REQ_SEP },
        { OPT_IPV6,                   "--ipv6",            SO_NONE    },
        { OPT_LEARN,                  "--learn",           SO_NONE    },
        { OPT_LEARN_MODE,             "--learn-mode",      SO_REQ_SEP },
        { OPT_LEARN_VERIFY,           "--learn-verify",    SO_NONE    },
        { OPT_L_PKT_MODE,             "--l-pkt-mode",      SO_REQ_SEP },
        { OPT_NO_FLOW_CONTROL,        "--no-flow-control-change", SO_NONE },
        { OPT_NO_HW_FLOW_STAT,        "--no-hw-flow-stat", SO_NONE },
        { OPT_X710_RESET_THRESHOLD,   "--x710-reset-threshold", SO_REQ_SEP },
        { OPT_VLAN,                   "--vlan",            SO_NONE    },
        { OPT_CLIENT_CFG_FILE,        "--client_cfg",      SO_REQ_SEP },
        { OPT_CLIENT_CFG_FILE,        "--client-cfg",      SO_REQ_SEP },
        { OPT_NO_KEYBOARD_INPUT,      "--no-key",          SO_NONE    },
        { OPT_VIRT_ONE_TX_RX_QUEUE,   "--software",        SO_NONE    },
        { OPT_PREFIX,                 "--prefix",          SO_REQ_SEP },
        { OPT_SEND_DEBUG_PKT,         "--send-debug-pkt",  SO_REQ_SEP },
        { OPT_MBUF_FACTOR,            "--mbuf-factor",     SO_REQ_SEP },
        { OPT_NO_WATCHDOG,            "--no-watchdog",     SO_NONE    },
        { OPT_ALLOW_COREDUMP,         "--allow-coredump",  SO_NONE    },
        { OPT_CHECKSUM_OFFLOAD,       "--checksum-offload", SO_NONE   },
        { OPT_CHECKSUM_OFFLOAD_DISABLE, "--checksum-offload-disable", SO_NONE   },
        { OPT_TSO_OFFLOAD_DISABLE,  "--tso-disable", SO_NONE   },
        { OPT_LRO_OFFLOAD_DISABLE,  "--lro-disable", SO_NONE   },
        { OPT_ACTIVE_FLOW,            "--active-flows",   SO_REQ_SEP  },
        { OPT_NTACC_SO,               "--ntacc-so", SO_NONE    },
        { OPT_MLX5_SO,                "--mlx5-so", SO_NONE    },
        { OPT_MLX4_SO,                "--mlx4-so", SO_NONE    },
        { OPT_CLOSE,                  "--close-at-end",    SO_NONE    },
        { OPT_ARP_REF_PER,            "--arp-refresh-period", SO_REQ_SEP },
        { OPT_NO_OFED_CHECK,          "--no-ofed-check",   SO_NONE    },
        { OPT_NO_SCAPY_SERVER,        "--no-scapy-server", SO_NONE    },
        { OPT_RT,                     "--rt",              SO_NONE    },
        { OPT_TCP_MODE,               "--astf",            SO_NONE},
        { OPT_ASTF_EMUL_DEBUG,        "--astf-emul-debug",  SO_NONE},
        { OPT_STL_MODE,               "--stl",             SO_NONE},
        { OPT_ASTF_SERVR_ONLY,        "--astf-server-only",            SO_NONE},
        { OPT_ASTF_CLIENT_MASK,       "--astf-client-mask",SO_REQ_SEP},
        { OPT_ASTF_TUNABLE,           "-t",                SO_REQ_SEP},
        { OPT_NO_TERMIO,              "--no-termio",       SO_NONE},
        { OPT_QUEUE_DROP,             "--queue-drop",      SO_NONE},
        { OPT_SLEEPY_SCHEDULER,       "--sleeps",          SO_NONE},

        SO_END_OF_OPTIONS
    };

static int __attribute__((cold)) usage() {

    printf(" Usage: t-rex-64 [mode] <options>\n\n");
    printf(" mode is one of:\n");
    printf("   -f <file> : YAML file with traffic template configuration (Will run TRex in 'stateful' mode)\n");
    printf("   -i        : Run TRex in 'stateless' mode\n");
    printf("\n");

    printf(" Available options are:\n");
    printf(" --astf                     : Enable advanced stateful mode. profile should be in py format and not YAML format \n");
    printf(" --astf-server-only         : Only server  side ports (1,3..) are enabled with ASTF service. Traffic won't be transmitted on clients ports. \n");
    printf(" --astf-client-mask         : Enable only specific client side ports with ASTF service. \n");
    printf("                              For example, with 4 ports setup. 0x1 means that only port 0 will be enabled. ports 2 won't be enabled. \n");
    printf("                              Can't be used with --astf-server-only. \n");
    printf("\n");
    printf(" --stl                      : Starts in stateless mode. must be provided along with '-i' for interactive mode \n");
    printf(" --active-flows             : An experimental switch to scale up or down the number of active flows.  \n");
    printf("                              It is not accurate due to the quantization of flow scheduler and in some case does not work. \n");
    printf("                              Example --active-flows 500000 wil set the ballpark of the active flow to be ~0.5M \n");
    printf(" --allow-coredump           : Allow creation of core dump \n");
    printf(" --arp-refresh-period       : Period in seconds between sending of gratuitous ARP for our addresses. Value of 0 means 'never send' \n");
    printf(" -c <num>>                  : Number of hardware threads to allocate for each port pair. Overrides the 'c' argument from config file \n");
    printf(" --cfg <file>               : Use file as TRex config file instead of the default /etc/trex_cfg.yaml \n");
    printf(" --checksum-offload         : Deprecated,enable by default. Enable IP, TCP and UDP tx checksum offloading, using DPDK. This requires all used interfaces to support this  \n");
    printf(" --checksum-offload-disable : Disable IP, TCP and UDP tx checksum offloading, using DPDK. This requires all used interfaces to support this  \n");
    printf(" --tso-disable              : disable TSO (advanced TCP mode) \n");
    printf(" --lro-disable              : disable LRO (advanced TCP mode) \n");
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
    printf(" -k  <num>                  : Run 'warm up' traffic for num seconds before starting the test.\n");
    printf("                               Works only with the latency test (-l option)\n");
    printf(" -l <rate>                  : In parallel to the test, run latency check, sending packets at rate/sec from each interface \n");
    printf(" --l-pkt-mode <0-3>         : Set mode for sending latency packets \n");
    printf("      0 (default)    send SCTP packets  \n");
    printf("      1              Send ICMP request packets  \n");
    printf("      2              Send ICMP requests from client side, and response from server side (for working with firewall) \n");
    printf("      3              Send ICMP requests with sequence ID 0 from both sides \n");
    printf("    Rate of zero means no latency check \n");
    printf(" --learn (deprecated). Replaced by --learn-mode. To get older behaviour, use --learn-mode 2 \n");
    printf(" --learn-mode [1-3]         : Used for working in NAT environments. Dynamically learn the NAT translation done by the DUT \n");
    printf("      1    In case of TCP flow, use TCP ACK in first SYN to pass NAT translation information. Initial SYN packet must be first packet in the TCP flow \n");
    printf("           In case of UDP stream, NAT translation information will pass in IP ID field of first packet in flow. This means that this field is changed by TRex\n");
    printf("      2    Add special IP option to pass NAT translation information to first packet of each flow. Will not work on certain firewalls if they drop packets with IP options \n");
    printf("      3    Like 1, but without support for sequence number randomization in server->client direction. Performance (flow/second) better than 1 \n");
    printf(" --learn-verify             : Test the NAT translation mechanism. Should be used when there is no NAT in the setup \n");
    printf(" --limit-ports              : Limit number of ports used. Must be even number (TRex always uses port pairs) \n");
    printf(" --lm                       : Hex mask of cores that should send traffic \n");
    printf("                              For example: Value of 0x5 will cause only ports 0 and 2 to send traffic \n");
    printf(" --lo                       : Only run latency test \n");
    printf(" -m <num>                   : Rate multiplier.  Multiply basic rate of templates by this number \n");
    printf(" --mbuf-factor              : Factor for packet memory \n");
    printf(" --nc                       : If set, will not wait for all flows to be closed, before terminating - see manual for more information \n");
    printf(" --no-flow-control-change   : By default TRex disables flow-control. If this option is given, it does not touch it \n");
    printf(" --no-hw-flow-stat          : Relevant only for Intel x710 stateless mode. Do not use HW counters for flow stats\n");
    printf("                            : Enabling this will support lower traffic rate, but will also report RX byte count statistics. See manual for more details\n");
    printf(" --no-key                   : Daemon mode, don't get input from keyboard \n");
    printf(" --no-ofed-check            : Disable the check of OFED version \n");
    printf(" --no-scapy-server          : Disable Scapy server implicit start at stateless \n");
    printf(" --no-termio                : Do not use TERMIO. useful when using GDB and ctrl+c is needed. \n");
    printf(" --no-watchdog              : Disable watchdog \n");
    printf(" --rt                       : Run TRex DP/RX cores in realtime priority \n");
    printf(" -p                         : Send all flow packets from the same interface (choosed randomly between client ad server ports) without changing their src/dst IP \n");
    printf(" -pm                        : Platform factor. If you have splitter in the setup, you can multiply the total results by this factor \n");
    printf("                              e.g --pm 2.0 will multiply all the results bps in this factor \n");
    printf(" --prefix <nam>             : For running multi TRex instances on the same machine. Each instance should have different name \n");
    printf(" --prom                     : Enable promiscuous for ASTF/STF mode  \n");
    printf(" -pubd                      : Disable monitors publishers \n");
    printf(" --queue-drop               : Do not retry to send packets on failure (queue full etc.)\n");
    printf(" --rx-check  <rate>         : Enable rx check. TRex will sample flows at 1/rate and check order, latency and more \n");
    printf(" -s                         : Single core. Run only one data path core. For debug \n");
    printf(" --send-debug-pkt <proto>   : Do not run traffic generator. Just send debug packet and dump receive queues \n");
    printf("                              Supported protocols are 1 for icmp, 2 for UDP, 3 for TCP, 4 for ARP, 5 for 9K UDP \n");
    printf(" --sleeps                   : Use sleeps instead of busy wait in scheduler (less accurate, more power saving)\n");
    printf(" --software                 : Do not configure any hardware rules. In this mode we use 1 core, and one RX queue and one TX queue per port\n");
    printf(" -v <verbosity level>       : The higher the value, print more debug information \n");
    printf(" --vlan                     : Relevant only for stateless mode with Intel 82599 10G NIC \n");
    printf("                              When configuring flow stat and latency per stream rules, assume all streams uses VLAN \n");
    printf(" -w  <num>                  : Wait num seconds between init of interfaces and sending traffic, default is 1 \n");
    

    printf("\n");
    printf(" Examples: ");
    printf(" basic trex run for 20 sec and multiplier of 10 \n");
    printf("  t-rex-64 -f cap2/dns.yaml -m 10 -d 20 \n");
    printf("\n\n");
    printf(" Copyright (c) 2015-2017 Cisco Systems, Inc.    \n");
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
    printf(" BPF        (BSD)       \n");
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
    
    TrexBuildInfo::show();
    
    return (0);
}


int gtest_main(int argc, char **argv) ;

static void parse_err(const std::string &msg) {
    std::cout << "\nArgument Parsing Error: \n\n" << "*** "<< msg << "\n\n";
    exit(-1);
}

/**
 * convert an option to string
 * 
 */
static std::string opt_to_str(int opt) {
    for (const CSimpleOpt::SOption &x : parser_options) {
        if (x.nId == opt) {
            return std::string(x.pszArg);
        }
    }
    
    assert(0);
    return "";
}


/**
 * checks options exclusivity
 * 
 */
static void check_exclusive(const OptHash &args_set,
                            const std::initializer_list<int> &opts,
                            bool at_least_one = false) {
    int seen = -1;
    
    for (int x : opts) {
        if (args_set.at(x)) {
            if (seen == -1) {
                seen = x;
            } else {
                parse_err("'" + opt_to_str(seen) + "' and '" + opt_to_str(x) + "' are mutual exclusive");
            }
        }
    }
    
    if ( (seen == -1) && at_least_one ) {
        std::string opts_str;
        for (int x : opts) {
            opts_str += "'" + opt_to_str(x) + "', ";
        }
        
        /* remove the last 2 chars */
        opts_str.pop_back();
        opts_str.pop_back();
        parse_err("please specify at least one from the following parameters: " + opts_str);
    }
}

struct ParsingOptException : public std::exception {
    const ESOError m_err_code;
    const char    *m_opt_text;
    ParsingOptException(CSimpleOpt &args):
                    m_err_code(args.LastError()),
                    m_opt_text(args.OptionText()) {}
};

static OptHash
args_first_pass(int argc, char *argv[], CParserOption* po) {
    CSimpleOpt args(argc, argv, parser_options);
    OptHash args_set;
    
    /* clear */
    for (int i = OPT_MIN + 1; i < OPT_MAX; i++) {
        args_set[i] = false;
    }
    
    /* set */
    while (args.Next()) {
        if (args.LastError() != SO_SUCCESS) {
            throw ParsingOptException(args);
        }
        args_set[args.OptionId()] = true;
    }
    
    if (args_set[OPT_HELP]) {
        usage();
        exit(0);
    }
    
    /* sanity */
    
    /* STL and TCP */
    check_exclusive(args_set, {OPT_STL_MODE, OPT_TCP_MODE});
    
    /* interactive, batch or dump */
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_MODE_BATCH, OPT_DUMP_INTERFACES}, true);
    
    /* interactive mutual exclusion options */
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_DURATION});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_CLIENT_CFG_FILE});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_RX_CHECK});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_LATENCY});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_ONLY_LATENCY});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_SINGLE_CORE});
    check_exclusive(args_set, {OPT_MODE_INTERACTIVE, OPT_RATE_MULT});
    
    return args_set;
}

static void get_dpdk_drv_params(CTrexDpdkParams &dpdk_p){
    CPlatformYamlInfo *cg = &global_platform_cfg_info;

    CTrexDpdkParamsOverride dpdk_over_p;

    get_mode()->get_dpdk_drv_params(dpdk_p);

    /* override by driver */
    if (get_ex_drv()->is_override_dpdk_params(dpdk_over_p)){
        /* need to override by driver */
        if (dpdk_over_p.rx_desc_num_data_q){
            dpdk_p.rx_desc_num_data_q = dpdk_over_p.rx_desc_num_data_q;
        }
        if (dpdk_over_p.rx_desc_num_drop_q){
            dpdk_p.rx_desc_num_drop_q = dpdk_over_p.rx_desc_num_drop_q;
        }
        if (dpdk_over_p.rx_desc_num_dp_q){
            dpdk_p.rx_desc_num_dp_q = dpdk_over_p.rx_desc_num_dp_q;
        }
        if (dpdk_over_p.tx_desc_num){
            dpdk_p.tx_desc_num = dpdk_over_p.tx_desc_num;
        }
    }
    /* override by configuration */
    if (cg->m_rx_desc){
        dpdk_p.rx_desc_num_data_q = cg->m_rx_desc;
        dpdk_p.rx_desc_num_dp_q = cg->m_rx_desc;
    }
    if (cg->m_tx_desc){
        dpdk_p.tx_desc_num = cg->m_tx_desc;
    }

    bool rx_scatter = get_ex_drv()->is_support_for_rx_scatter_gather();
    if (!rx_scatter){
        dpdk_p.rx_mbuf_type = MBUF_9k;
    }
}


static rte_mempool_t* get_rx_mem_pool(int socket_id) {
    CTrexDpdkParams dpdk_p;
    get_dpdk_drv_params(dpdk_p);

    switch(dpdk_p.rx_mbuf_type) {
    case MBUF_9k:
        return CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_9k;
    case MBUF_2048:
        return CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048;
    default:
        fprintf(stderr, "Internal error: Wrong rx_mem_pool");
        assert(0);
        return nullptr;
    }
}


static int parse_options(int argc, char *argv[], bool first_time ) {
    CSimpleOpt args(argc, argv, parser_options);

    CParserOption *po = &CGlobalInfo::m_options;
    CPlatformYamlInfo *cg = &global_platform_cfg_info;

    bool latency_was_set=false;
    (void)latency_was_set;
    char ** rgpszArg = NULL;
    bool opt_vlan_was_set = false;

    int a=0;
    int node_dump=0;

    po->preview.setFileWrite(true);
    po->preview.setRealTime(true);
    uint32_t tmp_data;
    float tmp_double;
    
    /* first run - pass all parameters for existance */
    OptHash args_set = args_first_pass(argc, argv, po);
    
    
    while ( args.Next() ){
        if (args.LastError() == SO_SUCCESS) {
            switch (args.OptionId()) {

            case OPT_UT :
                parse_err("Supported only in simulation");
                break;

            case OPT_HELP:
                usage();
                return -1;

            case OPT_ASTF_EMUL_DEBUG:
                po->preview.setEmulDebug(true);
                break;
            /* astf */
            case OPT_TCP_MODE:
                /* can be batch or non batch */
                set_op_mode((args_set[OPT_MODE_INTERACTIVE] ? OP_MODE_ASTF : OP_MODE_ASTF_BATCH));
                break;
                
            /* stl */
            case OPT_STL_MODE:
                set_op_mode(OP_MODE_STL);
                break;

            case OPT_ASTF_SERVR_ONLY:
                po->m_astf_mode = CParserOption::OP_ASTF_MODE_SERVR_ONLY;
                break;
            case OPT_ASTF_TUNABLE:
                /* do bothing with it */
                break;
            case OPT_ASTF_CLIENT_MASK:
                po->m_astf_mode = CParserOption::OP_ASTF_MODE_CLIENT_MASK;
                sscanf(args.OptionArg(),"%x", &po->m_astf_client_mask);
                break;

            case OPT_MODE_BATCH:
                po->cfg_file = args.OptionArg();
                break;

            case OPT_PROM :
                po->preview.setPromMode(true);
                break;

            case OPT_MODE_INTERACTIVE:
                /* defines the OP mode */
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

            case OPT_RT:
                po->preview.set_rt_prio_mode(true);
                break;

            case OPT_NTACC_SO:
                po->preview.set_ntacc_so_mode(true);
                break;

            case OPT_MLX5_SO:
                po->preview.set_mlx5_so_mode(true);
                break;

            case OPT_MLX4_SO:
                po->preview.set_mlx4_so_mode(true);
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

            case OPT_NO_HW_FLOW_STAT:
                po->preview.set_disable_hw_flow_stat(true);
                break;
            case OPT_NO_FLOW_CONTROL:
                po->preview.set_disable_flow_control_setting(true);
                break;
            case OPT_X710_RESET_THRESHOLD:
                po->set_x710_fdir_reset_threshold(atoi(args.OptionArg()));
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
                if ( !first_time ) {
                    rgpszArg = args.MultiArg(1);
                    if ( rgpszArg != NULL ) {
                        cg->m_if_list.clear();
                        do {
                            cg->m_if_list.push_back(rgpszArg[0]);
                            rgpszArg = args.MultiArg(1);
                        } while (rgpszArg != NULL);
                    }
                }
                set_op_mode(OP_MODE_DUMP_INTERFACES);
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
            case OPT_NO_TERMIO:
                po->preview.set_termio_disabled(true);
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
            case OPT_ACTIVE_FLOW:
                sscanf(args.OptionArg(),"%f", &tmp_double);
                po->m_active_flows=(uint32_t)tmp_double;
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
                get_mode()->force_software_mode(true);
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

            case OPT_CHECKSUM_OFFLOAD_DISABLE:
                po->preview.setChecksumOffloadDisable(true);
                break;

            case OPT_TSO_OFFLOAD_DISABLE:
                po->preview.setTsoOffloadDisable(true);
                break;
            case OPT_LRO_OFFLOAD_DISABLE:
                po->preview.setLroOffloadDisable(true);
                break;
            case OPT_CLOSE:
                po->preview.setCloseEnable(true);
                break;
            case  OPT_ARP_REF_PER:
                sscanf(args.OptionArg(),"%d", &tmp_data);
                po->m_arp_ref_per=(uint16_t)tmp_data;
                break;
            case OPT_NO_OFED_CHECK:
                break;
            case OPT_NO_SCAPY_SERVER:
                break;
            case OPT_QUEUE_DROP:
                CGlobalInfo::m_options.m_is_queuefull_retry = false;
                break;
            case OPT_SLEEPY_SCHEDULER:
                CGlobalInfo::m_options.m_is_sleepy_scheduler = true;
                break;

            default:
                printf("Error: option %s is not handled.\n\n", args.OptionText());
                return -1;
                break;
            } // End of switch
        }// End of IF
        else {
            throw ParsingOptException(args);
        }
    } // End of while


    /* if no specific mode was provided, fall back to defaults - stateless on intearactive, stateful on batch */
    if (get_op_mode() == OP_MODE_INVALID) {
        set_op_mode( (args_set.at(OPT_MODE_INTERACTIVE)) ? OP_MODE_STL : OP_MODE_STF);
    }

    if ( CGlobalInfo::m_options.m_is_lowend ) {
        po->preview.setCores(1);
    }

    if (CGlobalInfo::is_learn_mode() && po->preview.get_ipv6_mode_enable()) {
        parse_err("--learn mode is not supported with --ipv6, beacuse there is no such thing as NAT66 (ipv6 to ipv6 translation) \n" \
                  "If you think it is important, please open a defect or write to TRex mailing list\n");
    }

    if (po->preview.get_is_rx_check_enable() ||  po->is_latency_enabled() || CGlobalInfo::is_learn_mode()
        || (CGlobalInfo::m_options.m_arp_ref_per != 0)
        || (!get_dpdk_mode()->is_hardware_filter_needed()) ) {
        po->set_rx_enabled();
    }

    if ( node_dump ){
        po->preview.setVMode(a);
    }

    if (po->m_platform_factor==0.0){
        parse_err(" you must provide a non zero multipler for platform -pm 0 is not valid \n");
    }

    /* if we have a platform factor we need to devided by it so we can still work with normalized yaml profile  */
    po->m_factor = po->m_factor/po->m_platform_factor;

    if (po->m_factor==0.0) {
        parse_err(" you must provide a non zero multipler -m 0 is not valid \n");
    }


    if ( first_time ){
        /* only first time read the configuration file */
        if ( po->platform_cfg_file.length() >0  ) {
            if ( node_dump ){
                printf("Using configuration file %s \n",po->platform_cfg_file.c_str());
            }
            cg->load_from_yaml_file(po->platform_cfg_file);
            if ( node_dump ){
                cg->Dump(stdout);
            }
        }else{
            if ( utl_is_file_exists("/etc/trex_cfg.yaml") ){
                if ( node_dump ){
                    printf("Using configuration file /etc/trex_cfg.yaml \n");
                }
                cg->load_from_yaml_file("/etc/trex_cfg.yaml");
                if ( node_dump ){
                    cg->Dump(stdout);
                }
            }
        }
    } else {
        if ( cg->m_if_list.size() > CGlobalInfo::m_options.m_expected_portd ) {
            cg->m_if_list.resize(CGlobalInfo::m_options.m_expected_portd);
        }
    }

    if ( get_is_interactive() ) {
        if ( opt_vlan_was_set ) {
            // Only purpose of this in interactive is for configuring the 82599 rules correctly
            po->preview.set_vlan_mode(CPreviewMode::VLAN_MODE_NORMAL);
        }

    } else {
        if ( !po->m_duration ) {
            po->m_duration = 3600.0;
        }
        if ( global_platform_cfg_info.m_tw.m_info_exist ){

            CTimerWheelYamlInfo *lp=&global_platform_cfg_info.m_tw;
            std::string  err;
            if (!lp->Verify(err)){
                parse_err(err);
            }

            po->set_tw_bucket_time_in_usec(lp->m_bucket_time_usec);
            po->set_tw_buckets(lp->m_buckets);
            po->set_tw_levels(lp->m_levels);
        }
    }


    return 0;
}

void free_args_copy(int argc, char **argv_copy) {
    for(int i=0; i<argc; i++) {
        free(argv_copy[i]);
    }
    free(argv_copy);
}

static int parse_options_wrapper(int argc, char *argv[], bool first_time ) {
    // copy, as arg parser sometimes changes the argv
    char ** argv_copy = (char **) malloc(sizeof(char *) * argc);
    for(int i=0; i<argc; i++) {
        argv_copy[i] = strdup(argv[i]);
    }
    int ret = 0;
    try {
        ret = parse_options(argc, argv_copy, first_time);
    } catch (ParsingOptException &e) {
        if (e.m_err_code == SO_OPT_INVALID) {
            printf("Error: option %s is not recognized.\n\n", e.m_opt_text);
        } else if (e.m_err_code == SO_ARG_MISSING) {
            printf("Error: option %s is expected to have argument.\n\n", e.m_opt_text);
        }
        free_args_copy(argc, argv_copy);
        usage();
        return -1;
    }

    free_args_copy(argc, argv_copy);
    return ret;
}

int main_test(int argc , char * argv[]);



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
        printf(" Warning can't flush rx-queue for port %d \n",(int)m_tvpid);
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

void CPhyEthIF::configure(uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue,
                          const struct rte_eth_conf *eth_conf){
    int ret;
    ret = rte_eth_dev_configure(m_repid,
                                nb_rx_queue,
                                nb_tx_queue,
                                eth_conf);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: "
                 "err=%d, port=%u\n",
                 ret, m_repid);

    /* get device info */
    const struct rte_eth_dev_info *m_dev_info = m_port_attr->get_dev_info();

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
        /* check if the device supports TCP and UDP checksum offloading */
        if ((m_dev_info->tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) == 0) {
            rte_exit(EXIT_FAILURE, "Device does not support UDP checksum offload: "
                     "port=%u\n",
                     m_repid);
        }
        if ((m_dev_info->tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0) {
            rte_exit(EXIT_FAILURE, "Device does not support TCP checksum offload: "
                     "port=%u\n",
                     m_repid);
        }
    }
}

/*
  rx-queue 0 is the default queue. All traffic not going to queue 1
  will be dropped as queue 0 is disabled
  rx-queue 1 - Latency measurement packets and other features that need software processing will go here.
*/
void CPhyEthIF::configure_rx_duplicate_rules(){
    if ( get_dpdk_mode()->is_hardware_filter_needed() ){
        get_ex_drv()->configure_rx_filter_rules(this);
    }
}

int CPhyEthIF::set_port_rcv_all(bool is_rcv) {
    if ( get_dpdk_mode()->is_hardware_filter_needed() ){
        get_ex_drv()->set_rcv_all(this, is_rcv);
    }
    return 0;
}

void CPhyEthIF::stop_rx_drop_queue() {
    CDpdkModeBase * dpdk_mode = get_dpdk_mode();
    if ( dpdk_mode->is_drop_rx_queue_needed() ) {
        get_ex_drv()->stop_queue(this, MAIN_DPDK_DROP_Q);
    }
}


void CPhyEthIF::rx_queue_setup(uint16_t rx_queue_id,
                               uint16_t nb_rx_desc,
                               unsigned int socket_id,
                               const struct rte_eth_rxconf *rx_conf,
                               struct rte_mempool *mb_pool){

    int ret = rte_eth_rx_queue_setup(m_repid , rx_queue_id,
                                     nb_rx_desc,
                                     socket_id,
                                     rx_conf,
                                     mb_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: "
                 "err=%d, port=%u\n",
                 ret, m_repid);
}



void CPhyEthIF::tx_queue_setup(uint16_t tx_queue_id,
                               uint16_t nb_tx_desc,
                               unsigned int socket_id,
                               const struct rte_eth_txconf *tx_conf){

    int ret = rte_eth_tx_queue_setup( m_repid,
                                      tx_queue_id,
                                      nb_tx_desc,
                                      socket_id,
                                      tx_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
                 "err=%d, port=%u queue=%u\n",
                 ret, m_repid, tx_queue_id);

}

void CPhyEthIF::stop(){
    if (CGlobalInfo::m_options.preview.getCloseEnable()) {
        rte_eth_dev_stop(m_repid);
        rte_eth_dev_close(m_repid);
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
        ret = rte_eth_dev_start(m_repid);
        if (ret==0) {
            return;
        }
        delay(1000);
    }
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start: "
                 "err=%d, port=%u\n",
                 ret, m_repid);

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
        ret=rte_eth_dev_flow_ctrl_set(m_repid,&fc_conf);
        if (ret==0) {
            break;
        }
        delay(1000);
    }
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_flow_ctrl_set: "
                 "err=%d, port=%u\n probably link is down. Please check your link activity, or skip flow-control disabling, using: --no-flow-control-change option\n",
                 ret, m_repid);
}

int DpdkTRexPortAttr::add_mac(char * mac){
    struct ether_addr mac_addr;
    for (int i=0; i<6;i++) {
        mac_addr.addr_bytes[i] =mac[i];
    }

    if ( get_ex_drv()->hardware_support_mac_change() ) {
        if ( rte_eth_dev_mac_addr_add(m_repid, &mac_addr,0) != 0) {
            printf("Failed setting MAC for port %d \n", (int)m_repid);
            exit(-1);
        }
    }

    return 0;
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
    
    /* block until this succeeds */
    while (!get_extended_stats()) {
        delay(10);
    }
    
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

    if (isVerbose(2)) {
        fprintf(stdout, "Pre test statistics for port %d\n", m_tvpid);
        m_ignore_stats.dump(stdout);
    }
}

void CPhyEthIF::dump_stats(FILE *fd){

    update_counters();

    fprintf(fd,"port : %d \n",(int)m_tvpid);
    fprintf(fd,"------------\n");
    m_stats.DumpAll(fd);
    //m_stats.Dump(fd);
    printf (" Tx : %.1fMb/sec  \n",m_last_tx_rate);
    //printf (" Rx : %.1fMb/sec  \n",m_last_rx_rate);
}

void CPhyEthIF::stats_clear(){
    rte_eth_stats_reset(m_repid);
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

    bool redirect_to_rx_core(pkt_dir_t   dir,rte_mbuf_t * m);

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p);

    virtual pkt_dir_t port_id_to_dir(uint8_t port_id);
    void GetCoreCounters(CVirtualIFPerSideStats *stats);
    void DumpCoreStats(FILE *fd);
    void DumpIfStats(FILE *fd);
    static void DumpIfCfgHeader(FILE *fd);
    void DumpIfCfg(FILE *fd);

    socket_id_t get_socket_id(){
        return ( CGlobalInfo::m_socket.port_to_socket( m_ports[0].m_port->get_tvpid() ) );
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

protected:
    uint8_t      m_core_id;
    uint16_t     m_mbuf_cache;
    CCorePerPort m_ports[CS_NUM]; /* each core has 2 tx queues 1. client side and server side */
    CNodeRing *  m_ring_to_rx;

} __rte_cache_aligned;

class CCoreEthIFStateless : public CCoreEthIF {
public:
    virtual int send_node_flow_stat(rte_mbuf *m, CGenNodeStateless * node_sl, CCorePerPort *  lp_port
                                    , CVirtualIFPerSideStats  * lp_stats, bool is_const);

     /* works in sw multi core only, need to verify it */
    virtual uint16_t rx_burst(pkt_dir_t dir,
                              struct rte_mbuf **rx_pkts,
                              uint16_t nb_pkts);
    /**
     * fast path version
     */
    virtual int send_node(CGenNode *node);

    /**
     * slow path version
     */
    virtual int send_node_service_mode(CGenNode *node);

protected:
    template <bool SERVICE_MODE> inline int send_node_common(CGenNode *no);

    inline rte_mbuf_t * generate_node_pkt(CGenNodeStateless *node_sl)   __attribute__ ((always_inline));
    inline int send_node_packet(CGenNodeStateless      *node_sl,
                                rte_mbuf_t             *m,
                                CCorePerPort           *lp_port,
                                CVirtualIFPerSideStats *lp_stats)   __attribute__ ((always_inline));

    rte_mbuf_t * generate_slow_path_node_pkt(CGenNodeStateless *node_sl);

public:
    void set_rx_queue_id(uint16_t client_qid,
                         uint16_t server_qid){
        m_rx_queue_id[CLIENT_SIDE]=client_qid;
        m_rx_queue_id[SERVER_SIDE]=server_qid;
    }
public:
    uint16_t     m_rx_queue_id[CS_NUM]; 
};

class CCoreEthIFTcp : public CCoreEthIF {
public:
    CCoreEthIFTcp() {
        m_rx_queue_id[CLIENT_SIDE]=0xffff;
        m_rx_queue_id[SERVER_SIDE]=0xffff;
    }

    uint16_t     rx_burst(pkt_dir_t dir,
                          struct rte_mbuf **rx_pkts,
                          uint16_t nb_pkts);

    virtual int send_node(CGenNode *node);

    void set_rx_queue_id(uint16_t client_qid,
                         uint16_t server_qid){
        m_rx_queue_id[CLIENT_SIDE]=client_qid;
        m_rx_queue_id[SERVER_SIDE]=server_qid;
    }
public:
    uint16_t     m_rx_queue_id[CS_NUM]; 
};


uint16_t get_client_side_vlan(CVirtualIF * _ifs){
    CCoreEthIFTcp * lpif=(CCoreEthIFTcp *)_ifs;
    CCorePerPort *lp_port = (CCorePerPort *)lpif->get_ports();
    uint8_t port_id = lp_port->m_port->get_tvpid();
    uint16_t vlan=CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan();
    return(vlan);
}


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
             m_ports[CLIENT_SIDE].m_port->get_tvpid(),
             m_ports[CLIENT_SIDE].m_tx_queue_id,
             m_ports[SERVER_SIDE].m_port->get_tvpid(),
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
        fprintf (fd," port %d, queue id :%d  - %s \n",lp->m_port->get_tvpid(),lp->m_tx_queue_id,t[dir] );
        fprintf (fd," ---------------------------- \n");
        lpstats->Dump(fd);
    }
}

/**
 * when measureing performance with perf prefer drop in case of 
 * queue full 
 * this will allow us actually measure the max B/W possible 
 * without the noise of retrying 
 */

int CCoreEthIF::send_burst(CCorePerPort * lp_port,
                           uint16_t len,
                           CVirtualIFPerSideStats  * lp_stats){

    uint16_t ret = lp_port->m_port->tx_burst(lp_port->m_tx_queue_id,lp_port->m_table,len);
    if (likely( CGlobalInfo::m_options.m_is_queuefull_retry )) {
        while ( unlikely( ret<len ) ){
            rte_delay_us(1);
            lp_stats->m_tx_queue_full += 1;
            uint16_t ret1=lp_port->m_port->tx_burst(lp_port->m_tx_queue_id,
                                                    &lp_port->m_table[ret],
                                                    len-ret);
            ret+=ret1;
        }
    } else {
        /* CPU has burst of packets larger than TX can send. Need to drop packets */
        if ( unlikely(ret < len) ) {
            lp_stats->m_tx_drop += (len-ret);
            uint16_t i;
            for (i=ret; i<len;i++) {
                rte_mbuf_t * m=lp_port->m_table[i];
                rte_pktmbuf_free(m);
            }
        }
    }

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

    if (likely( CGlobalInfo::m_options.m_is_queuefull_retry )) {
        while ( unlikely( ret != 1 ) ){
            rte_delay_us(1);
            lp_stats->m_tx_queue_full += 1;
            ret = lp_port->m_port->tx_burst(lp_port->m_tx_queue_id_lat, &m, 1);
        }
    } else {
        if ( unlikely( ret != 1 ) ) {
            lp_stats->m_tx_drop ++;
            rte_pktmbuf_free(m);
            return 0;
        }
    }
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

uint16_t CCoreEthIFTcp::rx_burst(pkt_dir_t dir,
                                 struct rte_mbuf **rx_pkts,
                                 uint16_t nb_pkts){
    uint16_t res = m_ports[dir].m_port->rx_burst(m_rx_queue_id[dir],rx_pkts,nb_pkts);
    return (res);
}


int CCoreEthIFTcp::send_node(CGenNode *node){
    CNodeTcp * node_tcp = (CNodeTcp *) node;
    uint8_t dir=node_tcp->dir;
    CCorePerPort *lp_port = &m_ports[dir];
    CVirtualIFPerSideStats *lp_stats = &m_stats[dir];
    TrexCaptureMngr::getInstance().handle_pkt_tx(node_tcp->mbuf, lp_port->m_port->get_tvpid());
    send_pkt(lp_port,node_tcp->mbuf,lp_stats);
    return (0);
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

inline rte_mbuf_t *
CCoreEthIFStateless::generate_node_pkt(CGenNodeStateless *node_sl) {
    if (unlikely(node_sl->get_is_slow_path())) {
        return generate_slow_path_node_pkt(node_sl);
    }

    /* check that we have mbuf  */
    rte_mbuf_t *m;

    if ( likely(node_sl->is_cache_mbuf_array()) ) {
        m = node_sl->cache_mbuf_array_get_cur();
        rte_pktmbuf_refcnt_update(m,1);
    }else{
        m = node_sl->get_cache_mbuf();

        if (m) {
            /* cache case */
            rte_pktmbuf_refcnt_update(m,1);
        }else{
            m=node_sl->alloc_node_with_vm();
            assert(m);
        }
    }

    return m;
}

inline int
CCoreEthIFStateless::send_node_packet(CGenNodeStateless      *node_sl,
                                      rte_mbuf_t             *m,
                                      CCorePerPort           *lp_port,
                                      CVirtualIFPerSideStats *lp_stats) {

    if (unlikely(node_sl->is_stat_needed())) {
        if ( unlikely(node_sl->is_cache_mbuf_array()) ) {
            // No support for latency + cache. If user asks for cache on latency stream, we change cache to 0.
            // assert here just to make sure.
            assert(1);
        }
        return send_node_flow_stat(m, node_sl, lp_port, lp_stats, (node_sl->get_cache_mbuf()) ? true : false);
    } else {
        return send_pkt(lp_port, m, lp_stats);
    }
}

uint16_t CCoreEthIFStateless::rx_burst(pkt_dir_t dir,
                                 struct rte_mbuf **rx_pkts,
                                 uint16_t nb_pkts){
    uint16_t res = m_ports[dir].m_port->rx_burst(m_rx_queue_id[dir],rx_pkts,nb_pkts);
    return (res);
}


int CCoreEthIFStateless::send_node(CGenNode *node) {
    return send_node_common<false>(node);
}

int CCoreEthIFStateless::send_node_service_mode(CGenNode *node) {
    return send_node_common<true>(node);
}

/**
 * this is the common function and it is templated
 * for two compiler evaluation for performance
 *
 */
template <bool SERVICE_MODE>
int CCoreEthIFStateless::send_node_common(CGenNode *node) {
    CGenNodeStateless * node_sl = (CGenNodeStateless *) node;

    pkt_dir_t dir                     = (pkt_dir_t)node_sl->get_mbuf_cache_dir();
    CCorePerPort *lp_port             = &m_ports[dir];
    CVirtualIFPerSideStats *lp_stats  = &m_stats[dir];

    /* generate packet (can never fail) */
    rte_mbuf_t *m = generate_node_pkt(node_sl);

    /* template boolean - this will be removed at compile time */
    if (SERVICE_MODE) {
        TrexCaptureMngr::getInstance().handle_pkt_tx(m, lp_port->m_port->get_tvpid());
    }

    /* send */
    return send_node_packet(node_sl, m, lp_port, lp_stats);
}

/**
 * slow path code goes here
 *
 */
rte_mbuf_t *
CCoreEthIFStateless::generate_slow_path_node_pkt(CGenNodeStateless *node_sl) {

    if (node_sl->m_type == CGenNode::PCAP_PKT) {
        CGenNodePCAP *pcap_node = (CGenNodePCAP *)node_sl;
        return pcap_node->get_pkt();
    }

    /* unhandled case of slow path node */
    assert(0);
    return (NULL);
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
        assert(node->m_client_cfg);
        node->m_client_cfg->apply(m, dir);
    }

}

bool CCoreEthIF::redirect_to_rx_core(pkt_dir_t   dir,
                                     rte_mbuf_t * m){
    bool sent=false;

    CGenNodeLatencyPktInfo * node=(CGenNodeLatencyPktInfo * )CGlobalInfo::create_node();
    if ( node ) {
        node->m_msg_type = CGenNodeMsgBase::LATENCY_PKT;
        node->m_dir      = dir;
        node->m_latency_offset = 0xdead;
        node->m_pkt      = m;
        if ( m_ring_to_rx->Enqueue((CGenNode*)node)==0 ){
            sent=true;
        }else{
            rte_pktmbuf_free(m);
            CGlobalInfo::free_node((CGenNode *)node);
        }

#ifdef LATENCY_QUEUE_TRACE_
        printf("rx to cp --\n");
        rte_pktmbuf_dump(stdout,m, rte_pktmbuf_pkt_len(m));
#endif
    }

    if (sent==false) {
        /* inc counter */
        CVirtualIFPerSideStats *lp_stats = &m_stats[dir];
        lp_stats->m_tx_redirect_error++;
    }
    return (sent);
}


int CCoreEthIF::send_node(CGenNode * node) {


    CFlowPktInfo *  lp=node->m_pkt_info;
    rte_mbuf_t *    m=lp->generate_new_mbuf(node);

    pkt_dir_t       dir;
    bool            single_port;

    dir         = node->cur_interface_dir();
    single_port = node->get_is_all_flow_from_same_dir() ;


    if ( unlikely(CGlobalInfo::m_options.preview.get_vlan_mode()
                  != CPreviewMode::VLAN_MODE_NONE) ) {
        uint16_t vlan_id=0;

        if (CGlobalInfo::m_options.preview.get_vlan_mode()
            == CPreviewMode::VLAN_MODE_LOAD_BALANCE) {
            /* which vlan to choose 0 or 1*/
            uint8_t vlan_port = (node->m_src_ip & 1);
            vlan_id = CGlobalInfo::m_options.m_vlan_port[vlan_port];
            if (likely( vlan_id > 0 ) ) {
                dir = dir ^ vlan_port;
            } else {
                /* both from the same dir but with VLAN0 */
                vlan_id = CGlobalInfo::m_options.m_vlan_port[0];
            }
        } else if (CGlobalInfo::m_options.preview.get_vlan_mode()
            == CPreviewMode::VLAN_MODE_NORMAL) {
            CCorePerPort *lp_port = &m_ports[dir];
            uint8_t port_id = lp_port->m_port->get_tvpid();
            vlan_id = CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan();
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
    uint8_t p_id = lp_port->m_port->get_tvpid();

    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);

     /* when slowpath features are on */
    if ( unlikely( CGlobalInfo::m_options.preview.get_is_slowpath_features_on() ) ) {
        handle_slowpath_features(node, m, p, dir);
    }


    if ( unlikely( node->is_rx_check_enabled() ) ) {
        lp_stats->m_tx_rx_check_pkt++;
        lp->do_generate_new_mbuf_rxcheck(m, node, single_port);
        lp_stats->m_template.inc_template( node->get_template_id( ));
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
    uint8_t p_id=lp_port->m_port->get_tvpid();
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);
    return (0);
}

pkt_dir_t
CCoreEthIF::port_id_to_dir(uint8_t port_id) {

    for (pkt_dir_t dir = 0; dir < CS_NUM; dir++) {
        if (m_ports[dir].m_port->get_tvpid() == port_id) {
            return dir;
        }
    }

    return (CS_INVALID);
}


/**
 * apply HW VLAN
 */
void
CPortLatencyHWBase::apply_hw_vlan(rte_mbuf_t *m, uint8_t port_id) {

    uint8_t vlan_mode = CGlobalInfo::m_options.preview.get_vlan_mode();
    if ( likely( vlan_mode != CPreviewMode::VLAN_MODE_NONE) ) {
        if ( vlan_mode == CPreviewMode::VLAN_MODE_LOAD_BALANCE ) {
            add_vlan(m, CGlobalInfo::m_options.m_vlan_port[0]);
        } else if (vlan_mode == CPreviewMode::VLAN_MODE_NORMAL) {
            add_vlan(m, CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan());
        }
    }
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

    virtual int tx(rte_mbuf_t *m) {

        apply_hw_vlan(m, m_port->get_tvpid());
        return tx_raw(m);
    }


    virtual int tx_raw(rte_mbuf_t *m) {

        rte_mbuf_t *tx_pkts[2];
        tx_pkts[0] = m;

        uint16_t res=m_port->tx_burst(m_tx_queue_id, tx_pkts, 1);
        if ( res == 0 ) {
            //printf(" queue is full for latency packet !!\n");
            return (-1);

        }

        return 0;
    }


    /* nothing special with HW implementation */
    virtual int tx_latency(rte_mbuf_t *m) {
        return tx(m);
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
    void Create(uint8_t port_index,
                CNodeRing *ring,
                CLatencyManager *mgr,
                CPhyEthIF  *p,
                bool disable_rx_read) {

        m_dir        = (port_index % 2);
        m_ring_to_dp = ring;
        m_mgr        = mgr;
        m_port       = p;
        m_disable_rx = disable_rx_read;
    }


    virtual int tx(rte_mbuf_t *m) {
        return tx_common(m, false, true);
    }

    virtual int tx_raw(rte_mbuf_t *m) {
        return tx_common(m, false, false);
    }

    virtual int tx_latency(rte_mbuf_t *m) {
        return tx_common(m, true, true);
    }

    virtual rte_mbuf_t * rx() {
        if (m_disable_rx==false){
            rte_mbuf_t * rx_pkts[1];
            uint16_t cnt = m_port->rx_burst(0, rx_pkts, 1);
            if (cnt) {
                return (rx_pkts[0]);
            } else {
                return (0);
            }
        }else{
            return (0);
        }
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        if (m_disable_rx==false){
            uint16_t cnt = m_port->rx_burst(0, rx_pkts, nb_pkts);
            return (cnt);
        }else{
            return (0);
        }
    }

private:
      virtual int tx_common(rte_mbuf_t *m, bool fix_timestamp, bool add_hw_vlan) {

        if (add_hw_vlan) {
            apply_hw_vlan(m, m_port->get_tvpid());
        }

        /* allocate node */
        CGenNodeLatencyPktInfo *node=(CGenNodeLatencyPktInfo * )CGlobalInfo::create_node();
        if (!node) {
            return (-1);
        }

        node->m_msg_type = CGenNodeMsgBase::LATENCY_PKT;
        node->m_dir      = m_dir;
        node->m_pkt      = m;

        if (fix_timestamp) {
            node->m_latency_offset = m_mgr->get_latency_header_offset();
            node->m_update_ts = 1;
        } else {
            node->m_update_ts = 0;
        }

        if ( m_ring_to_dp->Enqueue((CGenNode*)node) != 0 ){
            CGlobalInfo::free_node((CGenNode *)node);
            return (-1);
        }

        return (0);
    }

    CPhyEthIF                       *m_port;
    uint8_t                          m_dir;
    bool                             m_disable_rx; /* TBD need to read from remote queue */
    CNodeRing                       *m_ring_to_dp;   /* ring dp -> latency thread */
    CLatencyManager                 *m_mgr;
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
    float m_rx_core_pps;
    float m_bw_per_core;
    uint8_t m_threads;

    uint32_t      m_num_of_ports;
    CPerPortStats m_port[TREX_MAX_PORTS];
public:
    void Dump(FILE *fd,DumpFormat mode);
    void DumpAllPorts(FILE *fd);

    void dump_json(std::string & json, bool baseline);

    void global_stats_to_json(Json::Value &output);
    void port_stats_to_json(Json::Value &output, uint8_t port_id);

private:
    bool is_dump_nat();

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
    json+=GET_FIELD(m_rx_core_pps);
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
        if ( CTVPort(i).is_dummy() ) {
            continue;
        }
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


void
CGlobalStats::global_stats_to_json(Json::Value &output) {

    output["m_cpu_util"] = m_cpu_util;
    output["m_cpu_util_raw"] = m_cpu_util_raw;
    output["m_bw_per_core"] = m_bw_per_core;
    output["m_rx_cpu_util"] = m_rx_cpu_util;
    output["m_rx_core_pps"] = m_rx_core_pps;
    output["m_platform_factor"] = m_platform_factor;
    output["m_tx_bps"] = m_tx_bps;
    output["m_rx_bps"] = m_rx_bps;
    output["m_tx_pps"] = m_tx_pps;
    output["m_rx_pps"] = m_rx_pps;
    output["m_tx_cps"] = m_tx_cps;
    output["m_tx_expected_cps"] = m_tx_expected_cps;
    output["m_tx_expected_pps"] = m_tx_expected_pps;
    output["m_tx_expected_bps"] = m_tx_expected_bps;
    output["m_total_alloc_error"] = m_total_alloc_error;
    output["m_total_queue_full"] = m_total_queue_full;
    output["m_total_queue_drop"] = m_total_queue_drop;
    output["m_rx_drop_bps"] = m_rx_drop_bps;
    output["m_active_flows"] = m_active_flows;
    output["m_open_flows"] = m_open_flows;
    output["m_total_tx_pkts"] = m_total_tx_pkts;
    output["m_total_rx_pkts"] = m_total_rx_pkts;
    output["m_total_tx_bytes"] = m_total_tx_bytes;
    output["m_total_rx_bytes"] = m_total_rx_bytes;
    output["m_total_clients"] = m_total_clients;
    output["m_total_servers"] = m_total_servers;
    output["m_active_sockets"] = m_active_sockets;
    output["m_socket_util"] = m_socket_util;
    output["m_total_nat_time_out"] = m_total_nat_time_out;
    output["m_total_nat_time_out_wait_ack"] = m_total_nat_time_out_wait_ack;
    output["m_total_nat_no_fid "] = m_total_nat_no_fid ;
    output["m_total_nat_active "] = m_total_nat_active ;
    output["m_total_nat_syn_wait"] = m_total_nat_syn_wait;
    output["m_total_nat_open   "] = m_total_nat_open   ;
    output["m_total_nat_learn_error"] = m_total_nat_learn_error;
}


bool CGlobalStats::is_dump_nat(){
    return( CGlobalInfo::is_learn_mode() && (get_is_tcp_mode()==0) );
}


void
CGlobalStats::port_stats_to_json(Json::Value &output, uint8_t port_id) {
     CPerPortStats *lp = &m_port[port_id];

     output["opackets"]        = lp->opackets;
     output["obytes"]          = lp->obytes;
     output["ipackets"]        = lp->ipackets;
     output["ibytes"]          = lp->ibytes;
     output["ierrors"]         = lp->ierrors;
     output["oerrors"]         = lp->oerrors;
     output["m_total_tx_bps"]  = lp->m_total_tx_bps;
     output["m_total_tx_pps"]  = lp->m_total_tx_pps;
     output["m_total_rx_bps"]  = lp->m_total_rx_bps;
     output["m_total_rx_pps"]  = lp->m_total_rx_pps;
     output["m_cpu_util"]      = lp->m_cpu_util;
}   


void CGlobalStats::DumpAllPorts(FILE *fd){


    if ( m_cpu_util > 0.1 ) {
        fprintf (fd," Cpu Utilization : %2.1f  %%  %2.1f Gb/core \n", m_cpu_util, m_bw_per_core);
    } else {
        fprintf (fd," Cpu Utilization : %2.1f  %%\n", m_cpu_util);
    }
    fprintf (fd," Platform_factor : %2.1f  \n",m_platform_factor);
    fprintf (fd," Total-Tx        : %s  ",double_to_human_str(m_tx_bps,"bps",KBYE_1000).c_str());
    if ( is_dump_nat() ) {
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
    if ( is_dump_nat() ) {
        fprintf (fd," NAT aged flow id: %8llu \n", (unsigned long long)m_total_nat_no_fid);
    }else{
        fprintf (fd,"\n");
    }

    fprintf (fd," Total-PPS       : %s  ",double_to_human_str(m_tx_pps,"pps",KBYE_1000).c_str());
    if ( is_dump_nat() ) {
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
    if ( is_dump_nat() ) {
        fprintf (fd," Total NAT opened: %8llu \n", (unsigned long long)m_total_nat_open);
    }else{
        fprintf (fd,"\n");
    }
    fprintf (fd,"\n");
    fprintf (fd," Expected-PPS    : %s  ",double_to_human_str(m_tx_expected_pps,"pps",KBYE_1000).c_str());
    if ( is_dump_nat() && CGlobalInfo::is_learn_verify_mode() ) {
        fprintf (fd," NAT learn errors: %8llu \n", (unsigned long long)m_total_nat_learn_error);
    }else{
        fprintf (fd,"\n");
    }
    fprintf (fd," Expected-CPS    : %s  \n",double_to_human_str(m_tx_expected_cps,"cps",KBYE_1000).c_str());
    fprintf (fd," Expected-%s : %s  \n", get_is_tcp_mode() ? "L7-BPS" : "BPS   "
             ,double_to_human_str(m_tx_expected_bps,"bps",KBYE_1000).c_str());
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
            if ( CTVPort(i).is_dummy() ) {
                continue;
            }
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
            if ( CTVPort(i).is_dummy() ) {
                continue;
            }
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
                if ( CTVPort(j).is_dummy() ) {
                    continue;
                }
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

#define INVALID_TX_QUEUE_ID 0xffff

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
        SHUTDOWN_RPC_REQ,
        SHUTDOWN_NOT_ENOGTH_CLIENTS

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
        m_stx = NULL;
        m_mark_for_shutdown = SHUTDOWN_NONE;
        m_mark_not_enogth_clients =false;
        m_sync_barrier=0;
    }

    bool Create();
    void Delete();
    int  device_prob_init();
    int  cores_prob_init();
    int  queues_prob_init();
    int  device_start();
    int  device_rx_queue_flush();
    void init_vif_cores();
    
    void rx_batch_conf();
    void rx_interactive_conf();
    
    TrexSTXCfg get_stx_cfg();
    
    void init_stl();
    void init_stf();
    void init_astf();

    void init_astf_batch();
    
    bool is_all_links_are_up(bool dump=false);
    void pre_test();
    void apply_pretest_results_to_stack(void);
    void abort_gracefully(const std::string &on_stdout,
                          const std::string &on_publisher) __attribute__ ((__noreturn__));

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
    void init_astf_vif_rx_queues();

    void register_signals();

    /* try to stop all datapath cores and RX core */
    void wait_for_all_cores();

    bool is_marked_for_shutdown() const {
        return (m_mark_for_shutdown != SHUTDOWN_NONE);
    }


    std::string get_shutdown_cause() const {
        switch (m_mark_for_shutdown) {

        case SHUTDOWN_NONE:
            return "";

        case SHUTDOWN_TEST_ENDED:
            return "test has ended";

        case SHUTDOWN_CTRL_C:
            return "CTRL + C detected";

        case SHUTDOWN_SIGINT:
            return "received signal SIGINT";

        case SHUTDOWN_SIGTERM:
            return "received signal SIGTERM";

        case SHUTDOWN_RPC_REQ:
            return "server received RPC 'shutdown' request";

        case SHUTDOWN_NOT_ENOGTH_CLIENTS :
            return "there are not enogth clients for this rate, try to add more";
        default:
            assert(0);
        }

    }


    /**
     * shutdown sequence
     *
     */
    void shutdown();

public:
    int start_master_astf_common();
    int start_master_astf_batch();
    int start_master_astf();

    int start_master_statefull();
    int start_master_stateless();
    int run_in_core(virtual_thread_id_t virt_core_id);
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
    bool is_all_dp_cores_finished();
    bool is_all_cores_finished();

    void check_for_ports_link_change();
    void check_for_io();
    void show_panel();

public:

    void sync_threads_stats();
    void publish_async_data(bool sync_now, bool baseline = false);
    void publish_async_barrier(uint32_t key);
    void publish_async_port_attr_changed(uint8_t port_id);

    void global_stats_to_json(Json::Value &output);
    void port_stats_to_json(Json::Value &output, uint8_t port_id);

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

    uint16_t get_rx_core_tx_queue_id();
    uint16_t get_latency_tx_queue_id();

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

    CPhyEthIF          *m_ports[TREX_MAX_PORTS];
    CCoreEthIF          m_cores_vif_stf[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserved - stateful */
    CCoreEthIFStateless m_cores_vif_stl[BP_MAX_CORES]; /* counted from 1 , 2,3 core zero is reserved - stateless*/
    CCoreEthIFTcp       m_cores_vif_tcp[BP_MAX_CORES];
    CCoreEthIF *        m_cores_vif[BP_MAX_CORES];
    CParserOption       m_po;
    CFlowGenList        m_fl;
    bool                m_fl_was_init;
    volatile uint8_t    m_signal[BP_MAX_CORES] __rte_cache_aligned ; // Signal to main core when DP thread finished
    CLatencyManager     m_mg; // statefull RX core
    CTrexGlobalIoMode   m_io_modes;
    CTRexExtendedDriverBase * m_drv;

private:
    CLatencyHWPort        m_latency_vports[TREX_MAX_PORTS];    /* read hardware driver */
    CLatencyVmPort        m_latency_vm_vports[TREX_MAX_PORTS]; /* vm driver */
    CLatencyPktInfo       m_latency_pkt;
    TrexPublisher         m_zmq_publisher;
    CGlobalStats          m_stats;
    uint32_t              m_stats_cnt;
    std::recursive_mutex  m_cp_lock;

    TrexMonitor           m_monitor;
    shutdown_rc_e         m_mark_for_shutdown;
    bool                  m_mark_not_enogth_clients;

public:
    TrexSTX              *m_stx;
    CSyncBarrier *        m_sync_barrier;

};

// Before starting, send gratuitous ARP on our addresses, and try to resolve dst MAC addresses.
void CGlobalTRex::pre_test() {
    CTrexDpdkParams dpdk_p;
    get_dpdk_drv_params(dpdk_p);
    CPretest pretest(m_max_ports, dpdk_p.get_total_rx_queues());

    int i;
    for (i=0; i<m_max_ports; i++) {
        pretest.set_port(i, m_ports[i]);
    }
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
        if ( isVerbose(1)) {
            fprintf(stdout, "*******Pretest for client cfg********\n");
            pretest.dump(stdout);
            }
    } else {
        for (int port_id = 0; port_id < m_max_ports; port_id++) {
            if ( m_ports[port_id]->is_dummy() ) {
                continue;
            }
            if (! memcmp( CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, empty_mac, ETHER_ADDR_LEN)) {
                resolve_needed = true;
            } else {
                resolve_needed = false;
            }

            if ( !m_ports[port_id]->get_port_attr()->is_link_up() && get_is_interactive() ) {
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
        CPhyEthIF *pif = m_ports[port_id];
        // Configure port to send all packets to software
        pif->set_port_rcv_all(true);
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

    if ( isVerbose(1) ) {
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
        if ( isVerbose(1) ) {
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
            if ( m_ports[port_id]->is_dummy() ) {
                continue;
            }
            if (! memcmp(CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, empty_mac, ETHER_ADDR_LEN)) {
                // we don't have dest MAC. Get it from what we resolved.
                uint32_t ip = CGlobalInfo::m_options.m_ip_cfg[port_id].get_def_gw();
                uint16_t vlan = CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan();

                if (!pretest.get_mac(port_id, ip, vlan, mac)) {
                    fprintf(stderr, "Failed resolving dest MAC for default gateway:%d.%d.%d.%d on port %d\n"
                            , (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port_id);

                    if (get_is_interactive()) {
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
        }
    }

    // some adapters (napatech at least) have a little delayed statistics
    if (get_ex_drv()->sleep_after_arp_needed() ){
        sleep(1);
    }

    // update statistics baseline, so we can ignore what happened in pre test phase
    for (int port_id = 0; port_id < m_max_ports; port_id++) {
        CPhyEthIF *pif = m_ports[port_id];
        if ( !pif->is_dummy() ) {
            CPreTestStats pre_stats = pretest.get_stats(port_id);
            pif->set_ignore_stats_base(pre_stats);
            // Configure port back to normal mode. Only relevant packets handled by software.
            pif->set_port_rcv_all(false);
        }
    }
}

void CGlobalTRex::apply_pretest_results_to_stack(void) {
    // wait up to 5 seconds for RX core to be up
    for (int i=0; i<50; i++) {
        if ( m_stx->get_rx()->is_active() ) {
            break;
        }
        delay(100);
    }

    assert(m_stx->get_rx()->is_active());

    for (int port_id = 0; port_id < m_max_ports; port_id++) {
        if ( m_ports[port_id]->is_dummy() ) {
            continue;
        }
        TrexPort *port = m_stx->get_port_by_id(port_id);
        uint32_t src_ipv4 = CGlobalInfo::m_options.m_ip_cfg[port_id].get_ip();
        uint32_t dg = CGlobalInfo::m_options.m_ip_cfg[port_id].get_def_gw();
        std::string dst_mac((char*)CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest, 6);

        /* L3 mode */
        if (src_ipv4 && dg) {
            if ( dst_mac == std::string("\0\0\0\0\0\0", 6) ) {
                port->set_l3_mode_async(utl_uint32_to_ipv4_buf(src_ipv4), utl_uint32_to_ipv4_buf(dg), nullptr);
            } else {
                port->set_l3_mode_async(utl_uint32_to_ipv4_buf(src_ipv4), utl_uint32_to_ipv4_buf(dg), &dst_mac);
            }

        /* L2 mode */
        } else if (CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.is_set) {
            port->set_l2_mode_async(dst_mac);
        }

        /* configure single VLAN */
        uint16_t vlan = CGlobalInfo::m_options.m_ip_cfg[port_id].get_vlan();
        if (vlan != 0) {
            port->set_vlan_cfg_async({vlan});
        }
        port->run_rx_cfg_tasks_initial_async();
    }

    bool success = true;
    for (int port_id = 0; port_id < m_max_ports; port_id++) {
        if ( m_ports[port_id]->is_dummy() ) {
            continue;
        }
        TrexPort *port = m_stx->get_port_by_id(port_id);
        while ( port->is_rx_running_cfg_tasks() ) {
            rte_pause_or_delay_lowend();
        }
        stack_result_t results;
        port->get_rx_cfg_tasks_results(0, results);
        if ( results.err_per_mac.size() ) {
            success = false;
            printf("Configure port node %d failed with following error:\n", port_id);
            printf("%s\n", results.err_per_mac.begin()->second.c_str());
        }
    }
    if ( !success ) {
        exit(1);
    }
}

/**
 * handle an abort
 *
 * when in stateless mode this routine will try to safely
 * publish over ZMQ the assert cause
 *
 * *BEWARE* - this function should be thread safe
 *            as any thread can call assert
 */
void
CGlobalTRex::abort_gracefully(const std::string &on_stdout,
                              const std::string &on_publisher) {

    /* first to stdout */
    std::cout << on_stdout << "\n";

    /* assert might be before the ZMQ publisher was connected */
    if (m_zmq_publisher.is_connected()) {

        /* generate the data */
        Json::Value data;
        data["cause"] = on_publisher;

        /* if this is the control plane thread - acquire the lock again (recursive), if it is dataplane - hold up */
        std::unique_lock<std::recursive_mutex> cp_lock(m_cp_lock);
        m_zmq_publisher.publish_event(TrexPublisher::EVENT_SERVER_STOPPED, data);

        /* close the publisher gracefully to ensure message was delivered */
        m_zmq_publisher.Delete(2);
    }


    /* so long... */
    abort();
}


bool CGlobalTRex::is_all_links_are_up(bool dump){
    bool all_link_are=true;
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=m_ports[i];
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

void CGlobalTRex::wait_for_all_cores(){

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

    Json::Value data;
    data["cause"] = get_shutdown_cause();
    m_zmq_publisher.publish_event(TrexPublisher::EVENT_SERVER_STOPPED, data);

    if ( all_core_finished ){
        printf(" All cores stopped !! \n");
    }else{
        if ( !m_stx->get_rx()->is_active() ) {
            printf(" ERROR RX core is stuck!\n");
        } else {
            printf(" ERROR one of the DP cores is stuck!\n");
        }
    }
}


int  CGlobalTRex::device_rx_queue_flush(){
    int i;
    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=m_ports[i];
        _if->flush_rx_queue();
    }
    return (0);
}


// init RX core for batch mode (STF, ASTF batch)
void CGlobalTRex::rx_batch_conf(void) {
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

    if ( !get_dpdk_mode()->is_hardware_filter_needed() ) {
        /* vm mode, indirect queues  */
        for (i=0; i<m_max_ports; i++) {
            CPhyEthIF * _if = m_ports[i];
            CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();

            uint8_t thread_id = (i>>1);

            CNodeRing * r = rx_dp->getRingCpToDp(thread_id);
            bool disable_rx_read = !get_dpdk_mode()->is_rx_core_read_from_queue();
            m_latency_vm_vports[i].Create((uint8_t)i, r, &m_mg, _if, disable_rx_read);
            mg_cfg.m_ports[i] = &m_latency_vm_vports[i];
        }

    } else {
        for (i=0; i<m_max_ports; i++) {
            CPhyEthIF * _if=m_ports[i];
            //_if->dump_stats(stdout);
            m_latency_vports[i].Create(_if, m_rx_core_tx_q_id, 1);

            mg_cfg.m_ports[i] = &m_latency_vports[i];
        }
    }

    m_mg.Create(&mg_cfg);
    m_mg.set_mask(CGlobalInfo::m_options.m_latency_mask);
}


void CGlobalTRex::rx_interactive_conf(void) {
    
    if ( !get_dpdk_mode()->is_hardware_filter_needed() ) {
        /* vm mode, indirect queues  */
        for (int i=0; i < m_max_ports; i++) {
            CPhyEthIF * _if = m_ports[i];
            CMessagingManager * rx_dp = CMsgIns::Ins()->getRxDp();
            uint8_t thread_id = (i >> 1);
            CNodeRing * r = rx_dp->getRingCpToDp(thread_id);
            bool disable_rx_read = (!get_dpdk_mode()->is_rx_core_read_from_queue());
            m_latency_vm_vports[i].Create(i, r, &m_mg, _if, disable_rx_read);
        }
    } else {
        for (int i = 0; i < m_max_ports; i++) {
            CPhyEthIF * _if = m_ports[i];
            m_latency_vports[i].Create(_if, m_rx_core_tx_q_id, 1);
        }
    }
}


int  CGlobalTRex::device_start(void){
    int i;
    for (i=0; i<m_max_ports; i++) {
        socket_id_t socket_id = CGlobalInfo::m_socket.port_to_socket((port_id_t)i);
        assert(CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);
        CTVPort ctvport = CTVPort(i);
        if ( ctvport.is_dummy() ) {
            m_ports[i] = new CPhyEthIFDummy();
        } else {
            m_ports[i] = new CPhyEthIF();
        }
        CPhyEthIF * _if=m_ports[i];
        _if->Create((uint8_t)i, ctvport.get_repid());
        _if->conf_queues();
        _if->stats_clear();
        _if->start();
        _if->configure_rss();
        if (CGlobalInfo::m_options.preview.getPromMode()) {
            _if->get_port_attr()->set_promiscuous(true);
            _if->get_port_attr()->set_multicast(true);
        }

        _if->configure_rx_duplicate_rules();

        if ( ! CGlobalInfo::m_options.preview.get_is_disable_flow_control_setting()
             && _if->get_port_attr()->is_fc_change_supported()) {
            _if->disable_flow_control();
        }

        _if->get_port_attr()->add_mac((char *)CGlobalInfo::m_options.get_src_mac_addr(i));

        fflush(stdout);
    }

    if ( !is_all_links_are_up()  ){
        /* wait for ports to be stable */
        get_ex_drv()->wait_for_stable_link();

        if ( !is_all_links_are_up() ){ // disable start with link down for now

            if ( get_is_interactive() ) {
                printf(" WARNING : there is no link on one of the ports, interactive mode can continue\n");
            } else if ( get_ex_drv()->drop_packets_incase_of_linkdown() ) {
                printf(" WARNING : there is no link on one of the ports, driver support auto drop in case of link down - continue\n");
            } else {
                dump_links_status(stdout);
                rte_exit(EXIT_FAILURE, " One of the links is down \n");
            }
        }
    } else {
        get_ex_drv()->wait_after_link_up();
    }

    dump_links_status(stdout);

    device_rx_queue_flush();

    return (0);
}


void
CGlobalTRex::init_vif_cores() {
    int port_offset = 0;
    uint8_t lat_q_id;

    if (get_dpdk_mode()->is_dp_latency_tx_queue()) {
        lat_q_id = get_latency_tx_queue_id();
    } else {
        lat_q_id = 0;
    }
    for (int i = 0; i < get_cores_tx(); i++) {
        int j=(i+1);
        int queue_id=((j-1)/get_base_num_cores() );   /* for the first min core queue 0 , then queue 1 etc */
     
        m_cores_vif[j]->Create(j,
                               queue_id,
                               m_ports[port_offset], /* 0,2*/
                               queue_id,
                               m_ports[port_offset+1], /*1,3*/
                               lat_q_id);
        port_offset+=2;
        if (port_offset == m_max_ports) {
            port_offset = 0;
            // We want to allow sending latency packets only from first core handling a port
            lat_q_id = CCoreEthIF::INVALID_Q_ID;
        }
    }

    m_rx_core_tx_q_id = get_rx_core_tx_queue_id();
    fprintf(stdout," -------------------------------\n");
    fprintf(stdout, "RX core uses TX queue number %d on all ports\n", (int)m_rx_core_tx_q_id);
    CCoreEthIF::DumpIfCfgHeader(stdout);
    for (int i = 0; i < get_cores_tx(); i++) {
        m_cores_vif[i+1]->DumpIfCfg(stdout);
    }
    fprintf(stdout," -------------------------------\n");
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

    register_signals();

    m_stats_cnt =0;
    
    /* End update pre flags */

    device_prob_init();
    cores_prob_init();
    queues_prob_init();


    if ( !m_zmq_publisher.Create( CGlobalInfo::m_options.m_zmq_port,
                                  !CGlobalInfo::m_options.preview.get_zmq_publish_enable() ) ){
        return (false);
    }


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
    CTrexDpdkParams dpdk_p;
    get_dpdk_drv_params(dpdk_p);

    if (isVerbose(0)) {
        dpdk_p.dump(stdout);
    }

    bool use_hugepages = !CGlobalInfo::m_options.m_is_vdev;
    CGlobalInfo::init_pools( m_max_ports * dpdk_p.get_total_rx_desc(),
                             dpdk_p.rx_mbuf_type,
                             use_hugepages);

    device_start();
    dump_config(stdout);
    m_sync_barrier =new CSyncBarrier(get_cores_tx(),1.0);


    switch (get_op_mode()) {

    case OP_MODE_STL:
        init_stl();
        break;

    case OP_MODE_ASTF:
        init_astf();
        break;

    case OP_MODE_STF:
        init_stf();
        break;

    case OP_MODE_ASTF_BATCH:
        init_astf_batch();
        break;

    default:
        assert(0);
    }

    
    return (true);

}

TrexSTXCfg
CGlobalTRex::get_stx_cfg() {

    TrexSTXCfg cfg;
    
    /* control plane config */
    cfg.m_rpc_req_resp_cfg.create(TrexRpcServerConfig::RPC_PROT_TCP,
                                  global_platform_cfg_info.m_zmq_rpc_port,
                                  &m_cp_lock);
    
    bool hw_filter = (get_dpdk_mode()->is_hardware_filter_needed());
    std::unordered_map<uint8_t, CPortLatencyHWBase*> ports;
    
    for (int i = 0; i < m_max_ports; i++) {
        if ( CTVPort(i).is_dummy() ) {
            continue;
        }
        if (!hw_filter) {
            ports[i] = &m_latency_vm_vports[i];
        } else {
            ports[i] = &m_latency_vports[i];
        }
    }
    
    /* RX core */
    cfg.m_rx_cfg.create(get_cores_tx(),
                        ports);
    
    cfg.m_publisher = &m_zmq_publisher;
    
    return cfg;
}


void CGlobalTRex::init_stl() {
    
    for (int i = 0; i < get_cores_tx(); i++) {
        m_cores_vif[i + 1] = &m_cores_vif_stl[i + 1];
    }

    if (get_dpdk_mode()->dp_rx_queues() ){
        /* multi-queue mode */
        for (int i = 0; i < get_cores_tx(); i++) {
           int qid =(i/get_base_num_cores());   
           int rx_qid=get_dpdk_mode()->get_dp_rx_queues(qid); /* 0,1,2,3*/
           m_cores_vif_stl[i+1].set_rx_queue_id(rx_qid,rx_qid);
       }
    }

    init_vif_cores();

    rx_interactive_conf();
    
    m_stx = new TrexStateless(get_stx_cfg());
    
    start_master_stateless();
}



void CGlobalTRex::init_astf_vif_rx_queues(){
    for (int i = 0; i < get_cores_tx(); i++) {
        int qid =(i/get_base_num_cores());   /* 0,2,3,..*/
        int rx_qid = get_dpdk_mode()->get_dp_rx_queues(qid);
        m_cores_vif_tcp[i+1].set_rx_queue_id(rx_qid,rx_qid);
    }
}

void CGlobalTRex::init_astf() {
        
    for (int i = 0; i < get_cores_tx(); i++) {
        m_cores_vif[i + 1] = &m_cores_vif_tcp[i + 1];
    }

    init_vif_cores();
    init_astf_vif_rx_queues();
    rx_interactive_conf();
    
    m_stx = new TrexAstf(get_stx_cfg());
    
    start_master_astf();
}


void CGlobalTRex::init_astf_batch() {
    
     for (int i = 0; i < get_cores_tx(); i++) {
        m_cores_vif[i + 1] = &m_cores_vif_tcp[i + 1];
    }
     
     init_vif_cores();
     init_astf_vif_rx_queues();
     rx_batch_conf();
     
     m_stx = new TrexAstfBatch(get_stx_cfg(), &m_mg);
     
     start_master_astf_batch();
}


void CGlobalTRex::init_stf() {
    CFlowsYamlInfo  pre_yaml_info;
    
    pre_yaml_info.load_from_yaml_file(CGlobalInfo::m_options.cfg_file);
    if ( isVerbose(0) ){
        CGlobalInfo::m_options.dump(stdout);
        CGlobalInfo::m_memory_cfg.Dump(stdout);
    }
    
    if (pre_yaml_info.m_vlan_info.m_enable) {
        CGlobalInfo::m_options.preview.set_vlan_mode_verify(CPreviewMode::VLAN_MODE_LOAD_BALANCE);
    }
        
    for (int i = 0; i < get_cores_tx(); i++) {
        m_cores_vif[i + 1] = &m_cores_vif_stf[i + 1];
    }
    
    init_vif_cores();
    rx_batch_conf();
        
    m_stx = new TrexStateful(get_stx_cfg(), &m_mg);
    
    start_master_statefull();
}


void CGlobalTRex::Delete(){

    m_zmq_publisher.Delete();

    if (m_stx) {
        delete m_stx;
        m_stx = nullptr;
    }

    for (int i = 0; i < m_max_ports; i++) {
        delete m_ports[i]->get_port_attr();
        delete m_ports[i];
    }

    m_fl.Delete();
    m_mg.Delete();
    
    /* imarom: effectively has no meaning as memory is not released (See msg_manager.cpp) */
    CMsgIns::Ins()->Delete();
    delete m_sync_barrier;
}



int  CGlobalTRex::device_prob_init(void){

    if ( CGlobalInfo::m_options.m_is_vdev ) {
        m_max_ports = rte_eth_dev_count() + CGlobalInfo::m_options.m_dummy_count;
    } else {
        m_max_ports = port_map.get_max_num_ports();
    }

    if (m_max_ports == 0)
        rte_exit(EXIT_FAILURE, "Error: Could not find supported ethernet ports. You are probably trying to use unsupported NIC \n");

    printf(" Number of ports found: %d", m_max_ports);
    if ( CGlobalInfo::m_options.m_dummy_count ) {
        printf(" (dummy among them: %d)", CGlobalInfo::m_options.m_dummy_count);
    }
    printf("\n");

    if ( m_max_ports %2 !=0 ) {
        rte_exit(EXIT_FAILURE, " Number of ports in config file is %d. It should be even. Please use --limit-ports, or change 'port_limit:' in the config file\n",
                 m_max_ports);
    }

    CParserOption * ps=&CGlobalInfo::m_options;
    if ( ps->get_expected_ports() > TREX_MAX_PORTS ) {
        rte_exit(EXIT_FAILURE, " Maximum number of ports supported is %d. You are trying to use %d. Please use --limit-ports, or change 'port_limit:' in the config file\n"
                 ,TREX_MAX_PORTS, ps->get_expected_ports());
    }

    if ( ps->get_expected_ports() > m_max_ports ){
        rte_exit(EXIT_FAILURE, " There are %d ports available. You are trying to use %d. Please use --limit-ports, or change 'port_limit:' in the config file\n",
                 m_max_ports,
                 ps->get_expected_ports());
    }
    if (ps->get_expected_ports() < m_max_ports ) {
        /* limit the number of ports */
        m_max_ports=ps->get_expected_ports();
    }
    assert(m_max_ports <= TREX_MAX_PORTS);


    if  ( ps->get_number_of_dp_cores_needed() > BP_MAX_CORES ){
        rte_exit(EXIT_FAILURE, " Your configuration require  %d DP cores but the maximum supported is %d - try to reduce `-c value ` or 'c:' in the config file\n",
                 (int)ps->get_number_of_dp_cores_needed(),
                 (int)BP_MAX_CORES);
    }

    int i;
    struct rte_eth_dev_info dev_info, dev_info1;
    bool found_non_dummy = false;

    for (i=0; i<m_max_ports; i++) {
        CTVPort ctvport = CTVPort(i);
        if ( ctvport.is_dummy() ) {
            continue;
        }
        if ( found_non_dummy ) {
            rte_eth_dev_info_get(CTVPort(i).get_repid(),&dev_info1);
            if ( strcmp(dev_info1.driver_name,dev_info.driver_name)!=0) {
                printf(" ERROR all device should have the same type  %s != %s \n",dev_info1.driver_name,dev_info.driver_name);
                exit(1);
            }
        } else {
            found_non_dummy = true;
            rte_eth_dev_info_get(ctvport.get_repid(), &dev_info);
        }
    }

    if ( ps->preview.getVMode() > 0){
        printf("\n\n");
        printf("if_index : %d \n",dev_info.if_index);
        printf("driver name : %s \n",dev_info.driver_name);
        printf("min_rx_bufsize : %d \n",dev_info.min_rx_bufsize);
        printf("max_rx_pktlen  : %d \n",dev_info.max_rx_pktlen);
        printf("max_rx_queues  : %d \n",dev_info.max_rx_queues);
        printf("max_tx_queues  : %d \n",dev_info.max_tx_queues);
        printf("max_mac_addrs  : %d \n",dev_info.max_mac_addrs);

        printf("rx_offload_capa : 0x%lx \n",dev_info.rx_offload_capa);
        printf("tx_offload_capa : 0x%lx \n",dev_info.tx_offload_capa);
        printf("rss reta_size   : %d \n",dev_info.reta_size);
        printf("flow_type_rss   : 0x%lx \n",dev_info.flow_type_rss_offloads);
    }

    m_drv = get_ex_drv();

    // check if firmware version is new enough
    for (i = 0; i < m_max_ports; i++) {
        if (m_drv->verify_fw_ver((tvpid_t)i) < 0) {
            // error message printed by verify_fw_ver
            exit(1);
        }
    }

    m_port_cfg.update_var();

    if (m_port_cfg.m_port_conf.rxmode.max_rx_pkt_len > dev_info.max_rx_pktlen ) {
        printf("WARNING: reduce max packet len from %d to %d \n",
               (int)m_port_cfg.m_port_conf.rxmode.max_rx_pkt_len,
               (int)dev_info.max_rx_pktlen);
         m_port_cfg.m_port_conf.rxmode.max_rx_pkt_len = dev_info.max_rx_pktlen;
    }

    uint16_t tx_queues = get_dpdk_mode()->dp_rx_queues();
     int dp_cores = CGlobalInfo::m_options.preview.getCores();

    if ( dev_info.max_tx_queues < tx_queues ) {
        printf("ERROR: driver maximum tx queues is (%d) required (%d) reduce number of cores to support it \n",
               (int)dev_info.max_tx_queues,
               (int)tx_queues);
        exit(1);
    }

    uint16_t rx_queues = get_dpdk_mode()->dp_rx_queues();
    if ( (dev_info.max_rx_queues < rx_queues) || (dp_cores < rx_queues) ) {
        printf("ERROR: driver maximum rx queues is (%d), number of cores (%d) and requested rx queues (%d) is higher, reduce the number of dp cores \n",
               (int)dev_info.max_tx_queues,
               (int)dp_cores,
               (int)rx_queues);
        exit(1);
    }
    

    if ( get_dpdk_mode()->is_hardware_filter_needed() ){
        m_port_cfg.update_global_config_fdir();
    }

    if ( ps->preview.getCores() ==0 ) {
        printf("Error: the number of cores can't be set to 0. Please use -c 1 \n \n");
        exit(1);
    }

    if ( get_dpdk_mode()->is_one_tx_rx_queue() ) {
        /* verify that we have only one thread/core per dual- interface */
        if ( ps->preview.getCores()>1 ) {
            printf("Error: the number of cores should be 1 when the driver support only one tx queue and one rx queue. Please use -c 1 \n");
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
    if ( (m_max_ports>>1) > get_cores_tx() ) {
        rte_exit(EXIT_FAILURE, "You don't have enough physical cores for this configuration dual_ports:%lu physical_cores:%lu dp_cores:%lu check lscpu \n",
                 (ulong)(m_max_ports>>1),
                 (ulong)m_max_cores,
                 (ulong)get_cores_tx());
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

    if (m_max_queues_per_port > BP_MAX_CORES) {
        rte_exit(EXIT_FAILURE,
                 "Error: Number of TX queues exceeds %d. Try running with lower -c <val> \n",BP_MAX_CORES);
    }

    assert(m_max_queues_per_port>0);
    return (0);
}


void CGlobalTRex::dump_config(FILE *fd){
    fprintf(fd," number of ports         : %u \n",m_max_ports);
    fprintf(fd," max cores for 2 ports   : %u \n",m_cores_to_dual_ports);
    fprintf(fd," tx queues per port      : %u \n",m_max_queues_per_port);
}


void CGlobalTRex::dump_links_status(FILE *fd){
    for (int i=0; i<m_max_ports; i++) {
        m_ports[i]->get_port_attr()->update_link_status_nowait();
        m_ports[i]->get_port_attr()->dump_link(fd);
    }
}

uint16_t CGlobalTRex::get_rx_core_tx_queue_id() {

   if ( !get_dpdk_mode()->is_hardware_filter_needed() ){
       /* not relevant */
       return (INVALID_TX_QUEUE_ID);
   }

    /* 2 spare tx queues per port 
      X is the number of dual_ports  0--x-1 for DP
      
    
       stateless 
       x+0 - DP
       x+1 - rx core ARPs 
       x+2 - low latency 

       STL/ASTF

       x+0 - DP
       x+1 - not used 
       x+2 - rx core, latency 
   */
    
    /* imarom: is this specific to stateless ? */

    if (get_dpdk_mode()->is_dp_latency_tx_queue()) {
        return (m_cores_to_dual_ports); /* stateless */
    } else {
        return (m_cores_to_dual_ports+1);
    }
}

uint16_t CGlobalTRex::get_latency_tx_queue_id() {
    /* 2 spare tx queues per port 
      X is the number of dual_ports  0--x-1 for DP


       stateless 
       x+0 - DP
       x+1 - rx core ARPs 
       x+2 - low latency 

       STL/ASTF

       x+0 - DP
       x+1 - not used 
       x+2 - rx core, latency 
   */
    
    /* imarom: is this specific to stateless ? */
    if (get_is_stateless()) {
        return (m_cores_to_dual_ports+1);
    } else {
        return (CCoreEthIF::INVALID_Q_ID);
    }
}


bool CGlobalTRex::lookup_port_by_mac(const uint8_t *mac, uint8_t &port_id) {
    for (int i = 0; i < m_max_ports; i++) {
        if ( m_ports[i]->is_dummy() ) {
            continue;
        }
        if (memcmp((char *)CGlobalInfo::m_options.get_src_mac_addr(i), mac, 6) == 0) {
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
        sw_pkt_out_err += stats.m_tx_drop +stats.m_tx_queue_full +stats.m_tx_alloc_error+stats.m_tx_redirect_error ;
        sw_pkt_out_bytes +=stats.m_tx_bytes;
    }


    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=m_ports[i];
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
        CPhyEthIF * _if=m_ports[i];
        _if->update_counters();
    }
    uint64_t total_open_flows=0;


    CFlowGenListPerThread   * lpt;
    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        total_open_flows +=   lpt->m_stats.m_total_open_flows ;
    }
    m_last_total_cps = m_cps.add(total_open_flows);

    bool all_init=true;
    CSTTCp  * lpstt;
    lpstt = m_fl.m_stt_cp;
    if ( lpstt ){
        if (!lpstt->m_init){
            /* check that we have all objects;*/
            for (i=0; i<get_cores_tx(); i++) {
                lpt = m_fl.m_threads_info[i];
                if ( (lpt->m_c_tcp==0) ||(lpt->m_s_tcp==0) ){
                    all_init=false;
                    break;
                }
            }
            if (all_init) {
                for (i=0; i<get_cores_tx(); i++) {
                    lpt = m_fl.m_threads_info[i];
                    lpstt->Add(TCP_CLIENT_SIDE, lpt->m_c_tcp);
                    lpstt->Add(TCP_SERVER_SIDE, lpt->m_s_tcp);
                }
                lpstt->Init();
                lpstt->m_init=true;
            }
        }

        if (lpstt->m_init){
            lpstt->Update();
        }
    }
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
                lpt->m_node_gen.m_v_if->get_stats()[port - port0].m_tx_per_flow[index];
            if (is_lat)
                lpt->m_node_gen.m_v_if->get_stats()[port - port0].m_lat_data[index - MAX_FLOW_STATS].reset();
        }
    }

    ret = m_stats.m_port[port].m_tx_per_flow[index] - m_stats.m_port[port].m_prev_tx_per_flow[index];

    // Since we return diff from prev, following "clears" the stats.
    m_stats.m_port[port].m_prev_tx_per_flow[index] = m_stats.m_port[port].m_tx_per_flow[index];

    return ret;
}

COLD_FUNC
void CGlobalTRex::get_stats(CGlobalStats & stats){

    int i;
    float total_tx=0.0;
    float total_rx=0.0;
    float total_tx_pps=0.0;
    float total_rx_pps=0.0;

    stats.m_total_tx_pkts       = 0;
    stats.m_total_rx_pkts       = 0;
    stats.m_total_tx_bytes      = 0;
    stats.m_total_rx_bytes      = 0;
    stats.m_total_alloc_error   = 0;
    stats.m_total_queue_full    = 0;
    stats.m_total_queue_drop    = 0;
    stats.m_rx_core_pps         = 0.0;

    stats.m_num_of_ports = m_max_ports;
    stats.m_cpu_util = m_fl.GetCpuUtil();
    stats.m_cpu_util_raw = m_fl.GetCpuUtilRaw();

    stats.m_rx_cpu_util = m_stx->get_rx()->get_cpu_util();
    stats.m_rx_core_pps = m_stx->get_rx()->get_pps_rate();
        
    stats.m_threads      = m_fl.m_threads_info.size();

    for (i=0; i<m_max_ports; i++) {
        CPhyEthIF * _if=m_ports[i];
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
        for (int flow = 0; flow <= CFlowStatRuleMgr::instance()->get_max_hw_id(); flow++) {
            stats.m_port[i].m_tx_per_flow[flow].clear();
        }
        // payload rules
        for (int flow = MAX_FLOW_STATS; flow <= MAX_FLOW_STATS
                 + CFlowStatRuleMgr::instance()->get_max_hw_id_payload(); flow++) {
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

    bool can_read_tuple_gen = true;
    if ( get_is_interactive() && get_is_tcp_mode() ) {
        TrexAstf *astf_stx = (TrexAstf*) m_stx;
        if ( astf_stx->get_state() != TrexAstf::STATE_TX ) {
            can_read_tuple_gen = false;
        }
    }

    for (i=0; i<get_cores_tx(); i++) {
        lpt = m_fl.m_threads_info[i];
        total_open_flows +=   lpt->m_stats.m_total_open_flows ;
        total_active_flows += (lpt->m_stats.m_total_open_flows-lpt->m_stats.m_total_close_flows) ;

        stats.m_total_alloc_error += lpt->m_node_gen.m_v_if->get_stats()[0].m_tx_alloc_error+
            lpt->m_node_gen.m_v_if->get_stats()[1].m_tx_alloc_error;
        stats.m_total_queue_full +=lpt->m_node_gen.m_v_if->get_stats()[0].m_tx_queue_full+
            lpt->m_node_gen.m_v_if->get_stats()[1].m_tx_queue_full;

        stats.m_total_queue_drop +=lpt->m_node_gen.m_v_if->get_stats()[0].m_tx_drop+
            lpt->m_node_gen.m_v_if->get_stats()[1].m_tx_drop;

        stats.m_template.Add(&lpt->m_node_gen.m_v_if->get_stats()[0].m_template);
        stats.m_template.Add(&lpt->m_node_gen.m_v_if->get_stats()[1].m_template);

        if ( can_read_tuple_gen ) {
            total_clients   += lpt->m_smart_gen.getTotalClients();
            total_servers   += lpt->m_smart_gen.getTotalServers();
            active_sockets  += lpt->m_smart_gen.ActiveSockets();
            total_sockets   += lpt->m_smart_gen.MaxSockets();
        }

        total_nat_time_out +=lpt->m_stats.m_nat_flow_timeout;
        total_nat_time_out_wait_ack += lpt->m_stats.m_nat_flow_timeout_wait_ack;
        total_nat_no_fid   +=lpt->m_stats.m_nat_lookup_no_flow_id ;
        total_nat_active   +=lpt->m_stats.m_nat_lookup_add_flow_id - lpt->m_stats.m_nat_lookup_remove_flow_id;
        total_nat_syn_wait += lpt->m_stats.m_nat_lookup_add_flow_id - lpt->m_stats.m_nat_lookup_wait_ack_state;
        total_nat_open     +=lpt->m_stats.m_nat_lookup_add_flow_id;
        total_nat_learn_error   +=lpt->m_stats.m_nat_flow_learn_error;
        uint8_t port0 = lpt->getDualPortId() *2;
        // IP ID rules
        for (int flow = 0; flow <= CFlowStatRuleMgr::instance()->get_max_hw_id(); flow++) {
            stats.m_port[port0].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->get_stats()[0].m_tx_per_flow[flow];
            stats.m_port[port0 + 1].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->get_stats()[1].m_tx_per_flow[flow];
        }
        // payload rules
        for (int flow = MAX_FLOW_STATS; flow <= MAX_FLOW_STATS
                 + CFlowStatRuleMgr::instance()->get_max_hw_id_payload(); flow++) {
            stats.m_port[port0].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->get_stats()[0].m_tx_per_flow[flow];
            stats.m_port[port0 + 1].m_tx_per_flow[flow] +=
                lpt->m_node_gen.m_v_if->get_stats()[1].m_tx_per_flow[flow];
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

#if 0
    if ((m_expected_cps == 0) && get_is_tcp_mode()) {
        // In astf mode, we know the info only after doing first get of data from json (which triggers analyzing the data)
        m_expected_cps = CAstfDB::instance()->get_expected_cps();
        m_expected_bps = CAstfDB::instance()->get_expected_bps();
    }
#endif

    stats.m_tx_expected_cps        = m_expected_cps*pf;
    stats.m_tx_expected_pps        = m_expected_pps*pf;
    stats.m_tx_expected_bps        = m_expected_bps*pf;
}

float
CGlobalTRex::get_cpu_util_per_interface(uint8_t port_id) {
    CPhyEthIF * _if = m_ports[port_id];

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

COLD_FUNC
bool CGlobalTRex::sanity_check(){

    if ( !get_is_interactive() ) {
        CFlowGenListPerThread   * lpt;
        uint32_t errors=0;
        int i;
        for (i=0; i<get_cores_tx(); i++) {
            lpt = m_fl.m_threads_info[i];
            errors   += lpt->m_smart_gen.getErrorAllocationCounter();
        }

        if ( errors && (get_is_tcp_mode()==false) ) {
            m_mark_not_enogth_clients = true;
            printf(" ERROR can't allocate tuple, not enough clients \n");
            printf(" you should allocate more clients in the pool \n");

            /* mark test end and get out */
            mark_for_shutdown(SHUTDOWN_NOT_ENOGTH_CLIENTS);

            return(true);
        }
    }

    return ( false);
}


/* dump the template info */
void CGlobalTRex::dump_template_info(std::string & json){
    CFlowGenListPerThread   * lpt = m_fl.m_threads_info[0];
    CFlowsYamlInfo * yaml_info=&lpt->m_yaml_info;
    if ( yaml_info->is_any_template()==false){ 
        json="";
        return;
    }

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

void CGlobalTRex::sync_threads_stats() {
    update_stats();
    get_stats(m_stats);
}

void
CGlobalTRex::publish_async_data(bool sync_now, bool baseline) {
    std::string json;

    if (sync_now) {
        sync_threads_stats();
    }

    /* common stats */
    m_stats.dump_json(json, baseline);
    m_zmq_publisher.publish_json(json);

    /* generator json , all cores are the same just sample the first one */
    m_fl.m_threads_info[0]->m_node_gen.dump_json(json);
    m_zmq_publisher.publish_json(json);

    /* config specific stats */
    m_stx->publish_async_data();
}


void
CGlobalTRex::publish_async_barrier(uint32_t key) {
    m_zmq_publisher.publish_barrier(key);
}

void
CGlobalTRex:: publish_async_port_attr_changed(uint8_t port_id) {
    Json::Value data;
    data["port_id"] = port_id;
    TRexPortAttr * _attr = m_ports[port_id]->get_port_attr();

    _attr->to_json(data["attr"]);

    m_zmq_publisher.publish_event(TrexPublisher::EVENT_PORT_ATTR_CHANGED, data);
}


void
CGlobalTRex::global_stats_to_json(Json::Value &output) {
    sync_threads_stats();
    m_stats.global_stats_to_json(output);
}

void
CGlobalTRex::port_stats_to_json(Json::Value &output, uint8_t port_id) {
    sync_threads_stats();
    m_stats.port_stats_to_json(output, port_id);
}


void
CGlobalTRex::check_for_ports_link_change() {
    
    // update speed, link up/down etc.
    for (int i=0; i<m_max_ports; i++) {
        bool changed = m_ports[i]->get_port_attr()->update_link_status_nowait();
        if (changed) {
            publish_async_port_attr_changed(i);
        }
    }

}

void
CGlobalTRex::check_for_io() {
    /* is IO allowed ? - if not, get out */
    if (CGlobalInfo::m_options.preview.get_no_keyboard()) {
        return;
    }
    
    bool rc = m_io_modes.handle_io_modes();
    if (rc) {
        mark_for_shutdown(SHUTDOWN_CTRL_C);
        return;
    }
    
}


void
CGlobalTRex::show_panel() {

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

    /* TCP stats */
    if (m_io_modes.m_g_mode == CTrexGlobalIoMode::gSTT) {
        CSTTCp   * lpstt=m_fl.m_stt_cp;
        if (lpstt) {
            if (lpstt->m_init) {
                lpstt->DumpTable();
            }
        }
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


}



void
CGlobalTRex::handle_slow_path() {
    m_stats_cnt++;

    /* sanity checks */
    sanity_check();
    
    /* handle port link changes */
    check_for_ports_link_change();
    
    /* keyboard input */
    check_for_io();
    
    /* based on the panel chosen, show it */
    show_panel();
    
    /* publish data */
    publish_async_data(false);
    
    /* provide the STX object a tick (used by implementing objects) */
    m_stx->slowpath_tick();
}


void
CGlobalTRex::handle_fast_path() {

    /* pass fast path tick to the polymorphic object */
    m_stx->fastpath_tick();

    /* measure CPU utilization by sampling (we sample 1000 to get an accurate sampling) */
    for (int i = 0; i < 1000; i++) {
        m_fl.UpdateFast();
        rte_pause();
    }

    /* in case of batch, when all DP cores are done, mark for shutdown */
    if ( is_all_dp_cores_finished() ) {
        mark_for_shutdown(SHUTDOWN_TEST_ENDED);
    }
}


/**
 * shutdown sequence
 *
 */
void CGlobalTRex::shutdown() {
    std::stringstream ss;

    assert(is_marked_for_shutdown());

    ss << " *** TRex is shutting down - cause: '";

    ss << get_shutdown_cause();

    ss << "'";

    /* report */
    std::cout << ss.str() << "\n";

    /* first stop the WD */
    TrexWatchDog::getInstance().stop();

    /* interactive shutdown */
    m_stx->shutdown();

    wait_for_all_cores();

    /* shutdown drivers */
    for (int i = 0; i < m_max_ports; i++) {
        m_ports[i]->stop();
    }

    if (m_mark_for_shutdown != SHUTDOWN_TEST_ENDED) {
        /* we should stop latency and exit to stop agents */
        Delete();
        if (!CGlobalInfo::m_options.preview.get_is_termio_disabled()) {
            utl_termio_reset();
        }
        exit(-1);
    }
}


int CGlobalTRex::run_in_master() {

    //rte_thread_setname(pthread_self(), "TRex Control");

    m_stx->launch_control_plane();
    
    /* exception and scope safe */
    std::unique_lock<std::recursive_mutex> cp_lock(m_cp_lock);

    uint32_t slow_path_counter = 0;

    const int FASTPATH_DELAY_MS = 10;
    const int SLOWPATH_DELAY_MS = 500;

    m_monitor.create("master", 2);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    TrexWatchDog::getInstance().start();

    if ( get_is_interactive() ) {
        apply_pretest_results_to_stack();
    }
    while (!is_marked_for_shutdown()) {

        /* fast path */
        handle_fast_path();

        /* slow path */
        if (slow_path_counter >= SLOWPATH_DELAY_MS) {
            handle_slow_path();
            slow_path_counter = 0;
        }

        m_monitor.disable(30); //assume we will wake up

        cp_lock.unlock();
        delay(FASTPATH_DELAY_MS);
        slow_path_counter += FASTPATH_DELAY_MS;
        cp_lock.lock();

        m_monitor.enable();
    }

    /* on exit release the lock */
    cp_lock.unlock();

    /* shutdown everything gracefully */
    shutdown();

    return (0);
}



int CGlobalTRex::run_in_rx_core(void){

    CPreviewMode *lp = &CGlobalInfo::m_options.preview;

    rte_thread_setname(pthread_self(), "TRex RX");

    /* set RT mode if set */
    if (lp->get_rt_prio_mode()) {
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
            perror("setting RT priroity mode on RX core failed with error");
            exit(EXIT_FAILURE);
        }
    }

    /* will block until RX core is signaled to stop */
    m_stx->get_rx()->start();
  
    return (0);
}

int CGlobalTRex::run_in_core(virtual_thread_id_t virt_core_id){
    std::stringstream ss;
    CPreviewMode *lp = &CGlobalInfo::m_options.preview;

    ss << "Trex DP core " << int(virt_core_id);
    rte_thread_setname(pthread_self(), ss.str().c_str());

    /* set RT mode if set */
    if (lp->get_rt_prio_mode()) {
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
            perror("setting RT priroity mode on DP core failed with error");
            exit(EXIT_FAILURE);
        }
    }


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

    lpt->start(CGlobalInfo::m_options.out_file, *lp);
    
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
        total_tx_rx_check+=erf_vif->get_stats()[CLIENT_SIDE].m_tx_rx_check_pkt+
            erf_vif->get_stats()[SERVER_SIDE].m_tx_rx_check_pkt;
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

    CSTTCp   * lpstt=m_fl.m_stt_cp;
    if (lpstt) {
        assert(lpstt);
        assert(lpstt->m_init);
        lpstt->DumpTable();
    }

    publish_async_data(false);

    if (m_mark_not_enogth_clients) {
        printf("ERROR: there are not enogth clients for this rate, try to add more clients to the pool ! \n");
    }

    return (0);
}


bool CGlobalTRex::is_all_dp_cores_finished() {
    for (int i = 0; i < get_cores_tx(); i++) {
        if (m_signal[i+1]==0) {
            return false;
        }
    }
    
    return true;
}

bool CGlobalTRex::is_all_cores_finished() {
    
    /* DP cores */
    if (!is_all_dp_cores_finished()) {
        return false;
    }
    
    /* RX core */
    if (m_stx->get_rx()->is_active()) {
        return false;
    }

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
        lpt->set_sync_barrier(m_sync_barrier);
    }
    m_fl_was_init=true;

    return (0);
}


int CGlobalTRex::start_master_astf_common() {
    for (int i = 0; i < BP_MAX_CORES; i++) {
        m_signal[i] = 0;
    }

    m_fl.Create();
    m_fl.load_astf();


    m_expected_pps = 0; // Can't know this in astf mode.
    // two below are computed later. Need to do this after analyzing data read from json.
    m_expected_cps = 0;
    m_expected_bps = 0;


    m_fl.generate_p_thread_info(get_cores_tx());

    for (int i = 0; i < get_cores_tx(); i++) {
        CFlowGenListPerThread *lpt = m_fl.m_threads_info[i];
        CVirtualIF *erf_vif = m_cores_vif[i+1];
        lpt->set_vif(erf_vif);
        lpt->set_sync_barrier(m_sync_barrier);
    }

    m_fl_was_init = true;

    return (0);
}


int CGlobalTRex::start_master_astf_batch() {

    std::string json_file_name = "/tmp/astf";
    if (CGlobalInfo::m_options.prefix.size() != 0) {
        json_file_name += "-" + CGlobalInfo::m_options.prefix;
    }
    json_file_name += ".json";

    fprintf(stdout, "Using json file %s\n", json_file_name.c_str());

    CAstfDB * db=CAstfDB::instance();
    /* load json */
    if (! db->parse_file(json_file_name) ) {
       exit(-1);
    }

    CTupleGenYamlInfo  tuple_info;
    db->get_tuple_info(tuple_info);

    start_master_astf_common();

    /* client config for ASTF this is a patch.. we would need to remove this in interactive mode */
    if (CGlobalInfo::m_options.client_cfg_file != "") {
        try {
            m_fl.load_client_config_file(CGlobalInfo::m_options.client_cfg_file);
        } catch (const std::runtime_error &e) {
            std::cout << "\n*** " << e.what() << "\n\n";
            exit(-1);
        }
        CGlobalInfo::m_options.preview.set_client_cfg_enable(true);
        m_fl.set_client_config_tuple_gen_info(&tuple_info); // build TBD YAML
        pre_test();

        /* set the ASTF db with the client information */
        db->set_client_cfg_db(&m_fl.m_client_config_info);
    }

    /* verify options */
    try {
        CGlobalInfo::m_options.verify();
    } catch (const std::runtime_error &e) {
        std::cout << "\n*** " << e.what() << "\n\n";
        exit(-1);
    }


    int num_dp_cores = CGlobalInfo::m_options.preview.getCores() * CGlobalInfo::m_options.get_expected_dual_ports();
    CJsonData_err err_obj = db->verify_data(num_dp_cores);

    if (err_obj.is_error()) {
        std::cerr << "Error: " << err_obj.description() << std::endl;
        exit(-1);
    }

    CTcpLatency lat;
    db->get_latency_params(lat);

    if (CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {

        m_mg.set_ip( lat.get_c_ip() ,
                   lat.get_s_ip(),
                   lat.get_mask(),
                   m_fl.m_client_config_info);
    } else {
        m_mg.set_ip( lat.get_c_ip() ,
                    lat.get_s_ip(),
                    lat.get_mask());
    }

    return (0);
}


int CGlobalTRex::start_master_astf() {
    start_master_astf_common();
    return (0);
}


int CGlobalTRex::start_master_statefull() {
    int i;
    for (i=0; i<BP_MAX_CORES; i++) {
        m_signal[i]=0;
    }

    m_fl.Create();

    m_fl.load_from_yaml(CGlobalInfo::m_options.cfg_file,get_cores_tx());
    if ( CGlobalInfo::m_options.m_active_flows>0 ) {
        m_fl.update_active_flows(CGlobalInfo::m_options.m_active_flows);
    } else if ( CGlobalInfo::m_options.m_is_lowend ) {
        m_fl.update_active_flows(LOWEND_LIMIT_ACTIVEFLOWS);
    }
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

    float dummy_factor = 1.0 - (float) CGlobalInfo::m_options.m_dummy_count / m_max_ports;
    m_expected_pps = m_fl.get_total_pps() * dummy_factor;
    m_expected_cps = 1000.0 * m_fl.get_total_kcps() * dummy_factor;
    m_expected_bps = m_fl.get_total_tx_bps() * dummy_factor;
    if ( m_fl.get_total_repeat_flows() > 2000) {
        /* disable flows cache */
        CGlobalInfo::m_options.preview.setDisableMbufCache(true);
    }

    CTupleGenYamlInfo * tg=&m_fl.m_yaml_info.m_tuple_gen;


    /* for client cluster configuration - pass the IP start entry */
    if (CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {

        m_mg.set_ip( tg->m_client_pool[0].get_ip_start(),
                     tg->m_server_pool[0].get_ip_start(),
                     tg->m_client_pool[0].getDualMask(),
                     m_fl.m_client_config_info);
    } else {

        m_mg.set_ip( tg->m_client_pool[0].get_ip_start(),
                     tg->m_server_pool[0].get_ip_start(),
                     tg->m_client_pool[0].getDualMask());
    }


    if (  isVerbose(0) ) {
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
        lpt->set_sync_barrier(m_sync_barrier);
    }
    m_fl_was_init=true;

    return (0);
}


////////////////////////////////////////////
static CGlobalTRex g_trex;

const static uint8_t server_rss_key[] = {
 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,

 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};

const static uint8_t client_rss_key[] = {
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0,

 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,

};




void CPhyEthIF::configure_rss_astf(bool is_client,
                                   uint16_t numer_of_queues,
                                   uint16_t skip_queue){ 

    struct rte_eth_dev_info dev_info;

    rte_eth_dev_info_get(m_repid,&dev_info);
    if (dev_info.reta_size == 0) {
        printf("ERROR driver does not support RSS table configuration for accurate latency measurement, \n");
        printf("You must add the flag --software to CLI \n");
        exit(1);
        return;
    }

    int reta_conf_size = std::max(1, dev_info.reta_size / RTE_RETA_GROUP_SIZE);

    struct rte_eth_rss_reta_entry64 reta_conf[reta_conf_size];

    uint16_t skip = 0;
    uint16_t q;
    uint16_t indx=0;
    for (int j = 0; j < reta_conf_size; j++) {
        reta_conf[j].mask = ~0ULL;
        for (int i = 0; i < RTE_RETA_GROUP_SIZE; i++) {
            while (true) {
                q=(indx + skip) % numer_of_queues;
                if (q != skip_queue) {
                    break;
                }
                skip += 1;
            }
            reta_conf[j].reta[i] = q;
            indx++;
        }
    }
    assert(rte_eth_dev_rss_reta_update(m_repid, &reta_conf[0], dev_info.reta_size)==0);

    #ifdef RSS_DEBUG
     rte_eth_dev_rss_reta_query(m_repid, &reta_conf[0], dev_info.reta_size);
     int j; int i;

     printf(" RSS port  %d \n",m_tvpid);
     /* verification */
     for (j = 0; j < reta_conf_size; j++) {
         for (i = 0; i<RTE_RETA_GROUP_SIZE; i++) {
             printf(" R %d  %d \n",(j*RTE_RETA_GROUP_SIZE+i),reta_conf[j].reta[i]);
         }
     }
    #endif
}



void CPhyEthIF::configure_rss(){
    trex_dpdk_rx_distro_mode_t rss_mode = 
         get_dpdk_mode()->get_rx_distro_mode();

    if ( rss_mode == ddRX_DIST_ASTF_HARDWARE_RSS ){
        CTrexDpdkParams dpdk_p;
        get_dpdk_drv_params(dpdk_p);

        configure_rss_astf(false,
                           dpdk_p.get_total_rx_queues(),
                           MAIN_DPDK_RX_Q);
    }
}

void CPhyEthIF::conf_multi_rx() {
    const struct rte_eth_dev_info *dev_info = m_port_attr->get_dev_info();
    uint8_t hash_key_size;

     if ( dev_info->hash_key_size==0 ) {
          hash_key_size = 40; /* for mlx5 */
        } else {
          hash_key_size = dev_info->hash_key_size;
     }

    g_trex.m_port_cfg.m_port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;

    struct rte_eth_rss_conf *lp_rss = 
        &g_trex.m_port_cfg.m_port_conf.rx_adv_conf.rss_conf;

    if (dev_info->flow_type_rss_offloads){
        lp_rss->rss_hf = (dev_info->flow_type_rss_offloads & (ETH_RSS_NONFRAG_IPV4_TCP |
                         ETH_RSS_NONFRAG_IPV4_UDP |
                         ETH_RSS_NONFRAG_IPV6_TCP |
                         ETH_RSS_NONFRAG_IPV6_UDP));
        lp_rss->rss_key =  (uint8_t*)&server_rss_key[0];
    }else{                 
        lp_rss->rss_key =0;
    }
    lp_rss->rss_key_len = hash_key_size;
}

void CPhyEthIF::conf_hardware_astf_rss() {

    const struct rte_eth_dev_info *dev_info = m_port_attr->get_dev_info();

    uint8_t hash_key_size;
    #ifdef RSS_DEBUG
    printf("reta_size : %d \n", dev_info->reta_size);
    printf("hash_key  : %d \n", dev_info->hash_key_size);
    #endif

    if ( dev_info->hash_key_size==0 ) {
        hash_key_size = 40; /* for mlx5 */
    } else {
        hash_key_size = dev_info->hash_key_size;
    }

    if (!rte_eth_dev_filter_supported(m_repid, RTE_ETH_FILTER_HASH)) {
        // Setup HW to use the TOEPLITZ hash function as an RSS hash function
        struct rte_eth_hash_filter_info info = {};
        info.info_type = RTE_ETH_HASH_FILTER_GLOBAL_CONFIG;
        info.info.global_conf.hash_func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
        if (rte_eth_dev_filter_ctrl(m_repid, RTE_ETH_FILTER_HASH,
                                    RTE_ETH_FILTER_SET, &info) < 0) {
            printf(" ERROR cannot set hash function on a port %d \n",m_repid);
            exit(1);
        }
    }
    /* set reta_mask, for now it is ok to set one value to all ports */
    uint8_t reta_mask=(uint8_t)(min(dev_info->reta_size,(uint16_t)256)-1);
    if (CGlobalInfo::m_options.m_reta_mask==0){
        CGlobalInfo::m_options.m_reta_mask = reta_mask ;
    }else{
        if (CGlobalInfo::m_options.m_reta_mask != reta_mask){
            printf("ERROR reta_mask should be the same to all nics \n!");
            exit(1);
        }
    }
    g_trex.m_port_cfg.m_port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    struct rte_eth_rss_conf *lp_rss =&g_trex.m_port_cfg.m_port_conf.rx_adv_conf.rss_conf;
    lp_rss->rss_hf = ETH_RSS_NONFRAG_IPV4_TCP |
                     ETH_RSS_NONFRAG_IPV4_UDP |
                     ETH_RSS_NONFRAG_IPV6_TCP |
                     ETH_RSS_NONFRAG_IPV6_UDP;

    bool is_client_side = ((get_tvpid()%2==0)?true:false);
    if (is_client_side) {
        lp_rss->rss_key =  (uint8_t*)&client_rss_key[0];
    }else{
        lp_rss->rss_key =  (uint8_t*)&server_rss_key[0];
    }
    lp_rss->rss_key_len = hash_key_size;
    
}


void CPhyEthIF::_conf_queues(uint16_t tx_qs,
                             uint32_t tx_descs,
                             uint16_t rx_qs,
                             rx_que_desc_t & rx_qs_descs,
                             uint16_t rx_qs_drop_qid,
                             trex_dpdk_rx_distro_mode_t rss_mode,
                             bool in_astf_mode) {

    socket_id_t socket_id = CGlobalInfo::m_socket.port_to_socket((port_id_t)m_tvpid);
    assert(CGlobalInfo::m_mem_pool[socket_id].m_mbuf_pool_2048);
    const struct rte_eth_dev_info *dev_info = m_port_attr->get_dev_info();

    switch (rss_mode) {
    case ddRX_DIST_NONE:
        break;
    case ddRX_DIST_ASTF_HARDWARE_RSS:
        conf_hardware_astf_rss();
        break;
    case ddRX_DIST_BEST_EFFORT:
        conf_multi_rx();
        break;
    case ddRX_DIST_FLOW_BASED:
        assert(0);
        break;
    default:
        assert(0);
    }

    /* configure tx, rx ports with right dpdk hardware  */
    port_cfg_t & cfg = g_trex.m_port_cfg;
    struct rte_eth_conf  & eth_cfg = g_trex.m_port_cfg.m_port_conf;

    uint64_t &tx_offloads = eth_cfg.txmode.offloads;
    tx_offloads = cfg.tx_offloads.common_best_effort;

    if ( in_astf_mode ) {
        tx_offloads |= cfg.tx_offloads.astf_best_effort;
    }
    // disable non-supported best-effort offloads
    tx_offloads &= dev_info->tx_offload_capa;

    tx_offloads |= cfg.tx_offloads.common_required;

    if ( CGlobalInfo::m_options.preview.getTsoOffloadDisable() ) {
        tx_offloads &= ~(
            DEV_TX_OFFLOAD_TCP_TSO | 
            DEV_TX_OFFLOAD_UDP_TSO);
    }

    /* configure global rte_eth_conf  */
    check_offloads(dev_info, &eth_cfg);
    configure(rx_qs, tx_qs, &eth_cfg);

    /* configure tx que */
    for (uint16_t qid = 0; qid < tx_qs; qid++) {
        tx_queue_setup(qid, tx_descs , socket_id, 
                       &g_trex.m_port_cfg.m_tx_conf);
    }

    for (uint16_t qid = 0; qid < rx_qs; qid++) {
        if (isVerbose(0)) {
           printf(" rx_qid: %d (%d) \n", qid,rx_qs_descs[qid]);
        }
        rx_queue_setup(qid, rx_qs_descs[qid], 
                       socket_id, 
                       &cfg.m_rx_conf,
                       get_rx_mem_pool(socket_id));
    }
    if (rx_qs_drop_qid){
        set_rx_queue(MAIN_DPDK_RX_Q);
    }
}


void CPhyEthIF::conf_queues(void){
    CTrexDpdkParams dpdk_p;
    get_dpdk_drv_params(dpdk_p);

    uint16_t tx_qs;
    uint32_t tx_descs;
    uint16_t rx_qs;
    rx_que_desc_t rx_qs_descs;
    uint16_t rx_qs_drop_qid;
    trex_dpdk_rx_distro_mode_t rss_mode;
    bool in_astf_mode;
    bool is_drop_q = get_dpdk_mode()->is_drop_rx_queue_needed();

    in_astf_mode = get_mode()->is_astf_mode();
    rx_qs = dpdk_p.get_total_rx_queues();
    tx_descs = dpdk_p.tx_desc_num;
    rss_mode =get_dpdk_mode()->get_rx_distro_mode();

    if (get_dpdk_mode()->is_one_tx_rx_queue()) {
       tx_qs = 1;
       assert(rx_qs==1);
       rx_qs_descs.push_back(dpdk_p.rx_desc_num_data_q);
       rx_qs_drop_qid=0;
    }else{
       tx_qs = g_trex.m_max_queues_per_port;
       int i;
       for (i=0; i<rx_qs; i++) {
           uint16_t desc;
           if ( is_drop_q && 
               (i==MAIN_DPDK_DROP_Q) ){
               desc =dpdk_p.rx_desc_num_drop_q;
           }else{
               desc =dpdk_p.rx_desc_num_data_q;
           }
           rx_qs_descs.push_back(desc);
       }
       if (is_drop_q){
           rx_qs_drop_qid = MAIN_DPDK_RX_Q;
       }else{
           rx_qs_drop_qid = 0;
       }
    }

   _conf_queues(tx_qs,
                tx_descs,
                rx_qs,
                rx_qs_descs,
                rx_qs_drop_qid,
                rss_mode,
                in_astf_mode);

}

/**
 * get extended stats might fail on some drivers (i40e_vf) 
 * so wrap it another a watch 
 */
bool CPhyEthIF::get_extended_stats() {
    bool rc = get_ex_drv()->get_extended_stats(this, &m_stats);
    if (!rc) {
        m_stats_err_cnt++;
        assert(m_stats_err_cnt <= 5);
        return false;
    }
    
    /* clear the counter */
    m_stats_err_cnt = 0;
    return true;
}


void CPhyEthIF::update_counters() {
    bool rc = get_extended_stats();
    if (!rc) {
        return;
    }
    
    CRXCoreIgnoreStat ign_stats;

    g_trex.m_stx->get_ignore_stats(m_tvpid, ign_stats, true);
    
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

bool CPhyEthIF::Create(tvpid_t  tvpid,
                       repid_t  repid) {
    m_tvpid      = tvpid;
    m_repid      = repid;

    m_last_rx_rate      = 0.0;
    m_last_tx_rate      = 0.0;
    m_last_tx_pps       = 0.0;

    m_port_attr    = g_trex.m_drv->create_port_attr(tvpid,repid);

    if ( !m_is_dummy ) {
        /* set src MAC addr */
        uint8_t empty_mac[ETHER_ADDR_LEN] = {0,0,0,0,0,0};
        if (! memcmp( CGlobalInfo::m_options.m_mac_addr[m_tvpid].u.m_mac.src, empty_mac, ETHER_ADDR_LEN)) {
            rte_eth_macaddr_get(m_repid,
                                (struct ether_addr *)&CGlobalInfo::m_options.m_mac_addr[m_tvpid].u.m_mac.src);
        }
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
                if (g_trex.m_cores_vif[core_id + 1]->get_ports()[dir].m_port->get_tvpid() == m_tvpid) {
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
        /* TODO: imarom: move this to a message to the RX core */
        get_stateless_obj()->get_stl_rx()->reset_rx_stats(get_tvpid());
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
        /* TODO: imarom: move this to a message to the RX core */
        get_stateless_obj()->get_stl_rx()->get_rx_stats(get_tvpid(), rx_stats, min, max, reset, TrexPlatformApi::IF_STAT_IPV4_ID);
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
                tx_stats[i - min] = g_trex.clear_flow_tx_stats(m_tvpid, i, false);
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
                tx_stats[i - min] = g_trex.get_flow_tx_stats(m_tvpid, i);
            }
        }
    }

    return 0;
}

int CPhyEthIF::get_flow_stats_payload(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset) {
    get_stateless_obj()->get_stl_rx()->get_rx_stats(get_tvpid(), rx_stats, min, max, reset, TrexPlatformApi::IF_STAT_PAYLOAD);
    
    for (int i = min; i <= max; i++) {
        if ( reset ) {
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.clear_flow_tx_stats(m_tvpid, i + MAX_FLOW_STATS, true);
            }
        } else {
            if (tx_stats != NULL) {
                tx_stats[i - min] = g_trex.get_flow_tx_stats(m_tvpid, i + MAX_FLOW_STATS);
            }
        }
    }

    return 0;
}

TrexSTX * get_stx() {
    return g_trex.m_stx;
}

/**
 * handles an abort
 *
 */
void abort_gracefully(const std::string &on_stdout,
                      const std::string &on_publisher) {

    g_trex.abort_gracefully(on_stdout, on_publisher);
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
    CParserOption *g_opts = &CGlobalInfo::m_options;

    CGlobalInfo::m_socket.Create(&cg->m_platform);


    if (!cg->m_info_exist) {
        /* nothing to do ! */
        return 0;
    }

    g_opts->prefix =cg->m_prefix;
    g_opts->preview.setCores(cg->m_thread_per_dual_if);
    if ( cg->m_is_lowend ) {
        g_opts->m_is_lowend = true;
        g_opts->m_is_sleepy_scheduler = true;
        g_opts->m_is_queuefull_retry = false;
    }
    if ( cg->m_stack_type.size() ) {
        g_opts->m_stack_type = cg->m_stack_type;
    }
    #ifdef TREX_PERF
    g_opts->m_is_sleepy_scheduler = true;
    g_opts->m_is_queuefull_retry = false;
    #endif

    if ( cg->m_port_limit_exist ){
        if (cg->m_port_limit > cg->m_if_list.size() ) {
            cg->m_port_limit = cg->m_if_list.size();
        }
        g_opts->m_expected_portd = cg->m_port_limit;
    }else{
        g_opts->m_expected_portd = cg->m_if_list.size();
    }

    if ( g_opts->m_expected_portd < 2 ){
        printf("ERROR need at least 2 ports \n");
        exit(-1);
    }


    if ( cg->m_enable_zmq_pub_exist ){
        g_opts->preview.set_zmq_publish_enable(cg->m_enable_zmq_pub);
        g_opts->m_zmq_port = cg->m_zmq_pub_port;
    }
    if ( cg->m_telnet_exist ){
        g_opts->m_telnet_port = cg->m_telnet_port;
    }

    if ( cg->m_mac_info_exist ){
        int i;
        /* cop the file info */

        int port_size=cg->m_mac_info.size();

        if ( port_size > TREX_MAX_PORTS ){
            port_size = TREX_MAX_PORTS;
        }
        for (i=0; i<port_size; i++){
            cg->m_mac_info[i].copy_src(( char *)g_opts->m_mac_addr[i].u.m_mac.src)   ;
            cg->m_mac_info[i].copy_dest(( char *)g_opts->m_mac_addr[i].u.m_mac.dest)  ;
            g_opts->m_mac_addr[i].u.m_mac.is_set = 1;

            g_opts->m_ip_cfg[i].set_def_gw(cg->m_mac_info[i].get_def_gw());
            g_opts->m_ip_cfg[i].set_ip(cg->m_mac_info[i].get_ip());
            g_opts->m_ip_cfg[i].set_mask(cg->m_mac_info[i].get_mask());
            g_opts->m_ip_cfg[i].set_vlan(cg->m_mac_info[i].get_vlan());
            // If one of the ports has vlan, work in vlan mode
            if (cg->m_mac_info[i].get_vlan() != 0) {
                g_opts->preview.set_vlan_mode_verify(CPreviewMode::VLAN_MODE_NORMAL);
            }
        }
    }

    return (0);
}

void update_memory_cfg() {
    CPlatformYamlInfo *cg=&global_platform_cfg_info;

    /* mul by interface type */
    float mul=1.0;
    if (cg->m_port_bandwidth_gb<10 || CGlobalInfo::m_options.m_is_lowend) {
        cg->m_port_bandwidth_gb=10.0;
    }

    mul = mul*(float)cg->m_port_bandwidth_gb/10.0;
    mul= mul * (float)CGlobalInfo::m_options.m_expected_portd/2.0;
    mul= mul * CGlobalInfo::m_options.m_mbuf_factor;


    CGlobalInfo::m_memory_cfg.set_pool_cache_size(RTE_MEMPOOL_CACHE_MAX_SIZE);

    CGlobalInfo::m_memory_cfg.set_number_of_dp_cors(
                                                    CGlobalInfo::m_options.get_number_of_dp_cores_needed() );

    CGlobalInfo::m_memory_cfg.set(cg->m_memory,mul);
    if ( CGlobalInfo::m_options.m_active_flows > CGlobalInfo::m_memory_cfg.m_mbuf[MBUF_DP_FLOWS] ) {
        printf("\n");
        printf("ERROR: current configuration has %u flow objects, and you are asking for %u active flows.\n",
                CGlobalInfo::m_memory_cfg.m_mbuf[MBUF_DP_FLOWS], CGlobalInfo::m_options.m_active_flows);
        printf("Either decrease active flows, or increase memory pool.\n");
        printf("For example put in platform config file:\n");
        printf("\n");
        printf(" memory:\n");
        printf("     dp_flows: %u\n", CGlobalInfo::m_options.m_active_flows);
        printf("\n");
        exit(1);
    }
}

void check_pdev_vdev_dummy() {
    bool found_vdev = false;
    bool found_pdev = false;
    uint32_t dev_id = 1e6;
    uint8_t if_index = 0;
    CParserOption *g_opts=&CGlobalInfo::m_options;
    for ( std::string &iface : global_platform_cfg_info.m_if_list ) {
        g_opts->m_dummy_port_map[if_index] = false;
        if ( iface == "dummy" ) {
            g_opts->m_dummy_count++;
            g_opts->m_dummy_port_map[if_index] = true;
            CTVPort(if_index).set_dummy();
        } else if ( iface.find("--vdev") != std::string::npos ) {
            found_vdev = true;
        } else if ( iface.find(":") == std::string::npos ) { // not PCI, assume af-packet
            iface = "--vdev=net_af_packet" + std::to_string(dev_id) + ",iface=" + iface;
            if ( getpagesize() == 4096 ) {
                // block size should be multiplication of PAGE_SIZE and frame size
                // frame size should be Jumbo packet size and multiplication of 16
                iface += ",blocksz=593920,framesz=9280,framecnt=256";
            } else {
                printf("WARNING:\n");
                printf("    Could not automatically set AF_PACKET arguments: blocksz, framesz, framecnt.\n");
                printf("    Will not be able to send Jumbo packets.\n");
                printf("    See link below for more details (section \"Other constraints\")\n");
                printf("    https://www.kernel.org/doc/Documentation/networking/packet_mmap.txt\n");
            }
            dev_id++;
            found_vdev = true;
        } else {
            found_pdev = true;
        }
        if_index++;
    }
    if ( found_vdev ) {
        if ( found_pdev ) {
            printf("\n");
            printf("ERROR: both --vdev and another interface type specified in config file.\n");
            printf("\n");
            exit(1);
        } else {
            g_opts->m_is_vdev = true;
        }
    } else if ( !found_pdev ) {
        printf("\n");
        printf("ERROR: should be specified at least one vdev or PCI-based interface in config file.\n");
        printf("\n");
        exit(1);
    } else { // require at least one port in pair (dual-port) is non-dummy
        for ( uint8_t i=0; i<global_platform_cfg_info.m_if_list.size(); i++ ) {
            if ( g_opts->m_dummy_port_map[i] && g_opts->m_dummy_port_map[dual_port_pair(i)] ) {
                printf("ERROR: got dummy pair of interfaces: %u %u.\nAt least one of them should be non-dummy.\n", i, dual_port_pair(i));
                exit(1);
            }
        }
    }

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
    CPlatformYamlInfo *cg=&global_platform_cfg_info;

    lpsock->set_rx_thread_is_enabled(get_is_rx_thread_enabled());
    lpsock->set_number_of_threads_per_ports(lpop->preview.getCores() );
    lpsock->set_number_of_dual_ports(lpop->get_expected_dual_ports());
    if ( !lpsock->sanity_check() ){
        printf(" ERROR in configuration file \n");
        return (-1);
    }

    if ( isVerbose(0)  ) {
        lpsock->dump(stdout);
    }

    if ( !CGlobalInfo::m_options.m_is_vdev ){
        std::string err;
        if ( port_map.set_cfg_input(cg->m_if_list,err)!= 0){
            printf("%s \n",err.c_str());
            return(-1);
        }
    }

    /* set the DPDK options */
    g_dpdk_args_num = 0;
    #define SET_ARGS(val) { g_dpdk_args[g_dpdk_args_num++] = (char *)(val); }

    SET_ARGS((char *)"xx");
    CPreviewMode *lpp=&CGlobalInfo::m_options.preview;

    if ( lpp->get_ntacc_so_mode() ){
        std::string &ntacc_so_str = get_ntacc_so_string();
        ntacc_so_str = "libntacc-64" + std::string(g_image_postfix) + ".so";
        SET_ARGS("-d");
        SET_ARGS(ntacc_so_str.c_str());
    }

    if ( lpp->get_mlx5_so_mode() ){
        std::string &mlx5_so_str = get_mlx5_so_string();
        mlx5_so_str = "libmlx5-64" + std::string(g_image_postfix) + ".so";
        SET_ARGS("-d");
        SET_ARGS(mlx5_so_str.c_str());
    }

    if ( lpp->get_mlx4_so_mode() ){
        std::string &mlx4_so_str = get_mlx4_so_string();
        mlx4_so_str = "libmlx4-64" + std::string(g_image_postfix) + ".so";
        SET_ARGS("-d");
        SET_ARGS(mlx4_so_str.c_str());
    }

    if ( CGlobalInfo::m_options.m_is_lowend ) { // assign all threads to core 0
        g_cores_str[0] = '(';
        lpsock->get_cores_list(g_cores_str + 1);
        strcat(g_cores_str, ")@0");
        SET_ARGS("--lcores");
        SET_ARGS(g_cores_str);
    } else {
        snprintf(g_cores_str, sizeof(g_cores_str), "0x%llx" ,(unsigned long long)lpsock->get_cores_mask());
        if (core_mask_sanity(strtol(g_cores_str, NULL, 16)) < 0) {
            return -1;
        }
        SET_ARGS("-c");
        SET_ARGS(g_cores_str);
    }

    SET_ARGS("-n");
    SET_ARGS("4");

    if ( lpp->getVMode() == 0  ) {
        SET_ARGS("--log-level");
        snprintf(g_loglevel_str, sizeof(g_loglevel_str), "%d", 4);
        SET_ARGS(g_loglevel_str);
    }else{
        SET_ARGS("--log-level");
        snprintf(g_loglevel_str, sizeof(g_loglevel_str), "%d", lpp->getVMode()+1);
        SET_ARGS(g_loglevel_str);
    }

    SET_ARGS("--master-lcore");

    snprintf(g_master_id_str, sizeof(g_master_id_str), "%u", lpsock->get_master_phy_id());
    SET_ARGS(g_master_id_str);

    /* add white list */
    if ( CGlobalInfo::m_options.m_is_vdev ) {
        for ( std::string &iface : cg->m_if_list ) {
            if ( iface != "dummy" ) {
                SET_ARGS(iface.c_str());
            }
        }
        SET_ARGS("--no-pci");
        SET_ARGS("--no-huge");
        std::string mem_str;
        SET_ARGS("-m");
        if ( cg->m_limit_memory.size() ) {
            mem_str = cg->m_limit_memory;
        } else if ( CGlobalInfo::m_options.m_is_lowend ) {
            mem_str = std::to_string(50 + 100 * cg->m_if_list.size());
        } else {
            mem_str = "1024";
        }
        SET_ARGS(mem_str.c_str());
    } else {
        dpdk_input_args_t & dif = *port_map.get_dpdk_input_args();

        for (int i=0; i<(int)dif.size(); i++) {
            if ( dif[i] != "dummy" ) {
                SET_ARGS("-w");
                SET_ARGS(dif[i].c_str());
            }
        }
    }

    SET_ARGS("--legacy-mem");

    if ( lpop->prefix.length() ) {
        SET_ARGS("--file-prefix");
        snprintf(g_prefix_str, sizeof(g_prefix_str), "%s", lpop->prefix.c_str());
        SET_ARGS(g_prefix_str);

        if ( !CGlobalInfo::m_options.m_is_lowend && !CGlobalInfo::m_options.m_is_vdev ) {
            SET_ARGS("--socket-mem");
            char *mem_str;
            if (cg->m_limit_memory.length()) {
                mem_str = (char *)cg->m_limit_memory.c_str();
            }else{
                mem_str = (char *)"1024";
            }
            int pos = snprintf(g_socket_mem_str, sizeof(g_socket_mem_str), "%s", mem_str);
            for (int socket = 1; socket < MAX_SOCKETS_SUPPORTED; socket++) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "/sys/devices/system/node/node%u/", socket);
                if (access(path, F_OK) == 0) {
                    pos += snprintf(g_socket_mem_str+pos, sizeof(g_socket_mem_str)-pos, ",%s", mem_str);
                } else {
                    break;
                }
            }
            SET_ARGS(g_socket_mem_str);
        }
    }


    if ( lpp->getVMode() > 0  ) {
        printf("DPDK args \n");
        int i;
        for (i=0; i<g_dpdk_args_num; i++) {
            printf(" %s ",g_dpdk_args[i]);
        }
        printf(" \n ");
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
        lpt->start_sim(op->out_file,op->preview);
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
    struct rte_eth_dev_info dev_info;
    struct rte_pci_device pci_dev;

    for (uint8_t port_id=0; port_id<m_max_ports; port_id++) {
        // PCI, MAC and Driver
        rte_eth_dev_info_get(port_id, &dev_info);
        rte_eth_macaddr_get(port_id, &mac_addr);
        ether_format_addr(mac_str, sizeof mac_str, &mac_addr);
        bool ret = fill_pci_dev(&dev_info, &pci_dev);
        if ( ret ) {
            struct rte_pci_addr *pci_addr = &pci_dev.addr;
            printf("PCI: %04x:%02x:%02x.%d", pci_addr->domain, pci_addr->bus, pci_addr->devid, pci_addr->function);
        } else {
            printf("PCI: N/A");
        }
        printf(" - MAC: %s - Driver: %s\n", mac_str, dev_info.driver_name);
    }
}


int learn_image_postfix(char * image_name){

    char *p = strstr(image_name,TREX_NAME);
    if (p) {
        strcpy(g_image_postfix,p+strlen(TREX_NAME));
    }
    return(0);
}

int main_test(int argc , char * argv[]){

    learn_image_postfix(argv[0]);

    int ret;
    unsigned lcore_id;
    
    if (TrexBuildInfo::is_sanitized()) {
         printf("\n*******************************************************\n");
         printf("\n***** Sanitized binary - Expect lower performance *****\n\n");
         printf("\n*******************************************************\n");
    }

    CParserOption * po=&CGlobalInfo::m_options;

    printf("Starting  TRex %s please wait  ... \n",VERSION_BUILD_NUM);

    po->preview.clean();

    if ( parse_options_wrapper(argc, argv, true ) != 0){
        exit(-1);
    }

    if (!po->preview.get_is_termio_disabled()) {
        utl_termio_init();
    }
    
    
    /* enable core dump if requested */
    if (po->preview.getCoreDumpEnable()) {
        utl_set_coredump_size(-1);
    }
    else {
        utl_set_coredump_size(0);
    }


    update_global_info_from_platform_file();

    /* It is not a mistake. Give the user higher priorty over the configuration file */
    if (parse_options_wrapper(argc, argv, false) != 0) {
        exit(-1);
    }


    update_memory_cfg();

    if ( po->preview.getVMode() > 0){
        po->dump(stdout);
        CGlobalInfo::m_memory_cfg.Dump(stdout);
    }

    check_pdev_vdev_dummy();

    if (update_dpdk_args() < 0) {
        return -1;
    }

    if ( po->preview.getVMode() == 0  ) {
        rte_log_set_global_level(1);
    }

    uid_t uid;
    uid = geteuid ();
    if ( uid != 0 ) {
        printf("ERROR you must run with superuser priviliges \n");
        printf("User id   : %d \n",uid);
        printf("try 'sudo' %s \n",argv[0]);
        return (-1);
    }

    if ( get_is_tcp_mode() ){

        if ( po->preview.get_is_rx_check_enable() ){
           printf("ERROR advanced stateful does not require --rx-check mode, it is done by default, please remove this switch\n");
           return (-1);
        }

        /* set latency to work in ICMP mode with learn mode */
        po->m_learn_mode = CParserOption::LEARN_MODE_IP_OPTION;
        if (po->m_l_pkt_mode ==0){
            po->m_l_pkt_mode =L_PKT_SUBMODE_REPLY;
        }

        if ( po->preview.getClientServerFlip() ){
            printf("ERROR advanced stateful does not support --flip option, please remove this switch\n");
            return (-1);
        }

        if ( po->preview.getClientServerFlowFlip() ){
            printf("ERROR advanced stateful does not support -p option, please remove this switch\n");
            return (-1);
        }

        if ( po->preview.getClientServerFlowFlipAddr() ){
            printf("ERROR advanced stateful does not support -e option, please remove this switch\n");
            return (-1);
        }

        if ( po->m_active_flows ){
            printf("ERROR advanced stateful does not support --active-flows option, please remove this switch  \n");
            return (-1);
        }
    }

    /* set affinity to the master core as default */
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(CGlobalInfo::m_socket.get_master_phy_id(), &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

    ret = rte_eal_init(g_dpdk_args_num, (char **)g_dpdk_args);
    if (ret < 0){
        printf(" You might need to run ./trex-cfg  once  \n");
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    }
    if (get_op_mode() == OP_MODE_DUMP_INTERFACES) {
        dump_interfaces_info();
        exit(0);
    }
    reorder_dpdk_ports();
    set_driver();

    uint32_t driver_cap = get_ex_drv()->get_capabilities();
    
    int res;
    res =CGlobalInfo::m_dpdk_mode.choose_mode((trex_driver_cap_t)driver_cap);
    if (res == tmDPDK_UNSUPPORTED) {
        exit(1);
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

    if (po->preview.get_is_rx_check_enable() &&  (po->m_rx_check_sample< get_min_sample_rate()) ) {
        po->m_rx_check_sample = get_min_sample_rate();
        printf("Warning:rx check sample rate should not be lower than %d. Setting it to %d\n",get_min_sample_rate(),get_min_sample_rate());
    }

    /* set dump mode */
    g_trex.m_io_modes.set_mode((CTrexGlobalIoMode::CliDumpMode)CGlobalInfo::m_options.m_io_mode);

    /* disable WD if needed */
    bool wd_enable = (CGlobalInfo::m_options.preview.getWDDisable() ? false : true);
    TrexWatchDog::getInstance().init(wd_enable);

    // For unit testing of HW rules and queues configuration. Just send some packets and exit.
    if (CGlobalInfo::m_options.m_debug_pkt_proto != 0) {
        CTrexDpdkParams dpdk_p;
        get_dpdk_drv_params(dpdk_p);
        CTrexDebug debug = CTrexDebug(g_trex.m_ports[0], g_trex.m_max_ports
                                      , dpdk_p.get_total_rx_queues());
        int ret;

        if (CGlobalInfo::m_options.m_debug_pkt_proto == D_PKT_TYPE_HW_TOGGLE_TEST) {
            // Unit test: toggle many times between receive all and stateless/stateful modes,
            // to test resiliency of add/delete fdir filters
            printf("Starting receive all/normal mode toggle unit test\n");
            for (int i = 0; i < 100; i++) {
                for (int port_id = 0; port_id < g_trex.m_max_ports; port_id++) {
                    CPhyEthIF *pif = g_trex.m_ports[port_id];
                    pif->set_port_rcv_all(true);
                }
                ret = debug.test_send(D_PKT_TYPE_HW_VERIFY_RCV_ALL);
                if (ret != 0) {
                    printf("Iteration %d: Receive all mode failed\n", i);
                    exit(ret);
                }

                for (int port_id = 0; port_id < g_trex.m_max_ports; port_id++) {
                    CPhyEthIF *pif = g_trex.m_ports[port_id];
                    get_ex_drv()->configure_rx_filter_rules(pif);
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
                    CPhyEthIF *pif = g_trex.m_ports[port_id];
                    pif->set_port_rcv_all(true);
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
    g_trex.device_rx_queue_flush();
    for (int i = 0; i < g_trex.m_max_ports; i++) {
        CPhyEthIF *_if = g_trex.m_ports[i];
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
    
    if (!CGlobalInfo::m_options.preview.get_is_termio_disabled()) {
        utl_termio_reset();
    }

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

#define TCP_UDP_OFFLOAD (DEV_TX_OFFLOAD_IPV4_CKSUM |DEV_TX_OFFLOAD_UDP_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM)

/* should be called after rte_eal_init() */
void set_driver() {
    uint8_t m_max_ports;
    if ( CGlobalInfo::m_options.m_is_vdev ) {
        m_max_ports = rte_eth_dev_count() + CGlobalInfo::m_options.m_dummy_count;
    } else {
        m_max_ports = port_map.get_max_num_ports();
    }

    if ( m_max_ports != CGlobalInfo::m_options.m_expected_portd ) {
        printf("Could not find all interfaces (asked for: %u, found: %u).\n", CGlobalInfo::m_options.m_expected_portd, m_max_ports);
        exit(1);
    }
    struct rte_eth_dev_info dev_info;
    for (int i=0; i<m_max_ports; i++) {
        CTVPort ctvport = CTVPort(i);
        if ( !ctvport.is_dummy() ) {
            rte_eth_dev_info_get(ctvport.get_repid(), &dev_info);
            break;
        }
    }

    if ( !CTRexExtendedDriverDb::Ins()->is_driver_exists(dev_info.driver_name) ){
        printf("\nError: driver %s is not supported. Please consult the documentation for a list of supported drivers\n"
               ,dev_info.driver_name);
        exit(1);
    }

    CTRexExtendedDriverDb::Ins()->set_driver_name(dev_info.driver_name);

    if ( CGlobalInfo::m_options.m_dummy_count ) {
        CTRexExtendedDriverDb::Ins()->create_dummy();
    }

    bool cs_offload=false;
    CPreviewMode * lp=&CGlobalInfo::m_options.preview;

    printf(" driver capability  :");
    if ( (dev_info.tx_offload_capa & TCP_UDP_OFFLOAD) == TCP_UDP_OFFLOAD ){
        cs_offload=true;
        printf(" TCP_UDP_OFFLOAD ");
    }

    if ( (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_TSO) == DEV_TX_OFFLOAD_TCP_TSO ){
        printf(" TSO ");
        lp->set_dev_tso_support(true);
    }
    printf("\n");

    if (lp->getTsoOffloadDisable() && lp->get_dev_tso_support()){
        printf("Warning TSO is supported and asked to be disabled by user \n");
        lp->set_dev_tso_support(false);
    }


    if (cs_offload) {
        if (lp->getChecksumOffloadEnable()) {
            printf("Warning --checksum-offload is turn on by default, no need to call it \n");
        }

        if (lp->getChecksumOffloadDisable()==false){
            lp->setChecksumOffloadEnable(true);
        }else{
            printf("checksum-offload disabled by user \n");
        }
    }

    
}



/*

 map requested ports to rte_eth scan

*/
void reorder_dpdk_ports() {

    CTRexPortMapper * lp=CTRexPortMapper::Ins();

    if ( CGlobalInfo::m_options.m_is_vdev ) {
        uint8_t if_index = 0;
        for (int i=0; i<global_platform_cfg_info.m_if_list.size(); i++) {
            if ( CTVPort(i).is_dummy() ) {
                continue;
            }
            lp->set_map(i, if_index);
            if_index++;
        }
        return;
    }

    #define BUF_MAX 200
    char buf[BUF_MAX];
    dpdk_input_args_t  dpdk_scan;
    dpdk_map_args_t res_map;

    std::string err;

    /* build list of dpdk devices */
    uint8_t cnt = rte_eth_dev_count();
    int i;
    for (i=0; i<cnt; i++) {
        if (rte_eth_dev_pci_addr((repid_t)i,buf,BUF_MAX)!=0){
            printf("Failed mapping TRex port id to DPDK id: %d\n", i);
            exit(1);
        }
        dpdk_scan.push_back(std::string(buf));
    }

    if ( port_map.get_map_args(dpdk_scan, res_map, err) != 0){
        printf("ERROR in DPDK map \n");
        printf("%s\n",err.c_str());
        exit(1);
    }

    /* update MAP */
    lp->set(cnt, res_map);

    if ( isVerbose(0) ){
        port_map.dump(stdout);
        lp->Dump(stdout);
    }
}


#if 0  
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
#endif

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
    return g_trex.m_ports[port_id]->get_port_attr()->get_xstats_values(xstats_values);
}

int TrexDpdkPlatformApi::get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names) const {
    return g_trex.m_ports[port_id]->get_port_attr()->get_xstats_names(xstats_names);
}

void
TrexDpdkPlatformApi::global_stats_to_json(Json::Value &output) const {
    g_trex.global_stats_to_json(output);
}

void
TrexDpdkPlatformApi::port_stats_to_json(Json::Value &output, uint8_t port_id) const {
    g_trex.port_stats_to_json(output, port_id);
}

uint32_t
TrexDpdkPlatformApi::get_port_count() const {
    return g_trex.m_max_ports;
}

uint8_t
TrexDpdkPlatformApi::get_dp_core_count() const {
    return CGlobalInfo::m_options.get_number_of_dp_cores_needed();
}


void
TrexDpdkPlatformApi::port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {

    CPhyEthIF *lpt = g_trex.m_ports[port_id];

    /* copy data from the interface */
    cores_id_list = lpt->get_core_list();
}


void
TrexDpdkPlatformApi::get_port_info(uint8_t port_id, intf_info_st &info) const {
    struct ether_addr rte_mac_addr = {{0}};

    if ( g_trex.m_ports[port_id]->is_dummy() ) {
        info.driver_name = "Dummy";
    } else {
        info.driver_name = CTRexExtendedDriverDb::Ins()->get_driver_name();
    }

    /* mac INFO */

    /* hardware */
    g_trex.m_ports[port_id]->get_port_attr()->get_hw_src_mac(&rte_mac_addr);
    assert(ETHER_ADDR_LEN == 6);

    memcpy(info.hw_macaddr, rte_mac_addr.addr_bytes, 6);

    info.numa_node = g_trex.m_ports[port_id]->get_port_attr()->get_numa();
    info.pci_addr = g_trex.m_ports[port_id]->get_port_attr()->get_pci_addr();
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
TrexDpdkPlatformApi::get_port_stat_info(uint8_t port_id, uint16_t &num_counters, uint16_t &capabilities
                                             , uint16_t &ip_id_base) const {
    get_ex_drv()->get_rx_stat_capabilities(capabilities, num_counters ,ip_id_base);
}

int TrexDpdkPlatformApi::get_flow_stats(uint8_t port_id, void *rx_stats, void *tx_stats, int min, int max, bool reset
                                        , TrexPlatformApi::driver_stat_cap_e type) const {
    if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
        return g_trex.m_ports[port_id]->get_flow_stats_payload((rx_per_flow_t *)rx_stats, (tx_per_flow_t *)tx_stats
                                                              , min, max, reset);
    } else {
        return g_trex.m_ports[port_id]->get_flow_stats((rx_per_flow_t *)rx_stats, (tx_per_flow_t *)tx_stats
                                                      , min, max, reset);
    }
}

int TrexDpdkPlatformApi::get_rfc2544_info(void *rfc2544_info, int min, int max, bool reset
                                          , bool period_switch) const {
    return get_stateless_obj()->get_stl_rx()->get_rfc2544_info((rfc2544_info_t *)rfc2544_info, min, max, reset, period_switch);
}

int TrexDpdkPlatformApi::get_rx_err_cntrs(void *rx_err_cntrs) const {
    return get_stateless_obj()->get_stl_rx()->get_rx_err_cntrs((CRxCoreErrCntrs *)rx_err_cntrs);
}

int TrexDpdkPlatformApi::reset_hw_flow_stats(uint8_t port_id) const {
    return g_trex.m_ports[port_id]->reset_hw_flow_stats();
}

int TrexDpdkPlatformApi::add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                               , uint8_t ipv6_next_h, uint16_t id) const {
    if (!get_dpdk_mode()->is_hardware_filter_needed()) {
        return 0;
    }
    CPhyEthIF * lp=g_trex.m_ports[port_id];

    return get_ex_drv()->add_del_rx_flow_stat_rule(lp, RTE_ETH_FILTER_ADD, l3_type, l4_proto, ipv6_next_h, id);
}

int TrexDpdkPlatformApi::del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                               , uint8_t ipv6_next_h, uint16_t id) const {
    if (!get_dpdk_mode()->is_hardware_filter_needed()) {
        return 0;
    }

    CPhyEthIF * lp=g_trex.m_ports[port_id];


    return get_ex_drv()->add_del_rx_flow_stat_rule(lp, RTE_ETH_FILTER_DELETE, l3_type, l4_proto, ipv6_next_h, id);
}

int TrexDpdkPlatformApi::get_active_pgids(flow_stat_active_t_new &result) const {
    return CFlowStatRuleMgr::instance()->get_active_pgids(result);
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

int TrexDpdkPlatformApi::get_pgid_stats(Json::Value &json, std::vector<uint32_t> pgids) const {
    g_trex.sync_threads_stats();
    CFlowStatRuleMgr::instance()->dump_json(json, pgids);
    return 0;
}

CFlowStatParser *TrexDpdkPlatformApi::get_flow_stat_parser() const {
    return get_ex_drv()->get_flow_stat_parser();
}

TRexPortAttr *TrexDpdkPlatformApi::getPortAttrObj(uint8_t port_id) const {
    return g_trex.m_ports[port_id]->get_port_attr();
}

bool DpdkTRexPortAttr::update_link_status_nowait(){
    rte_eth_link new_link;
    bool changed = false;
    rte_eth_link_get_nowait(m_repid, &new_link);

    if (new_link.link_speed != m_link.link_speed ||
                new_link.link_duplex != m_link.link_duplex ||
                    new_link.link_autoneg != m_link.link_autoneg ||
                        new_link.link_status != m_link.link_status) {
        changed = true;

        /* in case of link status change - notify the dest object */
        if (new_link.link_status != m_link.link_status && get_is_interactive()) {
            if ( g_trex.m_stx != nullptr ) {
                g_trex.m_stx->get_port_by_id(m_port_id)->invalidate_dst_mac();
            }
        }
    }

    m_link = new_link;
    return changed;
}

int DpdkTRexPortAttr::set_rx_filter_mode(rx_filter_mode_e rx_filter_mode) {

    if (rx_filter_mode == m_rx_filter_mode) {
        return (0);
    }

    CPhyEthIF *_if = g_trex.m_ports[m_tvpid];
    bool recv_all = (rx_filter_mode == RX_FILTER_MODE_ALL);
    int rc = _if->set_port_rcv_all(recv_all);
    if (rc != 0) {
        return (rc);
    }

    m_rx_filter_mode = rx_filter_mode;

    return (0);
}

bool DpdkTRexPortAttr::is_loopback() const {
    uint8_t port_id;
    return g_trex.lookup_port_by_mac(CGlobalInfo::m_options.get_dst_src_mac_addr(m_port_id), port_id);
}


int
DpdkTRexPortAttrMlnx5G::set_link_up(bool up) {
    TrexMonitor * cur_monitor = TrexWatchDog::getInstance().get_current_monitor();
    if (cur_monitor != NULL) {
        cur_monitor->disable(5); // should take ~2.5 seconds
    }
    int result = DpdkTRexPortAttr::set_link_up(up);
    if (cur_monitor != NULL) {
        cur_monitor->enable();
    }
    return result;
}


/**
 * marks the control plane for a total server shutdown
 *
 * @author imarom (7/27/2016)
 */
void TrexDpdkPlatformApi::mark_for_shutdown() const {
    g_trex.mark_for_shutdown(CGlobalTRex::SHUTDOWN_RPC_REQ);
}

CSyncBarrier* TrexDpdkPlatformApi::get_sync_barrier(void) const {
    return g_trex.m_sync_barrier;
}

CFlowGenList *
TrexDpdkPlatformApi::get_fl() const {
    return &g_trex.m_fl;
}


/**
 * DPDK API target
 */
TrexPlatformApi &get_platform_api() {
    static TrexDpdkPlatformApi api;
    
    return api;
}


