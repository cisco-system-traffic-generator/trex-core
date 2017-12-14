/*
 Itay Marom
 Hanoch Haim
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
#ifndef __TREX_STL_DP_CORE_H__
#define __TREX_STL_DP_CORE_H__

#include <vector>

#include "msg_manager.h"
#include "pal_utl.h"

#include "trex_dp_core.h"

class TrexCpToDpMsgBase;
class TrexStatelessDpStart;
class CFlowGenListPerThread;
class CGenNodeStateless;
class TrexStreamsCompiledObj;
class TrexStream;
class CGenNodePCAP;
class ServiceModeWrapper;

class CDpOneStream  {
public:
    void Create(){
    }

    void Delete(CFlowGenListPerThread   * core);
    void DeleteOnlyStream();

    CGenNodeStateless * m_node;      // schedule node
    TrexStream *        m_dp_stream; // stream info 
};

class TrexStatelessDpPerPort {

public:
        /* states */
    enum state_e {
        ppSTATE_IDLE,
        ppSTATE_TRANSMITTING,
        ppSTATE_PAUSE,
        ppSTATE_PCAP_TX,

    };

public:
    TrexStatelessDpPerPort(){
    }

    void create(CFlowGenListPerThread   *  core);

    bool pause_traffic(uint8_t port_id);
    bool pause_streams(uint8_t port_id, stream_ids_t &stream_ids); // filter by stream IDs, slower

    bool resume_traffic(uint8_t port_id);
    bool resume_streams(uint8_t port_id, stream_ids_t &stream_ids); // filter by stream IDs, slower

    bool update_traffic(uint8_t port_id, double factor);
    bool update_streams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream); // filter by stream IDs, slower

    bool push_pcap(uint8_t port_id,
                   const std::string &pcap_filename,
                   double ipg_usec,
                   double min_ipg_sec,
                   double speedup,
                   uint32_t count,
                   bool is_dual);

    bool stop_traffic(uint8_t port_id,
                      bool stop_on_id, 
                      int event_id);

    bool update_number_of_active_streams(uint32_t d);

    state_e get_state() const {
        return m_state;
    }

    bool is_active() const {
        return (get_state() != ppSTATE_IDLE);
    }

    void set_event_id(int event_id) {
        m_event_id = event_id;
    }

    int get_event_id() {
        return m_event_id;
    }

public:

    state_e                   m_state;

    uint32_t                  m_active_streams; /* how many active streams on this port  */
                                                
    std::vector<CDpOneStream> m_active_nodes;   /* holds the current active nodes */
    CGenNodePCAP              *m_active_pcap_node;
    CFlowGenListPerThread   *  m_core ;
    int                        m_event_id;
};

/* for now */
#define NUM_PORTS_PER_CORE 2


class TrexStatelessDpCore : public TrexDpCore {

public:

    #define SCHD_OFFSET_DTIME  (100.0/1000000.0)
 
    TrexStatelessDpCore(uint8_t thread_id, CFlowGenListPerThread *core);
    
    ~TrexStatelessDpCore();
    
    
    /**
     * dummy traffic creator
     * 
     * @author imarom (27-Oct-15)
     * 
     * @param pkt 
     * @param pkt_len 
     */
    void start_traffic(TrexStreamsCompiledObj *obj, 
                       double duration,
                       int event_id,
                       double start_at_ts);


    /* pause the streams, work only if all are continues  */
    void pause_traffic(uint8_t port_id);
    void pause_streams(uint8_t port_id, stream_ids_t &stream_ids);



    void resume_traffic(uint8_t port_id);
    void resume_streams(uint8_t port_id, stream_ids_t &stream_ids);


    /**
     * push a PCAP file on port
     * 
     */
    void push_pcap(uint8_t port_id,
                   int event_id,
                   const std::string &pcap_filename,
                   double ipg_usec,
                   double min_ipg_sec,
                   double speedup,
                   uint32_t count,
                   double duration,
                   bool   is_dual);


    /**
     * update current traffic rate
     * 
     * @author imarom (25-Nov-15)
     * 
     */
    void update_traffic(uint8_t port_id, double factor);
    void update_streams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream);

    /**
     * 
     * stop all traffic for this core
     * 
     */
    void stop_traffic(uint8_t port_id,bool stop_on_id, int event_id);


    /* return if all ports are idle */
    bool are_all_ports_idle();

  

    bool set_stateless_next_node(CGenNodeStateless * cur_node,
                                 CGenNodeStateless * next_node);


    TrexStatelessDpPerPort * get_port_db(uint8_t port_id) {
        assert((m_local_port_offset==port_id) ||(m_local_port_offset+1==port_id));
        uint8_t local_port_id = port_id -m_local_port_offset;
        assert(local_port_id<NUM_PORTS_PER_CORE);
        return (&m_ports[local_port_id]);
    }
        
    bool is_port_active(uint8_t port_id) {
        return get_port_db(port_id)->is_active();
    }

    /**
     * enabled/disable service mode
     */
    void set_service_mode(uint8_t port_id, bool enabled);

private:

    /**
     * real job is done when scheduler is launched
     * 
     */
    void start_scheduler();


    void add_port_duration(double duration,
                           uint8_t port_id,
                           int event_id);

    void update_mac_addr(TrexStream * stream,
                         CGenNodeStateless *node,
                         uint8_t dir,
                         char *raw_pkt);

    void add_stream(TrexStatelessDpPerPort * lp_port,
                    TrexStream * stream,
                    TrexStreamsCompiledObj *comp,
                    double start_at_ts = 0);


    void replay_vm_into_cache(TrexStream * stream, 
                              CGenNodeStateless *node);


    
    uint8_t                    m_local_port_offset;

    TrexStatelessDpPerPort     m_ports[NUM_PORTS_PER_CORE]; 

    double                     m_duration;
    
    ServiceModeWrapper        *m_wrapper;
    bool                       m_is_service_mode;
};

#endif /* __TREX_STL_DP_CORE_H__ */

