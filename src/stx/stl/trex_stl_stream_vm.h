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
#ifndef __TREX_STL_STREAM_VM_API_H__
#define __TREX_STL_STREAM_VM_API_H__

#include <string>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <assert.h>

#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/UdpHeader.h"
#include "common/Network/Packet/TcpHeader.h"
#include "pal_utl.h"
#include "mbuf.h"

#ifdef RTE_DPDK
#	include <rte_ip.h>
#endif /* RTE_DPDK */


class StreamVmInstructionFlowClient;


inline uint64_t   utl_split_limit(uint64_t limit, 
                           uint64_t phase, 
                           uint64_t step_mul){

    uint64_t per_core_limit = (limit / step_mul);

    if (phase == 0) {
        per_core_limit += (limit % step_mul);
    }

    if (per_core_limit == 0) {
        per_core_limit=1;
    }
    return ( per_core_limit);
}






/**
 * two functions ahead are used by both control plane and 
 * dataplane to allow fast inc/dec and handle overflow 
 *  
 * a - low bound 
 * b - high bound 
 * c - current value 
 * step - how many to advance / go back 
 */
static inline 
uint64_t inc_mod(uint64_t a, uint64_t b, uint64_t c, uint64_t step) {
    /* check if we have enough left for simple inc */
    uint64_t left = b - c;
    if (step <= left) {
        return (c + step);
    } else {
        return (a + (step - left - 1)); // restart consumes also 1
    }
}

static inline 
uint64_t dec_mod(uint64_t a, uint64_t b, uint64_t c, uint64_t step) {
    /* check if we have enough left for simple dec */
    uint64_t left = c - a;
    if (step <= left) {
        return (c - step);
    } else {
        return (b - (step - left - 1)); // restart consumes also 1
    }
}

/**
 * two functions ahead are used by both control plane and
 * dataplane to allow fast inc/dec and handle overflow
 *
 * size - value list size
 * index - current index in value list
 * step - how many to advance / go back
 */
static inline
uint64_t inc_mod_list(uint64_t size, uint64_t index, uint64_t step) {
    int next;
    next = (int)(index + step);
    if (next >= size) next = next - size;
    return next;
}

static inline
uint64_t dec_mod_list(uint64_t size, uint64_t index, uint64_t step) {
    int next;
    next = (int)(index - step);
    if (next < 0) next = size + index - step;
    return next;
}

/**
 * a slower set of functions that indicate an overflow
 * 
 */
static inline 
uint64_t inc_mod_of(uint64_t a, uint64_t b, uint64_t c, uint64_t step, bool &of) {
    /* check if we have enough left for simple inc */
    uint64_t left = b - c;
    if (step <= left) {
        of = false;
        return (c + step);
    } else {
        of = true;
        return (a + (step - left - 1)); // restart consumes also 1
    }
}

static inline 
uint64_t dec_mod_of(uint64_t a, uint64_t b, uint64_t c, uint64_t step, bool &of) {
    /* check if we have enough left for simple dec */
    uint64_t left = c - a;
    if (step <= left) {
        of = false;
        return (c - step);
    } else {
        of = true;
        return (b - (step - left - 1)); // restart consumes also 1
    }
}

//https://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/

//Used to seed the generator.
inline void fast_srand(uint32_t &g_seed, int seed ){
  g_seed = seed;
}


//fastrand routine returns one integer, similar output value range as C lib.

inline int fastrand(uint32_t &g_seed)
{
  g_seed = (214013*g_seed+2531011);
  return (g_seed>>16)&0x7FFF;
}         

static inline void vm_srand(uint32_t * per_thread_seed,uint64_t seedval)
{
    fast_srand( *per_thread_seed,seedval );
}

static inline uint32_t vm_rand16(uint32_t * per_thread_seed)
{
	return ( fastrand(*per_thread_seed));
}

static inline uint32_t vm_rand32(uint32_t * per_thread_seed)
{
	return ( (vm_rand16(per_thread_seed)<<16)+vm_rand16(per_thread_seed));
}

static inline uint64_t vm_rand64(uint32_t * per_thread_seed)
{
    uint64_t res;

    res=((uint64_t)vm_rand32(per_thread_seed)<<32)+vm_rand32(per_thread_seed);

    return (res);
}
                          

class StreamVm;

/* memory struct of rand_limit instruction */
/*******************************************************/

struct RandMemBss8 {
    uint8_t  m_val;
    uint8_t  m_cnt;
    uint32_t m_seed;
} __attribute__((packed));

struct RandMemBss16 {
    uint16_t  m_val;
    uint16_t  m_cnt;
    uint32_t  m_seed;
} __attribute__((packed));

struct RandMemBss32 {
    uint32_t  m_val;
    uint32_t  m_cnt;
    uint32_t  m_seed;
} __attribute__((packed));

struct RandMemBss64 {
    uint64_t  m_val;
    uint64_t  m_cnt;
    uint32_t  m_seed;
} __attribute__((packed));

struct StreamDPOpFlowRandLimit8 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint8_t m_limit;
    uint8_t m_min_val;
    uint8_t m_max_val;
    uint32_t m_seed;

public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var) {
        RandMemBss8 *p = (RandMemBss8 *)(flow_var + m_flow_offset);
        if (p->m_cnt  == m_limit){
            p->m_seed = m_seed;
            p->m_cnt=0;
        }
        uint32_t val = m_min_val + (vm_rand16(&p->m_seed)  % (int)(m_max_val - m_min_val + 1));
        p->m_val= (uint8_t)(val);
        p->m_cnt++;
    }
};

struct StreamDPOpFlowRandLimit16 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint16_t m_limit;
    uint16_t m_min_val;
    uint16_t m_max_val;

    uint32_t m_seed;
public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var) {
        RandMemBss16 *p = (RandMemBss16 *)(flow_var + m_flow_offset);
        if (p->m_cnt  == m_limit){
            p->m_seed = m_seed;
            p->m_cnt=0;
        }
        uint32_t val = m_min_val + (vm_rand16(&p->m_seed)  % (int)(m_max_val - m_min_val + 1)); 
        p->m_val= (uint16_t)(val);
        p->m_cnt++;
    }

};

struct StreamDPOpFlowRandLimit32 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint32_t m_limit;
    uint32_t m_min_val;
    uint32_t m_max_val;

    uint32_t m_seed;
public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var) {
        RandMemBss32 *p = (RandMemBss32 *)(flow_var + m_flow_offset);
        if (p->m_cnt  == m_limit){
            p->m_seed = m_seed;
            p->m_cnt=0;
        }
        uint32_t val = m_min_val + (vm_rand32(&p->m_seed)  % ((uint64_t)m_max_val - m_min_val + 1)); 
        p->m_val= val;
        p->m_cnt++;
    }
};

struct StreamDPOpFlowRandLimit64 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint64_t m_limit;
    uint64_t m_min_val;
    uint64_t m_max_val;

    uint32_t m_seed;
