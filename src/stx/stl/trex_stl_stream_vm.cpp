/*
 Itay Marom
 Hanoch Haim 
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

#include <sstream>
#include <assert.h>
#include <iostream>
#include <inttypes.h>
#include <numeric>

#include "common/Network/Packet/IPHeader.h"
#include "common/basic_utils.h"

#include "trex_stl.h"
#include "trex_stl_stream.h"
#include "trex_stl_stream_vm.h"

static const uint32_t roce_crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static inline uint32_t HOT_FUNC roce_crc32_update_byte(uint32_t crc, uint8_t byte) {
    return roce_crc32_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
}

static bool HOT_FUNC roce_mbuf_get_u8(const rte_mbuf_t *m, uint16_t offset, uint8_t &value) {
    const rte_mbuf_t *seg = m;
    uint32_t pos = offset;

    while (seg != NULL) {
        if (pos < seg->data_len) {
            value = *(rte_pktmbuf_mtod(seg, uint8_t*) + pos);
            return true;
        }
        pos -= seg->data_len;
        seg = seg->next;
    }

    return false;
}

static bool HOT_FUNC roce_mbuf_write_u8(rte_mbuf_t *m, uint16_t offset, uint8_t value) {
    rte_mbuf_t *seg = m;
    uint32_t pos = offset;

    while (seg != NULL) {
        if (pos < seg->data_len) {
            *(rte_pktmbuf_mtod(seg, uint8_t*) + pos) = value;
            return true;
        }
        pos -= seg->data_len;
        seg = seg->next;
    }

    return false;
}

static inline uint8_t HOT_FUNC roce_icrc_invariant_byte(uint32_t pkt_offset,
                                                       uint8_t byte,
                                                       bool is_ipv6,
                                                       uint16_t l2_len,
                                                       uint16_t l4_offset,
                                                       uint16_t bth_offset) {
    if (is_ipv6) {
        if (pkt_offset == l2_len) {
            return 0x6f;
        }
        if ((pkt_offset >= (uint32_t)l2_len + 1) && (pkt_offset <= (uint32_t)l2_len + 3)) {
            return 0xff;
        }
        if (pkt_offset == (uint32_t)l2_len + 7) {
            return 0xff;
        }
    } else {
        if ((pkt_offset == (uint32_t)l2_len + 1) ||
            (pkt_offset == (uint32_t)l2_len + 8) ||
            (pkt_offset == (uint32_t)l2_len + 10) ||
            (pkt_offset == (uint32_t)l2_len + 11)) {
            return 0xff;
        }
    }

    if ((pkt_offset == (uint32_t)l4_offset + 6) ||
        (pkt_offset == (uint32_t)l4_offset + 7) ||
        (pkt_offset == (uint32_t)bth_offset + 4)) {
        return 0xff;
    }

    return byte;
}

uint32_t trex_roce_v2_calc_icrc_bytes(const uint8_t *pkt, uint32_t pkt_len,
                                    uint16_t l2_len, uint16_t l3_len, uint16_t bth_offset) {
    if (!pkt || pkt_len < 4 || l2_len >= pkt_len) { return 0; }
    bool ipv6 = (pkt[l2_len] >> 4) == 6;
    uint32_t crc = 0xffffffff;
    for (unsigned i = 0; i < 8; ++i) { crc = roce_crc32_update_byte(crc, 0xff); }
    for (uint32_t i = l2_len; i < pkt_len - 4; ++i) {
        crc = roce_crc32_update_byte(crc, roce_icrc_invariant_byte(i, pkt[i], ipv6,
                                                                l2_len, l2_len + l3_len, bth_offset));
    }
    return crc ^ 0xffffffff;
}

void HOT_FUNC trex_roce_v2_update_icrc(rte_mbuf_t *m,
                                       uint16_t l2_len,
                                       uint16_t l3_len,
                                       uint16_t bth_offset) {
    if (unlikely((m == NULL) || (m->pkt_len < 4))) {
        return;
    }

    uint8_t first_l3_byte = 0;
    if (unlikely(!roce_mbuf_get_u8(m, l2_len, first_l3_byte))) {
        return;
    }

    bool is_ipv6 = ((first_l3_byte >> 4) == 6);
    uint16_t l4_offset = l2_len + l3_len;
    uint32_t crc = 0xffffffff;

    for (int i = 0; i < 8; i++) {
        crc = roce_crc32_update_byte(crc, 0xff);
    }

    uint32_t global = 0;
    uint32_t icrc_offset = m->pkt_len - 4;
    for (rte_mbuf_t *seg = m; seg != NULL; seg = seg->next) {
        uint8_t *data = rte_pktmbuf_mtod(seg, uint8_t*);
        for (uint16_t i = 0; i < seg->data_len; i++, global++) {
            if ((global < l2_len) || (global >= icrc_offset)) {
                continue;
            }
            uint8_t byte = roce_icrc_invariant_byte(global,
                                                    data[i],
                                                    is_ipv6,
                                                    l2_len,
                                                    l4_offset,
                                                    bth_offset);
            crc = roce_crc32_update_byte(crc, byte);
        }
    }

    crc ^= 0xffffffff;
    bool written = roce_mbuf_write_u8(m, icrc_offset, crc & 0xff);
    written &= roce_mbuf_write_u8(m, icrc_offset + 1, (crc >> 8) & 0xff);
    written &= roce_mbuf_write_u8(m, icrc_offset + 2, (crc >> 16) & 0xff);
    written &= roce_mbuf_write_u8(m, icrc_offset + 3, (crc >> 24) & 0xff);
    assert(written);
    (void)written;
}


/**
 * provides some tools for the fast rand function 
 * that is used by the datapath 
 * some features of this function is different 
 * from a regular random 
 * (such as average can be off by few percents) 
 * 
 * @author imarom (7/24/2016)
 */
class FastRandUtils {
public:

    /**
     * searches the target in the cache 
     * if not found iterativly calculate it 
     * and add it to the cache 
     * 
     */
    double calc_fastrand_avg(uint16_t target) {
        auto search = m_avg_cache.find(target);
        if (search != m_avg_cache.end()) {
            return search->second;
        }

        /* not found - calculate it */
        double avg = iterate_calc(target);

        /* if there is enough space - to the cache */
        if (m_avg_cache.size() <= G_MAX_CACHE_SIZE) {
            m_avg_cache[target] = avg;
        }

        return avg;
    }

private:

    /**
     * hard calculate a value using iterations
     * 
     */
    double iterate_calc(uint16_t target) {
         const int num_samples = 10000;
         uint64_t tmp = 0;
         uint32_t seed = 1;

         for (int i = 0; i < num_samples; i++) {
             tmp += fastrand(seed) % (target + 1);
         }

         return (tmp / double(num_samples));
    }

    std::unordered_map<uint16_t, double> m_avg_cache;
    static const uint16_t G_MAX_CACHE_SIZE = 9230;
};

static FastRandUtils g_fastrand_util;


void StreamVmInstructionFixChecksumIpv4::Dump(FILE *fd){
    fprintf(fd," fix_check_sum , %lu \n",(ulong)m_pkt_offset);
}

void StreamVmInstructionFixChecksumIcmpv6::Dump(FILE *fd){
    fprintf(fd," fix_checksum_icmpv6 , %lu:%lu \n", (ulong)m_l2_len, (ulong)m_l3_len);
}

void StreamVmInstructionFixHwChecksum::Dump(FILE *fd){
    fprintf(fd," fix_hw_cs  %lu:%lu \n",(ulong)m_l2_len,(ulong)m_l3_len);
}

void StreamVmInstructionFixRoceV2Icrc::Dump(FILE *fd){
    fprintf(fd," fix_rocev2_icrc  %lu:%lu:%lu \n",(ulong)m_l2_len,(ulong)m_l3_len,(ulong)m_bth_offset);
}

void StreamVmInstructionFlowMan::sanity_check_valid_size(uint32_t ins_id,StreamVm *lp){
    uint8_t valid[]={1,2,4,8};
    int i;
    for (i=0; i<sizeof(valid)/sizeof(valid[0]); i++) {
        if (valid[i]==m_size_bytes) {
            return;
        }
    }

    std::stringstream ss;

    ss << "instruction id '" << ins_id << "' has non valid length " << m_size_bytes ;

    lp->err(ss.str());
}

void StreamVmInstructionFlowMan::sanity_check_valid_opt(uint32_t ins_id,StreamVm *lp){
    uint8_t valid[]={FLOW_VAR_OP_INC,
        FLOW_VAR_OP_DEC,
        FLOW_VAR_OP_RANDOM};
    int i;
    for (i=0; i<sizeof(valid)/sizeof(valid[0]); i++) {
        if (valid[i]==m_op) {
            return;
        }
    }

    std::stringstream ss;

    ss << "instruction id '" << ins_id << "' has non valid op " << (int)m_op ;

    lp->err(ss.str());
}

void StreamVmInstructionFlowMan::sanity_check_valid_range(uint32_t ins_id,StreamVm *lp){
    //TBD check that init,min,max in valid range 
}



uint8_t StreamVmInstructionFlowMan::set_bss_init_value(uint8_t *p) {

    uint64_t prev = peek_prev();

    switch (m_size_bytes) {
    case 1:
        *p=(uint8_t)prev;
        return 1;
    case 2:
        *((uint16_t*)p)=(uint16_t)prev;
        return 2;
    case 4:
        *((uint32_t*)p)=(uint32_t)prev;
        return 4;
    case 8:
        *((uint64_t*)p)=(uint64_t)prev;
        return 8;
    default:
        assert(0);
        return(0);
    }
}


void StreamVmInstructionFlowMan::sanity_check(uint32_t ins_id,StreamVm *lp){

    sanity_check_valid_size(ins_id,lp);
    sanity_check_valid_opt(ins_id,lp);
    sanity_check_valid_range(ins_id,lp);
}



