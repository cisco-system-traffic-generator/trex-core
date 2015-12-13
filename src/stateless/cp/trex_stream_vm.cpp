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



void StreamVmInstructionFlowMan::sanity_check(uint32_t ins_id,StreamVm *lp){

    sanity_check_valid_size(ins_id,lp);
    sanity_check_valid_opt(ins_id,lp);
    sanity_check_valid_range(ins_id,lp);
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

    fprintf(fd," (%lu:%lu:%lu) \n",m_init_value,m_min_value,m_max_value);
}


void StreamVmInstructionWriteToPkt::Dump(FILE *fd){

    fprintf(fd," write_pkt , %s ,%lu, add, %ld, big, %lu \n",m_flow_var_name.c_str(),(ulong)m_pkt_offset,(long)m_add_value,(ulong)(m_is_big_endian?1:0));
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
    m_inst_list.push_back(inst);
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
                var.m_instruction = ins_man;
                var_add(ins_man->m_var_name,var);
                m_cur_var_offset += ins_man->m_size_bytes;

                /* limit the flow var size */
                if (m_cur_var_offset > StreamVm::svMAX_FLOW_VAR ) {
                    std::stringstream ss;
                    ss << "too many flow varibles current size is :" << m_cur_var_offset << " maximum support is " << StreamVm::svMAX_FLOW_VAR;
                    err(ss.str());
                }
            }
        }
        ins_id++;
    }

}

void StreamVm::alloc_bss(){
    free_bss();
    m_bss=(uint8_t *)malloc(m_cur_var_offset);
}

void StreamVm::free_bss(){
    if (m_bss) {
        free(m_bss);
        m_bss=0;
    }
}


void StreamVm::build_program(){

}


void StreamVm::build_bss() {
    alloc_bss();
    uint8_t * p=(uint8_t *)m_bss;

    for (auto inst : m_inst_list) {

        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_MAN ){

            StreamVmInstructionFlowMan * ins_man=(StreamVmInstructionFlowMan *)inst;

            switch (ins_man->m_size_bytes) {
            case 1:
                *p=(uint8_t)ins_man->m_init_value;
                p+=1;
                break;
            case 2:
                *((uint16_t*)p)=(uint16_t)ins_man->m_init_value;
                p+=2;
                break;
            case 4:
                *((uint32_t*)p)=(uint32_t)ins_man->m_init_value;
                p+=4;
                break;
            case 8:
                *((uint64_t*)p)=(uint64_t)ins_man->m_init_value;
                p+=8;
                break;
            default:
                assert(0);
            }
        }
    }
}



void StreamVm::compile_next() {

    /* build flow var offset table */
    build_flow_var_table() ;

    /* build init flow var memory */
    build_bss();

    build_program();


}


bool StreamVm::compile() {

    //m_flow_var_offset

    return (false);
}

StreamVm::~StreamVm() {
    for (auto inst : m_inst_list) {
        delete inst;
    }          
    free_bss();
}


void StreamVm::Dump(FILE *fd){
    uint32_t cnt=0;
    for (auto inst : m_inst_list) {
        fprintf(fd," [%04lu]   : ",(ulong)cnt);
        inst->Dump(fd);
        cnt++;
    }
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

    //uint8_t * p=get_program();


}


void StreamDPOpFlowVar8::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar16::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar32::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpFlowVar64::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flow_offset,(ulong)m_min_val,(ulong)m_max_val);
}

void StreamDPOpPktWr8::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr16::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr32::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}

void StreamDPOpPktWr64::dump(FILE *fd){
    fprintf(fd," %lu, %lu, %lu , %lu \n",  (ulong)m_op,(ulong)m_flags,(ulong)m_pkt_offset,(ulong)m_offset);
}


void StreamDPOpIpv4Fix::dump(FILE *fd){
    fprintf(fd," %lu, %lu \n",  (ulong)m_op,(ulong)m_offset);
}




