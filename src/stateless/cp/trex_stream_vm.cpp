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
#include <trex_stream_vm.h>
#include <sstream>
#include <assert.h>
#include <iostream>
#include <trex_stateless.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/basic_utils.h>
#include <inttypes.h>

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



uint8_t  StreamVmInstructionFlowMan::bss_init_value(uint8_t *p){
    uint8_t res;

    switch (m_size_bytes) {
    case 1:
        *p=(uint8_t)get_bss_init_value();
        res=1;
        break;
    case 2:
        *((uint16_t*)p)=(uint16_t)get_bss_init_value();
        res=2;
        break;
    case 4:
        *((uint32_t*)p)=(uint32_t)get_bss_init_value();
        res=4;
        break;
    case 8:
        *((uint64_t*)p)=(uint64_t)get_bss_init_value();
        res=8;
        break;
    default:
        assert(0);
    }
    return(res);
}


void StreamVmInstructionFlowMan::sanity_check(uint32_t ins_id,StreamVm *lp){

    sanity_check_valid_size(ins_id,lp);
    sanity_check_valid_opt(ins_id,lp);
    sanity_check_valid_range(ins_id,lp);
}



void StreamVmInstructionFlowRandLimit::Dump(FILE *fd){
    fprintf(fd," flow_var_rand_limit  , %s ,%lu,   ",m_var_name.c_str(),(ulong)m_size_bytes);
    fprintf(fd," (%lu:%lu:%lu) (min:%lu,max:%lu) \n",m_limit,(ulong)m_size_bytes,(ulong)m_seed,m_min_value,m_max_value);
}

void StreamVmInstructionFlowRandLimit::sanity_check(uint32_t ins_id,StreamVm *lp){
    sanity_check_valid_size(ins_id,lp);
}


uint8_t  StreamVmInstructionFlowRandLimit::bss_init_value(uint8_t *p){
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

    fprintf(fd," (%lu:%lu:%lu:%lu) \n",m_init_value,m_min_value,m_max_value,m_step);
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


uint8_t  StreamVmInstructionFlowClient::bss_init_value(uint8_t *p){

    if (m_client_min>0) {
       *((uint32_t*)p)=(uint32_t)(m_client_min-1);
    }else{
       *((uint32_t*)p)=(uint32_t)m_client_min;
    }

    p+=4;

    if (is_unlimited_flows() ) {
        *((uint16_t*)p)=StreamDPOpClientsUnLimit::CLIENT_UNLIMITED_MIN_PORT;
    }else{
        *((uint16_t*)p)=(uint16_t)m_port_min;
    }

    p+=2;

    *((uint32_t*)p)=0;
    p+=4;

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

            if ( var.m_ins.m_ins_flowv->m_max_value >  m_pkt_size) {
                var.m_ins.m_ins_flowv->m_max_value =m_pkt_size;
            }

            if (var.m_ins.m_ins_flowv->m_min_value > m_pkt_size) {
                var.m_ins.m_ins_flowv->m_min_value = m_pkt_size;
            }


            if ( var.m_ins.m_ins_flowv->m_min_value >= var.m_ins.m_ins_flowv->m_max_value  ) {
                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' min packet size " << var.m_ins.m_ins_flowv->m_min_value << " is bigger or eq to max packet size " << var.m_ins.m_ins_flowv->m_max_value;
                err(ss.str());
            }

            if ( var.m_ins.m_ins_flowv->m_min_value < 60) {
                var.m_ins.m_ins_flowv->m_min_value =60;
            }

            /* expected packet size calculation */

            /* for random packet size - we need to find the average */
            if (var.m_ins.m_ins_flowv->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM) {
                uint16_t range = var.m_ins.m_ins_flowv->m_max_value - var.m_ins.m_ins_flowv->m_min_value;
                m_expected_pkt_size = var.m_ins.m_ins_flowv->m_min_value + g_fastrand_util.calc_fastrand_avg(range);
            } else {
                m_expected_pkt_size = (var.m_ins.m_ins_flowv->m_min_value + var.m_ins.m_ins_flowv->m_max_value) / 2.0;
            }
            
        }
    }

}

