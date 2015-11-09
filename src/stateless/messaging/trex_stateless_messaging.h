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
#ifndef __TREX_STATELESS_MESSAGING_H__
#define __TREX_STATELESS_MESSAGING_H__

#include <msg_manager.h>

class TrexStatelessDpCore;
class TrexStreamsCompiledObj;

/**
 * defines the base class for CP to DP messages
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessCpToDpMsgBase() {
    }

    virtual ~TrexStatelessCpToDpMsgBase() {
    }

    /**
     * virtual function to handle a message
     * 
     */
    virtual bool handle(TrexStatelessDpCore *dp_core) = 0;


    /**
     * clone the current message
     * 
     */
    virtual TrexStatelessCpToDpMsgBase * clone() = 0;

    /* no copy constructor */
    TrexStatelessCpToDpMsgBase(TrexStatelessCpToDpMsgBase &) = delete;

};

/**
 * a message to start traffic
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStart : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStart(TrexStreamsCompiledObj *obj);

    ~TrexStatelessDpStart();

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    TrexStreamsCompiledObj *m_obj;
};

/**
 * a message to stop traffic
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStop : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStop(uint8_t port_id) : m_port_id(port_id) {
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    uint8_t m_port_id;
};


#endif /* __TREX_STATELESS_MESSAGING_H__ */
