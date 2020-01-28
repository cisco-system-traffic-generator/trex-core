/*
 Elad Aharon
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2018 Cisco Systems, Inc.

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

#include "tick_cmd_clock.h"
#include "tcp_var.h"
#include <functional>
#include <assert.h>

CAstfTickCmdClock::CAstfTickCmdClock(CTcpPerThreadCtx *ctx) {
    m_ctx = ctx;
    m_cur_tick = 1;
    m_tmr = new CAstfTimerFunctorObj();
    assert(m_tmr);
    m_tmr->m_cb = std::bind(&CAstfTickCmdClock::on_timer_update, this, m_tmr);
}

CAstfTickCmdClock::~CAstfTickCmdClock(){
    timer_stop();
    delete m_tmr;
}

void CAstfTickCmdClock::timer_start() {
    if ( !m_tmr->is_running()) {
        on_timer_update(m_tmr);
    }
}

void CAstfTickCmdClock::timer_stop() {
    if ( m_tmr->is_running()) {
        m_ctx->m_timer_w.timer_stop(m_tmr);
    }
}

void CAstfTickCmdClock::on_timer_update(CAstfTimerFunctorObj *tmr) {
    m_ctx->m_timer_w.timer_start(tmr, tw_time_msec_to_ticks(CAstfTickCmdClock::TICK_MSEC));
    m_cur_tick++;
}

uint64_t CAstfTickCmdClock::get_curr_tick() {
    return m_cur_tick;
}
