/*
 Itay Marom
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
#ifndef __TREX_ASTF_PORT_H__
#define __TREX_ASTF_PORT_H__


#include "trex_port.h"

/**
 * describes an ASTF port
 *
 */
class TrexAstfPort : public TrexPort {

public:

    TrexAstfPort(uint8_t port_id);

    ~TrexAstfPort();

    bool is_service_mode_on() const {
        /* for ASTF we under always under service mode */
        return true;
    }

};


#endif /* __TREX_ASTF_PORT_H__ */