void StreamVm::alloc_bss(){
    free_bss();
    m_bss=(uint8_t *)malloc(m_cur_var_offset);
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
    clean_max_field_cnt();
    uint32_t ins_id=0;

    for (auto inst : m_inst_list) {
        StreamVmInstruction::instruction_type_t ins_type=inst->get_instruction_type();

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
                m_instructions.add_command(&fv64,sizeof(fv64));
            }
        }

        /* flow man */
        if (ins_type == StreamVmInstruction::itFLOW_MAN) {
            StreamVmInstructionFlowMan *lpMan =(StreamVmInstructionFlowMan *)inst;

            var_cnt++;

            if (lpMan->m_size_bytes == 1 ){
                if ( (lpMan->m_step == 1) || (lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ) ){
                    uint8_t op=StreamDPVmInstructions::ditINC8;

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC8 ;
                    }

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC8 ;
                    }

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ){
                        op = StreamDPVmInstructions::ditRANDOM8 ;
                    }

                    StreamDPOpFlowVar8 fv8;
                    fv8.m_op = op;
                    fv8.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv8.m_min_val     = (uint8_t)lpMan->m_min_value;
                    fv8.m_max_val     = (uint8_t)lpMan->m_max_value;
                    m_instructions.add_command(&fv8,sizeof(fv8));
                }else{
                    uint8_t op=StreamDPVmInstructions::ditINC8_STEP;

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC8_STEP ;
                    }

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC8_STEP ;
                    }

                    StreamDPOpFlowVar8Step fv8;
                    fv8.m_op = op;
                    fv8.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv8.m_min_val     = (uint8_t)lpMan->m_min_value;
                    fv8.m_max_val     = (uint8_t)lpMan->m_max_value;
                    fv8.m_step        = (uint8_t)lpMan->m_step;
                    m_instructions.add_command(&fv8,sizeof(fv8));
                }
            }

            if (lpMan->m_size_bytes == 2 ){
                uint8_t op;
                if ( (lpMan->m_step == 1) || (lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ) ){


                op = StreamDPVmInstructions::ditINC16;

                if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                    op = StreamDPVmInstructions::ditINC16 ;
                }

                if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                    op = StreamDPVmInstructions::ditDEC16 ;
                }

                if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ){
                    op = StreamDPVmInstructions::ditRANDOM16 ;
                }

                StreamDPOpFlowVar16 fv16;
                fv16.m_op = op;
                fv16.m_flow_offset = get_var_offset(lpMan->m_var_name);
                fv16.m_min_val     = (uint16_t)lpMan->m_min_value;
                fv16.m_max_val     = (uint16_t)lpMan->m_max_value;
                m_instructions.add_command(&fv16,sizeof(fv16));
              }else{

                  op = StreamDPVmInstructions::ditINC16_STEP;

                  if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                      op = StreamDPVmInstructions::ditINC16_STEP ;
                  }

                  if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                      op = StreamDPVmInstructions::ditDEC16_STEP ;
                  }

                  StreamDPOpFlowVar16Step fv16;
                  fv16.m_op = op;
                  fv16.m_flow_offset = get_var_offset(lpMan->m_var_name);
                  fv16.m_min_val     = (uint16_t)lpMan->m_min_value;
                  fv16.m_max_val     = (uint16_t)lpMan->m_max_value;
                  fv16.m_step        = (uint16_t)lpMan->m_step;

                  m_instructions.add_command(&fv16,sizeof(fv16));
              }
            }

            if (lpMan->m_size_bytes == 4 ){
                uint8_t op;
                if ( (lpMan->m_step == 1) || (lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ) ){
                    op = StreamDPVmInstructions::ditINC32;
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC32 ;
                    }
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC32 ;
                    }
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ){
                        op = StreamDPVmInstructions::ditRANDOM32 ;
                    }
    
                    StreamDPOpFlowVar32 fv32;
                    fv32.m_op = op;
                    fv32.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv32.m_min_val     = (uint32_t)lpMan->m_min_value;
                    fv32.m_max_val     = (uint32_t)lpMan->m_max_value;
                    m_instructions.add_command(&fv32,sizeof(fv32));
                }else{
                    op = StreamDPVmInstructions::ditINC32_STEP;

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC32_STEP ;
                    }

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC32_STEP ;
                    }

                    StreamDPOpFlowVar32Step fv32;
                    fv32.m_op = op;
                    fv32.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv32.m_min_val     = (uint32_t)lpMan->m_min_value;
                    fv32.m_max_val     = (uint32_t)lpMan->m_max_value;
                    fv32.m_step        = (uint32_t)lpMan->m_step;
                    m_instructions.add_command(&fv32,sizeof(fv32));
                }
            }


            if (lpMan->m_size_bytes == 8 ){
                uint8_t op;

                if ( (lpMan->m_step == 1) || (lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ) ){

                    op = StreamDPVmInstructions::ditINC64;
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC64 ;
                    }
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC64 ;
                    }
    
                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_RANDOM ){
                        op = StreamDPVmInstructions::ditRANDOM64 ;
                    }
    
                    StreamDPOpFlowVar64 fv64;
                    fv64.m_op = op;
                    fv64.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv64.m_min_val     = (uint64_t)lpMan->m_min_value;
                    fv64.m_max_val     = (uint64_t)lpMan->m_max_value;
                    m_instructions.add_command(&fv64,sizeof(fv64));
                }else{

                    op = StreamDPVmInstructions::ditINC64_STEP;

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_INC ){
                        op = StreamDPVmInstructions::ditINC64_STEP ;
                    }

                    if ( lpMan->m_op == StreamVmInstructionFlowMan::FLOW_VAR_OP_DEC ){
                        op = StreamDPVmInstructions::ditDEC64_STEP ;
                    }

                    StreamDPOpFlowVar64Step fv64;
                    fv64.m_op = op;
                    fv64.m_flow_offset = get_var_offset(lpMan->m_var_name);
                    fv64.m_min_val     = (uint64_t)lpMan->m_min_value;
                    fv64.m_max_val     = (uint64_t)lpMan->m_max_value;
                    fv64.m_step        = (uint64_t)lpMan->m_step;
                    m_instructions.add_command(&fv64,sizeof(fv64));
                }
            }
        }

        if (ins_type == StreamVmInstruction::itPKT_WR) {
            StreamVmInstructionWriteToPkt *lpPkt =(StreamVmInstructionWriteToPkt *)inst;

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
                client_cmd.m_port_step   = lpMan->m_port.m_step;

                client_cmd.m_min_ip      = lpMan->m_ip.m_min_value;
                client_cmd.m_max_ip      = lpMan->m_ip.m_max_value;
                client_cmd.m_ip_step     = lpMan->m_ip.m_step;

                client_cmd.m_limit_flows = lpMan->m_limit_num_flows;
                m_instructions.add_command(&client_cmd,sizeof(client_cmd));
            }
        }


        if (ins_type == StreamVmInstruction::itPKT_SIZE_CHANGE ) {
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

            uint8_t       flow_offset = get_var_offset(lpPkt->m_flow_var_name);

            StreamDPOpPktSizeChange pkt_size_ch;
            pkt_size_ch.m_op =StreamDPVmInstructions::itPKT_SIZE_CHANGE;
            pkt_size_ch.m_flow_offset =  flow_offset;
            m_instructions.add_command(&pkt_size_ch,sizeof(pkt_size_ch));
        }

        ins_id++;
    }


    if ( var_cnt ==0 ){
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
        p+=inst->bss_init_value(p);
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

    if ( get_max_packet_update_offset() >svMAX_PACKET_OFFSET_CHANGE ){
        std::stringstream ss;
        ss << "maximum offset is" << get_max_packet_update_offset() << " bigger than maximum " <<svMAX_PACKET_OFFSET_CHANGE;
        err(ss.str());
    }

    /* calculate the mbuf size that we should allocate */
    m_prefix_size = calc_writable_mbuf_size(get_max_packet_update_offset(), m_pkt_size);

    m_is_compiled = true;
}


