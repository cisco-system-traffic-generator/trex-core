 /*------------------------------------------------------------------

 * Februrary 2002, Bo Berry
  * hhaim-  2013
   base on  Februrary 2002, Bo Berry 
 *
 * Copyright (c) 2005-2009 by Cisco Systems, Inc.
 * All rights reserved. 
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.

 *------------------------------------------------------------------
 */

#ifndef __STW_TIMER_H__
#define __STW_TIMER_H__

#undef TW_DEBUG 

#define MAGIC_TAG         ( 0xDEADBEEF )

#include <stdint.h>
#include <assert.h>
#include "pal_utl.h"
#include <rte_prefetch.h>



typedef enum {
    RC_STW_OK = 0,
    RC_STW_NULL_NAME,
    RC_STW_NULL_FV,
    RC_STW_NULL_WHEEL,
    RC_STW_NULL_TMR,
    RC_STW_TIMER_IS_ON,
    RC_STW_INVALID_WHEEL,
    RC_STW_INVALID_WHEEL_SIZE,
    RC_STW_INVALID_GRANULARITY,
    RC_STW_NO_RESOURCES,
} RC_STW_t;



#define STW_MIN_WHEEL_SIZE    (   8 )
#define STW_MAX_WHEEL_SIZE    ( 100000 )



class CTimerWheelLink {
public:
    CTimerWheelLink  *stw_next;
    CTimerWheelLink  *stw_prev;
} ;

class CTimerObj {

public:
    inline void reset(void){
        prepare ();
        m_rotation_count=0;
        m_last_update_tick=0;
        m_aging_ticks=0;  
    }

    inline bool is_running(){
        if (m_links.stw_next != 0) {
            return (true);
        }
        return (false);
    }

    inline void prepare () {
            m_links.stw_next = 0;
            m_links.stw_prev = 0;
    }

    inline uint32_t restart_aging_ticks(uint32_t cur_ticks){
        uint32_t dticks =(cur_ticks - m_last_update_tick );
        return (  m_aging_ticks -dticks );
    }

    void Dump(FILE *fd);
public:
    CTimerWheelLink  m_links;
    uint32_t         m_rotation_count;
    uint32_t         m_last_update_tick; /* cur timer tick the timer was set/restart */
    uint32_t         m_aging_ticks;      /* aging time in ticks */
    uint32_t         m_pad;
} ;


typedef void (*tw_on_tick_cb_t)(void *userdata,CTimerObj *tmr);

class CTimerWheelBucket {

public:

    friend class CTimerWheelBucketUT;

    RC_STW_t Create(uint32_t wheel_size);

    RC_STW_t Delete();

    inline RC_STW_t timer_restart(CTimerObj  *tmr){

        if ( tmr->is_running() ){
            tmr->m_last_update_tick = m_ticks;/* update the time with current tick */

            #ifdef TW_DEBUG 
            m_timer_restart++ ;
            #endif
        }
        return (RC_STW_OK);
    }



    inline RC_STW_t timer_restart(CTimerObj  *tmr,
                                uint32_t   ticks){

        if ( tmr->is_running() ){
            if ( tmr->restart_aging_ticks(m_ticks) < ticks) {
                tmr->m_last_update_tick = m_ticks;
                tmr->m_aging_ticks =ticks;

                #ifdef TW_DEBUG 
                m_timer_restart++ ;
                #endif
                return (RC_STW_OK);

            }else{
                timer_stop (tmr);
            }
        }
        return (timer_start(tmr,ticks));
    }

    inline RC_STW_t timer_start(CTimerObj  *tmr,
        						uint32_t   ticks){

        if ( tmr->is_running() ){
            return( RC_STW_TIMER_IS_ON);
        }
        
        #ifdef TW_DEBUG 
            CTimerWheelLink *next, *prev;

            if (this == 0) {
                return (RC_STW_NULL_WHEEL);
            }
        
            if (tmr == 0) {
                return (RC_STW_NULL_TMR);
            }
        
            if (m_magic_tag != MAGIC_TAG) {
               return (RC_STW_INVALID_WHEEL);
            }
        
            /*
             * First check to see if it is already running. If so, remove
             * it from the wheel.  We don't bother cleaning up the fields
             * because we will be setting them below.
             */
            next = tmr->m_links.stw_next;
            if (next) {
                prev = tmr->m_links.stw_prev;
                next->stw_prev = prev;
                prev->stw_next = next;
        
                /*
                 * stats book keeping
                 */
                m_timer_active--;
            }
        #endif

        tmr->m_last_update_tick = m_ticks;  /* save the tick */
        tmr->m_aging_ticks      = ticks;   /* set the original aging tick */
        tmr_enqueue( tmr, ticks);
    
    #ifdef TW_DEBUG 
        m_timer_starts++;
        m_timer_active++;
        if (m_timer_active > m_timer_hiwater_mark) {
            m_timer_hiwater_mark = m_timer_active;
        }
    #endif

        return (RC_STW_OK);
      }


