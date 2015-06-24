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

#ifndef TW_WHEEL_PQ
#define TW_WHEEL_PQ

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <queue>
#include <assert.h>


class CFlowTimerHandle;

struct CFlowTimer  {
public:
	CFlowTimer(){
		m_updated_time = -1.0;

	}
	/* C1 */
    /* time to expire */
	double              m_time;
    /* time when timer was updated */
	double              m_updated_time;

	CFlowTimerHandle *	m_flow; /* back pointer to the flow */

	bool is_valid(){
		return (m_flow ? true : false);
	}

public:
	bool operator <(const CFlowTimer * rsh) const {
		return (m_time<rsh->m_time);
	}
	bool operator ==(const CFlowTimer * rsh) const {
		return (m_time == rsh->m_time);
	}
	bool operator >(const CFlowTimer * rsh) const {
		return (m_time>rsh->m_time);
	}

};

struct CFlowTimerCompare
{
	bool operator() (const CFlowTimer * lhs, const CFlowTimer * rhs)
	{
		return lhs->m_time > rhs->m_time;
	}
};

class CFlowTimerHandle;
typedef void(*CallbackType_t)(CFlowTimerHandle * timer_handle);

class CFlowTimerHandle {
public:
	CFlowTimerHandle(){
		m_timer = 0;
		m_object = 0;
        m_object1=0;
		m_callback = 0;
		m_id = 0;
	}
	CFlowTimer *	m_timer;
	void *			m_object;
    void *			m_object1;
	CallbackType_t  m_callback; 
	uint32_t		m_id;
};

typedef CFlowTimer * timer_handle_t;

typedef std::priority_queue<CFlowTimer *, std::vector<CFlowTimer *>, CFlowTimerCompare> tw_pqueue_t;

class CTimerWheel {

public:
    CTimerWheel(){
      m_st_alloc=0;
      m_st_free=0;
      m_st_start=0;
      m_st_stop=0;
      m_st_handle=0;
    }
public:
	void restart_timer(CFlowTimerHandle *  timer,double new_time);
	void stop_timer(CFlowTimerHandle *  timer);
	bool peek_top_time(double & time);
    void try_handle_events(double now);
    void drain_all();

	bool handle();
public:
    void Dump(FILE *fd);
    void dump_json(std::string & json );

private:
	tw_pqueue_t   m_pq;
public:

    uint32_t   m_st_alloc;
    uint32_t   m_st_free;
    uint32_t   m_st_start;
    uint32_t   m_st_stop;
    uint32_t   m_st_handle;

};


#endif