void StreamVmInstructionFlowRandLimit::Dump(FILE *fd){
    fprintf(fd," flow_var_rand_limit  , %s ,%lu,   ",m_var_name.c_str(),(ulong)m_size_bytes);
    fprintf(fd," (%lu:%lu:%lu) (min:%lu,max:%lu), is_split_needed:%d, has_previous %d ",m_limit,(ulong)m_size_bytes,(ulong)m_seed, 
                                    m_min_value,m_max_value, m_is_split_needed, m_has_previous);
    fprintf(fd, "next_var: %s\n", m_next_var_name.c_str());
}

void StreamVmInstructionFlowRandLimit::sanity_check(uint32_t ins_id,StreamVm *lp){
    sanity_check_valid_size(ins_id,lp);
}


uint8_t  StreamVmInstructionFlowRandLimit::set_bss_init_value(uint8_t *p){
    uint8_t res;

    typedef union  ua_ {
        RandMemBss8 *lpv8;
        RandMemBss16 *lpv16;
        RandMemBss32 *lpv32;
        RandMemBss64  *lpv64;
    } ua_t ;

    ua_t u;


    switch (m_size_bytes) {
    case 1:
        u.lpv8=(RandMemBss8 *)p;
        u.lpv8->m_seed=m_seed;
        u.lpv8->m_cnt=0;
        u.lpv8->m_val=0;
        res=sizeof(RandMemBss8);
        break;
    case 2:
        u.lpv16=(RandMemBss16 *)p;
        u.lpv16->m_seed=m_seed;
        u.lpv16->m_cnt=0;
        u.lpv16->m_val=0;
        res=sizeof(RandMemBss16);
        break;
    case 4:
        u.lpv32=(RandMemBss32 *)p;
        u.lpv32->m_seed=m_seed;
        u.lpv32->m_cnt=0;
        u.lpv32->m_val=0;
        res=sizeof(RandMemBss32);
        break;
    case 8:
        u.lpv64=(RandMemBss64 *)p;
        u.lpv64->m_seed=m_seed;
        u.lpv64->m_cnt=0;
        u.lpv64->m_val=0;
        res=sizeof(RandMemBss64);
        break;
    default:
        assert(0);
    }
    return (res);
}


void StreamVmInstructionFlowRandLimit::sanity_check_valid_size(uint32_t ins_id,StreamVm *lp){
    uint8_t valid[]={1,2,4,8};
    int i;
    for (i=0; i<sizeof(valid)/sizeof(valid[0]); i++) {
        if (valid[i]==m_size_bytes) {
            uint64_t limit = (1ULL<<((i+1)*8))-1;
            /* check limit */
            if ( m_limit == 0) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' limit " << m_limit << " can't be zero " ;
                lp->err(ss.str());
            }

            if ( m_limit > limit) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' limit " << m_limit << " is bigger than size " << m_size_bytes ;
                lp->err(ss.str());
            }
            return;
        }
    }

    std::stringstream ss;

    ss << "instruction id '" << ins_id << "' has non valid length " << m_size_bytes ;

    lp->err(ss.str());
}



void StreamVmInstructionWriteMaskToPkt::Dump(FILE *fd){
    fprintf(fd," flow_var:%s, offset:%lu, cast_size:%lu, mask:0x%lx, shift:%ld, add:%ld, is_big:%lu \n",m_flow_var_name.c_str(),(ulong)m_pkt_offset,(ulong)m_pkt_cast_size,(ulong)m_mask,(long)m_shift,(long)m_add_value,(ulong)(m_is_big_endian?1:0));
}


void StreamVmInstructionFlowMan::Dump(FILE *fd){
    fprintf(fd," flow_var  , %s ,%lu,  ",m_var_name.c_str(),(ulong)m_size_bytes);

    switch (m_op) {
    
    case FLOW_VAR_OP_INC :
        fprintf(fd," INC    ,");
        break;
    case FLOW_VAR_OP_DEC :
        fprintf(fd," DEC    ,");
        break;
    case FLOW_VAR_OP_RANDOM :
        fprintf(fd," RANDOM ,");
        break;
    default:
        fprintf(fd," UNKNOWN,");
        break;
    };

    fprintf(fd," (%lu:%lu:%lu:%lu:), is_split_needed:%d, has_previous:%d ",m_init_value,m_min_value,m_max_value,m_step,m_is_split_needed, m_has_previous);
    fprintf(fd, "next_var: %s\n", m_next_var_name.c_str());
}


void StreamVmInstructionWriteToPkt::Dump(FILE *fd){
    fprintf(fd," write_pkt , %s ,%lu, add, %ld, big, %lu \n",m_flow_var_name.c_str(),(ulong)m_pkt_offset,(long)m_add_value,(ulong)(m_is_big_endian?1:0));
}


void StreamVmInstructionChangePktSize::Dump(FILE *fd){
    fprintf(fd," pkt_size_change  , %s  \n",m_flow_var_name.c_str() );
}


void StreamVmInstructionFlowClient::Dump(FILE *fd){

    fprintf(fd," client_var ,%s , ",m_var_name.c_str());

    //fprintf(fd," ip:(%x-%x) port:(%x-%x)  flow_limit:%lu  flags: %x\n",m_client_min,m_client_max, m_port_min,m_port_max,(ulong)m_limit_num_flows,m_flags);
}


uint8_t StreamVmInstructionFlowClient::set_bss_init_value(uint8_t *p) {

    uint32_t bss_ip;
    uint16_t bss_port;

    /* fetch the previous values by 1 */
    peek_prev(bss_ip, bss_port, 1);

    /* ip */
    *((uint32_t*)p) = bss_ip;
    p += 4;

    /* port */
    *((uint16_t*)p) = bss_port;
    p += 2;

    /* reserve */
    *((uint32_t*)p) = 0;
    p += 4;

    return (get_flow_var_size());
}




/***************************
 * StreamVmInstruction
 * 
 **************************/
StreamVmInstruction::~StreamVmInstruction() {

}

/***************************
 * StreamVm
 * 
 **************************/
void StreamVm::add_instruction(StreamVmInstruction *inst) {
    
    if (inst->get_instruction_type() == StreamVmInstruction::itFLOW_MAN) {
        StreamVmInstructionFlowMan * ins_man=(StreamVmInstructionFlowMan *)inst;
        if (ins_man->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM) {
            m_is_random_var = true;
        }
    }

    if (inst->get_instruction_type() == StreamVmInstruction::itPKT_SIZE_CHANGE) {
        m_is_change_pkt_size = true;
    }

    if (inst->need_split()) {
        m_is_split_needed = true;
    }

    m_inst_list.push_back(inst);
}

StreamDPVmInstructions  *
StreamVm::get_dp_instruction_buffer(){
    return &m_instructions;
}


const std::vector<StreamVmInstruction *> & 
StreamVm::get_instruction_list() {
    return m_inst_list;
}

void  StreamVm::var_clear_table(){
    m_flow_var_offset.clear();
}

bool StreamVm::var_add(const std::string &var_name,VmFlowVarRec & var){
    m_flow_var_offset[var_name] = var;
    return (true);
}


uint16_t StreamVm::get_var_offset(const std::string &var_name){
    VmFlowVarRec var;
    bool res=var_lookup(var_name,var);
    assert(res);
    return (var.m_offset);
}


bool StreamVm::var_lookup(const std::string &var_name,VmFlowVarRec & var){
    auto search = m_flow_var_offset.find(var_name);

    if (search != m_flow_var_offset.end()) {
        var =  search->second;
        return true;
    } else {
        return false;
    }
}



void  StreamVm::err(const std::string &err){
    throw TrexException("*** error: " + err);
}


