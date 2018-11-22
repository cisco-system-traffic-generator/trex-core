#ifndef TREX_DPDK_MODES_H
#define TREX_DPDK_MODES_H
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


/* 

 **DPDK mode of operations**

tmONE_QUE : 
   1. one dual ports 
   2. one DP core  - Tx 0 
   2. one RX core  - Rx 0 (all traffic to here, filter by hardware) 
                     Tx latency --> redirect to DP core to send 


tmMULTI_QUE:

   1. more than one DP core per dual interfaces   - Txq (0,1,2,3,4 )
                                                    Rxq (0,1,2,3,4 )
   
   2. one RX core  - get latency packets from DP->RX message queues 
                     send packets to DP (messaging) .. this is working like ONE_QUE and can optimized 
                     (can seperate the rx queue logic from tx queue logic) -- but this is not the main issue now

   3. no need for HW filters, no need for RSS. any rx distribtion will work 

   4. STL and ASTF* can work in this mode 
   (*FOR ASTF in case of a determesitic distribution need to change the flow context location once)

   5. latency won't be accurate in this mode -- both latency Tx and Rx are indirect
   

tmDROP_QUE_FILTER:
    
   1. more than one DP core per dual interfaces   - Txq (0,1,2,3,4 )
                                                    Rxq (0 = DROP queue,1 -latency  )
                                                    
   2. need to configure HW filter to pass latency packets (TOS& 0x01==0x01) to RXQ=1
   
   2. one RX core  - get latency packets directly from RX1

   3. accurate latency 

   4. STF/STL can work in this mode

   5. need service mode to pass all traffic to Rx 
   


tmRSS_DROP_QUE_FILTER:     

   in case on one DP core per dual:

   1. one DP core per dual interfaces   - Txq (0 )
                                          Rxq (0) - DP

                                          Rxq (1) RX core-- latency Filter by hardware  
   

   in case on multi ports per DP core :
                     
   1. more than one DP core per dual interfaces   - Txq (0,1,2,3,4 )
                                                    Rxq (0,2,3,4 ) - DP
                                                    Rxq (1) RX core-- latency Filter by hardware  
                                                    
   2. need to configure HW filter to pass latency packets (TOS& 0x01==0x01) to RXQ=1
   
   2. one RX core  - get latency packets directly from RX1

   3. accurate latency 

   4. ASTF can work in this mode

   5. redirect any non-TCP/UDP to Rx core (latency directly to this mode 


Queue mode               |  hw_filter to rx |  is_dp_latency   |  latency rx/tx             |
-----------------------------------------------------------------------------------------------
tmONE_QUE                |   false          |  false           |  message to/from DP        |
tmMULTI_QUE              |   false          |  false           |  message to/from DP        |
tmDROP_QUE_FILTER        |   true           |   STL=true?false | filter/queue               |
mRSS_DROP_QUE_FILTER     |   true           |  false           | filter/queue +             |
                         |                  |                  |  redirect message to RX 
                                                                  from DP
                         



    
   
**DPDK mode of operations**


tdCAP_ONE_QUE:
 driver supports only one queue

tdCAP_MULTI_QUE:

 driver supports multi queue Rx and Tx


tdCAP_DROP_QUE_FILTER:
  driver supports multi queue Rx and Tx and filter 

tdCAP_RSS_DROP_QUE_FILTER:

  driver supports multi queue Rx and Tx  filter and RSS full API 
        
*/ 

#include <stdint.h>
#include <vector>
#include <assert.h>
#include <stdio.h>


enum {
    MAIN_DPDK_DROP_Q = 0, /* first rx queue for drop packets, in case HW filter works */
    MAIN_DPDK_RX_Q = 1,   /* latency rx queue in case HW filter works */
};

class CTrexDpdkParams {
 public:
    uint16_t rx_data_q_num; /* number of rx queues for latency, 1 or 0, in case it is 1 it is always MAIN_DPDK_RX_Q  */
    uint16_t rx_drop_q_num; /* number of rx queues for drop  */
    uint16_t rx_dp_q_num;   /* number of rx queues for DP cores */

    uint16_t rx_desc_num_data_q;
    uint16_t rx_desc_num_drop_q;
    uint16_t rx_desc_num_dp_q;   /* number of descriptor for dp rx queues */

    uint16_t tx_desc_num;
    uint16_t rx_mbuf_type;
public:
    void dump(FILE *fd);
public:
    uint16_t get_total_rx_queues(){
        return (rx_data_q_num +rx_drop_q_num +rx_dp_q_num);
    }

    uint32_t get_total_rx_desc(){
        return ( (rx_data_q_num * rx_desc_num_data_q) + 
                 (rx_dp_q_num  * rx_desc_num_dp_q) +
                 (rx_drop_q_num * rx_desc_num_drop_q) );
    }

};



/* RX packet distribution model */

