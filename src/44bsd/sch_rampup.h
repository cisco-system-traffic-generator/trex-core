#ifndef __SCHED_RAMPUP_
#define __SCHED_RAMPUP_

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

class CTcpPerThreadCtx;
class CAstfTimerFunctorObj;

#include <stdint.h>
#include <stdlib.h>


class CAstfFifRampup {

private:
    const int TICK_MSEC=1000; /* each 1000msec a tick */

public:
    CAstfFifRampup(CTcpPerThreadCtx * ctx,
                   uint16_t           rampup_sec,
                   double             cps);
    ~CAstfFifRampup();

    /* update dtime_fif in sec and restart the timer if needed */
    void on_timer_update(CAstfTimerFunctorObj *tmr);

private:
    CTcpPerThreadCtx * m_ctx;
    CAstfTimerFunctorObj  * m_tmr;
    double             m_cps;
    uint16_t           m_ticks;
    uint16_t           m_cur_tick;
    uint16_t           m_rampup_sec;
};
      


#endif