void StreamVm::build_flow_var_table() {

    var_clear_table();
    m_cur_var_offset=0;
    uint32_t ins_id=0;

    /* scan all flow var instruction and build */

    /* if we found allocate BSS +4 bytes */
    if ( m_is_random_var ){
        VmFlowVarRec var;

        var.m_offset = m_cur_var_offset;
        var.m_ins.m_ins_flowv = NULL;
        var.m_size_bytes = sizeof(uint32_t);
        var_add("___random___",var);
        m_cur_var_offset += sizeof(uint32_t);
    }

    for (auto inst : m_inst_list) {
        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_MAN ){

            StreamVmInstructionFlowMan * ins_man=(StreamVmInstructionFlowMan *)inst;

            /* check that instruction is valid */
            ins_man->sanity_check(ins_id,this);

            VmFlowVarRec var;
            /* if this is the first time */ 
            if ( var_lookup( ins_man->m_var_name,var) == true){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' flow variable name " << ins_man->m_var_name << " already exists";
                err(ss.str());
            }else{

                var.m_offset=m_cur_var_offset;
                var.m_ins.m_ins_flowv = ins_man;
                var.m_size_bytes = ins_man->m_size_bytes;
                var_add(ins_man->m_var_name,var);
                m_cur_var_offset += ins_man->m_size_bytes;

            }
        }

        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_RAND_LIMIT ){

            StreamVmInstructionFlowRandLimit * ins_man=(StreamVmInstructionFlowRandLimit *)inst;

            /* check that instruction is valid */
            ins_man->sanity_check(ins_id,this);

            VmFlowVarRec var;
            /* if this is the first time */ 
            if ( var_lookup( ins_man->m_var_name,var) == true){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' flow variable name " << ins_man->m_var_name << " already exists";
                err(ss.str());
            }else{

                var.m_offset=m_cur_var_offset;
                var.m_ins.m_ins_flow_rand_limit = ins_man;
                var.m_size_bytes = ins_man->m_size_bytes; /* used for write*/
                var_add(ins_man->m_var_name,var);
                m_cur_var_offset += ins_man->m_size_bytes*2 + sizeof(uint32_t) ; /* see RandMemBss8 types */
            }
        }


        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_CLIENT ){
            StreamVmInstructionFlowClient * ins_man=(StreamVmInstructionFlowClient *)inst;

            VmFlowVarRec var;
            /* if this is the first time */ 
            if ( var_lookup( ins_man->m_var_name+".ip",var) == true){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' client variable name " << ins_man->m_var_name << " already exists";
                err(ss.str());
            }
            if ( var_lookup( ins_man->m_var_name+".port",var) == true){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' client variable name " << ins_man->m_var_name << " already exists";
                err(ss.str());
            }

            if ( var_lookup( ins_man->m_var_name+".flow_limit",var) == true){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' client variable name " << ins_man->m_var_name << " already exists";
                err(ss.str());
            }

            var.m_offset = m_cur_var_offset;
            var.m_ins.m_ins_flow_client = ins_man;
            var.m_size_bytes =4;

            VmFlowVarRec var_port;

            var_port.m_offset = m_cur_var_offset+4;
            var_port.m_ins.m_ins_flow_client = ins_man;
            var_port.m_size_bytes =2;

            VmFlowVarRec var_flow_limit;

            var_flow_limit.m_offset = m_cur_var_offset+6;
            var_flow_limit.m_ins.m_ins_flow_client = ins_man;
            var_flow_limit.m_size_bytes =4;


            var_add(ins_man->m_var_name+".ip",var);
            var_add(ins_man->m_var_name+".port",var_port);
            var_add(ins_man->m_var_name+".flow_limit",var_flow_limit);

            m_cur_var_offset += StreamVmInstructionFlowClient::get_flow_var_size(); 

            assert(sizeof(StreamDPFlowClient)==StreamVmInstructionFlowClient::get_flow_var_size());
        }

        /* limit the flow var size */
        if (m_cur_var_offset > StreamVm::svMAX_FLOW_VAR ) {
            std::stringstream ss;
            ss << "too many flow variables current size is :" << m_cur_var_offset << " maximum support is " << StreamVm::svMAX_FLOW_VAR;
            err(ss.str());
        }
        ins_id++;
    }


    ins_id=0;

    /* second interation for sanity check and fixups*/
    for (auto inst : m_inst_list) {


        if (inst->get_instruction_type() == StreamVmInstruction::itPKT_SIZE_CHANGE ) {
            StreamVmInstructionChangePktSize *lpPkt =(StreamVmInstructionChangePktSize *)inst;

            VmFlowVarRec var;
            if ( var_lookup(lpPkt->m_flow_var_name ,var) == false){

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet size with no valid flow variable name '" << lpPkt->m_flow_var_name << "'" ;
                err(ss.str());
            }

            if ( var.m_size_bytes != 2 ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet size change should point to a flow variable with size 2  ";
                err(ss.str());
            }

            if ( var.m_ins.m_ins_flowv->get_instruction_type() != StreamVmInstruction::itFLOW_MAN ){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet size change should point to a simple flow variable type (Random/Client) types are not supported ";
                err(ss.str());

            }

            if (var.m_ins.m_ins_flowv->m_value_list.empty()) {
                if (var.m_ins.m_ins_flowv->m_max_value > m_pkt_size) {
                    var.m_ins.m_ins_flowv->m_max_value = m_pkt_size;
                }

                if (var.m_ins.m_ins_flowv->m_min_value > m_pkt_size) {
                    var.m_ins.m_ins_flowv->m_min_value = m_pkt_size;
                }

                if (var.m_ins.m_ins_flowv->m_min_value >= var.m_ins.m_ins_flowv->m_max_value) {
                    std::stringstream ss;
                    ss << "instruction id '" << ins_id << "' min packet size " << var.m_ins.m_ins_flowv->m_min_value << " is bigger or eq to max packet size " << var.m_ins.m_ins_flowv->m_max_value;
                    err(ss.str());
                }

                if (var.m_ins.m_ins_flowv->m_min_value < 60) {
                    var.m_ins.m_ins_flowv->m_min_value = 60;
                }

                /* expected packet size calculation */

                /* for random packet size - we need to find the average */
                if (var.m_ins.m_ins_flowv->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM) {
                    uint16_t range = var.m_ins.m_ins_flowv->m_max_value - var.m_ins.m_ins_flowv->m_min_value;
                    m_pkt_len_data.m_expected_pkt_len = var.m_ins.m_ins_flowv->m_min_value + g_fastrand_util.calc_fastrand_avg(range);
                } else {
                    m_pkt_len_data.m_expected_pkt_len = (var.m_ins.m_ins_flowv->m_min_value + var.m_ins.m_ins_flowv->m_max_value) / 2.0;

                }
                m_pkt_len_data.m_max_pkt_len = var.m_ins.m_ins_flowv->m_max_value;
                m_pkt_len_data.m_min_pkt_len = var.m_ins.m_ins_flowv->m_min_value;
            }
            else {
                for (int i = 0; i < (int)var.m_ins.m_ins_flowv->m_value_list.size(); i++) {
                    if (var.m_ins.m_ins_flowv->m_value_list[i] > m_pkt_size ||
                        var.m_ins.m_ins_flowv->m_value_list[i] < 60) {
                        std::stringstream ss;
                        ss << "instruction id '" << ins_id << "' packet size in value_list " << var.m_ins.m_ins_flowv->m_value_list[i] << " is bigger than packet size " << m_pkt_size << " or smaller than 60";
                        err(ss.str());
                    }

                    if ( 0 == i ) {
                        m_pkt_len_data.m_max_pkt_len = var.m_ins.m_ins_flowv->m_value_list[i];
                        m_pkt_len_data.m_min_pkt_len = var.m_ins.m_ins_flowv->m_value_list[i];
                    }
                    else {
                        if (var.m_ins.m_ins_flowv->m_value_list[i] > m_pkt_len_data.m_max_pkt_len) {
                            m_pkt_len_data.m_max_pkt_len = var.m_ins.m_ins_flowv->m_value_list[i];
                        }
                        else if (var.m_ins.m_ins_flowv->m_value_list[i] < m_pkt_len_data.m_min_pkt_len) {
                            m_pkt_len_data.m_min_pkt_len = var.m_ins.m_ins_flowv->m_value_list[i];
                        }
                    }
                }

                /* expected packet size calculation */
                m_pkt_len_data.m_expected_pkt_len = std::accumulate(var.m_ins.m_ins_flowv->m_value_list.begin(), var.m_ins.m_ins_flowv->m_value_list.end(), 0.0)/var.m_ins.m_ins_flowv->m_value_list.size();
            }

        }
    }

    // third iteration - verifying that variables with next are ordered as they should
    for ( int i = 0; i < m_inst_list.size(); i++) {
        auto inst_type = m_inst_list[i]->get_instruction_type();
        if (inst_type != StreamVmInstruction::itFLOW_MAN && inst_type != StreamVmInstruction::itFLOW_RAND_LIMIT) {
            continue;
        }
        // if we got here then the instruction is a flow variable or flow rand limit
        std::string next_var_name = "";
        if (inst_type == StreamVmInstruction::itFLOW_MAN) {
            StreamVmInstructionFlowMan* inst = dynamic_cast<StreamVmInstructionFlowMan*>(m_inst_list[i]);
            next_var_name = inst->m_next_var_name;
        } else if (inst_type == StreamVmInstruction::itFLOW_RAND_LIMIT) {
            StreamVmInstructionFlowRandLimit* inst = dynamic_cast<StreamVmInstructionFlowRandLimit*>(m_inst_list[i]);
            next_var_name = inst->m_next_var_name;
        }
        if(next_var_name == "") {
            continue;
        }
        // if we got here then it is a flow var or flow rand limit with next
        if (i == m_inst_list.size() - 1) {
            err("Last instruction in the list of instructions has next_var");
        } else {
            auto next_inst_type = m_inst_list[i+1]->get_instruction_type();
            if (next_inst_type != StreamVmInstruction::itFLOW_MAN && next_inst_type != StreamVmInstruction::itFLOW_RAND_LIMIT) {
                err ("Next instruction isn't a variable or a repeatable random");
            } else {
                StreamVmInstructionVar* next_inst = dynamic_cast<StreamVmInstructionVar*>(m_inst_list[i+1]);
                if (next_inst->get_var_name() != next_var_name) {
                    err("Next instruction name does not match next_var");
                } else {
                    if (next_inst->need_split()) {
                        err("A variable that is being pointed at by some other variable must run on a single core.");
                    }
                    next_inst->set_has_previous(true);
                }
            }
        }
    }
}

void StreamVm::alloc_bss(){
    free_bss();
    if (m_cur_var_offset) {
        m_bss=(uint8_t *)malloc(m_cur_var_offset);
    }
}

void StreamVm::clean_max_field_cnt(){
    m_max_field_update=0;
}

void StreamVm::add_field_cnt(uint16_t new_cnt){

    if ( new_cnt > m_max_field_update) {
        m_max_field_update = new_cnt;
    }
}


void StreamVm::free_bss(){
    if (m_bss) {
        free(m_bss);
        m_bss=0;
    }
}


