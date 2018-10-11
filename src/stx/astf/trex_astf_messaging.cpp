/*
 Itay Marom
 Hanoch Haim
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

#include "trex_astf_dp_core.h"
#include "trex_astf_messaging.h"
#include "trex_rx_core.h"

//#include "trex_stl_streams_compiler.h"
//#include "trex_stl.h"
//#include "trex_stl_port.h"

//#include "bp_sim.h"

TrexAstfDpCore* astf_core(TrexDpCore *dp_core) {
    return (TrexAstfDpCore *)dp_core;
}


/*************************
  start traffic message
 ************************/
TrexAstfDpStart::TrexAstfDpStart(double duration,bool nc) {
    m_duration = duration;
    m_nc = nc;
}


bool TrexAstfDpStart::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->start_transmit(m_duration,m_nc);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStart::clone(void) {
    return new TrexAstfDpStart(m_duration,m_nc);
}

/*************************
  stop traffic message
 ************************/
TrexAstfDpStop::TrexAstfDpStop() {}

bool TrexAstfDpStop::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->stop_transmit();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStop::clone(void) {
    return new TrexAstfDpStop();
}

/*************************
  delete tcp batch
 ************************/
TrexAstfDpDeleteTcp::TrexAstfDpDeleteTcp() {}

bool TrexAstfDpDeleteTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->delete_tcp_batch();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpDeleteTcp::clone(void) {
    return new TrexAstfDpDeleteTcp();
}