         RC_STW_t timer_stop (CTimerObj *tmr);


        inline void timer_tick(){

        	#ifdef TW_DEBUG 
            if ((this == 0) || (m_magic_tag != MAGIC_TAG)) {
                return;
            }
        	/*
        	 * keep track of rolling the wheel
        	 */
        	#endif

            m_ticks++;
        
        	m_bucket_index++;
        
        	if ( m_bucket_index == m_wheel_size ) {
        		m_bucket_index=0;
        	}
        	m_active_bucket = &m_buckets[m_bucket_index];
            m_active_tick_timer = m_active_bucket;
        }

        bool do_tick(void *userdata,tw_on_tick_cb_t cb,int32_t limit=0);


        void  dump_link_list(void *userdata,tw_on_tick_cb_t cb,FILE *fd) ;


        inline CTimerObj *  timer_tick_get_next(void) {

            if ( m_active_tick_timer == NULL ){
                return ((CTimerObj *)0);
            }

            CTimerWheelLink  *bucket, *next, *prev;
            CTimerObj *tmr;

        #ifdef TW_DEBUG 
            if ((this == 0) || (m_magic_tag != MAGIC_TAG)) {
                return (CTimerObj *)0;
            }
        #endif
        
            bucket = m_active_bucket; /* point the last/first */
            tmr = (CTimerObj *)m_active_tick_timer->stw_next;

            while( (CTimerWheelLink *)tmr != bucket) {
        
                next = (CTimerWheelLink *)tmr->m_links.stw_next;
                rte_prefetch0(next);
        
                /*
                 * if the timer is a long one and requires one or more rotations
                 * decrement rotation count and leave for next turn.
                 */
                if (tmr->m_rotation_count != 0) {
                    tmr->m_rotation_count--;
                } else {

                    uint32_t reschedule = tmr->restart_aging_ticks(m_ticks);

                    if ( reschedule == 0){
                        /* no reschedule */
                        prev = (CTimerWheelLink *)tmr->m_links.stw_prev;

                        prev->stw_next = next;
                        next->stw_prev = prev;

                        tmr->m_links.stw_next = 0;
                        tmr->m_links.stw_prev = 0;

                        #ifdef TW_DEBUG 
                        /* book keeping */
                        m_timer_active--;
                        m_timer_expired++;
                        #endif
                        
                        m_active_tick_timer = next->stw_prev; 
            			return(tmr);
                    }else{
                        if ( (reschedule % m_wheel_size ==0)) {
                            /* same spoke */
                            tmr->m_rotation_count = (reschedule / m_wheel_size)-1;
                        }else{
                            /* diff spoke */

                            prev = (CTimerWheelLink *)tmr->m_links.stw_prev;

                            prev->stw_next = next;
                            next->stw_prev = prev;

                            tmr->m_links.stw_next = 0;
                            tmr->m_links.stw_prev = 0;
                            tmr_enqueue (tmr, reschedule);
                        }

                    }
                }
        
                tmr = (CTimerObj *)next;
            }
            m_active_tick_timer = NULL; /* point to the bucket */
            return (CTimerObj *)0;
        }

public:
      void timer_stats_dump(FILE *fd);

      uint32_t get_ticks(){
          return (m_ticks);
      }

private:

        inline void tmr_enqueue (CTimerObj *tmr, 
								 uint32_t ticks) {
            CTimerWheelLink  *prev, *spoke;
        
            uint32_t cursor;
        
            if (ticks >= m_wheel_size) {
                tmr->m_rotation_count = (ticks / m_wheel_size);
            }else{
                tmr->m_rotation_count=0;
            }
            cursor = ((m_bucket_index + ticks) % m_wheel_size);
            spoke = &m_buckets[cursor];
            prev = spoke->stw_prev;
            tmr->m_links.stw_next = spoke;     
            tmr->m_links.stw_prev = prev;
        
            prev->stw_next   = (CTimerWheelLink *)tmr;
            spoke->stw_prev = (CTimerWheelLink *)tmr;
        }



private:
	CTimerWheelLink  * m_buckets;
	CTimerWheelLink  * m_active_bucket;     /* point to the current bucket m_buckets[m_bucket_index] */
    CTimerWheelLink  * m_active_tick_timer; /* interator of current tick, could be NULL in case we finish scanning the line */


    uint32_t           m_ticks;               
    uint32_t           m_magic_tag;           

    uint32_t           m_wheel_size;
    uint32_t           m_bucket_index; 

protected:
    /* stats */
    uint32_t  m_timer_hiwater_mark;
    uint32_t  m_timer_active;
    uint32_t  m_timer_cancelled;
    uint32_t  m_timer_expired;
    uint32_t  m_timer_starts;
    uint32_t  m_timer_restart;
};





#endif /* __STW_TIMER_H__ */