void StreamVm::build_program(){

    /* build the commands into a buffer */
    m_instructions.clear();
    int var_cnt=0;
    int var_ref_cnt=0;
    clean_max_field_cnt();
    m_pkt_trailer_size = 0;
    uint32_t ins_id=0;
    uint32_t skip=0;


    for ( int i = m_inst_list.size() - 1; i>=0 ; i-- ){
        auto inst = m_inst_list[i];
        StreamVmInstruction::instruction_type_t ins_type=inst->get_instruction_type();

        if (ins_type != StreamVmInstruction::itFLOW_MAN && ins_type != StreamVmInstruction::itFLOW_RAND_LIMIT) {
            skip = 0;
        }

        if (ins_type == StreamVmInstruction::itFIX_HW_CS) {
            StreamVmInstructionFixHwChecksum *lpFix =(StreamVmInstructionFixHwChecksum *)inst;
            bool is_ip = (lpFix->m_l4_type==StreamVmInstructionFixHwChecksum::L4_TYPE_IP?true:false);
            uint16_t l3_len =lpFix->m_l3_len;

            if (lpFix->m_l2_len < 14 ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' fix hw offset l2 " << lpFix->m_l2_len << "  is lower than 14 ";
                err(ss.str());
            }

            if ((is_ip==false) && (l3_len < 20 ) ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' fix hw offset l3 " << l3_len << "  is lower than 20 ";
                err(ss.str());
            }

            uint16_t total_l4_offset = lpFix->m_l2_len + l3_len;
            uint16_t l4_header_size =0;


            if ( m_pkt ) {
                /* not pre compile for estimation of average packet size */
                bool packet_is_ipv4=true;
                IPHeader * ipv4= (IPHeader *)(m_pkt+lpFix->m_l2_len);
                if (ipv4->getVersion() ==4 ) {
                    packet_is_ipv4=true;
                    if (is_ip){
                        /* take it from the packet */
                        l3_len = ipv4->getSize();
                    }else{
                        if (ipv4->getSize() != l3_len ) {
                            std::stringstream ss;
                            ss << "instruction id '" << ins_id << "' fix hw command IPv4 header size is not valid  " << ipv4->getSize() ;
                            err(ss.str());
                        }
                        if ( !((ipv4->getNextProtocol() == IPHeader::Protocol::TCP) || 
                              (ipv4->getNextProtocol() == IPHeader::Protocol::UDP) ) ) {
                            std::stringstream ss;
                            ss << "instruction id '" << ins_id << "' fix hw command L4 should be TCP or UDP  " << ipv4->getSize() ;
                            err(ss.str());
                        }
                    }
                }else{
                    if (ipv4->getVersion() ==6) {
                        packet_is_ipv4=false;
                    }else{
                        std::stringstream ss;
                        ss << "instruction id '" << ins_id << "' fix hw command should work on IPv4 or IPv6  "  ;
                        err(ss.str());
                    }
                }

                StreamDPOpHwCsFix ipv_fix;
                ipv_fix.m_l2_len = lpFix->m_l2_len;
                ipv_fix.m_l3_len = l3_len;
                ipv_fix.m_op = StreamDPVmInstructions::ditFIX_HW_CS;

                if (packet_is_ipv4) {
                    if ( ipv4->getNextProtocol() == IPHeader::Protocol::TCP ){
                        /* Ipv4 TCP */
                        ipv_fix.m_ol_flags = (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_TCP_CKSUM);
                        l4_header_size = TCP_HEADER_LEN;
                    }else{
                        if (ipv4->getNextProtocol() == IPHeader::Protocol::UDP) {
                            /* Ipv4 UDP */
                            ipv_fix.m_ol_flags = (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_UDP_CKSUM);
                            l4_header_size = UDP_HEADER_LEN;
                        }else{
                            ipv_fix.m_ol_flags = (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM );
                            l4_header_size = 0;
                        }
                    }
                }else{
                    /* Ipv6*/
                    /* in this case we need to scan the Ipv6 headers */
                    /* TBD replace with parser of IPv6 function */
                    if ( lpFix->m_l4_type==StreamVmInstructionFixHwChecksum::L4_TYPE_TCP ){
                        ipv_fix.m_ol_flags = (RTE_MBUF_F_TX_IPV6 | RTE_MBUF_F_TX_TCP_CKSUM);
                        l4_header_size = TCP_HEADER_LEN;
                    }else{
                        if ( lpFix->m_l4_type==StreamVmInstructionFixHwChecksum::L4_TYPE_UDP ){
                            ipv_fix.m_ol_flags = (RTE_MBUF_F_TX_IPV6 | RTE_MBUF_F_TX_UDP_CKSUM);
                            l4_header_size = UDP_HEADER_LEN;
                        }else{
                            std::stringstream ss;
                            ss << "instruction id '" << ins_id << "' fix hw command offsets should be TCP or UDP for IPv6 ";
                            err(ss.str());
                        }
                    }
                }

                if ( (total_l4_offset + l4_header_size) > m_pkt_size  ) {

                    std::stringstream ss;
                    ss << "instruction id '" << ins_id << "' fix hw command offsets  " << (total_l4_offset + l4_header_size) << "  is too high relative to packet size  "<< m_pkt_size;
                    err(ss.str());
                }

                /* add the instruction*/
                m_instructions.add_command(&ipv_fix,sizeof(ipv_fix));

                /* mark R/W of the packet */
                add_field_cnt(total_l4_offset + l4_header_size);
            }
        }

        /* itFIX_IPV4_CS */
        if (ins_type == StreamVmInstruction::itFIX_IPV4_CS) {
            StreamVmInstructionFixChecksumIpv4 *lpFix =(StreamVmInstructionFixChecksumIpv4 *)inst;

            if ( (lpFix->m_pkt_offset + IPV4_HDR_LEN) > m_pkt_size  ) {

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' fix ipv4 command offset  " << lpFix->m_pkt_offset << "  is too high relative to packet size  "<< m_pkt_size;
                err(ss.str());
            }

            uint16_t offset_next_layer = IPV4_HDR_LEN;

            if ( m_pkt ){
                IPHeader * ipv4= (IPHeader *)(m_pkt+lpFix->m_pkt_offset);
                offset_next_layer = ipv4->getSize();
            }

            if (offset_next_layer<IPV4_HDR_LEN) {
                offset_next_layer=IPV4_HDR_LEN;
            }

            if ( (lpFix->m_pkt_offset + offset_next_layer) > m_pkt_size  ) {

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' fix ipv4 command offset  " << lpFix->m_pkt_offset << "plus "<<offset_next_layer<< " is too high relative to packet size  "<< m_pkt_size;
                err(ss.str());
            }
            /* calculate this offset from the packet */
            add_field_cnt(lpFix->m_pkt_offset + offset_next_layer);

            StreamDPOpIpv4Fix ipv_fix;
            ipv_fix.m_offset = lpFix->m_pkt_offset;
            ipv_fix.m_op = StreamDPVmInstructions::ditFIX_IPV4_CS;
            m_instructions.add_command(&ipv_fix,sizeof(ipv_fix));
        }

        /* itFIX_ICMPV6_CS */
        if (ins_type == StreamVmInstruction::itFIX_ICMPV6_CS) {
            StreamVmInstructionFixChecksumIcmpv6* icmpv6Fix =(StreamVmInstructionFixChecksumIcmpv6 *)inst;
            std::stringstream errorBase;
            errorBase << "instruction id '" << ins_id << "' fix icmpv6 checksum command ";

            if (m_pkt) {
                IPv6Header * ipv6 = (IPv6Header *)(m_pkt+icmpv6Fix->m_l2_len);
                if (ipv6->getVersion() != 6) {
                    errorBase << " works only with IPv6";
                    err(errorBase.str());
                }

                if (ipv6->getNextHdr() != IPHeader::Protocol::IPV6_ICMP) {
                    errorBase << " didn't find ICMPv6 layer, IPv6's next header type is " << (uint16_t)ipv6->getNextHdr();
                    err(errorBase.str());
                }

            }

            const uint16_t ICMP_CHECKSUM_OFFSET = 2;
            const uint16_t ICMP_CHECKSUM_SIZE = 2;

            add_field_cnt(icmpv6Fix->m_l2_len + icmpv6Fix->m_l3_len + ICMP_CHECKSUM_OFFSET + ICMP_CHECKSUM_SIZE);

            StreamDPOpIcmpv6Fix ipv_fix;
            ipv_fix.m_l2_len = icmpv6Fix->m_l2_len;
            ipv_fix.m_l3_len = icmpv6Fix->m_l3_len;
            ipv_fix.m_op = StreamDPVmInstructions::ditFIX_ICMPV6_CS;
            m_instructions.add_command(&ipv_fix,sizeof(ipv_fix));
        }

        /* itFIX_ROCEV2_ICRC */
        if (ins_type == StreamVmInstruction::itFIX_ROCEV2_ICRC) {
            StreamVmInstructionFixRoceV2Icrc *lpFix =(StreamVmInstructionFixRoceV2Icrc *)inst;
            std::stringstream errorBase;
            errorBase << "instruction id '" << ins_id << "' fix RoCEv2 ICRC command ";

            if (m_is_change_pkt_size) {
                errorBase << "cannot be used with packet size changes";
                err(errorBase.str());
            }
            if (i != (int)m_inst_list.size() - 1) {
                errorBase << "must be the final VM instruction";
                err(errorBase.str());
            }

            uint32_t l4_offset = (uint32_t)lpFix->m_l2_len + lpFix->m_l3_len;
            if ((m_pkt_size < 4) ||
                (lpFix->m_l2_len < 14) || (lpFix->m_l3_len < 20) ||
                (lpFix->m_l2_len >= m_pkt_size) ||
                (l4_offset + UDP_HEADER_LEN != lpFix->m_bth_offset) ||
                ((uint32_t)lpFix->m_bth_offset + 12 > (uint32_t)m_pkt_size - 4)) {
                errorBase << "offsets are invalid relative to packet size " << m_pkt_size;
                err(errorBase.str());
            }

            if (m_pkt) {
                uint8_t version = m_pkt[lpFix->m_l2_len] >> 4;
                if (version == 4) {
                    IPHeader *ipv4 = (IPHeader *)(m_pkt + lpFix->m_l2_len);
                    if (ipv4->getNextProtocol() != IPHeader::Protocol::UDP) {
                        errorBase << "requires IPv4/UDP";
                        err(errorBase.str());
                    }
                    if (ipv4->getSize() != lpFix->m_l3_len) {
                        errorBase << "IPv4 header size " << ipv4->getSize() << " does not match l3_len " << lpFix->m_l3_len;
                        err(errorBase.str());
                    }
                } else if (version == 6) {
                    if (lpFix->m_l3_len != 40) {
                        errorBase << "supports IPv6 without extension headers";
                        err(errorBase.str());
                    }
                    IPv6Header *ipv6 = (IPv6Header *)(m_pkt + lpFix->m_l2_len);
                    if (ipv6->getNextHdr() != IPHeader::Protocol::UDP) {
                        errorBase << "requires IPv6/UDP without extension headers";
                        err(errorBase.str());
                    }
                } else {
                    errorBase << "requires IPv4 or IPv6";
                    err(errorBase.str());
                }

                uint8_t *udp = m_pkt + l4_offset;
                uint16_t dport = ((uint16_t)udp[2] << 8) | udp[3];
                if (dport != 4791) {
                    errorBase << "requires UDP destination port 4791";
                    err(errorBase.str());
                }
            }

            StreamDPOpRoceIcrcFix roce_fix;
            roce_fix.m_l2_len = lpFix->m_l2_len;
            roce_fix.m_l3_len = lpFix->m_l3_len;
            roce_fix.m_bth_offset = lpFix->m_bth_offset;
            roce_fix.m_op = StreamDPVmInstructions::ditFIX_ROCEV2_ICRC;
            m_instructions.add_command(&roce_fix, sizeof(roce_fix));
            m_pkt_trailer_size = 4;
        }

        if (ins_type == StreamVmInstruction::itFLOW_RAND_LIMIT) {
            StreamVmInstructionFlowRandLimit *lpMan =(StreamVmInstructionFlowRandLimit *)inst;
            var_cnt++;

            if (lpMan->m_size_bytes == 1 ){
                StreamDPOpFlowRandLimit8 fv8;
                fv8.m_op    = StreamDPVmInstructions::ditRAND_LIMIT8 ;
                fv8.m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv8.m_limit     = (uint8_t)lpMan->m_limit;
                fv8.m_seed      = (uint32_t)lpMan->m_seed;
                fv8.m_min_val = (uint8_t)lpMan->m_min_value;
                fv8.m_max_val = (uint8_t)lpMan->m_max_value;
                fv8.m_skip    = skip;
                if ( lpMan->m_has_previous ){
                    skip += sizeof(fv8);
                } else {
                    skip = 0;
                }
                m_instructions.add_command(&fv8,sizeof(fv8));
            }

            if (lpMan->m_size_bytes == 2 ){
                StreamDPOpFlowRandLimit16 fv16;
                fv16.m_op    = StreamDPVmInstructions::ditRAND_LIMIT16 ;
                fv16.m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv16.m_limit     = (uint16_t)lpMan->m_limit;
                fv16.m_seed      = (uint32_t)lpMan->m_seed;
                fv16.m_min_val   = (uint16_t)lpMan->m_min_value;
                fv16.m_max_val   = (uint16_t)lpMan->m_max_value;
                fv16.m_skip      = skip;
                if ( lpMan->m_has_previous ){
                    skip += sizeof(fv16);
                } else {
                    skip = 0;
                }
                m_instructions.add_command(&fv16,sizeof(fv16));
            }

            if (lpMan->m_size_bytes == 4 ){
                StreamDPOpFlowRandLimit32 fv32;
                fv32.m_op    = StreamDPVmInstructions::ditRAND_LIMIT32 ;
                fv32.m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv32.m_limit     = (uint32_t)lpMan->m_limit;
                fv32.m_seed      = (uint32_t)lpMan->m_seed;
                fv32.m_min_val   = (uint32_t)lpMan->m_min_value;
                fv32.m_max_val   = (uint32_t)lpMan->m_max_value;
                fv32.m_skip      = skip;
                if ( lpMan->m_has_previous ){
                    skip += sizeof(fv32);
                } else {
                    skip = 0;
                }
                m_instructions.add_command(&fv32,sizeof(fv32));
            }

            if (lpMan->m_size_bytes == 8 ){
                StreamDPOpFlowRandLimit64 fv64;
                fv64.m_op    = StreamDPVmInstructions::ditRAND_LIMIT64 ;
                fv64.m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv64.m_limit     = lpMan->m_limit;
                fv64.m_seed      = (uint32_t)lpMan->m_seed;
                fv64.m_min_val   = lpMan->m_min_value;
                fv64.m_max_val   = lpMan->m_max_value;
                fv64.m_skip      = skip;
                if ( lpMan->m_has_previous ){
                    skip += sizeof(fv64);
                } else {
                    skip = 0;
                }
                m_instructions.add_command(&fv64,sizeof(fv64));
            }
        }

        /* flow man */
        if (ins_type == StreamVmInstruction::itFLOW_MAN) {
            StreamVmInstructionFlowMan *lpMan =(StreamVmInstructionFlowMan *)inst;

            var_cnt++;

            /* flow var size 1 */
            if (lpMan->m_size_bytes == 1 ) {
                uint8_t op = StreamDPVmInstructions::ditINC8_STEP;

                switch (lpMan->m_op) {
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_INC:
                    op = StreamDPVmInstructions::ditINC8_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC:
                    op = StreamDPVmInstructions::ditDEC8_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM:
                    op = StreamDPVmInstructions::ditRANDOM8;
                    break;
                default:
                    assert(0);
                }

                size_t st_size = sizeof(StreamDPOpFlowVar8Step) +
                                 sizeof(uint8_t) * lpMan->m_value_list.size();
                StreamDPOpFlowVar8Step *fv8 = (StreamDPOpFlowVar8Step *)malloc(st_size);
                fv8->m_op = op;
                fv8->m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv8->m_min_val     = (uint8_t)lpMan->m_min_value;
                fv8->m_max_val     = (uint8_t)lpMan->m_max_value;
                fv8->m_step        = (uint8_t)lpMan->m_step;
                fv8->m_skip        = skip;
                fv8->m_list_size   = (uint16_t)lpMan->m_value_list.size();
                fv8->m_list_index  = (uint16_t)lpMan->m_init_value;
                for (int i = 0; i < (int)lpMan->m_value_list.size(); i++) {
                    fv8->m_val_list[i] = (uint8_t)lpMan->m_value_list[i];
                }
                if ( lpMan->m_has_previous ){
                    skip += st_size;
                } else {
                    skip = 0;
                }
                m_instructions.add_command(fv8, st_size);
                free(fv8);
            }

            /* flow var size 2 */
            if (lpMan->m_size_bytes == 2) {
                uint8_t op = StreamDPVmInstructions::ditINC16_STEP;

                switch (lpMan->m_op) {
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_INC:
                    op = StreamDPVmInstructions::ditINC16_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC:
                    op = StreamDPVmInstructions::ditDEC16_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM:
                    op = StreamDPVmInstructions::ditRANDOM16;
                    break;
                default:
                    assert(0);
                }

                size_t st_size = sizeof(StreamDPOpFlowVar16Step) +
                                 sizeof(uint16_t) * lpMan->m_value_list.size();
                StreamDPOpFlowVar16Step *fv16 = (StreamDPOpFlowVar16Step *)malloc(st_size);
                fv16->m_op = op;
                fv16->m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv16->m_min_val     = (uint16_t)lpMan->m_min_value;
                fv16->m_max_val     = (uint16_t)lpMan->m_max_value;
                fv16->m_step        = (uint16_t)lpMan->m_step;
                fv16->m_skip        = skip;
                fv16->m_list_size   = (uint16_t)lpMan->m_value_list.size();
                fv16->m_list_index  = (uint16_t)lpMan->m_init_value;
                for (int i = 0; i < (int)lpMan->m_value_list.size(); i++) {
                    fv16->m_val_list[i] = (uint16_t)lpMan->m_value_list[i];
                }
                if ( lpMan->m_has_previous ){
                    skip += st_size;
                } else {
                    skip = 0;
                }
                m_instructions.add_command(fv16, st_size);
                free(fv16);
            }


            /* flow var size 4 */
            if (lpMan->m_size_bytes == 4) {
                uint8_t op = StreamDPVmInstructions::ditINC32_STEP;

                switch (lpMan->m_op) {
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_INC:
                    op = StreamDPVmInstructions::ditINC32_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC:
                    op = StreamDPVmInstructions::ditDEC32_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM:
                    op = StreamDPVmInstructions::ditRANDOM32;
                    break;
                default:
                    assert(0);
                }

                size_t st_size = sizeof(StreamDPOpFlowVar32Step) +
                                 sizeof(uint32_t) * lpMan->m_value_list.size();
                StreamDPOpFlowVar32Step *fv32 = (StreamDPOpFlowVar32Step *)malloc(st_size);
                fv32->m_op = op;
                fv32->m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv32->m_min_val     = (uint32_t)lpMan->m_min_value;
                fv32->m_max_val     = (uint32_t)lpMan->m_max_value;
                fv32->m_step        = (uint32_t)lpMan->m_step;
                fv32->m_skip        = skip;
                fv32->m_list_size   = (uint16_t)lpMan->m_value_list.size();
                fv32->m_list_index  = (uint16_t)lpMan->m_init_value;
                for (int i = 0; i < (int)lpMan->m_value_list.size(); i++) {
                    fv32->m_val_list[i] = (uint32_t)lpMan->m_value_list[i];
                }
                if ( lpMan->m_has_previous ){
                    skip += st_size;
                } else {
                    skip = 0;
                }
                m_instructions.add_command(fv32, st_size);
                free(fv32);
            }

            /* flow var size 8 */
            if (lpMan->m_size_bytes == 8) {
                uint8_t op = StreamDPVmInstructions::ditINC64_STEP;

                switch (lpMan->m_op) {
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_INC:
                    op = StreamDPVmInstructions::ditINC64_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC:
                    op = StreamDPVmInstructions::ditDEC64_STEP;
                    break;
                case StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM:
                    op = StreamDPVmInstructions::ditRANDOM64;
                    break;
                default:
                    assert(0);
                }

                size_t st_size = sizeof(StreamDPOpFlowVar64Step) +
                                 sizeof(uint64_t) * lpMan->m_value_list.size();
                StreamDPOpFlowVar64Step *fv64 = (StreamDPOpFlowVar64Step *)malloc(st_size);
                fv64->m_op = op;
                fv64->m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv64->m_min_val     = (uint64_t)lpMan->m_min_value;
                fv64->m_max_val     = (uint64_t)lpMan->m_max_value;
                fv64->m_step        = (uint64_t)lpMan->m_step;
                fv64->m_skip        = skip;
                fv64->m_list_size   = (uint16_t)lpMan->m_value_list.size();
                fv64->m_list_index  = (uint16_t)lpMan->m_init_value;
                for (int i = 0; i < (int)lpMan->m_value_list.size(); i++) {
                    fv64->m_val_list[i] = (uint64_t)lpMan->m_value_list[i];
                }
                if ( lpMan->m_has_previous ){
                    skip += st_size;
                } else {
                    skip = 0;
                }
                m_instructions.add_command(fv64, st_size);
                free(fv64);
            }

        }

        if (ins_type == StreamVmInstruction::itPKT_WR) {
            StreamVmInstructionWriteToPkt *lpPkt =(StreamVmInstructionWriteToPkt *)inst;

            var_ref_cnt++;

            VmFlowVarRec var;
            if ( var_lookup(lpPkt->m_flow_var_name ,var) == false){

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet write with no valid flow variable name '" << lpPkt->m_flow_var_name << "'" ;
                err(ss.str());
            }

            if (lpPkt->m_pkt_offset + var.m_size_bytes > m_pkt_size ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet write with packet_offset   " << lpPkt->m_pkt_offset + var.m_size_bytes  << "   bigger than packet size   "<< m_pkt_size;
                err(ss.str());
            }


            add_field_cnt(lpPkt->m_pkt_offset + var.m_size_bytes);


            uint8_t       op_size=var.m_size_bytes;
            bool is_big    = lpPkt->m_is_big_endian;
            uint8_t       flags = (is_big?StreamDPOpPktWrBase::PKT_WR_IS_BIG:0);
            uint8_t       flow_offset = get_var_offset(lpPkt->m_flow_var_name);

            if (op_size == 1) {
                StreamDPOpPktWr8 pw8;
                pw8.m_op = StreamDPVmInstructions::itPKT_WR8;
                pw8.m_flags =flags;
                pw8.m_offset =flow_offset;
                pw8.m_pkt_offset = lpPkt->m_pkt_offset;
                pw8.m_val_offset = (int8_t)lpPkt->m_add_value;
                m_instructions.add_command(&pw8,sizeof(pw8));
            }

            if (op_size == 2) {
                StreamDPOpPktWr16 pw16;
                pw16.m_op = StreamDPVmInstructions::itPKT_WR16;
                pw16.m_flags =flags;
                pw16.m_offset =flow_offset;
                pw16.m_pkt_offset = lpPkt->m_pkt_offset;
                pw16.m_val_offset = (int16_t)lpPkt->m_add_value;
                m_instructions.add_command(&pw16,sizeof(pw16));
            }

            if (op_size == 4) {
                StreamDPOpPktWr32 pw32;
                pw32.m_op = StreamDPVmInstructions::itPKT_WR32;
                pw32.m_flags =flags;
                pw32.m_offset =flow_offset;
                pw32.m_pkt_offset = lpPkt->m_pkt_offset;
                pw32.m_val_offset = (int32_t)lpPkt->m_add_value;
                m_instructions.add_command(&pw32,sizeof(pw32));
            }

            if (op_size == 8) {
                StreamDPOpPktWr64 pw64;
                pw64.m_op = StreamDPVmInstructions::itPKT_WR64;
                pw64.m_flags =flags;
                pw64.m_offset =flow_offset;
                pw64.m_pkt_offset = lpPkt->m_pkt_offset;
                pw64.m_val_offset = (int64_t)lpPkt->m_add_value;
                m_instructions.add_command(&pw64,sizeof(pw64));
            }

        }

        if (ins_type == StreamVmInstruction::itPKT_WR_MASK) {
            StreamVmInstructionWriteMaskToPkt *lpPkt =(StreamVmInstructionWriteMaskToPkt *)inst;

            var_ref_cnt++;

            VmFlowVarRec var;

            uint8_t cast_size = lpPkt->m_pkt_cast_size;
            if (!((cast_size==4)||(cast_size==2)||(cast_size==1))){
                std::stringstream ss;
                ss << "instruction id '" << ins_id << " cast size should be 1,2,4 it is "<<lpPkt->m_pkt_cast_size;
                err(ss.str());
            }

            if ( var_lookup(lpPkt->m_flow_var_name ,var) == false){

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet write with no valid flow variable name '" << lpPkt->m_flow_var_name << "'" ;
                err(ss.str());
            }

            if (lpPkt->m_pkt_offset + lpPkt->m_pkt_cast_size > m_pkt_size ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet write with packet_offset   " << (lpPkt->m_pkt_offset + lpPkt->m_pkt_cast_size)  << "   bigger than packet size   "<< m_pkt_size;
                err(ss.str());
            }


            add_field_cnt(lpPkt->m_pkt_offset + lpPkt->m_pkt_cast_size);


            uint8_t       op_size = var.m_size_bytes;
            bool is_big           = lpPkt->m_is_big_endian;
            uint8_t       flags   = (is_big?StreamDPOpPktWrMask::MASK_PKT_WR_IS_BIG:0);
            uint8_t       flow_offset = get_var_offset(lpPkt->m_flow_var_name);

            /* read LSB in case of 64bit variable */
            if (op_size == 8) {
                op_size = 4;
                if ( is_big ) {
                    flow_offset +=4;
                }
            }

            StreamDPOpPktWrMask pmask;
            pmask.m_op = StreamDPVmInstructions::itPKT_WR_MASK;
            pmask.m_flags      =   flags;
            pmask.m_var_offset =   flow_offset;
            pmask.m_shift      =   lpPkt->m_shift;
            pmask.m_add_value  =   lpPkt->m_add_value;
            pmask.m_pkt_cast_size =   cast_size;
            pmask.m_flowv_cast_size = op_size;
            pmask.m_pkt_offset      = lpPkt->m_pkt_offset;
            pmask.m_mask            = lpPkt->m_mask;

            m_instructions.add_command(&pmask,sizeof(pmask));
        }


        if (ins_type == StreamVmInstruction::itFLOW_CLIENT) {
            var_cnt++;
            StreamVmInstructionFlowClient *lpMan =(StreamVmInstructionFlowClient *)inst;

            if ( lpMan->is_unlimited_flows() ){
                StreamDPOpClientsUnLimit  client_cmd;
                client_cmd.m_op =  StreamDPVmInstructions::itCLIENT_VAR_UNLIMIT;

                client_cmd.m_flow_offset = get_var_offset(lpMan->m_var_name+".ip"); /* start offset */
                client_cmd.m_flags       = 0; /* not used */
                client_cmd.m_pad         = 0;
                client_cmd.m_min_ip      = lpMan->m_ip.m_min_value;
                client_cmd.m_max_ip      = lpMan->m_ip.m_max_value;
                m_instructions.add_command(&client_cmd,sizeof(client_cmd));

            }else{
                StreamDPOpClientsLimit  client_cmd;
                client_cmd.m_op =  StreamDPVmInstructions::itCLIENT_VAR;

                client_cmd.m_flow_offset = get_var_offset(lpMan->m_var_name+".ip"); /* start offset */
                client_cmd.m_flags       = 0; /* not used */
                client_cmd.m_pad         = 0;

                client_cmd.m_min_port    = lpMan->m_port.m_min_value;
                client_cmd.m_max_port    = lpMan->m_port.m_max_value;
                client_cmd.m_step_port   = lpMan->m_port.m_step;
                
                client_cmd.m_min_ip      = lpMan->m_ip.m_min_value;
                client_cmd.m_max_ip      = lpMan->m_ip.m_max_value;
                client_cmd.m_step_ip     = lpMan->m_ip.m_step;

                client_cmd.m_init_ip     = lpMan->m_ip.m_init_value;
                client_cmd.m_init_port   = lpMan->m_port.m_init_value;

                client_cmd.m_limit_flows = lpMan->m_limit_num_flows;
                m_instructions.add_command(&client_cmd,sizeof(client_cmd));
            }
        }


        if (ins_type == StreamVmInstruction::itPKT_SIZE_CHANGE ) {
            StreamVmInstructionChangePktSize *lpPkt =(StreamVmInstructionChangePktSize *)inst;

            var_ref_cnt++;

            VmFlowVarRec var;
            if ( var_lookup(lpPkt->m_flow_var_name ,var) == false){

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet size with no valid flow variable name '" << lpPkt->m_flow_var_name << "'" ;
                err(ss.str());
            }

            if ( var.m_size_bytes != 2 ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet size change should point to a flow variable with size 2  ";
                err(ss.str());
            }

            uint8_t       flow_offset = get_var_offset(lpPkt->m_flow_var_name);

            StreamDPOpPktSizeChange pkt_size_ch;
            pkt_size_ch.m_op =StreamDPVmInstructions::itPKT_SIZE_CHANGE;
            pkt_size_ch.m_flow_offset =  flow_offset;
            m_instructions.add_command(&pkt_size_ch,sizeof(pkt_size_ch));
        }

        ins_id++;
    }

    m_instructions.build_instruction_list();


    if ( var_ref_cnt > 0 && var_cnt ==0 ){
        std::stringstream ss;
        ss << "It is not valid to have a VM program without a variable  or tuple generator ";
        err(ss.str());
    }
}


