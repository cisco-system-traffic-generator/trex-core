#ifndef CMSG_MANAGER_H
#define CMSG_MANAGER_H
/*
 Hanoh Haim
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


#include "CRing.h"
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


class CMsgIns {
public:
    static  CMsgIns  * Ins();
    static  void Free();
    bool Create(uint8_t num_threads);
public:
    CMessagingManager * getRxDp(){
        return (&m_rx_dp);
    }
    CMessagingManager * getCpDp(){
        return (&m_cp_dp);
    }

    uint8_t get_num_threads(){
        return (m_rx_dp.get_num_threads());
    }

private:
    CMessagingManager m_rx_dp;
    CMessagingManager m_cp_dp;


private:
    /* one instance */
    static  CMsgIns  * m_ins; 
};

#endif
