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


#include "bp_sim.h"
#include "os_time.h"
#include "trex_client_config.h"

#include <unordered_map>
#include <string>

#include "common/arg/SimpleGlob.h"
#include "common/arg/SimpleOpt.h"

#include "stl/trex_stl.h"

#include "sim/trex_sim.h"

using namespace std;

// An enum for all the option types
enum { OPT_HELP, OPT_CFG, OPT_NODE_DUMP, OP_STATS,
       OPT_FILE_OUT, OPT_UT, OPT_PCAP, OPT_IPV6, OPT_CLIENT_CFG_FILE,
       OPT_SL, OPT_ASF, OPT_DP_CORE_COUNT, OPT_DP_CORE_INDEX, OPT_LIMIT,
       OPT_ASTF_SIM_MODE,OPT_ASTF_FULL,
       OPT_ASTF_SIM_ARG,

       OPT_DRY_RUN, OPT_DURATION,
       OPT_DUMP_JSON};



/**
 * type of run 
 * GTEST 
 * Stateful 
 * Stateless 
 */
typedef enum {
    OPT_TYPE_GTEST = 7,
    OPT_TYPE_SF,
    OPT_TYPE_ASF,
    OPT_TYPE_SL
} opt_type_e;


/* these are the argument types:
   SO_NONE --    no argument needed
   SO_REQ_SEP -- single required argument
   SO_MULTI --   multiple arguments needed
*/
static CSimpleOpt::SOption parser_options[] =
{
    { OPT_HELP,               "-?",           SO_NONE    },
    { OPT_HELP,               "-h",           SO_NONE    },
    { OPT_HELP,               "--help",       SO_NONE    },
    { OPT_UT,                 "--ut",         SO_NONE    },
    { OP_STATS,               "-s",           SO_NONE    },
    { OPT_CFG,                "-f",           SO_REQ_SEP },
    { OPT_CLIENT_CFG_FILE,    "--client_cfg", SO_REQ_SEP },
    { OPT_CLIENT_CFG_FILE,    "--client-cfg", SO_REQ_SEP },
    { OPT_FILE_OUT ,          "-o",           SO_REQ_SEP },
    { OPT_NODE_DUMP ,         "-v",           SO_REQ_SEP },
    { OPT_DURATION,           "-d",           SO_REQ_SEP },
    { OPT_PCAP,               "--pcap",       SO_NONE    },
    { OPT_IPV6,               "--ipv6",       SO_NONE    },
    { OPT_SL,                 "--sl",         SO_NONE    },
    { OPT_ASF,                "--tcp_cfg",    SO_REQ_SEP   },
    { OPT_ASTF_FULL,          "--full",       SO_NONE    },
    { OPT_DP_CORE_COUNT,      "--cores",      SO_REQ_SEP },
    { OPT_DP_CORE_INDEX,      "--core_index", SO_REQ_SEP },
    { OPT_LIMIT,              "--limit",      SO_REQ_SEP },
    { OPT_DUMP_JSON,          "--sim-json", SO_NONE },
    { OPT_ASTF_SIM_MODE,      "--sim-mode", SO_REQ_SEP },
    { OPT_ASTF_SIM_ARG,       "--sim-arg",  SO_REQ_SEP },
    { OPT_DRY_RUN,            "--dry",      SO_NONE },

    
    SO_END_OF_OPTIONS
};


static TrexSTX *m_sim_stx;
static char *g_exe_name;

static asrtf_args_t  asrtf_args;

const char *get_exe_name() {
    return g_exe_name;
}

