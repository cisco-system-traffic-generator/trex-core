#ifndef SIM_EVENT_DRIVEN_H
#define SIM_EVENT_DRIVEN_H
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


#include <queue>
#include <algorithm>


class CSimEventDriven ;

class CSimEventBase {
public:
    double   m_time;    
public:
    virtual ~CSimEventBase(){
    };

    /* do somthing return true for quit  */
    virtual bool on_event(CSimEventDriven *sched,
                          bool & reschedule)=0;
};


struct CSimEventBaseCompare {
   bool operator() (const CSimEventBase * lhs, const CSimEventBase * rhs)
   {
       return lhs->m_time > rhs->m_time;
   }
};


typedef std::priority_queue<CSimEventBase *, std::vector<CSimEventBase *>,CSimEventBaseCompare> pqueue_sim_t;

class CSimEventDriven {

public:
    CSimEventDriven(){
        m_verbose = false;
        m_c_time =0.0;
    }

    void set_verbose(bool verbose){
        m_verbose = verbose;
    }

    void  add_event(CSimEventBase * event){
        m_p_queue.push(event);
    }

    int size(){
        return(m_p_queue.size());
    }

    double get_time(){
        return (m_c_time);
    }

    void remove_all();

    bool run_sim();

private:
    double        m_c_time;
    bool          m_verbose;
    pqueue_sim_t  m_p_queue;
};


#endif

