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

class CHTimerBucket : public CHTimerWheelLink {
public:
    inline void reset_count(void){
        m_count=0;
    }

    inline void append(CHTimerWheelLink * obj){

        CHTimerWheelLink::append(obj);
        m_count++;
    }
    inline void dec_count(){
        assert(m_count>0);
        m_count--;
    }


    uint32_t m_count;
};


typedef uint32_t  htw_ticks_t;

class CHTimerObj : public CHTimerWheelLink  {

public:
    inline void reset(void){
        CHTimerWheelLink::reset();
        m_ticks_left=0;
        m_wheel=0;
        m_root=(CHTimerBucket*)(0);
        m_type=0;
        m_pad=0;
    }

    inline bool is_running(){
        if (m_next != 0) {
            return (true);
        }
        return (false);
    }


    void detach(void){
        assert(m_root);
        m_root->dec_count();
        m_root=0;
        CHTimerWheelLink::detach();
    }


    void Dump(FILE *fd);

public:
    /* CACHE LINE 0*/
    CHTimerBucket    * m_root;
    htw_ticks_t       m_ticks_left; /* abs ticks left */
    uint8_t            m_wheel;
    uint8_t            m_type;
    uint16_t           m_pad;
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

    inline uint32_t  get_bucket_total_events(void) {
        return(m_active_bucket->m_count);
    }


    inline CHTimerObj *  pop_event(void) {

        if ( m_active_bucket->is_self() ){
            return ((CHTimerObj *)0);
        }

        CHTimerObj * first = (CHTimerObj *)m_active_bucket->m_next;

        rte_prefetch0(first->m_next);

        first->detach();
        return (first);
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
        CHTimerBucket *cur;
    
        uint32_t cursor = ((m_bucket_index + ticks) & m_wheel_mask);
        cur = &m_buckets[cursor];

        tmr->m_root=cur; /* set root */
        cur->append((CHTimerWheelLink *)tmr);
    }

private:
	CHTimerBucket     * m_buckets;
    CHTimerBucket     * m_active_bucket;     /* point to the current bucket m_buckets[m_bucket_index] */

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




#define HNA_TIMER_LEVELS  (2)
#define HNA_MAX_LEVEL1_EVENTS (64) /* small bursts */

typedef enum {
    TW_FIRST_FINISH =17,
    TW_FIRST_FINISH_ANY =18,
    TW_FIRST_BATCH   =19,
    TW_NEXT_BATCH   =20,
    TW_END_BATCH    =21
} NA_HTW_STATE_t;

typedef uint8_t na_htw_state_num_t;



/* two levels 0,1. level 1 would be less accurate */ 
class CNATimerWheel {

public:
    CNATimerWheel(){
        reset();
    }

    RC_HTW_t Create(uint32_t wheel_size,uint8_t level1_div);

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

    void on_tick_level0(void *userdata,htw_on_tick_cb_t cb);

    na_htw_state_num_t on_tick_level1(void *userdata,htw_on_tick_cb_t cb);


    /* we can use this function only for one level.
       if we have one timer with one level this would be the level=0
       if we have two level, it will be level 1 (with the higher ticks 
       
       split the event of level1 by the root->m_count/div 
       for example in case res=100msec and we have 10,000 events in a bucket 
       and div in 1000 (every 100usec sub-tick ) 
       give every sub-tick min(10,000/1000,min_events) =min(10,min_event)

       return the tick in div, 0 is the first one                             

       restar timer to sub-tick ();
       
     */

    uint16_t on_tick_level_count(int level,
                                  void *userdata,
                                  htw_on_tick_cb_t cb,
                                  uint16_t min_events,
                                  uint32_t & left);


    bool is_any_events_left(){
        return(m_total_events>0?true:false);
    }

    /* iterate all, detach and call the callback */
    void detach_all(void *userdata,htw_on_tick_cb_t cb);

    void set_level1_cnt_div(uint16_t div){
        m_cnt_div=div;
    }

    /* set the default div to the second layer */
    void set_level1_cnt_div();

    htw_ticks_t get_ticks(int level){
        return(m_ticks[level]);
    }


private:
    void reset(void);

    void on_tick_level_inc(int level);


    RC_HTW_t timer_start_rest(CHTimerObj  *tmr, 
                              htw_ticks_t  ticks);


private:
    htw_ticks_t         m_ticks[HNA_TIMER_LEVELS];               
    uint32_t            m_wheel_size;  //e.g. 256
    uint32_t            m_wheel_mask;  //e.g 256-1
    uint32_t            m_wheel_shift; // e.g 8
    uint32_t            m_wheel_level1_shift; //e.g 16 
    uint32_t            m_wheel_level1_err; //e.g 16 

    uint64_t            m_total_events;
    CHTimerOneWheel     m_timer_w[HNA_TIMER_LEVELS];
    na_htw_state_num_t  m_state;
    uint16_t            m_cnt_div;      /* div of time for level1 
                                        in case of tick of 20msec and 20usec sub-tick we need 1000 div 
                                        */
    uint16_t            m_cnt_state; /* the state of level1 for cnt mode */
    uint32_t            m_cnt_per_iter; /* per iteration events */
} ;


#endif
