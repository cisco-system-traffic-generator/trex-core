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
#ifndef __TREX_EXCEPTION_H__
#define __TREX_EXCEPTION_H__

#include <stdexcept>
#include <string>

/**
 * generic exception for errors
 * TODO: move this to a better place
 */

class TrexException : public std::runtime_error
{
 public:
    enum TrexExceptionTypes_t {
        T_FLOW_STAT_PG_ID_DIFF_L4,
        T_FLOW_STAT_DUP_PG_ID,
        T_FLOW_STAT_ALLOC_FAIL,
        T_FLOW_STAT_DEL_NON_EXIST,
        T_FLOW_STAT_ASSOC_NON_EXIST_ID,
        T_FLOW_STAT_ASSOC_OCC_ID,
        T_FLOW_STAT_NON_EXIST_ID,
        T_FLOW_STAT_BAD_PKT_FORMAT,
        T_FLOW_STAT_UNSUPP_PKT_FORMAT,
        T_FLOW_STAT_BAD_RULE_TYPE,
        T_FLOW_STAT_BAD_RULE_TYPE_FOR_IF,
        T_FLOW_STAT_BAD_RULE_TYPE_FOR_MODE,
        T_FLOW_STAT_FAILED_FIND_L3,
        T_FLOW_STAT_FAILED_FIND_L4,
        T_FLOW_STAT_PAYLOAD_TOO_SHORT,
        T_FLOW_STAT_NO_STREAMS_EXIST,
        T_FLOW_STAT_ALREADY_STARTED,
        T_FLOW_STAT_ALREADY_EXIST,
        T_FLOW_STAT_FAILED_CHANGE_IP_ID,
        T_FLOW_STAT_NO_FREE_HW_ID,
        T_FLOW_STAT_RX_CORE_START_FAIL,
        T_FLOW_STAT_BAD_HW_ID,
        T_INVALID
    };

 TrexException() : std::runtime_error(""), m_type(T_INVALID) {
    }

 TrexException(enum TrexExceptionTypes_t type=T_INVALID) : std::runtime_error(""), m_type(type) {
    }

 TrexException(const std::string &what, enum TrexExceptionTypes_t type=T_INVALID) : std::runtime_error(what), m_type(type) {
    }

    enum TrexExceptionTypes_t type() {return m_type;}

 private:
    enum TrexExceptionTypes_t m_type;
};

#endif /* __TREX_EXCEPTION_H__ */
