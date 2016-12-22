#ifndef H_TIMER_WHEEL_H
#define H_TIMER_WHEEL_H
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

#include <common/basic_utils.h>
#include <stdint.h>
#include <assert.h>
#include <rte_prefetch.h>
#include "pal_utl.h"
#include "mbuf.h"



typedef enum {
    RC_HTW_OK = 0,
    RC_HTW_ERR_NO_RESOURCES = -1,
    RC_HTW_ERR_TIMER_IS_ON  = -2,
    RC_HTW_ERR_NO_LOG2      = -3,
    RC_HTW_ERR_MAX_WHEELS   = -4,
    RC_HTW_ERR_NOT_ENOUGH_BITS  = -5,

} RC_HTW_t;

class CHTimerWheelErrorStr {
public:
    CHTimerWheelErrorStr(RC_HTW_t val){
        m_err=val;
    }
    const char * get_str(void){
        switch (m_err) {
        case RC_HTW_OK:
            return ("RC_HTW_OK");
            break;
        case RC_HTW_ERR_NO_RESOURCES :
            return ("RC_HTW_ERR_NO_RESOURCES");
            break;
        case RC_HTW_ERR_TIMER_IS_ON :
            return ("RC_HTW_ERR_TIMER_IS_ON");
            break;
        case RC_HTW_ERR_NO_LOG2 :
            return ("RC_HTW_ERR_NO_LOG2");
            break;
        case RC_HTW_ERR_MAX_WHEELS :
            return ("RC_HTW_ERR_MAX_WHEELS");
            break;
        case RC_HTW_ERR_NOT_ENOUGH_BITS :
            return ("RC_HTW_ERR_NOT_ENOUGH_BITS");
            break;
        default:
            assert(0);
        }
    }

    const char * get_help_str(void){
        switch (m_err) {
        case RC_HTW_OK:
            return ("ok");
            break;
        case RC_HTW_ERR_NO_RESOURCES :
            return ("not enough memory");
            break;
        case RC_HTW_ERR_TIMER_IS_ON :
            return ("timer is already on, you should stop before start");
            break;
        case RC_HTW_ERR_NO_LOG2 :
            return ("number of buckets should be log2");
            break;
        case RC_HTW_ERR_MAX_WHEELS :
            return ("maximum number of wheels is limited to 4");
            break;
        case RC_HTW_ERR_NOT_ENOUGH_BITS :
            return ("(log2(buckets) * number of wheels)  should be less than 32, try to reduce the number of wheels");
            break;
        default:
            assert(0);
        }
    }

private:
    RC_HTW_t m_err;
};

class CHTimerWheelLink {

public:
    CHTimerWheelLink  * m_next;
    CHTimerWheelLink  * m_prev;

public:
    void reset(){
        m_next = 0;
        m_prev = 0;
    }
    void set_self(){
        m_next=this;
        m_prev=this;
    }

    bool is_self(){
        if (m_next == this) {
            return (true);
        }
        return (false);
    }

    void append(CHTimerWheelLink * obj){
        obj->m_next = this;     
        obj->m_prev = m_prev;

        m_prev->m_next   = obj;
        m_prev           = obj;
    }

    void detach(void){
        #ifdef _DEBUG 
        assert(m_next);
        #endif
        CHTimerWheelLink *next;

        next = m_next;
        next->m_prev = m_prev;
        m_prev->m_next = next;
        m_next=0;
        m_prev=0;
    }
} ;

typedef uint32_t  htw_ticks_t;

class CHTimerObj : public CHTimerWheelLink  {

public:
    inline void reset(void){
        CHTimerWheelLink::reset();
        m_ticks_left=0;
        m_wheel=0;
    }

    inline bool is_running(){
        if (m_next != 0) {
            return (true);
        }
        return (false);
    }


    void Dump(FILE *fd);

public:
    /* CACHE LINE 0*/
    htw_ticks_t       m_ticks_left; /* abs ticks left */
    uint32_t          m_wheel;

    uint32_t          m_pad[2];      /* aging time in ticks */
} ;

typedef void (*htw_on_tick_cb_t)(void *userdata,CHTimerObj *tmr);

class CHTimerOneWheel {

public:
    CHTimerOneWheel(){
        reset();
    }