StreamVm::~StreamVm() {
    for (auto inst : m_inst_list) {
        delete inst;
    }          
    free_bss();
}

/**
 * calculate expected packet size of stream's VM
 * 
 */
double
StreamVm::calc_expected_pkt_size(uint16_t regular_pkt_size) const {

    /* if no packet size change - simply return the regular packet size */
    if (!m_is_change_pkt_size) {
        return regular_pkt_size;
    }
    /* if we have an instruction that changes the packet size
       so find the expected size
       we must compile the VM temporarly to get this value
     */

    StreamVm dummy;

    this->clone(dummy);
    dummy.compile(regular_pkt_size);

    assert(dummy.m_expected_pkt_size != 0);

    return (dummy.m_expected_pkt_size);
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


void StreamDPVmInstructions::add_command(void *buffer,uint16_t size){
    int i;
    uint8_t *p= (uint8_t *)buffer;
    /* push byte by byte */
    for (i=0; i<size; i++) {
        m_inst_list.push_back(*p);
        p++;
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

    StreamDPOpFlowVar8  *lpv8;
    StreamDPOpFlowVar16 *lpv16;
    StreamDPOpFlowVar32 *lpv32;
    StreamDPOpFlowVar64 *lpv64;

    StreamDPOpFlowVar8Step  *lpv8s;
    StreamDPOpFlowVar16Step *lpv16s;
    StreamDPOpFlowVar32Step *lpv32s;
    StreamDPOpFlowVar64Step *lpv64s;

    StreamDPOpIpv4Fix   *lpIpv4Fix;
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

        case   ditINC8  :
            lpv8 =(StreamDPOpFlowVar8 *)p;
            lpv8->dump(fd,"INC8");
            p+=sizeof(StreamDPOpFlowVar8);
            break;
        case  ditINC16  :
            lpv16 =(StreamDPOpFlowVar16 *)p;
            lpv16->dump(fd,"INC16");
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  ditINC32 :
            lpv32 =(StreamDPOpFlowVar32 *)p;
            lpv32->dump(fd,"INC32");
            p+=sizeof(StreamDPOpFlowVar32);
             break;
        case  ditINC64 :
            lpv64 =(StreamDPOpFlowVar64 *)p;
            lpv64->dump(fd,"INC64");
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  ditDEC8 :
            lpv8 =(StreamDPOpFlowVar8 *)p;
            lpv8->dump(fd,"DEC8");
            p+=sizeof(StreamDPOpFlowVar8);
            break;
        case  ditDEC16 :
            lpv16 =(StreamDPOpFlowVar16 *)p;
            lpv16->dump(fd,"DEC16");
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  ditDEC32 :
            lpv32 =(StreamDPOpFlowVar32 *)p;
            lpv32->dump(fd,"DEC32");
            p+=sizeof(StreamDPOpFlowVar32);
            break;
        case  ditDEC64 :
            lpv64 =(StreamDPOpFlowVar64 *)p;
            lpv64->dump(fd,"DEC64");
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  ditRANDOM8 :
            lpv8 =(StreamDPOpFlowVar8 *)p;
            lpv8->dump(fd,"RAND8");
            p+=sizeof(StreamDPOpFlowVar8);
            break;
        case  ditRANDOM16 :
            lpv16 =(StreamDPOpFlowVar16 *)p;
            lpv16->dump(fd,"RAND16");
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  ditRANDOM32 :
            lpv32 =(StreamDPOpFlowVar32 *)p;
            lpv32->dump(fd,"RAND32");
            p+=sizeof(StreamDPOpFlowVar32);
            break;
        case  ditRANDOM64 :
            lpv64 =(StreamDPOpFlowVar64 *)p;
            lpv64->dump(fd,"RAND64");
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  ditFIX_IPV4_CS :
            lpIpv4Fix =(StreamDPOpIpv4Fix *)p;
            lpIpv4Fix->dump(fd,"Ipv4Fix");
            p+=sizeof(StreamDPOpIpv4Fix);
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
            p+=sizeof(StreamDPOpFlowVar8Step);
            break;
        case  ditINC16_STEP :
            lpv16s =(StreamDPOpFlowVar16Step *)p;
            lpv16s->dump(fd,"INC16_STEP");
            p+=sizeof(StreamDPOpFlowVar16Step);
            break;
        case  ditINC32_STEP :
            lpv32s =(StreamDPOpFlowVar32Step *)p;
            lpv32s->dump(fd,"INC32_STEP");
            p+=sizeof(StreamDPOpFlowVar32Step);
             break;
        case  ditINC64_STEP :
            lpv64s =(StreamDPOpFlowVar64Step *)p;
            lpv64s->dump(fd,"INC64_STEP");
            p+=sizeof(StreamDPOpFlowVar64Step);
             break;

        case  ditDEC8_STEP :
            lpv8s =(StreamDPOpFlowVar8Step *)p;
            lpv8s->dump(fd,"DEC8_DEC");
            p+=sizeof(StreamDPOpFlowVar8Step);
            break;
        case  ditDEC16_STEP :
            lpv16s =(StreamDPOpFlowVar16Step *)p;
            lpv16s->dump(fd,"DEC16_STEP");
            p+=sizeof(StreamDPOpFlowVar16Step);
            break;
        case  ditDEC32_STEP :
            lpv32s =(StreamDPOpFlowVar32Step *)p;
            lpv32s->dump(fd,"DEC32_STEP");
            p+=sizeof(StreamDPOpFlowVar32Step);
            break;
        case  ditDEC64_STEP :  
            lpv64s =(StreamDPOpFlowVar64Step *)p;
            lpv64s->dump(fd,"DEC64_STEP");
            p+=sizeof(StreamDPOpFlowVar64Step);
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
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step);
}

void StreamDPOpFlowVar16Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step);
}