void StreamVm::build_bss() {
    alloc_bss();
    uint8_t * p=(uint8_t *)m_bss;

    if ( m_is_random_var ){
        *((uint32_t*)p)=rand();
        p+=sizeof(uint32_t);
    }

    for (auto inst : m_inst_list) {
        p+=inst->set_bss_init_value(p);
    }
}


/**
 * clone VM from this VM to 'other'
 * 
 * @author imarom (22-Dec-15)
 * 
 * @param other 
 */
void 
StreamVm::clone(StreamVm &other) const {
    /* clear previous if any exists */
    for (auto instr : other.m_inst_list) {
        delete instr;
    }

    other.m_is_random_var        = false;
    other.m_is_change_pkt_size   = false;
    other.m_is_split_needed      = false;
    other.m_is_compiled          = false;
    other.m_pkt_trailer_size     = 0;

    other.m_inst_list.clear();

    for (auto instr : m_inst_list) {
        StreamVmInstruction *new_instr = instr->clone();
        other.add_instruction(new_instr);
    }
}

/**
 * actual work - compile the VM
 * 
 */
void StreamVm::compile(uint16_t pkt_len) {

    if (is_vm_empty()) {
        return;
    }

    m_pkt_size = pkt_len;

    /* build flow var offset table */
    build_flow_var_table() ;

    /* build init flow var memory */
    build_bss();

    build_program();

    if (m_pkt_trailer_size && (get_max_packet_update_offset() > (m_pkt_size - m_pkt_trailer_size))) {
        std::stringstream ss;
        ss << "regular VM writes cannot target the final " << (uint16_t)m_pkt_trailer_size << " byte RoCEv2 ICRC trailer";
        err(ss.str());
    }

    if ( get_max_packet_update_offset() >svMAX_PACKET_OFFSET_CHANGE ){
        std::stringstream ss;
        ss << "maximum offset is" << get_max_packet_update_offset() << " bigger than maximum " <<svMAX_PACKET_OFFSET_CHANGE;
        err(ss.str());
    }

    /* calculate the mbuf size that we should allocate */
    m_prefix_size = calc_writable_mbuf_size(get_max_packet_update_offset(), m_pkt_size);
    if (m_pkt_trailer_size) {
        if (m_pkt_size <= m_pkt_trailer_size) {
            err("RoCEv2 ICRC trailer is bigger than packet size");
        }
        uint16_t max_prefix_size = m_pkt_size - m_pkt_trailer_size;
        if (m_prefix_size > max_prefix_size) {
            m_prefix_size = max_prefix_size;
        }
    }

    m_is_compiled = true;
}