public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var) {
        RandMemBss64 *p = (RandMemBss64 *)(flow_var + m_flow_offset);
        if (p->m_cnt  == m_limit){
            p->m_seed = m_seed;
            p->m_cnt=0;
        }
        uint64_t val;
        if ((m_max_val - m_min_val) == UINT64_MAX) {
            val = vm_rand64(&p->m_seed);
        } else {
            val = m_min_val + ( vm_rand64(&p->m_seed)  % ( (uint64_t)m_max_val - m_min_val + 1) );
        }
        
        p->m_val= val;
        p->m_cnt++;
    }
};

/*******************************************************/

/* in memory program */

struct StreamDPOpFlowVar8Step {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint8_t m_min_val;
    uint8_t m_max_val;
    uint8_t m_step;
    uint16_t m_list_size;
    uint16_t m_list_index;
    uint8_t m_val_list[];
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint8_t *p = (flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = inc_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = inc_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint8_t *p = (flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = dec_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = dec_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_rand(uint8_t * flow_var,uint32_t *per_thread_random) {
        uint8_t *p=(flow_var+m_flow_offset);
        if (m_list_size == 0) {
            *p = m_min_val + (vm_rand16(per_thread_random) % (int)(m_max_val - m_min_val + 1));
        }
        else {
            *p = m_val_list[vm_rand16(per_thread_random) % m_list_size];
        }
    }

    inline uint16_t get_sizeof_list() {
        return sizeof(uint8_t) * m_list_size;
    }

} __attribute__((packed)) ;

struct StreamDPOpFlowVar16Step {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint16_t m_min_val;
    uint16_t m_max_val;
    uint16_t m_step;
    uint16_t m_list_size;
    uint16_t m_list_index;
    uint16_t m_val_list[];
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint16_t *p = (uint16_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = inc_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = inc_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint16_t *p = (uint16_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = dec_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = dec_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_rand(uint8_t * flow_var,uint32_t *per_thread_random) {
        uint16_t *p = (uint16_t *)(flow_var+m_flow_offset);
        if (m_list_size == 0) {
            *p = m_min_val + (vm_rand16(per_thread_random) % (int)(m_max_val - m_min_val + 1));
        }
        else {
            *p = m_val_list[vm_rand16(per_thread_random) % m_list_size];
        }
    }

    inline uint16_t get_sizeof_list() {
        return sizeof(uint16_t) * m_list_size;
    }

} __attribute__((packed)) ;

struct StreamDPOpFlowVar32Step {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint32_t m_min_val;
    uint32_t m_max_val;
    uint32_t m_step;
    uint16_t m_list_size;
    uint16_t m_list_index;
    uint32_t m_val_list[];
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint32_t *p = (uint32_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = inc_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = inc_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint32_t *p = (uint32_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = dec_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = dec_mod_list(m_list_size, m_list_index, m_step);
        }
    }


    inline void run_rand(uint8_t * flow_var,uint32_t *per_thread_random) {
        uint32_t *p = (uint32_t *)(flow_var+m_flow_offset);
        if (m_list_size == 0) {
            *p = m_min_val + (vm_rand32(per_thread_random) % ((uint64_t)(m_max_val) - m_min_val + 1));
        }
        else {
            *p = m_val_list[vm_rand16(per_thread_random) % m_list_size];
        }
    }

    inline uint16_t get_sizeof_list() {
        return sizeof(uint32_t) * m_list_size;
    }

} __attribute__((packed)) ;

struct StreamDPOpFlowVar64Step {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint64_t m_min_val;
    uint64_t m_max_val;
    uint64_t m_step;
    uint16_t m_list_size;
    uint16_t m_list_index;
    uint64_t m_val_list[];
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint64_t *p = (uint64_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = inc_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = inc_mod_list(m_list_size, m_list_index, m_step);
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint64_t *p = (uint64_t *)(flow_var + m_flow_offset);
        if (m_list_size == 0) {
            *p = dec_mod(m_min_val, m_max_val, *p, m_step);
        }
        else {
            *p = m_val_list[m_list_index];
            m_list_index = dec_mod_list(m_list_size, m_list_index, m_step);
        }
    }


    inline void run_rand(uint8_t * flow_var,uint32_t *per_thread_random) {
        uint64_t * p=(uint64_t *)(flow_var+m_flow_offset);

        if (m_list_size == 0) {
            if ((m_max_val - m_min_val) == UINT64_MAX) {
                *p = vm_rand64(per_thread_random);
            }
            else {
                *p = m_min_val + (vm_rand64(per_thread_random) % ((uint64_t)m_max_val - m_min_val + 1));
            }
        }
        else {
            *p = m_val_list[vm_rand16(per_thread_random) % m_list_size];
        }
    }

    inline uint16_t get_sizeof_list() {
        return sizeof(uint64_t) * m_list_size;
    }

} __attribute__((packed)) ;

///////////////////////////////////////////////////////////////////////


struct StreamDPOpPktWrBase {
    enum {
        PKT_WR_IS_BIG = 1
    }; /* for flags */

    uint8_t m_op;
    uint8_t m_flags;
    uint8_t  m_offset; 

public:
    bool is_big(){
        return ( (m_flags &StreamDPOpPktWrBase::PKT_WR_IS_BIG) == StreamDPOpPktWrBase::PKT_WR_IS_BIG ?true:false);
    }

} __attribute__((packed)) ;


struct StreamDPOpPktWr8 : public StreamDPOpPktWrBase {
    int8_t  m_val_offset;
    uint16_t m_pkt_offset;

public:
    void dump(FILE *fd,std::string opt);

    inline void wr(uint8_t * flow_var_base,uint8_t * pkt_base) {
        uint8_t * p_pkt      = (pkt_base+m_pkt_offset);
        uint8_t * p_flow_var = (flow_var_base+m_offset);
        *p_pkt=(*p_flow_var+m_val_offset);

    }


} __attribute__((packed)) ;


struct StreamDPOpPktWr16 : public StreamDPOpPktWrBase {
    uint16_t m_pkt_offset;
    int16_t  m_val_offset;
public:
    void dump(FILE *fd,std::string opt);

    inline void wr(uint8_t * flow_var_base,uint8_t * pkt_base) {
        uint16_t * p_pkt      = (uint16_t*)(pkt_base+m_pkt_offset);
        uint16_t * p_flow_var = (uint16_t*)(flow_var_base+m_offset);

        if ( likely(is_big())){
            *p_pkt=PKT_HTONS((*p_flow_var+m_val_offset));
        }else{
            *p_pkt=(*p_flow_var+m_val_offset);
        }

    }
} __attribute__((packed));

struct StreamDPOpPktWr32 : public StreamDPOpPktWrBase {
    uint16_t m_pkt_offset;
    int32_t  m_val_offset;
public:
    void dump(FILE *fd,std::string opt);

    inline void wr(uint8_t * flow_var_base,uint8_t * pkt_base) {
        uint32_t * p_pkt      = (uint32_t*)(pkt_base+m_pkt_offset);
        uint32_t * p_flow_var = (uint32_t*)(flow_var_base+m_offset);
        if ( likely(is_big())){
            *p_pkt=PKT_HTONL((*p_flow_var+m_val_offset));
        }else{
            *p_pkt=(*p_flow_var+m_val_offset);
        }
    }

} __attribute__((packed));

struct StreamDPOpPktWr64 : public StreamDPOpPktWrBase {
    uint16_t m_pkt_offset;
    int64_t  m_val_offset;

public:
    void dump(FILE *fd,std::string opt);

    inline void wr(uint8_t * flow_var_base,uint8_t * pkt_base) {
        uint64_t * p_pkt      = (uint64_t*)(pkt_base+m_pkt_offset);
        uint64_t * p_flow_var = (uint64_t*)(flow_var_base+m_offset);
        if ( likely(is_big())){
            *p_pkt=pal_ntohl64((*p_flow_var+m_val_offset));
        }else{
            *p_pkt=(*p_flow_var+m_val_offset);
        }
    }


} __attribute__((packed));


struct StreamDPOpPktWrMask  {

    enum {
        MASK_PKT_WR_IS_BIG = 1
    }; /* for flags */

    uint8_t  m_op;
    uint8_t  m_flags;
    uint8_t  m_var_offset; 
    int8_t   m_shift;    
    uint8_t  m_pkt_cast_size; 
    uint8_t  m_flowv_cast_size; /* 1,2,4 */
    uint16_t m_pkt_offset;
    uint32_t m_mask;
    int32_t  m_add_value;   
    bool is_big(){
        return ( (m_flags &StreamDPOpPktWrMask::MASK_PKT_WR_IS_BIG) == StreamDPOpPktWrMask::MASK_PKT_WR_IS_BIG ?true:false);
    }


public:
    void dump(FILE *fd,std::string opt);

    void wr(uint8_t * flow_var_base,
                   uint8_t * pkt_base) ;

} __attribute__((packed));


static inline uint16_t fast_csum(const void *iph, unsigned int ihl) {
    const uint16_t *ipv4 = (const uint16_t *)iph;

    uint32_t sum = 0;
    for (int i = 0; i < (ihl >> 1); i++) {
        sum += ipv4[i];
    }

    /* two folds are required */
    sum = (sum & 0xffff) + (sum >> 16);
    sum = (sum & 0xffff) + (sum >> 16);

    return (uint16_t)(~sum);
}


struct StreamDPOpIpv4Fix {
    uint8_t   m_op;
    uint16_t  m_offset;
public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * pkt_base) {

        uint8_t *ipv4 = pkt_base + m_offset;

        IPHeader *ipv4h = (IPHeader *)(pkt_base + m_offset);
        ipv4h->myChecksum = 0;

        if (ipv4h->myVer_HeaderLength == 0x45) {
            ipv4h->myChecksum = fast_csum(ipv4, 20);
        } else {
            ipv4h->myChecksum = fast_csum(ipv4, ipv4h->getHeaderLength());
        }
    }

} __attribute__((packed));


/* fix checksum using hardware */
struct StreamDPOpHwCsFix {
    uint8_t   m_op;
    uint16_t  m_l2_len;
    uint16_t  m_l3_len;
    uint64_t  m_ol_flags;   

public:
    void dump(FILE *fd,std::string opt);
    void run(uint8_t * pkt_base,rte_mbuf_t   * m){
        IPHeader *      ipv4 = (IPHeader *)(pkt_base+m_l2_len);
        union {
            TCPHeader *     tcp;
            UDPHeader *     udp;
        } u;

        u.tcp =  (TCPHeader*)(pkt_base+m_l2_len+m_l3_len);
        /* set the mbuf info */
         m->l2_len = m_l2_len;
         m->l3_len = m_l3_len;
         m->ol_flags |= m_ol_flags;
         if (m_ol_flags & PKT_TX_IPV4 ){ /* splitting to 4 instructions didn't improve performance .. */
             ipv4->ClearCheckSum();
             if ((m_ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM ){
                 u.tcp->setChecksumRaw(rte_ipv4_phdr_cksum((struct ipv4_hdr *)ipv4,m_ol_flags));
             }else{
                 u.udp->setChecksumRaw(rte_ipv4_phdr_cksum((struct ipv4_hdr *)ipv4,m_ol_flags));
             }

         }else{
             if ((m_ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM ){
                 u.tcp->setChecksumRaw(rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv4,m_ol_flags));
             }else{
                 u.udp->setChecksumRaw(rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv4,m_ol_flags));
             }
         }
    }
} __attribute__((packed));



/* flow varible of Client command */
struct StreamDPFlowClient {
    uint32_t cur_ip;
    uint16_t cur_port;
    uint32_t cur_flow_id;
} __attribute__((packed));


struct StreamDPOpClientsLimit {
    uint8_t m_op;
    uint8_t m_flow_offset; /* offset into the flow var  bytes */
    uint8_t m_flags;
    uint8_t m_pad;

    uint16_t    m_min_port;
    uint16_t    m_max_port;
    uint16_t    m_step_port;
    uint16_t    m_init_port;

    uint32_t    m_min_ip;
    uint32_t    m_max_ip;
    uint32_t    m_step_ip;
    uint32_t    m_init_ip;

    uint32_t    m_limit_flows; /* limit the number of flows */

public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var_base) {
        bool of;

        StreamDPFlowClient *lp = (StreamDPFlowClient *)(flow_var_base + m_flow_offset);

        /* first advance the outer var (IP) */
        lp->cur_ip = inc_mod_of(m_min_ip, m_max_ip, lp->cur_ip, m_step_ip, of);

        /* advance the port - most of the time zero but in extreme cases can have each time wrap around */
        lp->cur_port = inc_mod(m_min_port, m_max_port, lp->cur_port, m_step_port);
        
        /* in case of overflow of IP - add 1 to the port */
        if (of) {
            lp->cur_port = inc_mod(m_min_port, m_max_port, lp->cur_port, 1);
        }

        if (m_limit_flows) {
            lp->cur_flow_id++;
            if ( lp->cur_flow_id > m_limit_flows ){
                /* reset to the first flow */
                lp->cur_flow_id = 1;
                lp->cur_ip      = m_init_ip;
                lp->cur_port    = m_init_port;
            }
        }
    }


} __attribute__((packed));

struct StreamDPOpClientsUnLimit {
    enum __MIN_PORT {
        CLIENT_UNLIMITED_MIN_PORT = 1025
    };

    uint8_t m_op;
    uint8_t m_flow_offset; /* offset into the flow var  bytes */
    uint8_t m_flags;
    uint8_t m_pad;
    uint32_t    m_min_ip;
    uint32_t    m_max_ip;

public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var_base) {
        StreamDPFlowClient * lp= (StreamDPFlowClient *)(flow_var_base+m_flow_offset);
        lp->cur_ip++;
        if (lp->cur_ip > m_max_ip ) {
            lp->cur_ip= m_min_ip;
            lp->cur_port++;
            if (lp->cur_port == 0) {
                lp->cur_port = StreamDPOpClientsUnLimit::CLIENT_UNLIMITED_MIN_PORT;
            }
        }
    }

} __attribute__((packed));


struct StreamDPOpPktSizeChange  {
    uint8_t  m_op;
    uint8_t  m_flow_offset; /* offset to the flow var */

