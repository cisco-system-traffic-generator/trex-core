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

#ifndef RX_CHECK_HEADER
#define RX_CHECK_HEADER

#include <stdint.h>
#include <stdio.h>
#include <common/bitMan.h> 
#include <common/Network/Packet/CPktCmn.h>

#define RX_CHECK_LEN    (sizeof(struct CRx_check_header))

// IPv4 option type:
//   bit[0] = copy flag (0=do not copy to fragment)
//   bit[1:2] = option class (2=debugging)
//   bit[3:7] = option number (23=TRex)
#define RX_CHECK_V4_OPT_TYPE    0x57
#define RX_CHECK_V4_OPT_LEN   RX_CHECK_LEN

// IPv6 extension header:
//   type = 60 (destination options)
#define RX_CHECK_V6_OPT_TYPE    0x3c
#define RX_CHECK_V6_OPT_LEN ((RX_CHECK_LEN - 8) / 8)

// IPv6 subfield option type:
//   bit[0:1] = unrecognied option action (00=skip option)
//   bit[2] = change allowed flag (0=no changes enroute)
//   bit[3:7] = option number (23=TRex)
#define RX_CHECK_V6_SF_OPT_TYPE    0x17
#define RX_CHECK_V6_SF_OPT_LEN   (RX_CHECK_LEN - 2)

// Magic field overlays IPv6 subfield option type
#define RX_CHECK_MAGIC  (((RX_CHECK_V6_SF_OPT_LEN & 0xff) << 8) | \
                                           RX_CHECK_V6_SF_OPT_TYPE)

struct  CRx_check_header {
    // 24 bytes, watch alignment and keep this in 8 byte increments
    uint8_t m_option_type;
    uint8_t m_option_len;
    uint16_t m_magic;

    uint32_t m_time_stamp;
    uint64_t m_flow_id; // include thread_id , template_id
    uint16_t m_pkt_id;    /* pkt_id inside the flow - zero base 0,1,2*/
    uint16_t m_flow_size; /* how many packets in flow */

    uint8_t m_flags;
    uint8_t m_template_id;
    uint16_t m_aging_sec;

public:
    bool is_fif_dir(void){
        return (m_pkt_id==0?true:false);
    }
    bool is_eof_dir(void){
        return ( (m_pkt_id==m_flow_size-1)?true:false);
    }

    void set_dir(int dir){
        btSetMaskBit8(m_flags,0,0,dir?1:0);
    }

    int get_dir(void){
        return (btGetMaskBit8(m_flags,0,0) ? 1:0);
    }
    void clean(){
        memset(this,0,sizeof(CRx_check_header));
    }

    /* need to mark if we expect to see both sides of the flow, this is know offline  */
    void set_both_dir(int both){
        btSetMaskBit8(m_flags,1,1,both?1:0);
    }

    int get_both_dir(void){
        return (btGetMaskBit8(m_flags,1,1) ? 1:0);
    }



    void dump(FILE *fd);
};

#endif