StreamVm::~StreamVm() {
    for (auto inst : m_inst_list) {
        delete inst;
    }          
    free_bss();
}

/**
 * calculate packet len data (min, max, expected size) of stream's VM
 * 
 */
void StreamVm::calc_pkt_len_data(uint16_t regular_pkt_size, TrexStreamPktLenData &pkt_len_data) const {

    /* if no packet size change - simply return the regular packet size */
    if (!m_is_change_pkt_size) {
        pkt_len_data.m_expected_pkt_len = regular_pkt_size;
        pkt_len_data.m_min_pkt_len = regular_pkt_size;
        pkt_len_data.m_max_pkt_len = regular_pkt_size;
        return;
    }
    /* if we have an instruction that changes the packet size,
       we must compile the VM temporarly to get values
     */

    StreamVm dummy;

    this->clone(dummy);
    dummy.compile(regular_pkt_size);

    assert(dummy.m_pkt_len_data.m_expected_pkt_len != 0);

    pkt_len_data = dummy.m_pkt_len_data;
}

/** 
* return a pointer to a flow var / client var 
* by name if exists, otherwise NULL 
* 
*/
StreamVmInstructionVar *
StreamVm::lookup_var_by_name(const std::string &var_name) {
    for (StreamVmInstruction *inst : m_inst_list) {

        /* try to cast up to a variable */
        StreamVmInstructionVar *var = dynamic_cast<StreamVmInstructionVar *>(inst);
        if (!var) {
            continue;
        }

        if (var->get_var_name() == var_name) {
            return var;
        }

    }

    return NULL;
}


