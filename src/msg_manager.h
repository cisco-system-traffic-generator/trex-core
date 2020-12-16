#ifndef CMSG_MANAGER_H
#define CMSG_MANAGER_H
/*
 Hanoh Haim
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


#include "CRing.h"
#include "mbuf.h"
#include <string>


/* messages from CP->DP Ids */

struct CGenNodeMsgBase  {
    enum {
        NAT_FIRST     = 7,
        LATENCY_PKT   = 8,
    } msg_types;

public:
    uint8_t       m_msg_type; /* msg type */
};

/*

e.g DP with 4 threads
will look like this

      cp_to_dp

      master :push
      dpx    : pop

      -       --> dp0
cp    -       --> dp1
      -       --> dp2
      -       --> dp3

      dp_to_cp

      cp     : pop
      dpx    : push


       <-      -- dp0
cp     <-      -- dp1
       <-      -- dp2
       <-      -- dp3


*/

class CGenNode ;
typedef CTRingSp<CGenNode>  CNodeRing;

/* CP == latency thread
   DP == traffic pkt generator */
class CMessagingManager {
public:
    CMessagingManager(){
        m_cp_to_dp=0;
        m_dp_to_cp=0;
        m_num_dp_threads=0;
    }
    bool Create(uint8_t num_dp_threads,std::string name);
    void Delete();
    CNodeRing * getRingCpToDp(uint8_t thread_id);
    CNodeRing * getRingDpToCp(uint8_t thread_id);
    uint8_t get_num_threads(){
        return (m_num_dp_threads);
    }
private:
    CNodeRing * m_cp_to_dp;
    CNodeRing * m_dp_to_cp;
    uint8_t     m_num_dp_threads;
};


typedef CTRing<rte_mbuf_t> CMbufRing;


/** This is a manager that manages the rings each data plane core offers to other data plane cores 
 *in order to transfer packets 
 */
class CDpToDpMessagingManager {

public:
    CDpToDpMessagingManager() {
        // Initiate values to zeroes.
        m_dp_to_dp = nullptr;
        m_num_dp_threads = 0;
    }

    bool Create(uint8_t num_dp_threads, std::string name);

    void Delete();

    CMbufRing* getRingToDp(uint8_t thread_id);

private:

    CMbufRing* m_dp_to_dp;       // Pointer to the DP to DP packet ring.

    uint8_t    m_num_dp_threads; // Number of Data plane threads/cores.

};


class CMsgIns {
public:
    static  CMsgIns  * Ins();
    static  void Free();
    bool Create(uint8_t num_threads, bool is_software_rss);
    void Delete();

public:

    CMessagingManager * getRxDp(){
        return (&m_rx_dp);
    }

    CMessagingManager * getCpDp(){
        return (&m_cp_dp);
    }

    CMessagingManager * getCpRx(){
        return (&m_cp_rx);
    }

    /**
     * Get the Dp to Dp messaging manager.
     */
    CDpToDpMessagingManager* getDpDp() {
        return &m_dp_dp;
    }

    uint8_t get_num_threads(){
        return (m_rx_dp.get_num_threads());
    }

private:
    CMessagingManager       m_rx_dp;
    CMessagingManager       m_cp_dp;
    CMessagingManager       m_cp_rx;
    CDpToDpMessagingManager m_dp_dp;           // Dp to Dp messaging manager. This is created only if the mode is software_rss.
    bool                    m_is_software_rss; // Are we running TRex with software receive side scaling (RSS)?

private:
    /* one instance */
    static  CMsgIns  * m_ins;
};

#endif