    inline void run(uint8_t * flow_var_base,uint16_t & new_size) {
        uint16_t * p_flow_var = (uint16_t*)(flow_var_base+m_flow_offset);
        new_size  = (*p_flow_var);
    }

    void dump(FILE *fd,std::string opt);


} __attribute__((packed)); ;


/* datapath instructions */
class StreamDPVmInstructions {
public:
    enum INS_TYPE {
        ditINC8     =7 ,
        ditINC16        ,
        ditINC32        ,
        ditINC64        ,

        ditDEC8         ,
        ditDEC16        ,
        ditDEC32        ,
        ditDEC64        ,

        ditRANDOM8      ,
        ditRANDOM16     ,
        ditRANDOM32     ,
        ditRANDOM64     ,

        ditFIX_IPV4_CS  ,
        ditFIX_HW_CS  ,

        itPKT_WR8       ,
        itPKT_WR16       ,
        itPKT_WR32       ,
        itPKT_WR64       ,
        itCLIENT_VAR       ,
        itCLIENT_VAR_UNLIMIT ,
        itPKT_SIZE_CHANGE ,

        /* inc/dec with step*/
        ditINC8_STEP         ,
        ditINC16_STEP        ,
        ditINC32_STEP        ,
        ditINC64_STEP        ,

        ditDEC8_STEP         ,
        ditDEC16_STEP        ,
        ditDEC32_STEP        ,
        ditDEC64_STEP        ,
        itPKT_WR_MASK        ,