void StreamVm::Dump(FILE *fd){
    fprintf(fd," instructions \n");
    uint32_t cnt=0;
    for (auto inst : m_inst_list) {
        fprintf(fd," [%04lu]   : ",(ulong)cnt);
        inst->Dump(fd);
        cnt++;
    }

    if ( get_bss_size() ) {
        fprintf(fd," BSS  size %lu\n",(ulong)get_bss_size());
        utl_DumpBuffer(fd,get_bss_ptr(),get_bss_size(),0);
    }

    if  ( m_instructions.get_program_size() > 0 ){
        fprintf(fd," RAW instructions \n");
        m_instructions.Dump(fd);
    }
}


void StreamDPVmInstructions::clear(){
    m_inst_list.clear();
}


void StreamDPVmInstructions::add_command(void *buffer,uint32_t size){
    uint8_t *p= (uint8_t *)buffer;
    std::vector<uint8_t> tmp;
    // push buffer byte by byte in tmp
    for ( int i=0; i<size; i++ ) {
        tmp.push_back(*p);
        p++;
    }
    commands.push_back(tmp);
}

void StreamDPVmInstructions::build_instruction_list() {
    for (int i = commands.size()-1; i >= 0; i--) {
        for (int j = 0; j < commands[i].size(); j++) {
            m_inst_list.push_back(commands[i][j]);
        }
    }
}

uint8_t * StreamDPVmInstructions::get_program(){
    return (&m_inst_list[0]);
}

uint32_t StreamDPVmInstructions::get_program_size(){
    return (m_inst_list.size());
}

void StreamDPVmInstructions::Dump(FILE *fd){

    uint8_t * p=get_program();


    uint32_t program_size = get_program_size();
    uint8_t * p_end=p+program_size;

    StreamDPOpFlowVar8Step  *lpv8s;
    StreamDPOpFlowVar16Step *lpv16s;
    StreamDPOpFlowVar32Step *lpv32s;
    StreamDPOpFlowVar64Step *lpv64s;

    StreamDPOpHwCsFix   *lpHwFix;
    StreamDPOpRoceIcrcFix *lpRoceIcrcFix;

    StreamDPOpIpv4Fix   *lpIpv4Fix;
    StreamDPOpIcmpv6Fix *lpIcmpv6Fix;
    StreamDPOpPktWr8     *lpw8;
    StreamDPOpPktWr16    *lpw16;
    StreamDPOpPktWr32    *lpw32;
    StreamDPOpPktWr64    *lpw64;
    StreamDPOpPktWrMask  *lpwrmask;
    StreamDPOpClientsLimit   *lp_client;
    StreamDPOpClientsUnLimit  *lp_client_unlimited;
    StreamDPOpPktSizeChange  *lp_pkt_size_change;

    StreamDPOpFlowRandLimit8  * lpv_rl8;
    StreamDPOpFlowRandLimit16 * lpv_rl16;
    StreamDPOpFlowRandLimit32 * lpv_rl32;
    StreamDPOpFlowRandLimit64 * lpv_rl64;

    while ( p < p_end) {
        uint8_t op_code=*p;
        switch (op_code) {

        case  ditRANDOM8 :
            lpv8s =(StreamDPOpFlowVar8Step *)p;
            lpv8s->dump(fd,"RAND8");
            p+=sizeof(StreamDPOpFlowVar8Step) + lpv8s->get_sizeof_list();
            break;
        case  ditRANDOM16 :
            lpv16s =(StreamDPOpFlowVar16Step *)p;
            lpv16s->dump(fd,"RAND16");
            p+=sizeof(StreamDPOpFlowVar16Step) + lpv16s->get_sizeof_list();
            break;
        case  ditRANDOM32 :
            lpv32s =(StreamDPOpFlowVar32Step *)p;
            lpv32s->dump(fd,"RAND32");
            p+=sizeof(StreamDPOpFlowVar32Step) + lpv32s->get_sizeof_list();
            break;
        case  ditRANDOM64 :
            lpv64s =(StreamDPOpFlowVar64Step *)p;
            lpv64s->dump(fd,"RAND64");
            p+=sizeof(StreamDPOpFlowVar64Step) + lpv64s->get_sizeof_list();
            break;

        case  ditFIX_HW_CS :
            lpHwFix =(StreamDPOpHwCsFix *)p;
            lpHwFix->dump(fd,"HwFixCs");
            p+=sizeof(StreamDPOpHwCsFix);
            break;

        case  ditFIX_ROCEV2_ICRC :
            lpRoceIcrcFix =(StreamDPOpRoceIcrcFix *)p;
            lpRoceIcrcFix->dump(fd,"RoceIcrcFix");
            p+=sizeof(StreamDPOpRoceIcrcFix);
            break;

        case  ditFIX_IPV4_CS :
            lpIpv4Fix =(StreamDPOpIpv4Fix *)p;
            lpIpv4Fix->dump(fd,"Ipv4Fix");
            p+=sizeof(StreamDPOpIpv4Fix);
            break;

        case  ditFIX_ICMPV6_CS :
            lpIcmpv6Fix =(StreamDPOpIcmpv6Fix *)p;
            lpIcmpv6Fix->dump(fd,"Icmpv6Fix");
            p+=sizeof(StreamDPOpIcmpv6Fix);
            break;

        case  itPKT_WR8  :
            lpw8 =(StreamDPOpPktWr8 *)p;
            lpw8->dump(fd,"Wr8");
            p+=sizeof(StreamDPOpPktWr8);
            break;

        case  itPKT_WR16 :
            lpw16 =(StreamDPOpPktWr16 *)p;
            lpw16->dump(fd,"Wr16");
            p+=sizeof(StreamDPOpPktWr16);
            break;

        case  itPKT_WR32 :
            lpw32 =(StreamDPOpPktWr32 *)p;
            lpw32->dump(fd,"Wr32");
            p+=sizeof(StreamDPOpPktWr32);
            break;

        case  itPKT_WR64 :      
            lpw64 =(StreamDPOpPktWr64 *)p;
            lpw64->dump(fd,"Wr64");
            p+=sizeof(StreamDPOpPktWr64);
            break;

        case  itCLIENT_VAR :      
            lp_client =(StreamDPOpClientsLimit *)p;
            lp_client->dump(fd,"Client");
            p+=sizeof(StreamDPOpClientsLimit);
            break;

        case  itCLIENT_VAR_UNLIMIT :      
            lp_client_unlimited =(StreamDPOpClientsUnLimit *)p;
            lp_client_unlimited->dump(fd,"ClientUnlimted");
            p+=sizeof(StreamDPOpClientsUnLimit);
            break;

        case  itPKT_SIZE_CHANGE :      
            lp_pkt_size_change =(StreamDPOpPktSizeChange *)p;
            lp_pkt_size_change->dump(fd,"pkt_size_c");
            p+=sizeof(StreamDPOpPktSizeChange);
            break;

        case  ditINC8_STEP   :
            lpv8s =(StreamDPOpFlowVar8Step *)p;
            lpv8s->dump(fd,"INC8_STEP");
            p+=sizeof(StreamDPOpFlowVar8Step) + lpv8s->get_sizeof_list();
            break;
        case  ditINC16_STEP :
            lpv16s =(StreamDPOpFlowVar16Step *)p;
            lpv16s->dump(fd,"INC16_STEP");
            p+=sizeof(StreamDPOpFlowVar16Step) + lpv16s->get_sizeof_list();
            break;
        case  ditINC32_STEP :
            lpv32s =(StreamDPOpFlowVar32Step *)p;
            lpv32s->dump(fd,"INC32_STEP");
            p+=sizeof(StreamDPOpFlowVar32Step) + lpv32s->get_sizeof_list();
             break;
        case  ditINC64_STEP :
            lpv64s =(StreamDPOpFlowVar64Step *)p;
            lpv64s->dump(fd,"INC64_STEP");
            p+=sizeof(StreamDPOpFlowVar64Step) + lpv64s->get_sizeof_list();
             break;

        case  ditDEC8_STEP :
            lpv8s =(StreamDPOpFlowVar8Step *)p;
            lpv8s->dump(fd,"DEC8_DEC");
            p+=sizeof(StreamDPOpFlowVar8Step) + lpv8s->get_sizeof_list();
            break;
        case  ditDEC16_STEP :
            lpv16s =(StreamDPOpFlowVar16Step *)p;
            lpv16s->dump(fd,"DEC16_STEP");
            p+=sizeof(StreamDPOpFlowVar16Step) + lpv16s->get_sizeof_list();
            break;
        case  ditDEC32_STEP :
            lpv32s =(StreamDPOpFlowVar32Step *)p;
            lpv32s->dump(fd,"DEC32_STEP");
            p+=sizeof(StreamDPOpFlowVar32Step) + lpv32s->get_sizeof_list();
            break;
        case  ditDEC64_STEP :  
            lpv64s =(StreamDPOpFlowVar64Step *)p;
            lpv64s->dump(fd,"DEC64_STEP");
            p+=sizeof(StreamDPOpFlowVar64Step) + lpv64s->get_sizeof_list();
            break;

        case  itPKT_WR_MASK :  
            lpwrmask =(StreamDPOpPktWrMask *)p;
            lpwrmask->dump(fd,"WR_MASK");
            p+=sizeof(StreamDPOpPktWrMask);
            break;

        case  ditRAND_LIMIT8 :  
            lpv_rl8 =(StreamDPOpFlowRandLimit8 *)p;
            lpv_rl8->dump(fd,"RAND_LIMIT8");
            p+=sizeof(StreamDPOpFlowRandLimit8);
            break;

        case  ditRAND_LIMIT16 :  
            lpv_rl16 =(StreamDPOpFlowRandLimit16 *)p;
            lpv_rl16->dump(fd,"RAND_LIMIT16");
            p+=sizeof(StreamDPOpFlowRandLimit16);
            break;

        case  ditRAND_LIMIT32 :  
            lpv_rl32 =(StreamDPOpFlowRandLimit32 *)p;
            lpv_rl32->dump(fd,"RAND_LIMIT32");
            p+=sizeof(StreamDPOpFlowRandLimit32);
            break;

        case  ditRAND_LIMIT64 :  
            lpv_rl64 =(StreamDPOpFlowRandLimit64 *)p;
            lpv_rl64->dump(fd,"RAND_LIMIT64");
            p+=sizeof(StreamDPOpFlowRandLimit64);
            break;

        default:
            assert(0);
        }
    };
}