static int usage(){

    printf(" Usage: bp_sim [OPTION] -f cfg.yaml -o outfile.erf   \n");
    printf(" \n");
    printf(" \n");
    printf(" options:\n");
    printf(" -d  [s]   duration time of simulated traffic in seconds\n");
    printf(" -v  [1-3]   verbose mode  \n");
    printf("      1    show only stats  \n");
    printf("      2    run preview do not write to file  \n");
    printf("      3    run preview write stats file  \n");
    printf("  Note in case of verbose mode you don't need to add the output file \n");
    printf("   \n");
    printf("  Warning : This program can generate huge-files (TB ) watch out! try this only on local drive \n");
    printf(" \n");
    printf(" --pcap  export the file in pcap mode \n");
    printf(" Examples: ");
    printf("  1) preview show csv stats \n");
    printf("  #>bp_sim -f cfg.yaml -v 1 \n");
    printf("  \n ");
    printf("  2) more detail preview preview show csv stats \n");
    printf("  #>bp_sim -f cfg.yaml -v 2 \n");
    printf("  \n ");
    printf("  3) more detail preview plus stats  \n");
    printf("  #>bp_sim -f cfg.yaml -v 3 \n");
    printf("  \n ");
    printf("  4) do the job  ! \n");
    printf("  #>bp_sim -f cfg.yaml -o outfile.erf \n");
    printf("\n");
    printf("\n");
    printf(" ASTF modes :\n");
    printf("                                     \n");
    printf("                                     \n");
    printf("            csSIM_RST_SYN       0x17 23 \n");
    printf("            csSIM_RST_SYN1      0x18 24 \n");
    printf("            csSIM_WRONG_PORT    0x19 25 \n");
    printf("            csSIM_RST_MIDDLE    0x1a 26 \n");
    printf("            csSIM_RST_MIDDLE2   0x1b 27 \n");
    printf("            csSIM_DROP,         0x1c 28 \n");
    printf("            csSIM_REORDER,      0x1d 29  \n");
    printf("            csSIM_REORDER_DROP  0x1e 30  \n");
    printf("                                      \n");
    printf(" Copyright (C) 2015 by hhaim Cisco-System for IL dev-test \n");
    printf(" version : 1.0 beta  \n");
    return (0);
}


static int parse_options(int argc,
                         char *argv[],
                         CParserOption* po,
                         std::unordered_map<std::string, int> &params) {

     CSimpleOpt args(argc, argv, parser_options);

     int a=0;
     int node_dump=0;
     po->preview.clean();
     po->preview.setFileWrite(true);

     /* by default - type is stateful */
     params["type"] = OPT_TYPE_SF;

     while ( args.Next() ){
        if (args.LastError() == SO_SUCCESS) {
            switch (args.OptionId()) {
            case OPT_UT :
                params["type"] = OPT_TYPE_GTEST;
                return (0);
                break;

            case OPT_HELP: 
                usage();
                return -1;

            case OPT_SL:
                params["type"] = OPT_TYPE_SL;
                break;

            case OPT_ASF:
                params["type"] = OPT_TYPE_ASF;
                po->astf_cfg_file = args.OptionArg();
                break;

            case OPT_CFG:
                po->cfg_file = args.OptionArg();
                break;

            case OPT_CLIENT_CFG_FILE:
                po->client_cfg_file = args.OptionArg();
                break;

            case OPT_FILE_OUT:
                po->out_file = args.OptionArg();
                break;

            case OPT_IPV6:
                po->preview.set_ipv6_mode_enable(true);
                break;

            case OPT_NODE_DUMP:
                a=atoi(args.OptionArg());
                node_dump=1;
                po->preview.setFileWrite(false);
                break;

            case OPT_DUMP_JSON:
                asrtf_args.dump_json =true;
                break;
            case OPT_ASTF_SIM_MODE:
                asrtf_args.sim_mode = atoi(args.OptionArg());
                break;
            case OPT_ASTF_SIM_ARG:
                float a;
                sscanf(args.OptionArg(),"%f", &a);
                asrtf_args.sim_arg=(double)a;
                break;

                break;
            case OPT_PCAP:
                po->preview.set_pcap_mode_enable(true);
                break;

            case OPT_ASTF_FULL :
                asrtf_args.full_sim=true;
                break;
            case OPT_DP_CORE_COUNT:
                params["dp_core_count"] = atoi(args.OptionArg());
                break;

            case OPT_DP_CORE_INDEX:
                params["dp_core_index"] = atoi(args.OptionArg());
                break;

            case OPT_LIMIT:
                params["limit"] = atoi(args.OptionArg());
                break;

            case OPT_DURATION:
                sscanf(args.OptionArg(),"%f", &po->m_duration);
                break;

            case OPT_DRY_RUN:
                params["dry"] = 1;
                break;

            default:
                printf("Error: option %s is defined, but not handled.\n\n", args.OptionText());
                return -1;
                break;
            } // End of switch
         }// End of IF
        else {
            usage();
            return -1;
        }
     } // End of while

     if ((po->cfg_file =="") ) {
         if (po->astf_cfg_file == "") {
             printf("Invalid combination of parameters you must add either -f or --tcp_cfg \n");
             usage();
             return -1;
         }
     } else {
         if (po->astf_cfg_file != "") {
             printf("Invalid combination of parameters. Can't specify both -f and --tcp_cfg \n");
             usage();
             return -1;
         }
     }
     if ( node_dump ){
         po->preview.setVMode(a);
     }else{
         if  (po->out_file=="" ){
             printf("Invalid combination of parameters you must give output file using -o  \n");
             usage();
             return -1;
         }
     }

    /* did the user configure dp core count or dp core index ? */

    if (params.count("dp_core_count") > 0) {
        if (!in_range(params["dp_core_count"], 1, 8)) {
            printf("dp core count must be a value between 1 and 8\n");
            return (-1);
        }
    }

     if (params.count("dp_core_index") > 0) {
        if (!in_range(params["dp_core_index"], 0, params["dp_core_count"] - 1)) {
            printf("dp core index must be a value between 0 and cores - 1\n");
            return (-1);
        }
    }

    return 0;
}

