/*
 hhaim
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
#ifndef __TREX_STACK_RC_H__
#define __TREX_STACK_RC_H__


/**
 * a  class for a RC results of batch commands
 *
 */
class TrexStackResultsRC {

public:
    TrexStackResultsRC() {
        m_in_progress =false;
        m_total_cmds = 0;
        m_total_exec_cmds = 0;
        m_total_errs_cmds = 0;
    }

public:
    bool      m_in_progress;
    uint32_t  m_total_cmds;      /* valid in case in_progress is true */
    uint32_t  m_total_exec_cmds; /* total exec */
    uint32_t  m_total_errs_cmds; /* total errors */
};


#endif /* __TREX_STACK_RC_H__ */

