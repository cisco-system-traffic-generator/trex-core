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
#ifndef __TREX_API_CLASS_H__
#define __TREX_API_CLASS_H__

#include <assert.h>

#include "common/basic_utils.h"
#include "trex_exception.h"

/**
 * API exception
 * 
 * @author imarom (03-Apr-16)
 */
class TrexAPIException : public TrexException {
public:
    TrexAPIException(const std::string &what) : TrexException(what) {
    }
};

/**
 * define an API class
 * 
 * @author imarom (03-Apr-16)
 */
class APIClass {
public:

    enum type_e {
        API_CLASS_TYPE_CORE = 0,
        API_CLASS_TYPE_MAX,

        API_CLASS_TYPE_NO_API
    };

    static const char * type_to_name(type_e type) {
        switch (type) {
        case API_CLASS_TYPE_CORE:
            return "core";
        default:
            assert(0);
        }
    }

    APIClass() {
        /* invalid */
        m_type = API_CLASS_TYPE_MAX;
    }

    void init(type_e type, int major, int minor) {
         m_type    = type;
         m_major   = major;
         m_minor   = minor;

         unsigned int seed = time(NULL);
         m_handler = utl_generate_random_str(seed, 8);
    }

    std::string ver(int major, int minor) {
        std::stringstream ss;
        ss << major << "." << minor;
        return ss.str();
    }

    std::string get_server_ver() {
        return ver(m_major, m_minor);
    }

    std::string & verify_api(int major, int minor) {
        std::stringstream ss;
        ss << "API type '" << type_to_name(m_type) << "': ";

        assert(m_type < API_CLASS_TYPE_MAX);

        bool fail = false;

        /* for now a simple major check */
        if (major < m_major) {
            ss << "server has a newer major API version";
            fail = true;
        }

        if (major > m_major) {
            ss << "server has an older major API version";
            fail = true;
        }

        if (minor > m_minor) {
            ss << "client revision API is newer than server";
            fail = true;
        }

        if (fail) {
            ss <<  " - server: '" << get_server_ver() << "', client: '" << ver(major, minor) << "'";
            throw TrexAPIException(ss.str());
        }

        return get_api_handler();
    }

    std::string & get_api_handler() {
        return m_handler;
    }

private:
    type_e         m_type;
    int            m_major;
    int            m_minor;
    std::string    m_handler;

};

#endif /* __TREX_API_CLASS_H__ */
