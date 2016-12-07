#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stw_timer.h"

/* todo :

1. add jitter support 
2. in case ticks take too much time add a quata and keep the time in the current bucket then speed up 

hhaim
*/


void CTimerObj::Dump(FILE *fd){

    fprintf(fd,"m_rotation_count        :%lu \n", (ulong)m_rotation_count);
    fprintf(fd,"m_last_update_tick      :%lu \n", (ulong)m_last_update_tick);
    fprintf(fd,"m_aging_ticks           :%lu \n", (ulong)m_aging_ticks);
}


void  CTimerWheelBucket::dump_link_list(void *userdata,tw_on_tick_cb_t cb,FILE *fd){


    CTimerWheelLink  *bucket, *next;
    CTimerObj *tmr;


    bucket = m_active_bucket;

    tmr = (CTimerObj *)bucket->stw_next;
    bool found=false;
    if ((CTimerWheelLink *)tmr != bucket) {
        fprintf(fd,"[%lu,\n",(ulong)m_bucket_index);
        found=true;
    }

    while( (CTimerWheelLink *)tmr != bucket) {

        next = (CTimerWheelLink *)tmr->m_links.stw_next;

        tmr->Dump(fd);
        cb(userdata,tmr);

        tmr = (CTimerObj *)next;
    }
    if (found){
        fprintf(fd,"]\n");
    }
}


bool CTimerWheelBucket::do_tick(void *userdata,
                                tw_on_tick_cb_t cb,
                                int32_t limit){

    
    CTimerObj * tmr;
    int cnt=0;
    while (  true ) {
        tmr = timer_tick_get_next();
        if (!tmr) {
            break;
        }
        cb(userdata,tmr);
        cnt++;
        if (cnt>limit && (limit>0)) {
            return(false);
        }
    }
    timer_tick();
    return(true);
}


void CTimerWheelBucket::timer_stats_dump(FILE *fd){
    fprintf(fd,"wheel_size         :%lu \n", (ulong)m_wheel_size);
    fprintf(fd,"ticks              :%lu \n", (ulong)m_ticks);
    fprintf(fd,"bucket_index       :%lu \n", (ulong)m_bucket_index);
    fprintf(fd,"timer_active       :%lu \n", (ulong)m_timer_active);
    fprintf(fd,"timer_expired      :%lu \n", (ulong)m_timer_expired);
    fprintf(fd,"timer_hiwater_mark :%lu \n", (ulong)m_timer_hiwater_mark);
    fprintf(fd,"timer_starts       :%lu \n", (ulong)m_timer_starts);
    fprintf(fd,"timer_cancelled    :%lu \n", (ulong)m_timer_cancelled);
    fprintf(fd,"m_timer_restart    :%lu \n", (ulong)m_timer_restart);
    
}


RC_STW_t CTimerWheelBucket::timer_stop (CTimerObj *tmr)
{
    CTimerWheelLink *next, *prev;

#ifdef TW_DEBUG 

    if (this == 0) {
        return (RC_STW_NULL_WHEEL);
    }

    if (tmr == 0) {
        return (RC_STW_NULL_TMR);
    }

    if (m_magic_tag != MAGIC_TAG ) {
        return (RC_STW_INVALID_WHEEL);
    }

#endif

    next = tmr->m_links.stw_next;
    if (next) {
        prev = tmr->m_links.stw_prev;
        next->stw_prev = prev;
        prev->stw_next = next;
        tmr->m_links.stw_next = 0;    /* 0 == tmr is free */
        tmr->m_links.stw_prev = 0;

        /*
         * stats bookkeeping
         */
        m_timer_active--;
        m_timer_cancelled++;
    }
    return (RC_STW_OK);
}

RC_STW_t CTimerWheelBucket::Delete() {
    uint32_t  j;
    CTimerWheelLink *spoke;

    CTimerObj *tmr;
    if (this == 0) {
        return (RC_STW_NULL_WHEEL);
    }

    if (m_magic_tag != MAGIC_TAG ) {
        return (RC_STW_INVALID_WHEEL);
    }

    for (j = 0; j < m_wheel_size; j++) {
        spoke = &m_buckets[j];

        tmr = (CTimerObj *)spoke->stw_next;

        while ( (CTimerWheelLink *)tmr != spoke) {
            timer_stop(tmr);
            tmr = (CTimerObj *)spoke->stw_next;
        } /* end while */

    } /* end for */

    /*
     * clear the magic so we do not mistakenly access this wheel
     */
    m_magic_tag = 0;

    /*
     * now free the wheel structures
     */
    free(m_buckets);
    m_buckets=0;

    return (RC_STW_OK);
}

RC_STW_t CTimerWheelBucket::Create(uint32_t wheel_size){
    uint32_t j;
    CTimerWheelLink *bucket;

    if (wheel_size < STW_MIN_WHEEL_SIZE || wheel_size > STW_MAX_WHEEL_SIZE) {
        return (RC_STW_INVALID_WHEEL_SIZE);
    }

    m_buckets = (CTimerWheelLink *)malloc(wheel_size * sizeof(CTimerWheelLink));
    if (m_buckets == 0) {
        return (RC_STW_NO_RESOURCES);
    }

    m_magic_tag = MAGIC_TAG;
    m_ticks = 0;
    m_bucket_index = 0;
    m_wheel_size  = wheel_size;

    m_timer_hiwater_mark  = 0;
    m_timer_active = 0;
    m_timer_cancelled=0;
    m_timer_expired=0;
    m_timer_starts=0;
    m_timer_restart=0;

    bucket = &m_buckets[0];
	m_active_bucket=bucket;
    m_active_tick_timer = m_active_bucket;
    /* link list point to itself */
    for (j = 0; j < wheel_size; j++) {
        bucket->stw_next = bucket;    
        bucket->stw_prev = bucket;
        bucket++;
    }
    return (RC_STW_OK);
}


