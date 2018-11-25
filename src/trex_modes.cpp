#include "trex_modes.h"

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

#include "trex_modes.h"
#include <drivers/trex_driver_defines.h>
#include "trex_global.h"
#include <string>


void CTrexDpdkParams::dump(FILE *fd){

    fprintf(fd," rx_data_q_num : %d \n",(int)rx_data_q_num);
    fprintf(fd," rx_drop_q_num : %d \n",(int)rx_drop_q_num);
    fprintf(fd," rx_dp_q_num   : %d \n",(int)rx_dp_q_num);
    fprintf(fd," rx_que_total : %d \n",get_total_rx_queues());
    fprintf(fd," --   \n");

    fprintf(fd," rx_desc_num_data_q   : %d \n",(int)rx_desc_num_data_q);
    fprintf(fd," rx_desc_num_drop_q   : %d \n",(int)rx_desc_num_drop_q);
    fprintf(fd," rx_desc_num_dp_q     : %d \n",(int)rx_desc_num_dp_q);
    fprintf(fd," total_desc           : %d \n",(int)get_total_rx_desc());
    
    fprintf(fd," --   \n");

    fprintf(fd," tx_desc_num     : %d \n",(int)tx_desc_num);
}

uint16_t CDpdkMode_ONE_QUE::total_tx_queues(){
    return(1);
}


bool CDpdkMode_ONE_QUE::is_rx_core_read_from_queue(){
   return (m_mode->is_astf_mode()?false:true);
}

bool CDpdkMode_ONE_QUE::is_dp_latency_tx_queue(){
  return (false);
}

uint16_t CDpdkMode_MULTI_QUE::total_tx_queues(){
    return (CGlobalInfo::m_options.preview.getCores());
}

uint16_t CDpdkMode_MULTI_QUE::dp_rx_queues(){
    return (CGlobalInfo::m_options.preview.getCores());
}

bool CDpdkMode_DROP_QUE_FILTER::is_dp_latency_tx_queue(){
    return ((m_mode->get_opt_mode()==OP_MODE_STL) ?true:false);
}

uint16_t CDpdkMode_DROP_QUE_FILTER::total_tx_queues(){
    return (CGlobalInfo::m_options.preview.getCores()+2);
}

uint16_t CDpdkMode_RSS_DROP_QUE_FILTER::total_tx_queues(){
    return (CGlobalInfo::m_options.preview.getCores()+2);
}

trex_dpdk_rx_distro_mode_t CDpdkMode_RSS_DROP_QUE_FILTER::get_rx_distro_mode(){
    if (CGlobalInfo::m_options.preview.getCores()>1) {
        return (ddRX_DIST_ASTF_HARDWARE_RSS);
    }else{
        return(ddRX_DIST_NONE);
    }
}

uint16_t CDpdkMode_RSS_DROP_QUE_FILTER::dp_rx_queues(){
    return (CGlobalInfo::m_options.preview.getCores());
}


void CDpdkMode::set_dpdk_mode(trex_dpdk_mode_t  dpdk_mode){

    if (m_dpdk_obj){
        delete m_dpdk_obj;
        m_dpdk_obj=0;
    }
    create_mode_cb_t cbs[] = {
        CDpdkMode_ONE_QUE::create,
        CDpdkMode_MULTI_QUE::create,
        CDpdkMode_DROP_QUE_FILTER::create,
        CDpdkMode_RSS_DROP_QUE_FILTER::create
    };
    
    m_dpdk_mode = dpdk_mode;
    create_mode_cb_t cb=cbs[dpdk_mode];
    m_dpdk_obj = cb();
    /* set back pointer */
    m_dpdk_obj->set_info(this);
}

bool CDpdkMode::is_interactive(){
    if ( (m_ttm_mode == OP_MODE_STF) ||
         (m_ttm_mode == OP_MODE_ASTF_BATCH)){
        return(false);
    }
    return (true);
}


/* switch mode for testing */
void CDpdkMode::switch_mode_debug(trex_traffic_mode_t mode){
    if (m_ttm_mode !=mode) {
        m_ttm_mode = mode;
        choose_mode(tdCAP_ONE_QUE);
    }
}


bool CDpdkMode::is_astf_mode(){
    if ( (m_ttm_mode == OP_MODE_ASTF) ||
         (m_ttm_mode == OP_MODE_ASTF_BATCH)){
        return(true);
    }
    return (false);
}