    RC_HTW_t Create(uint32_t wheel_size);

    RC_HTW_t Delete();

    inline RC_HTW_t timer_start(CHTimerObj  *tmr, 
                                htw_ticks_t   ticks){

        #ifdef _DEBUG 
        if ( tmr->is_running() ){
            return( RC_HTW_ERR_TIMER_IS_ON);
        }
        #endif

        append( tmr, ticks);
        return (RC_HTW_OK);
    }

    RC_HTW_t timer_stop (CHTimerObj *tmr);

    uint32_t detach_all(void *userdata,htw_on_tick_cb_t cb);

    inline bool check_timer_tick_cycle(){
        return (m_tick_done);
    }


    inline bool timer_tick(){

        m_ticks++;
        m_bucket_index++;

        if (m_tick_done) {
            m_tick_done=false;
        }
        if ( m_bucket_index == m_wheel_size ) {
            m_bucket_index = 0;
            m_tick_done=true;
        }
        m_active_bucket = &m_buckets[m_bucket_index];
        return (m_tick_done);
    }


    inline CHTimerObj *  pop_event(void) {

        if ( m_active_bucket->is_self() ){
            return ((CHTimerObj *)0);
        }

        CHTimerWheelLink * first = m_active_bucket->m_next;

        rte_prefetch0(first->m_next);

        first->detach();
        return ((CHTimerObj *)first);
    }



public:

      void  dump_link_list(uint32_t bucket_index,
                           void *userdata,
                           htw_on_tick_cb_t cb,
                           FILE *fd);


      uint32_t get_bucket_index(void){
          return ( m_bucket_index);
      }

private:

    inline void reset(void){
       m_buckets=0;
       m_active_bucket=0;    
       m_ticks=0;               
       m_wheel_size=0; 
       m_wheel_mask=0; 
       m_bucket_index=0; 
       m_tick_done=false;
    }


    inline void append (CHTimerObj *tmr, 
                        uint32_t ticks) {
        CHTimerWheelLink *cur;
    
        uint32_t cursor = ((m_bucket_index + ticks) & m_wheel_mask);
        cur = &m_buckets[cursor];

        cur->append((CHTimerWheelLink *)tmr);
    }

private:
	CHTimerWheelLink  * m_buckets;
    CHTimerWheelLink  * m_active_bucket;     /* point to the current bucket m_buckets[m_bucket_index] */

    htw_ticks_t         m_ticks;               
    uint32_t            m_wheel_size; //e.g. 256
    uint32_t            m_wheel_mask; // 256-1
    uint32_t            m_bucket_index; 
    bool                m_tick_done;
};




#define MAX_H_TIMER_WHEELS  (4)

class CHTimerWheel {

public:
    CHTimerWheel(){
        reset();
    }

    RC_HTW_t Create(uint32_t wheel_size,
                    uint32_t num_wheels);

    RC_HTW_t Delete();


    inline RC_HTW_t timer_start(CHTimerObj  *tmr, 
                                htw_ticks_t  ticks){
        m_total_events++;
        if (likely(ticks<m_wheel_size)) {
            tmr->m_ticks_left=0;
            tmr->m_wheel=0;
            return (m_timer_w[0].timer_start(tmr,ticks));
        }
        return ( timer_start_rest(tmr, ticks));
    }

    RC_HTW_t timer_stop (CHTimerObj *tmr);

    void on_tick(void *userdata,htw_on_tick_cb_t cb);

    bool is_any_events_left(){
        return(m_total_events>0?true:false);
    }

    /* iterate all, detach and call the callback */
    void detach_all(void *userdata,htw_on_tick_cb_t cb);


private:
    void reset(void);

    RC_HTW_t timer_start_rest(CHTimerObj  *tmr, 
                              htw_ticks_t  ticks);

private:
    htw_ticks_t         m_ticks;               
    uint32_t            m_num_wheels;
    uint32_t            m_wheel_size;  //e.g. 256
    uint32_t            m_wheel_mask;  //e.g 256-1
    uint32_t            m_wheel_shift; // e.g 8
    uint64_t            m_total_events;
    CHTimerOneWheel     m_timer_w[MAX_H_TIMER_WHEELS];
} ;


#endif