typedef enum { ddRX_DIST_NONE=0,                 /* no need to distribute as there is one rx queue */

               ddRX_DIST_ASTF_HARDWARE_RSS,      /*  server side will use RSS with full key, client side will use special 
                                                     RSS key and RETA in such way that generate tuple will be forward back to sender  */

               ddRX_DIST_BEST_EFFORT,           /* any distribtion */

               ddRX_DIST_FLOW_BASED              /* any as long as determenistic flow distribtion, one flow to one core */
        
             } trex_dpdk_rx_distro_mode_t; 


/* capability of the drivers */
typedef enum { tdNONE = 0x00,
               tdCAP_ONE_QUE         = 0x01,     /* driver support only one TX and one RX queue */

               tdCAP_MULTI_QUE       = 0x02,      /* driver support multi TX and multi RX queue -
                                                    - distribution to RX could be random */

               tdCAP_DROP_QUE_FILTER = 0x04, /* driver support multi TX and multi RX queue, 
                                                and support DROP QUEUE and TOS filter for specific queue*/
        
               tdCAP_RSS_DROP_QUE_FILTER = 0x08, /* driver support multi TX and multi RX queue, 
                                                   DROP queue and filter and support full RSS API for IPv6,IPV4 for */

               tdCAP_ALL = tdCAP_RSS_DROP_QUE_FILTER | tdCAP_DROP_QUE_FILTER | tdCAP_MULTI_QUE | tdCAP_ONE_QUE,

               tdCAP_ALL_NO_RSS =  tdCAP_DROP_QUE_FILTER | tdCAP_MULTI_QUE | tdCAP_ONE_QUE

             } trex_driver_cap_t;


/*

DPDK mode of operation from filter, tx/rx queues model, rss configration 

*/

typedef enum { tmONE_QUE=0,       /* driver support only one TX and one RX queue */

               tmMULTI_QUE,      /* driver support multi TX and multi RX queue -
                                    - distribution to RX could be random */

               tmDROP_QUE_FILTER,      /* driver support multi TX and multi RX queue, 
                                         and support DROP QUEUE and TOS filter for specific queue*/

               tmRSS_DROP_QUE_FILTER, /* driver support multi TX and multi RX queue, 
                                            DROP queue and filter and support full RSS API for IPv6,IPV4 for */
               tmDPDK_MODES,

               tmDPDK_UNSUPPORTED
        
             } trex_dpdk_mode_t; 


typedef enum { 
                OP_MODE_INVALID,
                OP_MODE_STF,
                OP_MODE_STL,
                OP_MODE_ASTF,
                OP_MODE_ASTF_BATCH,
                OP_MODE_DUMP_INTERFACES
             } trex_traffic_mode_t; 

typedef std::vector<int>  rx_que_desc_t;


class CDpdkMode;

class CDpdkModeBase {
public:
    virtual ~CDpdkModeBase(){
    }
    /* rx queue 0 is drop queue */
    virtual bool is_drop_rx_queue_needed()=0; 

    /* do we need hardware filter? in case we won't get hardware filter it is asumed the software will get 
      all the packets, in case we need hardware filter, latency will go to RX_QUEUE 1 */
    virtual bool is_hardware_filter_needed()=0;

    /* return the distribtion model */
    virtual trex_dpdk_rx_distro_mode_t get_rx_distro_mode()=0;

    /* get rx queue, per each dp, in case there is rx disto !- NONE */
    virtual uint8_t get_dp_rx_queues(uint8_t core_index)=0;

    /* there is one tx queue and on rx queue per port, it is not posible to open mode queues  */
    virtual bool is_one_tx_rx_queue()=0;

    /* will get the result from messaging, do not read from ANY rx queue. 
      in case of ASTF there is no need to read from RX queue when there is one queue  */
    virtual bool is_rx_core_read_from_queue()=0;

    /* do we have a special queue for latency in dp 
       e.g stateless is using this in case of hardwre drop mode. in all other case it should be zero  */
    virtual bool is_dp_latency_tx_queue()=0;

    /* number of rx queues in DP, in case of oneQ env should return 0 */
    virtual uint16_t dp_rx_queues()=0;


public:
    /* accurate latency **/
    bool is_accurate_latency(){
        return(is_hardware_filter_needed());
    }

    void set_info(CDpdkMode * mode){
        m_mode =mode ;
    }

protected:
    CDpdkMode * m_mode;
};


/* could be supported on all modes */
class CDpdkMode_ONE_QUE : public CDpdkModeBase {

public:
      static CDpdkModeBase * create(){
          return ( new CDpdkMode_ONE_QUE() );
      }

      virtual bool is_drop_rx_queue_needed(){
          return (false);
      }

      virtual bool is_hardware_filter_needed(){
          return (false);
      }

      virtual trex_dpdk_rx_distro_mode_t get_rx_distro_mode(){
          return (ddRX_DIST_NONE);
      }

