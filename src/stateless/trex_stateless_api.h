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
#ifndef __TREX_STATELESS_API_H__
#define __TREX_STATELESS_API_H__

#include <stdint.h>
#include <string>
#include <stdexcept>

#include <trex_stream_api.h>

/**
 * generic exception for errors
 * TODO: move this to a better place
 */
class TrexException : public std::runtime_error 
{
public:
    TrexException() : std::runtime_error("") {

    }
    TrexException(const std::string &what) : std::runtime_error(what) {
    }
};

/**
 * describes a stateless port
 * 
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort {
public:

    TrexStatelessPort(uint8_t port_id) : m_port_id(port_id) {
    }

    /**
     * access the stream table
     * 
     */
    TrexStreamTable *get_stream_table() {
        return &m_stream_table;
    }

private:
    TrexStreamTable  m_stream_table;
    uint8_t          m_port_id;
};

/**
 * defines the T-Rex stateless operation mode
 * 
 */
class TrexStateless {
public:
    /**
     * create a T-Rex stateless object
     * 
     * @author imarom (31-Aug-15)
     * 
     * @param port_count 
     */
    TrexStateless(uint8_t port_count);
    ~TrexStateless();

    TrexStatelessPort *get_port_by_id(uint8_t port_id);
    uint8_t            get_port_count();

protected:
    TrexStatelessPort  **m_ports;
    uint8_t             m_port_count;
};

/****** HACK *******/
TrexStateless *get_trex_stateless();

#endif /* __TREX_STATELESS_API_H__ */

