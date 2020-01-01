#ifndef __TICK_CMD_CLOCK_
#define __TICK_CMD_CLOCK_

/*
 Hanoh Haim
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

#include <stdint.h>
#include <stdlib.h>

class CTcpPerThreadCtx;
class CAstfTimerFunctorObj;

class CAstfTickCmdClock {

public:
#ifndef TREX_SIM
    static const int TICK_MSEC = 10; /* each 10 msec a tick */
#else
    static const int TICK_MSEC = 500; /* each 500 msec a tick */
#endif

public:
    CAstfTickCmdClock(CTcpPerThreadCtx *ctx);
    ~CAstfTickCmdClock();
    void on_timer_update(CAstfTimerFunctorObj *tmr);
    void timer_start();
    void timer_stop();
    uint64_t get_curr_tick();

private:
    CAstfTimerFunctorObj  * m_tmr;
    CTcpPerThreadCtx      * m_ctx;
    uint64_t           m_cur_tick;
};

#endif