        ditRAND_LIMIT8      ,
        ditRAND_LIMIT16     ,
        ditRAND_LIMIT32     ,
        ditRAND_LIMIT64     ,

    };


public:
    void clear();
    void add_command(void *buffer,uint16_t size);
    uint8_t * get_program();
    uint32_t get_program_size();


    void Dump(FILE *fd);


private:
    std::vector<uint8_t> m_inst_list;
};


class StreamDPVmInstructionsRunner {
public:
    StreamDPVmInstructionsRunner(){
        m_new_pkt_size=0;
        m_m=0;
    }

    void   slow_commands(uint8_t op_code,
                           uint8_t * flow_var, /* flow var */
                           uint8_t * pkt,
                           uint8_t * & p);

    inline void run(uint32_t * per_thread_random,
                    uint32_t program_size,
                    uint8_t * program,  /* program */
                    uint8_t * flow_var, /* flow var */
                    uint8_t * pkt);      /* pkt */

    inline uint16_t get_new_pkt_size(){
        return (m_new_pkt_size);
    }

    inline void set_mbuf(rte_mbuf_t        * mbuf){
        m_m=mbuf;
    }
    inline rte_mbuf_t        * get_mbuf(){
        return (m_m);
    }


private:
    uint16_t    m_new_pkt_size;
    rte_mbuf_t        * m_m;
};


typedef union  ua_ {
        StreamDPOpHwCsFix  * lpHwFix;
        StreamDPOpIpv4Fix   *lpIpv4Fix;
        StreamDPOpPktWr8     *lpw8;
        StreamDPOpPktWr16    *lpw16;
        StreamDPOpPktWr32    *lpw32;
        StreamDPOpPktWr64    *lpw64;
        StreamDPOpPktSizeChange    *lpw_pkt_size;

        StreamDPOpClientsLimit      *lpcl;
        StreamDPOpClientsUnLimit    *lpclu;

        StreamDPOpFlowVar8Step  *lpv8s;
        StreamDPOpFlowVar16Step *lpv16s;
        StreamDPOpFlowVar32Step *lpv32s;
        StreamDPOpFlowVar64Step *lpv64s;
        StreamDPOpPktWrMask     *lpwr_mask;

        StreamDPOpFlowRandLimit8  *lpv_rl8;
        StreamDPOpFlowRandLimit16 *lpv_rl16;
        StreamDPOpFlowRandLimit32 *lpv_rl32;
        StreamDPOpFlowRandLimit64 *lpv_rl64;

} ua_t ;



inline void StreamDPVmInstructionsRunner::run(uint32_t * per_thread_random,
                                              uint32_t program_size,
                                              uint8_t * program,  /* program */
                                              uint8_t * flow_var, /* flow var */
                                              uint8_t * pkt
                                              ){

    uint8_t * p=program;
    uint8_t * p_end=p+program_size;

    ua_t ua;

    while ( p < p_end) {
        uint8_t op_code=*p;
        switch (op_code) {

        case StreamDPVmInstructions::itCLIENT_VAR :      
            ua.lpcl =(StreamDPOpClientsLimit *)p;
            ua.lpcl->run(flow_var);
            p+=sizeof(StreamDPOpClientsLimit);
            break;

        case StreamDPVmInstructions::itCLIENT_VAR_UNLIMIT :      
            ua.lpclu =(StreamDPOpClientsUnLimit *)p;
            ua.lpclu->run(flow_var);
            p += sizeof(StreamDPOpClientsUnLimit);
            break;

        case StreamDPVmInstructions::ditINC8_STEP:
            ua.lpv8s =(StreamDPOpFlowVar8Step *)p;
            ua.lpv8s->run_inc(flow_var);
            p += sizeof(StreamDPOpFlowVar8Step) + ua.lpv8s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditINC16_STEP:
            ua.lpv16s =(StreamDPOpFlowVar16Step *)p;
            ua.lpv16s->run_inc(flow_var);
            p += sizeof(StreamDPOpFlowVar16Step) + ua.lpv16s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditINC32_STEP:
            ua.lpv32s =(StreamDPOpFlowVar32Step *)p;
            ua.lpv32s->run_inc(flow_var);
            p += sizeof(StreamDPOpFlowVar32Step) + ua.lpv32s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditINC64_STEP:
            ua.lpv64s =(StreamDPOpFlowVar64Step *)p;
            ua.lpv64s->run_inc(flow_var);
            p += sizeof(StreamDPOpFlowVar64Step) + ua.lpv64s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditDEC8_STEP:
            ua.lpv8s =(StreamDPOpFlowVar8Step *)p;
            ua.lpv8s->run_dec(flow_var);
            p += sizeof(StreamDPOpFlowVar8Step) + ua.lpv8s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditDEC16_STEP:
            ua.lpv16s =(StreamDPOpFlowVar16Step *)p;
            ua.lpv16s->run_dec(flow_var);
            p += sizeof(StreamDPOpFlowVar16Step) + ua.lpv16s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditDEC32_STEP:
            ua.lpv32s =(StreamDPOpFlowVar32Step *)p;
            ua.lpv32s->run_dec(flow_var);
            p += sizeof(StreamDPOpFlowVar32Step) + ua.lpv32s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditDEC64_STEP:
            ua.lpv64s =(StreamDPOpFlowVar64Step *)p;
            ua.lpv64s->run_dec(flow_var);
            p += sizeof(StreamDPOpFlowVar64Step) + ua.lpv64s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditRANDOM8 :
            ua.lpv8s =(StreamDPOpFlowVar8Step *)p;
            ua.lpv8s->run_rand(flow_var, per_thread_random);
            p += sizeof(StreamDPOpFlowVar8Step) + ua.lpv8s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditRANDOM16:
            ua.lpv16s =(StreamDPOpFlowVar16Step *)p;
            ua.lpv16s->run_rand(flow_var, per_thread_random);
            p += sizeof(StreamDPOpFlowVar16Step) + ua.lpv16s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditRANDOM32:
            ua.lpv32s =(StreamDPOpFlowVar32Step *)p;
            ua.lpv32s->run_rand(flow_var, per_thread_random);
            p += sizeof(StreamDPOpFlowVar32Step) + ua.lpv32s->get_sizeof_list();
            break;

        case StreamDPVmInstructions::ditRANDOM64:
            ua.lpv64s =(StreamDPOpFlowVar64Step *)p;
            ua.lpv64s->run_rand(flow_var, per_thread_random);
            p += sizeof(StreamDPOpFlowVar64Step) + ua.lpv64s->get_sizeof_list();
            break;

        case  StreamDPVmInstructions::ditFIX_IPV4_CS :
            ua.lpIpv4Fix =(StreamDPOpIpv4Fix *)p;
            ua.lpIpv4Fix->run(pkt);
            p+=sizeof(StreamDPOpIpv4Fix);
            break;

        case  StreamDPVmInstructions::ditFIX_HW_CS :
            ua.lpHwFix =(StreamDPOpHwCsFix *)p;
            ua.lpHwFix->run(pkt,m_m);
            p+=sizeof(StreamDPOpHwCsFix);
            break;

        case  StreamDPVmInstructions::itPKT_WR8  :
            ua.lpw8 =(StreamDPOpPktWr8 *)p;
            ua.lpw8->wr(flow_var,pkt);
            p+=sizeof(StreamDPOpPktWr8);
            break;

        case  StreamDPVmInstructions::itPKT_WR16 :
            ua.lpw16 =(StreamDPOpPktWr16 *)p;
            ua.lpw16->wr(flow_var,pkt);
            p+=sizeof(StreamDPOpPktWr16);
            break;

        case  StreamDPVmInstructions::itPKT_WR32 :
            ua.lpw32 =(StreamDPOpPktWr32 *)p;
            ua.lpw32->wr(flow_var,pkt);
            p+=sizeof(StreamDPOpPktWr32);
            break;

        case  StreamDPVmInstructions::itPKT_WR64 :      
            ua.lpw64 =(StreamDPOpPktWr64 *)p;
            ua.lpw64->wr(flow_var,pkt);
            p+=sizeof(StreamDPOpPktWr64);
            break;

        case  StreamDPVmInstructions::itPKT_SIZE_CHANGE :      
            ua.lpw_pkt_size =(StreamDPOpPktSizeChange *)p;
            ua.lpw_pkt_size->run(flow_var,m_new_pkt_size);
            p+=sizeof(StreamDPOpPktSizeChange);
            break;
        
        default:
            slow_commands(op_code,flow_var,pkt,p);
            break;
        }
    };
};




/**
 * interface for stream VM instruction
 * 
 */
class StreamVmInstruction {
public:

    enum INS_TYPE {
        itNONE         = 0,
        itFIX_IPV4_CS  = 4,
        itFLOW_MAN     = 5,
        itPKT_WR       = 6,
        itFLOW_CLIENT  = 7 ,
        itPKT_SIZE_CHANGE = 8,
        itPKT_WR_MASK     = 9,
        itFLOW_RAND_LIMIT = 10, /* random with limit & seed */
        itFIX_HW_CS       =11


    };

    typedef uint8_t instruction_type_t ;

    virtual instruction_type_t get_instruction_type() const = 0;

    virtual ~StreamVmInstruction();

    virtual void Dump(FILE *fd)=0;
    
    virtual StreamVmInstruction * clone() = 0;

    /* by default a regular instruction is not splitable for multicore */
    virtual bool need_split() const {
        return false;
    }

    bool is_var_instruction() const {
        instruction_type_t type = get_instruction_type();
        return ( (type == itFLOW_MAN) || (type == itFLOW_CLIENT) || (type == itFLOW_RAND_LIMIT) );
    }

    /* nothing to init */
    virtual uint8_t set_bss_init_value(uint8_t *p) {
        return (0);
    }

private:
    static const std::string m_name;
};

/**
 * abstract class that defines a flow var
 * 
 * @author imarom (23-Dec-15)
 */
class StreamVmInstructionVar : public StreamVmInstruction {

public:

    StreamVmInstructionVar(const std::string &var_name) : m_var_name(var_name) {

    }

    const std::string & get_var_name() const {
        return m_var_name;
    }


    /**
     * allows a var instruction to be updated 
     * for multicore (split) 
     * 
     */
    virtual void update(uint64_t phase, uint64_t step_mul) = 0;


public:
    
    /* flow var name */
    const std::string m_var_name;
};

/**
 * fix checksum for ipv4
 * 
 */
class StreamVmInstructionFixChecksumIpv4 : public StreamVmInstruction {
public:
    StreamVmInstructionFixChecksumIpv4(uint16_t offset) : m_pkt_offset(offset) {

    }

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFIX_IPV4_CS);
    }

    virtual void Dump(FILE *fd);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFixChecksumIpv4(m_pkt_offset);
    }

public:
    uint16_t m_pkt_offset; /* the offset of IPv4 header from the start of the packet  */
};

/**
 * fix Ipv6/Ipv6 TCP/UDP L4 headers using HW ofload to fix only IPv6 header use software it would be faster 
 * 
 */
class StreamVmInstructionFixHwChecksum : public StreamVmInstruction {
public:

    enum {
        L4_TYPE_UDP = 11,
        L4_TYPE_TCP = 13,
        L4_TYPE_IP  = 17

    }; /* for flags */

    StreamVmInstructionFixHwChecksum(uint16_t l2_len,
                                     uint16_t l3_len,
                                     uint8_t l4_type) { 
        m_l2_len = l2_len;
        m_l3_len = l3_len;
        m_l4_type   = l4_type;
    }

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFIX_HW_CS);
    }

    virtual void Dump(FILE *fd);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFixHwChecksum(m_l2_len,m_l3_len,m_l4_type);
    }

public:
    uint16_t  m_l2_len;
    uint16_t  m_l3_len;
    uint8_t   m_l4_type; /* should be either TCP or UDP - TBD could be fixed and calculated by a scan function */
};


/**
 * flow manipulation instruction
 * 
 * @author imarom (07-Sep-15)
 */
class StreamVmInstructionFlowMan : public StreamVmInstructionVar {