      virtual uint8_t get_dp_rx_queues(uint8_t core_index){
          return(0);
      }

      virtual bool is_one_tx_rx_queue(){
          return (true);
      }

      virtual bool is_rx_core_read_from_queue();

      virtual bool is_dp_latency_tx_queue();

      virtual uint16_t dp_rx_queues(){
          /* this is exception, 
          in case of ASTF the Rx is used by DP */
          return(0);
      }
};


class CDpdkMode_MULTI_QUE : public CDpdkModeBase {

public:
      static CDpdkModeBase * create(){
          return ( new CDpdkMode_MULTI_QUE() );
      }

      virtual bool is_drop_rx_queue_needed(){
          return (false);
      }

      virtual bool is_hardware_filter_needed(){
          return (false);
      }

      virtual trex_dpdk_rx_distro_mode_t get_rx_distro_mode(){
          return (ddRX_DIST_BEST_EFFORT);
      }

      virtual uint8_t get_dp_rx_queues(uint8_t core_index){
          return(core_index);
      }

      virtual bool is_one_tx_rx_queue(){
          return (false);
      }

      virtual bool is_rx_core_read_from_queue(){
          return (false);
      }

      virtual bool is_dp_latency_tx_queue(){
          return (false);
      }

      virtual uint16_t dp_rx_queues();
};


class CDpdkMode_DROP_QUE_FILTER : public CDpdkModeBase {

public:
    static CDpdkModeBase * create(){
        return ( new CDpdkMode_DROP_QUE_FILTER() );
    }

    virtual bool is_drop_rx_queue_needed(){
        return (true);
    }

    virtual bool is_hardware_filter_needed(){
        return (true);
    }

    virtual trex_dpdk_rx_distro_mode_t get_rx_distro_mode(){
        return (ddRX_DIST_NONE);
    }

    virtual uint8_t get_dp_rx_queues(uint8_t core_index){
        return(0);
    }

    virtual bool is_one_tx_rx_queue(){
        return (false);
    }

    virtual bool is_rx_core_read_from_queue(){
        return (true);
    }

    virtual bool is_dp_latency_tx_queue();

    virtual uint16_t dp_rx_queues(){
        return (0);
    }
};

class CDpdkMode_RSS_DROP_QUE_FILTER : public CDpdkModeBase {

public:
    static CDpdkModeBase * create(){
        return ( new CDpdkMode_RSS_DROP_QUE_FILTER() );
    }

    virtual bool is_drop_rx_queue_needed(){
        return (false);
    }

    virtual bool is_hardware_filter_needed(){
        return (true);
    }

    virtual trex_dpdk_rx_distro_mode_t get_rx_distro_mode();

    virtual uint8_t get_dp_rx_queues(uint8_t core_index){
        if (core_index==0) {
            return(0);
        }
        return(core_index+1);
        /*0,2,3,4*/
    }

    virtual bool is_one_tx_rx_queue(){
        return (false);
    }

    virtual bool is_rx_core_read_from_queue(){
        return (true);
    }

    virtual bool is_dp_latency_tx_queue(){
        return (false);
    }

    virtual uint16_t dp_rx_queues();
};



class CDpdkMode {

public:
   CDpdkMode(){
       m_dpdk_obj =0;
       m_ttm_mode = OP_MODE_INVALID;
       m_force_sw_mode = false;
   }
   ~CDpdkMode(){
       if (m_dpdk_obj){
           delete m_dpdk_obj;
       }
   }

   int choose_mode(uint32_t cap);

   CDpdkModeBase * get_mode(){
       assert(m_dpdk_obj);
       return m_dpdk_obj;
   }

   void force_software_mode(bool sw_mode){
       m_force_sw_mode = sw_mode;
   }
   bool get_force_sw_mode(){
       return (m_force_sw_mode);
   }

   void set_opt_mode(trex_traffic_mode_t mode){
       m_ttm_mode = mode;
   }
   trex_traffic_mode_t get_opt_mode() {
       return(m_ttm_mode);
   }
   bool is_interactive();
   bool is_astf_mode();

   void get_dpdk_drv_params(CTrexDpdkParams &p);

public:
    void switch_mode_debug(trex_traffic_mode_t mode);
private:
    void set_dpdk_mode(trex_dpdk_mode_t  dpdk_mode);
    trex_driver_cap_t get_cap_for_mode(trex_dpdk_mode_t dpdk_mode,bool one_core);

private:
   bool                 m_force_sw_mode;
   CDpdkModeBase  *     m_dpdk_obj;
   trex_traffic_mode_t  m_ttm_mode;
   trex_dpdk_mode_t     m_dpdk_mode;
   trex_driver_cap_t    m_drv_cap;
};

struct choose_mode_priorty_t {
    trex_traffic_mode_t m_mode;
    bool                m_one_core;
    std::vector<int>    m_pm;
};


typedef CDpdkModeBase * (*create_mode_cb_t)(void);


#endif