void StreamDPOpFlowVar32Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step);
}

void StreamDPOpFlowVar64Step::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val,(ulong)m_step);
}


void StreamDPOpFlowVar8::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu- %lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar16::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar32::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar64::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, of:%lu, (%lu-%lu) \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
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


void StreamDPOpIpv4Fix::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, offset: %lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_offset);
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

    case   StreamDPVmInstructions::ditINC8_STEP  :
        ua.lpv8s =(StreamDPOpFlowVar8Step *)p;
        ua.lpv8s->run_inc(flow_var);
        p+=sizeof(StreamDPOpFlowVar8Step);
        break;

    case  StreamDPVmInstructions::ditINC16_STEP  :
        ua.lpv16s =(StreamDPOpFlowVar16Step *)p;
        ua.lpv16s->run_inc(flow_var);
        p+=sizeof(StreamDPOpFlowVar16Step);
        break;
    case  StreamDPVmInstructions::ditINC32_STEP :
        ua.lpv32s =(StreamDPOpFlowVar32Step *)p;
        ua.lpv32s->run_inc(flow_var);
        p+=sizeof(StreamDPOpFlowVar32Step);
         break;
    case  StreamDPVmInstructions::ditINC64_STEP :
        ua.lpv64s =(StreamDPOpFlowVar64Step *)p;
        ua.lpv64s->run_inc(flow_var);
        p+=sizeof(StreamDPOpFlowVar64Step);
        break;

    case  StreamDPVmInstructions::ditDEC8_STEP :
        ua.lpv8s =(StreamDPOpFlowVar8Step *)p;
        ua.lpv8s->run_dec(flow_var);
        p+=sizeof(StreamDPOpFlowVar8Step);
        break;
    case  StreamDPVmInstructions::ditDEC16_STEP :
        ua.lpv16s =(StreamDPOpFlowVar16Step *)p;
        ua.lpv16s->run_dec(flow_var);
        p+=sizeof(StreamDPOpFlowVar16Step);
        break;
    case  StreamDPVmInstructions::ditDEC32_STEP :
        ua.lpv32s =(StreamDPOpFlowVar32Step *)p;
        ua.lpv32s->run_dec(flow_var);
        p+=sizeof(StreamDPOpFlowVar32Step);
        break;
    case  StreamDPVmInstructions::ditDEC64_STEP :
        ua.lpv64s =(StreamDPOpFlowVar64Step *)p;
        ua.lpv64s->run_dec(flow_var);
        p+=sizeof(StreamDPOpFlowVar64Step);
        break;
    case  StreamDPVmInstructions::itPKT_WR_MASK:
        ua.lpwr_mask =(StreamDPOpPktWrMask *)p;
        ua.lpwr_mask->wr(flow_var,pkt);
        p+=sizeof(StreamDPOpPktWrMask);
        break;

    case StreamDPVmInstructions::ditRAND_LIMIT8:
        ua.lpv_rl8 =(StreamDPOpFlowRandLimit8 *)p;
        ua.lpv_rl8->run(flow_var);
        p+=sizeof(StreamDPOpFlowRandLimit8);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT16:
        ua.lpv_rl16 =(StreamDPOpFlowRandLimit16 *)p;
        ua.lpv_rl16->run(flow_var);
        p+=sizeof(StreamDPOpFlowRandLimit16);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT32:
        ua.lpv_rl32 =(StreamDPOpFlowRandLimit32 *)p;
        ua.lpv_rl32->run(flow_var);
        p+=sizeof(StreamDPOpFlowRandLimit32);
        break;
    case StreamDPVmInstructions::ditRAND_LIMIT64:
        ua.lpv_rl64 =(StreamDPOpFlowRandLimit64 *)p;
        ua.lpv_rl64->run(flow_var);
        p+=sizeof(StreamDPOpFlowRandLimit64);
        break;

    default:
        assert(0);
    }
}