    friend class StreamVmInstructionFlowClient;

public:

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFLOW_MAN);
    }

    virtual bool need_split() const {
        /* random does not need split */
        return (m_op != FLOW_VAR_OP_RANDOM);
    }

    virtual uint64_t get_range() const {
        return (m_max_value - m_min_value + 1);
    }


    virtual uint8_t set_bss_init_value(uint8_t *p);


    /**
     * different types of operations on the object
     */
    enum flow_var_op_e {
        FLOW_VAR_OP_INC,
        FLOW_VAR_OP_DEC,
        FLOW_VAR_OP_RANDOM
    };

    StreamVmInstructionFlowMan(const std::string &var_name,
                               uint8_t size,
                               flow_var_op_e op,
                               uint64_t init_value,
                               uint64_t min_value,
                               uint64_t max_value,
                               uint64_t step=1) : StreamVmInstructionVar(var_name) {

        m_op          = op;
        m_size_bytes  = size;
        m_init_value  = init_value;
        m_min_value   = min_value;
        m_max_value   = max_value;
        m_value_list.clear();
        m_step        = step % get_range(); // support step overflow by modulu
        m_wa          = step / get_range(); // save the overflow count (for complex vars such as tuple)

        assert(m_init_value >= m_min_value);
        assert(m_init_value <= m_max_value);
    }

    StreamVmInstructionFlowMan(const std::string &var_name,
                               uint8_t size,
                               flow_var_op_e op,
                               std::vector<uint64_t> value_list,
                               uint64_t step=1) : StreamVmInstructionVar(var_name) {

        m_op          = op;
        m_size_bytes  = size;
        /* initialize init, min, max value as index of value_list */
        if (m_op == FLOW_VAR_OP_INC) {
            m_init_value = 0;
        }
        else {
            m_init_value = value_list.size() - 1;
        }
        m_min_value   = 0;
        m_max_value   = value_list.size() - 1;
        m_value_list  = value_list;
        m_step        = step % value_list.size(); // support step overflow by modulu
        m_wa          = step / value_list.size(); // save the overflow count (for complex vars such as tuple)
    }

    uint32_t get_wrap_arounds(uint32_t steps = 1) const {
        uint32_t wa = m_wa * steps;

        wa += (steps * m_step) / get_range();
        
        return wa;
    }
    
    virtual void update(uint64_t phase, uint64_t step_mul) {

        /* update the init value to be with a phase */
        m_init_value = peek_next(phase);

        /* multiply the step */
        
        /* reconstruct the original step, multiply and recalculate */
        uint64_t step = (m_wa * get_range()) + m_step;
        m_step = (step * step_mul) % get_range();
        m_wa   = (step * step_mul) / get_range();
        
        assert(m_init_value >= m_min_value);
        assert(m_init_value <= m_max_value);
    }


    uint64_t peek_next(uint64_t skip = 1) const {
        bool dummy;
        return peek(skip, true, false, dummy);
    }

    uint64_t peek_prev(uint64_t skip = 1) const {
        bool dummy;
        return peek(skip, false, false, dummy);
    }


    virtual void Dump(FILE *fd);

    void sanity_check(uint32_t ins_id,StreamVm *lp);

    virtual StreamVmInstruction * clone() {
        if (m_value_list.empty()) {
            return new StreamVmInstructionFlowMan(m_var_name,
                                                  m_size_bytes,
                                                  m_op,
                                                  m_init_value,
                                                  m_min_value,
                                                  m_max_value,
                                                  m_step);
        }
        else {
            return new StreamVmInstructionFlowMan(m_var_name,
                                                  m_size_bytes,
                                                  m_op,
                                                  m_value_list,
                                                  m_step);
        }
    }


protected:

    /* fetch the next value in the variable (used for core phase and etc.) */
    uint64_t peek(int skip, bool forward, bool carry, bool &of) const {

        if (m_op == FLOW_VAR_OP_RANDOM) {
            return m_init_value;
        }

        assert( (m_op == FLOW_VAR_OP_INC) || (m_op == FLOW_VAR_OP_DEC) );
        bool add = ( (m_op == FLOW_VAR_OP_INC) ? forward : !forward );

        uint64_t next_step = m_step * skip;
        if (carry) {
            next_step++;
        }
        
        next_step = next_step % get_range();

        if (add) {
            return inc_mod_of(m_min_value, m_max_value, m_init_value, next_step, of);
        } else {
            return dec_mod_of(m_min_value, m_max_value, m_init_value, next_step, of);
        }
    }


private:
    void sanity_check_valid_size(uint32_t ins_id,StreamVm *lp);
    void sanity_check_valid_opt(uint32_t ins_id,StreamVm *lp);
    void sanity_check_valid_range(uint32_t ins_id,StreamVm *lp);

public:

    /* flow var size */
    uint8_t m_size_bytes;

    /* type of op */
    flow_var_op_e m_op;

    /* range */
    uint64_t m_init_value;
    uint64_t m_min_value;
    uint64_t m_max_value;

    /* value list */
    std::vector<uint64_t> m_value_list;

    uint64_t m_step;
    uint32_t m_wa;
};


/**
 * flow var random with limit 
 * 
 * @author hhaim 9/2016
 */
class StreamVmInstructionFlowRandLimit : public StreamVmInstructionVar {

public:

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFLOW_RAND_LIMIT);
    }

    virtual bool need_split() const {
        return true;
    }

    StreamVmInstructionFlowRandLimit(const std::string &var_name,
                                     uint8_t  size,
                                     uint64_t limit,
                                     uint64_t min_value,
                                     uint64_t max_value,
                                     uint64_t seed
                                     ) : StreamVmInstructionVar(var_name) {

        m_size_bytes = size;
        m_seed       = seed;
        m_limit      = limit;
        m_min_value  = min_value;
        m_max_value  = max_value;
    }

    virtual void Dump(FILE *fd);

    void sanity_check(uint32_t ins_id,StreamVm *lp);

    virtual uint8_t set_bss_init_value(uint8_t *p);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFlowRandLimit(m_var_name,
                                                    m_size_bytes,
                                                    m_limit,
                                                    m_min_value,
                                                    m_max_value,
                                                    m_seed);
    }

    virtual void update(uint64_t phase, uint64_t step_mul) {

        /* phase */
        m_seed = m_seed * ( ( (phase + 1) * 514229 )  & 0xFFFFFFFF );

        /* limit */
        m_limit = utl_split_limit(m_limit, phase, step_mul);
    }

private:
    void sanity_check_valid_size(uint32_t ins_id,StreamVm *lp);

public:

    uint64_t       m_limit;
    uint64_t       m_min_value;
    uint64_t       m_max_value;

    uint64_t       m_seed;
    uint8_t        m_size_bytes;
};



/**
 * write flow-write-mask  to packet, hhaim
 * 
 *  uint32_t var_tmp=(uint32_t )(flow_var_t size )flow_var;
 *  if (shift){
 *     var_tmp=var_tmp<<shift
 *  }else{
 *      var_tmp=var_tmp>>shift
 *  }
 *  
 *  pkt_data=read_pkt_size()
 *  pkt_data =  (pkt_data & ~mask) |(var_tmp & mask)   
 *  write_pkt(pkt_data)
 * 
 */
class StreamVmInstructionWriteMaskToPkt : public StreamVmInstruction {
public:

    StreamVmInstructionWriteMaskToPkt(const std::string &flow_var_name,
                                  uint16_t           pkt_offset,
                                  uint8_t            pkt_cast_size, /* valid 1,2,4 */
                                  uint32_t           mask,
                                  int                shift,       /* positive is shift left, negetive shift right */
                                  int32_t            add_value = 0,
                                  bool               is_big_endian = true) :
                                                        m_flow_var_name(flow_var_name),
                                                        m_pkt_offset(pkt_offset),
                                                        m_pkt_cast_size(pkt_cast_size), 
                                                        m_mask(mask),
                                                        m_shift(shift),       
                                                        m_add_value(add_value),
                                                        m_is_big_endian(is_big_endian) {}

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itPKT_WR_MASK);
    }

    virtual void Dump(FILE *fd);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionWriteMaskToPkt(m_flow_var_name,
                                                     m_pkt_offset,
                                                     m_pkt_cast_size,
                                                     m_mask,
                                                     m_shift,
                                                     m_add_value,
                                                     m_is_big_endian);
    }

