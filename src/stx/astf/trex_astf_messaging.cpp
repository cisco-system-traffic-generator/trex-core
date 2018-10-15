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

using namespace std;

TrexAstfDpCore* astf_core(TrexDpCore *dp_core) {
    return (TrexAstfDpCore *)dp_core;
}


/*************************
  start traffic message
 ************************/
TrexAstfDpStart::TrexAstfDpStart() {
}


bool TrexAstfDpStart::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->start_transmit();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStart::clone() {
    return new TrexAstfDpStart();
}

/*************************
  stop traffic message
 ************************/
TrexAstfDpStop::TrexAstfDpStop() {}

bool TrexAstfDpStop::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->stop_transmit();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStop::clone() {
    return new TrexAstfDpStop();
}

/*************************
  update traffic message
 ************************/
TrexAstfDpUpdate::TrexAstfDpUpdate(double old_new_ratio) {
    m_old_new_ratio = old_new_ratio;
}

bool TrexAstfDpUpdate::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->update_rate(m_old_new_ratio);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpUpdate::clone() {
    return new TrexAstfDpUpdate(m_old_new_ratio);
}

/*************************
  create tcp batch
 ************************/
TrexAstfDpCreateTcp::TrexAstfDpCreateTcp() {}

bool TrexAstfDpCreateTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->create_tcp_batch();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpCreateTcp::clone() {
    return new TrexAstfDpCreateTcp();
}

/*************************
  delete tcp batch
 ************************/
TrexAstfDpDeleteTcp::TrexAstfDpDeleteTcp() {}

bool TrexAstfDpDeleteTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->delete_tcp_batch();
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpDeleteTcp::clone() {
    return new TrexAstfDpDeleteTcp();
}


/*************************
  parse ASTF JSON from string
 ************************/
TrexAstfLoadDB::TrexAstfLoadDB(string *profile_buffer) {
    m_profile_buffer = profile_buffer;
}

bool TrexAstfLoadDB::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->parse_astf_json(m_profile_buffer);
    return true;
}

TrexCpToDpMsgBase* TrexAstfLoadDB::clone() {
    assert(0); // should not be cloned [and sent to several cores]
    return nullptr;
}

