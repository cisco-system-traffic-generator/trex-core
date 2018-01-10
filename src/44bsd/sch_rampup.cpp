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

#include "sch_rampup.h"
#include "tcp_var.h"
#include <functional>
#include <assert.h>


CAstfFifRampup::CAstfFifRampup(CTcpPerThreadCtx * ctx,
                   uint16_t           rampup_sec,
                   double             cps){
        /* total ticks */
        m_ctx=ctx;
        m_ticks = (uint32_t)rampup_sec*1000/TICK_MSEC;
        m_rampup_sec =rampup_sec;
        assert(m_ticks>1);
        m_cur_tick = 1;
        m_cps = cps;
        m_tmr = new CAstfTimerFunctorObj();
        assert(m_tmr);
        m_tmr->m_cb = std::bind(&CAstfFifRampup::on_timer_update,this,m_tmr);
        on_timer_update(m_tmr);
}

CAstfFifRampup::~CAstfFifRampup(){
    if (m_tmr) {
        assert(m_tmr->is_running());
        m_ctx->m_timer_w.timer_stop(m_tmr);
        delete m_tmr;
        m_tmr=0;
    }
}

void CAstfFifRampup::on_timer_update(CAstfTimerFunctorObj *tmr){

    double cur_cps = m_cps*(double)m_cur_tick/((double)m_ticks);
    m_ctx->m_fif_d_time  = 1.0/cur_cps;

    double max_rampup_sec = (double)m_rampup_sec/4.0;

    /* make sure the d time is not bigger than the rampup time (could be in smaller numbers) */
    if (m_ctx->m_fif_d_time > max_rampup_sec ) {
        m_ctx->m_fif_d_time = max_rampup_sec;
    }
    //printf("tick  %d %d (%f,%f) dtime:%f \n",(int)m_cur_tick,(int)m_ticks,cur_cps,m_cps,m_ctx->m_fif_d_time);
    if (m_cur_tick == m_ticks) {
        /* stop timer by free */
        delete tmr;
        m_tmr=0;
        return;
    }

    m_ctx->m_timer_w.timer_start(tmr,
                                 tw_time_msec_to_ticks(CAstfFifRampup::TICK_MSEC) 
                                 );
    m_cur_tick++;
}