public:

    /* flow var name to write */
    std::string   m_flow_var_name;

    /* where to write */
    uint16_t      m_pkt_offset;
    uint8_t       m_pkt_cast_size; /* valid 1,2,4 */

    uint32_t      m_mask;
    int           m_shift;
    int           m_add_value;
    bool          m_is_big_endian;
};




/**
 * flow client instruction - save state for client range and port range 
 * 
 * @author hhaim
 */
class StreamVmInstructionFlowClient : public StreamVmInstructionVar {

public:
    enum client_flags_e {
        CLIENT_F_UNLIMITED_FLOWS=1, /* unlimited number of flow, don't care about ports  */
    };


    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFLOW_CLIENT);
    }

    virtual bool need_split() const {
        return true;
    }

    StreamVmInstructionFlowClient(const std::string &var_name,
                                  uint32_t client_min_value,
                                  uint32_t client_max_value,
                                  uint16_t port_min,
                                  uint16_t port_max,
                                  uint32_t limit_num_flows, /* zero means don't limit */
                                  uint16_t flags
                               ) : StreamVmInstructionVar(var_name),
                                   m_ip("ip", 4, StreamVmInstructionFlowMan::FLOW_VAR_OP_INC, client_min_value, client_min_value, client_max_value, 1),
                                   m_port("port", 2, StreamVmInstructionFlowMan::FLOW_VAR_OP_INC, port_min, port_min, port_max, 1) { 

        m_limit_num_flows = limit_num_flows;
        m_flags = flags;
        
        /* construct as per single core */
        update(0, 1);
    }

    StreamVmInstructionFlowClient(const StreamVmInstructionFlowClient &other) :StreamVmInstructionVar(other.m_var_name),
                                                                               m_ip(other.m_ip),
                                                                               m_port(other.m_port) {
        m_limit_num_flows = other.m_limit_num_flows;
        m_flags = other.m_flags;
    }

    virtual void Dump(FILE *fd);

    static uint8_t get_flow_var_size(){
        return  (4+2+4);
    }

    uint32_t get_ip_range() const {
        return m_ip.get_range();
    }

    uint16_t get_port_range() const {
        return m_port.get_range();
    }

    virtual uint64_t get_range() const {
        return get_ip_range();
    }


    virtual uint8_t set_bss_init_value(uint8_t *p);


    bool is_unlimited_flows(){
        return ( (m_flags &   StreamVmInstructionFlowClient::CLIENT_F_UNLIMITED_FLOWS ) == 
                  StreamVmInstructionFlowClient::CLIENT_F_UNLIMITED_FLOWS );
    }

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFlowClient(*this);
    }

    
    virtual void update(uint64_t phase, uint64_t step_mul) {

        /* calculate the phase BEFORE adjusting the outer var (IP) */
        uint16_t port_phase = m_ip.get_wrap_arounds(phase);
        
        /* update outer var */
        m_ip.update(phase, step_mul);

        /* inner var should advance as the whole wrap arounds */
        uint16_t port_step_mul  = m_ip.get_wrap_arounds();

        /* ugly, but we need to set the reference to 1 (default is zero) */
        m_port.m_step = 1;
        m_port.update(port_phase, port_step_mul);
    
        /* update the limit per core */
        if (m_limit_num_flows) {
            m_limit_num_flows = utl_split_limit(m_limit_num_flows, phase, step_mul);
        }
        
    }


protected:

    void peek_prev(uint32_t &next_ip, uint16_t &next_port, int skip = 1) {
        peek(next_ip, next_port, skip, false);
    }

    void peek_next(uint32_t &next_ip, uint16_t &next_port, int skip = 1) {
        peek(next_ip, next_port, skip, true);
    }

    /**
     * defines a froward/backward method
     * 
     */
    void peek(uint32_t &next_ip, uint16_t &next_port, int skip = 1, bool forward = true) const {
        bool of = false;

        next_ip = m_ip.peek(skip, forward, false, of);
        next_port = m_port.peek(skip, forward, of, of);

    }

public:

    StreamVmInstructionFlowMan   m_ip;
    StreamVmInstructionFlowMan   m_port;

    uint32_t  m_limit_num_flows;   // number of flows
    uint16_t  m_flags;
};



class VmFlowVarRec {
public:
    uint32_t    m_offset;
    union {
        StreamVmInstructionFlowMan * m_ins_flowv;
        StreamVmInstructionFlowClient * m_ins_flow_client;
        StreamVmInstructionFlowRandLimit * m_ins_flow_rand_limit;
    } m_ins;
    uint8_t    m_size_bytes;
};




/**
 * write flow var to packet, hhaim
 * 
 */
class StreamVmInstructionWriteToPkt : public StreamVmInstruction {
public:

      StreamVmInstructionWriteToPkt(const std::string &flow_var_name,
                                    uint16_t           pkt_offset,
                                    int32_t            add_value = 0,
                                    bool               is_big_endian = true) :

                                                        m_flow_var_name(flow_var_name),
                                                        m_pkt_offset(pkt_offset),
                                                        m_add_value(add_value),
                                                        m_is_big_endian(is_big_endian) {}

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itPKT_WR);
    }

    virtual void Dump(FILE *fd);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionWriteToPkt(m_flow_var_name,
                                                 m_pkt_offset,
                                                 m_add_value,
                                                 m_is_big_endian);
    }

public:

    /* flow var name to write */
    std::string   m_flow_var_name;

    /* where to write */
    uint16_t      m_pkt_offset;

    /* add/dec value from field when writing */
    int32_t       m_add_value;

    /* writing endian */
    bool         m_is_big_endian;
};



/**
 * change packet size,  
 * 
 */
class StreamVmInstructionChangePktSize : public StreamVmInstruction {
public:

    StreamVmInstructionChangePktSize(const std::string &flow_var_name) :

                                                        m_flow_var_name(flow_var_name)
                                                        {}

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itPKT_SIZE_CHANGE );
    }

    virtual void Dump(FILE *fd);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionChangePktSize(m_flow_var_name);
    }

public:

    /* flow var name to write */
    std::string   m_flow_var_name;
};


/**
 * describes a VM program for DP 
 * 
 */

class StreamVmDp {
public:
    StreamVmDp(){
        m_bss_ptr=NULL; 
        m_program_ptr =NULL ;
        m_bss_size=0;
        m_program_size=0;
        m_max_pkt_offset_change=0;
        m_prefix_size = 0;
        m_is_pkt_size_var=false;
    }

    StreamVmDp( uint8_t * bss,
                uint16_t bss_size,
                uint8_t * prog,
                uint16_t prog_size,
                uint16_t max_pkt_offset,
                uint16_t prefix_size,
                bool a_is_pkt_size_var,
                bool a_is_random_seed
                ){

        if (bss) {
            assert(bss_size);
            m_bss_ptr=(uint8_t*)malloc(bss_size);
            assert(m_bss_ptr);
            memcpy(m_bss_ptr,bss,bss_size);
            m_bss_size=bss_size;
        }else{
            m_bss_ptr=NULL; 
            m_bss_size=0;
        }

        if (prog) {
            assert(prog_size);
            m_program_ptr=(uint8_t*)malloc(prog_size);
            memcpy(m_program_ptr,prog,prog_size);
            m_program_size = prog_size;
        }else{
            m_program_ptr = NULL;
            m_program_size=0;
        }

        m_max_pkt_offset_change = max_pkt_offset;
        m_prefix_size = prefix_size;
        m_is_pkt_size_var=a_is_pkt_size_var;
        m_is_random_seed=a_is_random_seed;
    }