void set_default_mac_addr(){

    int i;
    for (i=0; i<4; i++) {
        memset(CGlobalInfo::m_options.get_dst_src_mac_addr(i),((i+1)<<4),6);
        memset(CGlobalInfo::m_options.get_src_mac_addr(i),((i+1)<<4)+8,6);
    }
}

TrexSTX * get_stx() {
    return m_sim_stx;
}


void set_stx(TrexSTX *obj) {
    m_sim_stx = obj;
}


void abort_gracefully(const std::string &on_stdout,
                      const std::string &on_publisher) {
    
    std::cout << on_stdout << "\n";
    abort();
}


int astf_full_sim(void){

    return(0);
}



int main(int argc , char * argv[]){
    g_exe_name = argv[0];

    std::unordered_map<std::string, int> params;

    if ( parse_options(argc, argv, &CGlobalInfo::m_options , params) != 0) {
        exit(-1);
    }
    set_default_mac_addr();

    opt_type_e type = (opt_type_e) params["type"];

    switch (type) {
    case OPT_TYPE_GTEST:
        {
            SimGtest test;
            return test.run(argc, argv);
        }

    case OPT_TYPE_SF:
        {
            SimStateful sf;
            CGlobalInfo::m_options.m_op_mode = CParserOption::OP_MODE_STF;
            return sf.run();
        }

    case OPT_TYPE_ASF:
        {
            CGlobalInfo::m_options.preview.setFileWrite(true);
            if (asrtf_args.full_sim){
                SimAstf sf;
                sf.args=&asrtf_args;
                return sf.run();
            }else{
                SimAstfSimple sf;
                sf.args=&asrtf_args;
                return sf.run();
            }
        }
    case OPT_TYPE_SL:
        {
            SimStateless &st = SimStateless::get_instance();
            CGlobalInfo::m_options.m_op_mode = CParserOption::OP_MODE_STL;

            if (params.count("dp_core_count") == 0) {
                params["dp_core_count"] = 1;
            }

            if (params.count("dp_core_index") == 0) {
                params["dp_core_index"] = -1;
            }

            if (params.count("limit") == 0) {
                params["limit"] = 5000;
            }

            if (params.count("dry") == 0) {
                params["dry"] = 0;
            }

            return st.run(CGlobalInfo::m_options.cfg_file,
                          CGlobalInfo::m_options.out_file,
                          2,
                          params["dp_core_count"],
                          params["dp_core_index"],
                          params["limit"],
                          (params["dry"] == 1)
                          );
        }
    }
}


/**
 * SIM API target
 */
TrexPlatformApi &get_platform_api() {
    static SimPlatformApi api(1);
    
    return api;
}

