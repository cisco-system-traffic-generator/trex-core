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

#ifndef __TREX_OWNER_H__
#define __TREX_OWNER_H__

#include <string>

/**
 * TRex owner can perform
 * write commands
 * while port/context is owned - others can
 * do read only commands
 *
 */
class TrexOwner {
public:

    TrexOwner();

    /**
     * is port free to acquire
     */
    bool is_free() {
        return m_is_free;
    }

    void release();

    bool is_owned_by(const std::string &user);

    void own(const std::string &owner_name, uint32_t session_id);

    bool verify(const std::string &handler);

    const std::string &get_name();

    const std::string &get_handler();

    const uint32_t get_session_id() {
        return m_session_id;
    }

private:

    /* is this port owned by someone ? */
    bool         m_is_free;

    /* user provided info */
    std::string  m_owner_name;

    /* which session of the user holds this port*/
    uint32_t     m_session_id;

    /* handler genereated internally */
    std::string  m_handler;

    /* seed for generating random values */
    unsigned int m_seed;

    /* just references defaults... */
    static const std::string g_unowned_name;
    static const std::string g_unowned_handler;
};

#endif /* __TREX_OWNER_H__ */