    ~StreamVmDp(){
        if (m_bss_ptr) {
            free(m_bss_ptr);
            m_bss_ptr=0;
            m_bss_size=0;
        }
        if (m_program_ptr) {
            free(m_program_ptr);
            m_program_size=0;
            m_program_ptr=0;
        }
    }

    StreamVmDp * clone() const {
        StreamVmDp * lp = new StreamVmDp(m_bss_ptr,
                                         m_bss_size,
                                         m_program_ptr,
                                         m_program_size,
                                         m_max_pkt_offset_change,
                                         m_prefix_size,
                                         m_is_pkt_size_var,
                                         m_is_random_seed
                                         );
        assert(lp);
        return (lp);
    }

    uint8_t* clone_bss(){
        assert(m_bss_size>0);
        uint8_t *p=(uint8_t *)malloc(m_bss_size);
        assert(p);
        memcpy(p,m_bss_ptr,m_bss_size);
        return (p);
    }

    uint16_t get_bss_size(){
        return(m_bss_size);
    }

    uint8_t*  get_bss(){
        return (m_bss_ptr);
    }

    uint8_t*  get_program(){
        return (m_program_ptr);
    }

    uint16_t  get_program_size(){
        return (m_program_size);
    }

    uint16_t get_max_packet_update_offset(){
        return (m_max_pkt_offset_change);
    }

    uint16_t get_prefix_size() {
        return m_prefix_size;
    }

    void set_prefix_size(uint16_t prefix_size) {
        m_prefix_size = prefix_size;
    }

    void set_pkt_size_is_var(bool pkt_size_var){
        m_is_pkt_size_var=pkt_size_var;
    }
    bool is_pkt_size_var(){
        return (m_is_pkt_size_var);
    }
    bool is_random_seed(){
        return (m_is_random_seed);
    }


private:
    uint8_t *  m_bss_ptr; /* pointer to the data section */
    uint8_t *  m_program_ptr; /* pointer to the program */
    uint16_t   m_bss_size;
    uint16_t   m_program_size; /* program size*/
    uint16_t   m_max_pkt_offset_change;
    uint16_t   m_prefix_size;
    bool       m_is_pkt_size_var;
    bool       m_is_random_seed;

};

class TrexStreamPktLenData {
 public:
    TrexStreamPktLenData() {
        m_expected_pkt_len = 0;
    }

    double m_expected_pkt_len;
    uint16_t m_min_pkt_len;
    uint16_t m_max_pkt_len;
};

/**
 * describes a VM program
 * 
 */
class StreamVm {

public:
    enum STREAM_VM {
        svMAX_FLOW_VAR   = 64, /* maximum flow varible */
        svMAX_PACKET_OFFSET_CHANGE = 512
    };



    StreamVm(){
        m_prefix_size=0;
        m_bss=0;
        m_pkt_size=0;
        m_cur_var_offset=0;

        m_is_random_var      = false;
        m_is_change_pkt_size = false;
        m_is_split_needed      = false;

        m_is_compiled = false;
        m_pkt=0;
    }


    /**
     * calculate the expected packet size 
     * might be different from regular_pkt_size 
     * if the VM changes the packet length (random) 
     * 
     */
    void calc_pkt_len_data(uint16_t regular_pkt_size, TrexStreamPktLenData &pkt_len_data) const;


    StreamVmDp * generate_dp_object(){

        if (!m_is_compiled) {
            return NULL;
        }

        StreamVmDp * lp= new StreamVmDp(get_bss_ptr(),
                                        get_bss_size(),
                                        get_dp_instruction_buffer()->get_program(),
                                        get_dp_instruction_buffer()->get_program_size(),
                                        get_max_packet_update_offset(),
                                        get_prefix_size(),
                                        is_var_pkt_size(),
                                        m_is_random_var
                                        );
        assert(lp);
        return (lp);
        
    }

    /**
     * clone VM instructions
     * 
     */
    void clone(StreamVm &other) const;

    
    bool is_vm_empty() const {
        return (m_inst_list.size() == 0);
    }

    /**
     * return true if the VM is splitable
     * for multicore
     */
    bool need_split() const {
        return m_is_split_needed;
    }

    /**
     * add new instruction to the VM
     * 
     */
    void add_instruction(StreamVmInstruction *inst);

    /**
     * get const access to the instruction list
     * 
     */
    const std::vector<StreamVmInstruction *> & get_instruction_list();

    StreamDPVmInstructions  *           get_dp_instruction_buffer();

    uint16_t                                  get_bss_size(){
        return (m_cur_var_offset );
    }

    uint8_t *                                 get_bss_ptr(){
        return (m_bss );
    }


    uint16_t                                  get_max_packet_update_offset(){
        return ( m_max_field_update );
    }

    uint16_t get_prefix_size() {
        return m_prefix_size;
    }

    bool is_var_pkt_size(){
        return (m_is_change_pkt_size);
    }
    

    bool is_compiled() const {
        return m_is_compiled;
    }

    /**
     * compile the VM 
     * return true of success, o.w false 
     * 
     */
    void compile(uint16_t pkt_len);

    ~StreamVm();

    void Dump(FILE *fd);

    /* raise exception */
    void  err(const std::string &err);

    void set_pkt(uint8_t *pkt){
        m_pkt=pkt;
    }


    /**
     * return a pointer to a flow var / client var 
     * by name if exists, otherwise NULL 
     * 
     */
    StreamVmInstructionVar * lookup_var_by_name(const std::string &var_name);

private:

    /* lookup for varible offset, */
    bool var_lookup(const std::string &var_name,VmFlowVarRec & var);

    void  var_clear_table();

    bool  var_add(const std::string &var_name,VmFlowVarRec & var);

    uint16_t get_var_offset(const std::string &var_name);
    
    void build_flow_var_table() ;

    void build_bss();

    void build_program();

    void alloc_bss();

    void free_bss();

private:

    void clean_max_field_cnt();

    void add_field_cnt(uint16_t new_cnt);

private:
    bool                               m_is_random_var;
    bool                               m_is_change_pkt_size;
    bool                               m_is_split_needed;
    bool                               m_is_compiled;
    uint16_t                           m_prefix_size;

    uint16_t                           m_pkt_size;
    TrexStreamPktLenData               m_pkt_len_data;

    uint16_t                           m_cur_var_offset;
    uint16_t                           m_max_field_update; /* the location of the last byte that is going to be changed in the packet */ 

    std::vector<StreamVmInstruction *> m_inst_list;
    std::unordered_map<std::string, VmFlowVarRec> m_flow_var_offset;
    uint8_t *                          m_bss;

    StreamDPVmInstructions             m_instructions;
    
    uint8_t                            *m_pkt;

    
    
};

#endif /* __TREX_STREAM_VM_API_H__ */
