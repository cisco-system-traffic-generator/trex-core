/*
 Itay Marom
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
#ifndef __TREX_STATELESS_DP_CORE_H__
#define __TREX_STATELESS_DP_CORE_H__

#include <stdint.h>

/**
 * stateless DP core object
 * 
 */
class TrexStatelessDpCore {
public:

    TrexStatelessDpCore(uint8_t core_id);

    /* starts the DP core run */
    void run();

private:
    void test_inject_dummy_pkt();
    uint8_t m_core_id;
};

#endif /* __TREX_STATELESS_DP_CORE_H__ */
