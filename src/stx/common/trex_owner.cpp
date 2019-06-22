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

#include "trex_owner.h"
#include "common/basic_utils.h"

TrexOwner::TrexOwner() {
    static unsigned int next_seed = time(nullptr);
    m_is_free = true;
    m_session_id = 0;

    /* for handlers random generation */
    m_seed = next_seed++;
}

const std::string TrexOwner::g_unowned_name = "<FREE>";
const std::string TrexOwner::g_unowned_handler = "";

void TrexOwner::release() {
    m_is_free = true;
    m_owner_name = "";
    m_handler = "";
    m_session_id = 0;
}

bool TrexOwner::is_owned_by(const std::string &user) {
    return ( !m_is_free && (m_owner_name == user) );
}

void TrexOwner::own(const std::string &owner_name, uint32_t session_id) {

    /* save user data */
    m_owner_name = owner_name;
    m_session_id = session_id;

    /* internal data */
    m_handler = utl_generate_random_str(m_seed, 8);
    m_is_free = false;
}

bool TrexOwner::verify(const std::string &handler) {
    return ( (!m_is_free) && (m_handler == handler) );
}

const std::string &TrexOwner::get_name() {
    return (!m_is_free ? m_owner_name : g_unowned_name);
}

const std::string &TrexOwner::get_handler() {
    return (!m_is_free ? m_handler : g_unowned_handler);
}