void StreamDPOpFlowVar8Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu), ls:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step,(ulong)m_list_size);
}

void StreamDPOpFlowVar16Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu), ls:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step,(ulong)m_list_size);
}

void StreamDPOpFlowVar32Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu), ls:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step,(ulong)m_list_size);
}

void StreamDPOpFlowVar64Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu), ls:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step,(ulong)m_list_size);
}


void StreamDPOpPktWr8::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flags:%lu, pkt_of:%lu,  f_of:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr16::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flags:%lu, pkt_of:%lu , f_of:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr32::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flags:%lu, pkt_of:%lu , f_of:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr64::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flags:%lu, pkt_of:%lu , f_of:%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWrMask::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flags:%lu, var_of:%lu , (%ld-%lu-%lu-%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flags,(ulong)m_var_offset,(long)m_shift,(ulong)m_pkt_offset,(ulong)m_mask,(ulong)m_pkt_cast_size,(ulong)m_flowv_cast_size);
}


void StreamDPOpHwCsFix::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, lens: %lu,%lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_l2_len,(ulong)m_l3_len);

}

void StreamDPOpRoceIcrcFix::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, lens: %lu,%lu, bth: %lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_l2_len,(ulong)m_l3_len,(ulong)m_bth_offset);
}

void StreamDPOpIpv4Fix::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, offset: %lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_offset);
}

void StreamDPOpIcmpv6Fix::dump(FILE *fd, std::string opt){
    fprintf(fd," %10s  op:%lu, len: %lu,%lu \n",  opt.c_str(), (ulong)m_op, (ulong)m_l2_len, (ulong)m_l3_len);
}

void StreamDPOpClientsLimit::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flow_offset: %lu (%x-%x) (%x-%x) flow_limit :%lu flags:%x \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,m_min_ip,m_max_ip,m_min_port,m_max_port,(ulong)m_limit_flows,m_flags);
}

void StreamDPOpClientsUnLimit::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flow_offset: %lu (%x-%x) flags:%x \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,m_min_ip,m_max_ip,m_flags);
}

void StreamDPOpPktSizeChange::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flow_offset: %lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset);
}


void StreamDPOpFlowRandLimit8::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s, flow_offset: %lu  limit :%lu seed:%x (%x-%x) \n",  opt.c_str(),(ulong)m_flow_offset,(ulong)m_limit,m_seed,m_min_val,m_max_val);
}

void StreamDPOpFlowRandLimit16::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s, flow_offset: %lu  limit :%lu seed:%x (%x-%x) \n",  opt.c_str(),(ulong)m_flow_offset,(ulong)m_limit,m_seed,m_min_val,m_max_val);
}

void StreamDPOpFlowRandLimit32::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s, flow_offset: %lu  limit :%lu seed:%x (%x-%x) \n",  opt.c_str(),(ulong)m_flow_offset,(ulong)m_limit,m_seed,m_min_val,m_max_val);
}

void StreamDPOpFlowRandLimit64::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s, flow_offset: %lu  limit :%lu seed:%x (%lu-%lu) \n",  opt.c_str(),(ulong)m_flow_offset,(ulong)m_limit,m_seed,m_min_val,m_max_val);
}




void StreamDPOpPktWrMask::wr(uint8_t * flow_var_base,
                             uint8_t * pkt_base) {
        uint32_t val=0;
        uint8_t * pv=(flow_var_base+m_var_offset);
        /* read flow var with the right size */
        switch (m_flowv_cast_size) {
        case 1:
            val= (uint32_t)(*((uint8_t*)pv));
            break;
        case 2:
            val=(uint32_t)(*((uint16_t*)pv));
            break;
        case 4:
            val=(*((uint32_t*)pv));
            break;
        default:
          assert(0);    
        }

        val+=m_add_value;

        /* shift the flow var val */
        if (m_shift>0) {
            val=val<<m_shift;
        }else{
            if (m_shift<0) {
                val=val>>(-m_shift);
            }
        }

        uint8_t * p_pkt = pkt_base+m_pkt_offset;
        uint32_t pkt_val=0;

        /* RMW */
        if ( likely( is_big() ) ) {

            switch (m_pkt_cast_size) {
            case 1:
                pkt_val= (uint32_t)(*((uint8_t*)p_pkt));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) & 0xff;   
                *p_pkt=pkt_val;
                break;
            case 2:
                pkt_val= (uint32_t)PKT_NTOHS((*((uint16_t*)p_pkt)));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) & 0xffff;   
                *((uint16_t*)p_pkt)=PKT_NTOHS(pkt_val);
                break;
            case 4:
                pkt_val= (uint32_t)PKT_NTOHL((*((uint32_t*)p_pkt)));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) ;   
                *((uint32_t*)p_pkt)=PKT_NTOHL(pkt_val);
                break;
            default:
              assert(0);    
            }
        }else{
            switch (m_flowv_cast_size) {
            case 1:
                pkt_val= (uint32_t)(*((uint8_t*)p_pkt));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) & 0xff;   
                *p_pkt=pkt_val;
                break;
            case 2:
                pkt_val= (uint32_t)(*((uint16_t*)p_pkt));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) & 0xffff;   
                *((uint16_t*)p_pkt)=pkt_val;
                break;
            case 4:
                pkt_val= (uint32_t)(*((uint32_t*)p_pkt));
                pkt_val = ((pkt_val & ~m_mask) | (val & m_mask)) ;   
                *((uint32_t*)p_pkt)=pkt_val;
                break;
            default:
              assert(0);    
            }
        }
}




void   StreamDPVmInstructionsRunner::slow_commands(uint8_t op_code,
                       uint8_t * flow_var, /* flow var */
                       uint8_t * pkt,
                       uint8_t * & p){
    ua_t ua;

    switch (op_code) {
    case  StreamDPVmInstructions::itPKT_WR_MASK:
        ua.lpwr_mask =(StreamDPOpPktWrMask *)p;
        ua.lpwr_mask->wr(flow_var,pkt);
        p+=sizeof(StreamDPOpPktWrMask);
        break;

    case StreamDPVmInstructions::ditRAND_LIMIT8:
        ua.lpv_rl8 =(StreamDPOpFlowRandLimit8 *)p;
        ua.lpv_rl8->run(flow_var, &p);
        p+=sizeof(StreamDPOpFlowRandLimit8);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT16:
        ua.lpv_rl16 =(StreamDPOpFlowRandLimit16 *)p;
        ua.lpv_rl16->run(flow_var, &p);
        p+=sizeof(StreamDPOpFlowRandLimit16);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT32:
        ua.lpv_rl32 =(StreamDPOpFlowRandLimit32 *)p;
        ua.lpv_rl32->run(flow_var, &p);
        p+=sizeof(StreamDPOpFlowRandLimit32);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT64:
        ua.lpv_rl64 =(StreamDPOpFlowRandLimit64 *)p;
        ua.lpv_rl64->run(flow_var, &p);
        p+=sizeof(StreamDPOpFlowRandLimit64);
        break;

    default:
        assert(0);
    }
}
