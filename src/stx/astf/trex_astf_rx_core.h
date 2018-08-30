/*
 TRex team
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
#ifndef __TREX_ASTF_RX_CORE_H__
#define __TREX_ASTF_RX_CORE_H__

#include "common/trex_rx_core.h"

/**
 * TRex ASTF RX core
 * 
 */
class CRxAstfCore : public CRxCore {
public:
    CRxAstfCore(void);

protected:
    virtual uint32_t handle_msg_packets(void);
    virtual uint32_t handle_rx_one_queue(uint8_t thread_id, CNodeRing *r);
    virtual bool work_tick(void);

    CMessagingManager *m_rx_dp;
};

#endif /* __TREX_ASTF_RX_CORE_H__ */

