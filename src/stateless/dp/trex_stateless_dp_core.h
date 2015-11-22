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
#ifndef __TREX_STATELESS_DP_CORE_H__
#define __TREX_STATELESS_DP_CORE_H__

#include <vector>

#include <msg_manager.h>
#include <pal_utl.h>

class TrexStatelessCpToDpMsgBase;
class TrexStatelessDpStart;
class CFlowGenListPerThread;
class CGenNodeStateless;
class TrexStreamsCompiledObj;
class TrexStream;


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
        ppSTATE_PAUSE

    };

public:
    TrexStatelessDpPerPort(){
    }

    void create(CFlowGenListPerThread   *  core);

    bool pause_traffic(uint8_t port_id);

    bool resume_traffic(uint8_t port_id);

    bool stop_traffic(uint8_t port_id,
                      bool stop_on_id, 
                      int event_id);

    bool update_number_of_active_streams(uint32_t d);

    state_e get_state() {
        return m_state;
    }

    void set_event_id(int event_id) {
        m_event_id = event_id;
    }

    int get_event_id() {
        return m_event_id;
    }

public:

    state_e                   m_state;
    uint8_t                   m_port_id;

    uint32_t                  m_active_streams; /* how many active streams on this port  */
                                                
    std::vector<CDpOneStream> m_active_nodes;   /* holds the current active nodes */
    CFlowGenListPerThread   *  m_core ;
    int                        m_event_id;
};

/* for now */
#define NUM_PORTS_PER_CORE 2

class TrexStatelessDpCore {

public:

    /* states */
    enum state_e {
        STATE_IDLE,
        STATE_TRANSMITTING,
        STATE_TERMINATE

    };

    TrexStatelessDpCore() {
        m_thread_id = 0;
        m_core = NULL;
        m_duration = -1;
    }

    /**
     * "static constructor"
     * 
     * @param thread_id 
     * @param core 
     */
    void create(uint8_t thread_id, CFlowGenListPerThread *core);

    /**
     * launch the stateless DP core code
     * 
     */
    void start();


    /* exit after batch of commands */
    void run_once();

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
                       int m_event_id);


    /* pause the streams, work only if all are continues  */
    void pause_traffic(uint8_t port_id);



    void resume_traffic(uint8_t port_id);


    /**
     * 
     * stop all traffic for this core
     * 
     */
    void stop_traffic(uint8_t port_id,bool stop_on_id, int event_id);


    /* return if all ports are idel */
    bool are_all_ports_idle();


    /**
     * check for and handle messages from CP
     * 
     * @author imarom (27-Oct-15)
     */
    void periodic_check_for_cp_messages() {
        // doing this inline for performance reasons

        /* fast path */
        if ( likely ( m_ring_from_cp->isEmpty() ) ) {
            return;
        }

        while ( true ) {
            CGenNode * node = NULL;
            if (m_ring_from_cp->Dequeue(node) != 0) {
                break;
            }
            assert(node);

            TrexStatelessCpToDpMsgBase * msg = (TrexStatelessCpToDpMsgBase *)node;
            handle_cp_msg(msg);
        }

    }

    /* quit the main loop, work in both stateless in stateful, don't free memory trigger from  master  */
    void quit_main_loop();

    state_e get_state() {
        return m_state;
    }

    bool set_stateless_next_node(CGenNodeStateless * cur_node,
                                 CGenNodeStateless * next_node);


    TrexStatelessDpPerPort * get_port_db(uint8_t port_id){
        assert((m_local_port_offset==port_id) ||(m_local_port_offset+1==port_id));
        uint8_t local_port_id = port_id -m_local_port_offset;
        assert(local_port_id<NUM_PORTS_PER_CORE);
        return (&m_ports[local_port_id]);
    }
        


private:

    void schedule_exit();


    /**
     * in idle state loop, the processor most of the time sleeps 
     * and periodically checks for messages 
     * 
     */
    void idle_state_loop();

    /**
     * real job is done when scheduler is launched
     * 
     */
    void start_scheduler();

    /**
     * handles a CP to DP message
     * 
     * @author imarom (27-Oct-15)
     * 
     * @param msg 
     */
    void handle_cp_msg(TrexStatelessCpToDpMsgBase *msg);


    void add_port_duration(double duration,
                           uint8_t port_id,
                           int event_id);

    void add_global_duration(double duration);

    void add_cont_stream(TrexStatelessDpPerPort * lp_port,
                         TrexStream * stream,
                         TrexStreamsCompiledObj *comp);

    uint8_t              m_thread_id;
    uint8_t              m_local_port_offset;

    state_e              m_state; /* state of all ports */
    CNodeRing           *m_ring_from_cp;
    CNodeRing           *m_ring_to_cp;

    TrexStatelessDpPerPort   m_ports[NUM_PORTS_PER_CORE]; 

    /* pointer to the main object */
    CFlowGenListPerThread   * m_core;

    double                 m_duration;
};

#endif /* __TREX_STATELESS_DP_CORE_H__ */