void CDpdkMode::get_dpdk_drv_params(CTrexDpdkParams &p){
    CDpdkModeBase * mode= get_mode();

    if ( mode->is_hardware_filter_needed() ||
         mode->is_one_tx_rx_queue() ) {
        p.rx_data_q_num = 1;
    }else{
        p.rx_data_q_num = 0;
    }

    if (mode->is_drop_rx_queue_needed()){
        p.rx_drop_q_num = 1;
        p.rx_desc_num_drop_q = RX_DESC_NUM_DROP_Q;
    }else{
       p.rx_drop_q_num = 0;
       p.rx_desc_num_drop_q = RX_DESC_NUM_DATA_Q;
    }

    p.rx_dp_q_num =  mode->dp_rx_queues();

    p.rx_desc_num_data_q = RX_DESC_NUM_DATA_Q;
    if (p.rx_dp_q_num>0) {
         p.rx_desc_num_dp_q = RX_DESC_NUM_DATA_Q;
    }else{
         p.rx_desc_num_dp_q = 0;
    }
    p.tx_desc_num = TX_DESC_NUM;
    /* TBD need to ask driver capability for scatter gather */
    p.rx_mbuf_type = MBUF_2048;
}


trex_driver_cap_t CDpdkMode::get_cap_for_mode(trex_dpdk_mode_t dpdk_mode,bool one_core){

   const trex_driver_cap_t m[tmDPDK_MODES]={ tdCAP_ONE_QUE,
                                             tdCAP_MULTI_QUE,
                                             tdCAP_DROP_QUE_FILTER,
                                             one_core?tdCAP_DROP_QUE_FILTER:
                                             tdCAP_RSS_DROP_QUE_FILTER }; /* In case of one core, only filter cap is needed*/

 const trex_driver_cap_t sw[tmDPDK_MODES]={ tdCAP_ONE_QUE,
                                            tdCAP_MULTI_QUE,
                                            tdNONE,
                                            tdNONE };

  const trex_driver_cap_t *t;
  t=m_force_sw_mode?sw:m;
  return (t[dpdk_mode]);
}

std::string get_str_mode(trex_dpdk_mode_t que_mode){

  std::string str[]= {"ONE_QUE",
                      "MULTI_QUE",     
                      "DROP_QUE_FILTER",      
                      "RSS_DROP_QUE_FILTER"};

  return (str[que_mode]);
}

std::string 
get_str_trex_traffic_mode(trex_traffic_mode_t ttm_mode){

    std::string str[] = { "INVALID",
                          "STF",
                          "STL",
                          "ASTF",
                          "STF_BATCH",
                          "DUMP_INTERFACES" };

   return (str[ttm_mode]);
}


int CDpdkMode::choose_mode(uint32_t cap){

    int dp_cores = CGlobalInfo::m_options.preview.getCores();
    m_drv_cap = (trex_driver_cap_t)cap;

    std::vector<choose_mode_priorty_t>  modes;

    /* valid options in each traffic mode */
    modes.push_back( {OP_MODE_STF, true,  { tmDROP_QUE_FILTER,tmONE_QUE } } );
    modes.push_back( {OP_MODE_STF, false, { tmDROP_QUE_FILTER } } );

    modes.push_back( {OP_MODE_STL, true,  { tmDROP_QUE_FILTER,tmONE_QUE } } );
    modes.push_back( {OP_MODE_STL, false, { tmDROP_QUE_FILTER,tmMULTI_QUE } } );

    modes.push_back( {OP_MODE_ASTF_BATCH, true,  { tmRSS_DROP_QUE_FILTER,tmONE_QUE } } ); /* only filter is needed here */
    modes.push_back( {OP_MODE_ASTF_BATCH, false, { tmRSS_DROP_QUE_FILTER } } );

    modes.push_back( {OP_MODE_ASTF, true,  { tmRSS_DROP_QUE_FILTER,tmONE_QUE } } ); 
    modes.push_back( {OP_MODE_ASTF, false, { tmRSS_DROP_QUE_FILTER,tmMULTI_QUE } } );

    bool one_core =  (dp_cores==1?true:false);

    int i;
    int j;
    for (i=0; i<modes.size(); i++) {
        choose_mode_priorty_t * lp=&modes[i];
        if ( (lp->m_one_core == one_core) && 
             (lp->m_mode == m_ttm_mode)) {
            for (j=0;j<lp->m_pm.size(); j++) {
                trex_dpdk_mode_t dpdk_mode = (trex_dpdk_mode_t)lp->m_pm[j];
                if ( get_cap_for_mode(dpdk_mode,one_core) & cap ){
                    printf(" set dpdk queues mode to %s \n",get_str_mode(dpdk_mode).c_str());
                    set_dpdk_mode(dpdk_mode);
                    return(dpdk_mode);
                }
            }
        }
    }
    /* can't be supported */
    printf(" %s mode does not support more than one core by dual interfaces\n", 
           get_str_trex_traffic_mode(m_ttm_mode).c_str()); 
    printf("try with setting the number of cores to 1 (-c 1)\n");
    return(tmDPDK_UNSUPPORTED);
}



