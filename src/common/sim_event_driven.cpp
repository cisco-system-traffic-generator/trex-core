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

#include "sim_event_driven.h"
#include <stdio.h>
#include <stdint.h>



void CSimEventDriven::remove_all(){

    CSimEventBase *event;
    while (!m_p_queue.empty()) {
        event = m_p_queue.top();
        m_p_queue.pop();
        /* sanity check */
        delete event;
    }
}


bool CSimEventDriven::run_sim(){

    CSimEventBase *event=NULL;
    while (!m_p_queue.empty()) {
        event = m_p_queue.top();
        m_p_queue.pop();
        m_c_time =event->m_time;

        bool reschedule=false;
        if ( m_verbose ){
            fprintf(stdout," event at time %.2f \n",event->m_time);
        }
        if ( event->on_event(this,reschedule) ){
            delete event;
            break;
        }

        if (reschedule){
            m_p_queue.push(event);
        }else{
            delete event;
        }
    }
    remove_all();
    return (true);
}



class CSimEventStop : public CSimEventBase {

public:
     CSimEventStop(double time){
         m_time =time;
     }
     virtual bool on_event(CSimEventDriven *sched,
                           bool & reschedule){
         reschedule=false;
         return(true);
     }
};


class CSimEventDump : public CSimEventBase {

public:
    CSimEventDump(uint32_t  id,
                  uint32_t  cnt,
                  double    start_time,
                  double    d_time){
        m_id = id;
        m_cnt = cnt;
        m_time = start_time;
        m_dtime = d_time;
    }

    virtual bool on_event(CSimEventDriven *sched,
                          bool & reschedule){

        printf(" event id:%lu  cnt:%lu %.2f  \n",(ulong)m_id, (ulong)m_cnt, m_time);
        reschedule=false;
        if (m_cnt==0) {
            return (false);
        }
        m_cnt--;
        m_time += m_dtime;
        reschedule=true;
        return(false);
    }

private:
    double   m_dtime;
    uint32_t m_id;
    uint32_t m_cnt;
};



int event_driven_sim_test(){

    CSimEventDriven sim;

    /* should create 10 ticks */
    sim.add_event(new CSimEventDump(1,10,0.1,0.5));
    sim.add_event(new CSimEventDump(2,20,0.6,0.2));
    sim.add_event(new CSimEventStop(10.0));
    sim.set_verbose(true);
    sim.run_sim();
    return(0);
}


