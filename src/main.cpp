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

#include <unordered_map>
#include <string>

#include <common/arg/SimpleGlob.h>
#include <common/arg/SimpleOpt.h>
#include <stateless/cp/trex_stateless.h>
#include <sim/trex_sim.h>

using namespace std;

// An enum for all the option types
enum { OPT_HELP, OPT_CFG, OPT_NODE_DUMP, OP_STATS,
       OPT_FILE_OUT, OPT_UT, OPT_PCAP, OPT_IPV6, OPT_MAC_FILE,
       OPT_SL, OPT_DP_CORE_COUNT, OPT_DP_CORE_INDEX, OPT_LIMIT,
       OPT_DRY_RUN};



/**
 * type of run 
 * GTEST 
 * Stateful 
 * Stateless 
 */
typedef enum {
    OPT_TYPE_GTEST = 7,
    OPT_TYPE_SF,
    OPT_TYPE_SL
} opt_type_e;


/* these are the argument types:
   SO_NONE --    no argument needed
   SO_REQ_SEP -- single required argument
   SO_MULTI --   multiple arguments needed
*/
static CSimpleOpt::SOption parser_options[] =
{
    { OPT_HELP,          "-?",           SO_NONE    },
    { OPT_HELP,          "-h",           SO_NONE    },
    { OPT_HELP,          "--help",       SO_NONE    },
    { OPT_UT,            "--ut",         SO_NONE    },
    { OP_STATS,          "-s",           SO_NONE    },
    { OPT_CFG,           "-f",           SO_REQ_SEP },
    { OPT_MAC_FILE,      "--mac",        SO_REQ_SEP },
    { OPT_FILE_OUT ,     "-o",           SO_REQ_SEP },
    { OPT_NODE_DUMP ,    "-v",           SO_REQ_SEP },
    { OPT_PCAP,          "--pcap",       SO_NONE    },
    { OPT_IPV6,          "--ipv6",       SO_NONE    },
    { OPT_SL,            "--sl",         SO_NONE    },
    { OPT_DP_CORE_COUNT, "--cores",      SO_REQ_SEP },
    { OPT_DP_CORE_INDEX, "--core_index", SO_REQ_SEP },
    { OPT_LIMIT,         "--limit",      SO_REQ_SEP },
    { OPT_DRY_RUN,       "--dry",      SO_NONE },

    
    SO_END_OF_OPTIONS
};

static int usage(){

    printf(" Usage: bp_sim [OPTION] -f cfg.yaml -o outfile.erf   \n");
    printf(" \n");
    printf(" \n");
    printf(" options \n");
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

            case OPT_CFG:
                po->cfg_file = args.OptionArg();
                break;

            case OPT_MAC_FILE:
                po->mac_file = args.OptionArg();
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

            case OPT_PCAP:
                po->preview.set_pcap_mode_enable(true);
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

            case OPT_DRY_RUN:
                params["dry"] = 1;
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
     

    if ((po->cfg_file =="") ) {
         printf("Invalid combination of parameters you must add -f with configuration file \n");
         usage();
         return -1;
     }

    if ( node_dump ){
        po->preview.setVMode(a);
    }else{
        if  (po->out_file=="" ){
         printf("Invalid combination of parameters you must give output file iwth -o  \n");
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


int main(int argc , char * argv[]){

    std::unordered_map<std::string, int> params;

    if ( parse_options(argc, argv, &CGlobalInfo::m_options , params) != 0) {
        exit(-1);
    }

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
            return sf.run();
        }

    case OPT_TYPE_SL:
        {
            SimStateless &st = SimStateless::get_instance();

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



