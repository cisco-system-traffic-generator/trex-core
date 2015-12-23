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



void StreamVmInstructionFlowClient::Dump(FILE *fd){

    fprintf(fd," client_var ,%s , ",m_var_name.c_str());

    fprintf(fd," ip:(%x-%x) port:(%x-%x)  flow_limit:%lu  flags: %x\n",m_client_min,m_client_max, m_port_min,m_port_max,(ulong)m_limit_num_flows,m_flags);
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
            ss << "too many flow varibles current size is :" << m_cur_var_offset << " maximum support is " << StreamVm::svMAX_FLOW_VAR;
            err(ss.str());
        }
        ins_id++;
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
    clean_max_field_cnt();
    uint32_t ins_id=0;

    for (auto inst : m_inst_list) {
        StreamVmInstruction::instruction_type_t ins_type=inst->get_instruction_type();

        /* itFIX_IPV4_CS */
        if (ins_type == StreamVmInstruction::itFIX_IPV4_CS) {
            StreamVmInstructionFixChecksumIpv4 *lpFix =(StreamVmInstructionFixChecksumIpv4 *)inst;

            add_field_cnt(lpFix->m_pkt_offset +12);

            if ( (lpFix->m_pkt_offset + IPV4_HDR_LEN) > m_pkt_size  ) {

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' fix ipv4 command offset  " << lpFix->m_pkt_offset << "  is too high relative to packet size  "<< m_pkt_size;
                err(ss.str());
            }

            StreamDPOpIpv4Fix ipv_fix;
            ipv_fix.m_offset = lpFix->m_pkt_offset;
            ipv_fix.m_op = StreamDPVmInstructions::ditFIX_IPV4_CS;
            m_instructions.add_command(&ipv_fix,sizeof(ipv_fix));
        }


        /* flow man */
        if (ins_type == StreamVmInstruction::itFLOW_MAN) {
            StreamVmInstructionFlowMan *lpMan =(StreamVmInstructionFlowMan *)inst;


            if (lpMan->m_size_bytes == 1 ){

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
            }

            if (lpMan->m_size_bytes == 2 ){
                uint8_t op;

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
            }

            if (lpMan->m_size_bytes == 4 ){
                uint8_t op;

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
            }

            if (lpMan->m_size_bytes == 8 ){
                uint8_t op;

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
            }
        }

        if (ins_type == StreamVmInstruction::itPKT_WR) {
            StreamVmInstructionWriteToPkt *lpPkt =(StreamVmInstructionWriteToPkt *)inst;

            VmFlowVarRec var;
            if ( var_lookup(lpPkt->m_flow_var_name ,var) == false){

                std::stringstream ss;
                ss << "instruction id '" << ins_id << "' packet write with no valid flow varible name '" << lpPkt->m_flow_var_name << "'" ;
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


        if (ins_type == StreamVmInstruction::itFLOW_CLIENT) {
            StreamVmInstructionFlowClient *lpMan =(StreamVmInstructionFlowClient *)inst;

            if ( lpMan->is_unlimited_flows() ){
                StreamDPOpClientsUnLimit  client_cmd;
                client_cmd.m_op =  StreamDPVmInstructions::itCLIENT_VAR_UNLIMIT;

                client_cmd.m_flow_offset = get_var_offset(lpMan->m_var_name+".ip"); /* start offset */
                client_cmd.m_flags       = 0; /* not used */
                client_cmd.m_pad         = 0;
                client_cmd.m_min_ip      = lpMan->m_client_min;
                client_cmd.m_max_ip      = lpMan->m_client_max;
                m_instructions.add_command(&client_cmd,sizeof(client_cmd));

            }else{
                StreamDPOpClientsLimit  client_cmd;
                client_cmd.m_op =  StreamDPVmInstructions::itCLIENT_VAR;

                client_cmd.m_flow_offset = get_var_offset(lpMan->m_var_name+".ip"); /* start offset */
                client_cmd.m_flags       = 0; /* not used */
                client_cmd.m_pad         = 0;
                client_cmd.m_min_port    = lpMan->m_port_min;
                client_cmd.m_max_port    = lpMan->m_port_max;
                client_cmd.m_min_ip      = lpMan->m_client_min;
                client_cmd.m_max_ip      = lpMan->m_client_max;
                client_cmd.m_limit_flows = lpMan->m_limit_num_flows;
                m_instructions.add_command(&client_cmd,sizeof(client_cmd));
            }
        }

        ins_id++;
    }
}


void StreamVm::build_bss() {
    alloc_bss();
    uint8_t * p=(uint8_t *)m_bss;

    for (auto inst : m_inst_list) {

        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_MAN ){

            StreamVmInstructionFlowMan * ins_man=(StreamVmInstructionFlowMan *)inst;

            switch (ins_man->m_size_bytes) {
            case 1:
                *p=(uint8_t)ins_man->get_bss_init_value();
                p+=1;
                break;
            case 2:
                *((uint16_t*)p)=(uint16_t)ins_man->get_bss_init_value();
                p+=2;
                break;
            case 4:
                *((uint32_t*)p)=(uint32_t)ins_man->get_bss_init_value();
                p+=4;
                break;
            case 8:
                *((uint64_t*)p)=(uint64_t)ins_man->get_bss_init_value();
                p+=8;
                break;
            default:
                assert(0);
            }
        }

        if ( inst->get_instruction_type() == StreamVmInstruction::itFLOW_CLIENT ){

            StreamVmInstructionFlowClient * ins_man=(StreamVmInstructionFlowClient *)inst;
            if (ins_man->m_client_min>0) {
                *((uint32_t*)p)=(uint32_t)(ins_man->m_client_min-1);
            }else{
                *((uint32_t*)p)=(uint32_t)ins_man->m_client_min;
            }
            p+=4;

            if (ins_man->is_unlimited_flows() ) {
                *((uint16_t*)p)=StreamDPOpClientsUnLimit::CLIENT_UNLIMITED_MIN_PORT;
            }else{
                *((uint16_t*)p)=(uint16_t)ins_man->m_port_min;
            }
            p+=2;

            *((uint32_t*)p)=0;
            p+=4;
        }

    }
}

/**
 * set the VM split instruction 
 * instr is a pointer to an instruction inside 
 * the VM program 
 * 
 */
void
StreamVm::set_split_instruction(StreamVmInstructionVar *instr) {
    m_split_instr = instr;
}

/**
 * copy instructions from this VM to 'other'
 * 
 * @author imarom (22-Dec-15)
 * 
 * @param other 
 */
void 
StreamVm::copy_instructions(StreamVm &other) const {
    /* clear previous if any exists */
    for (auto instr : other.m_inst_list) {
        delete instr;
    }

    other.m_inst_list.clear();

    for (auto instr : m_inst_list) {
        StreamVmInstruction *new_instr = instr->clone();
        other.m_inst_list.push_back(new_instr);

        /* for the split instruction - find the right one */
        if (instr == m_split_instr) {
            /* dynamic cast must succeed here */
            other.m_split_instr = dynamic_cast<StreamVmInstructionVar *>(new_instr);
            assert(other.m_split_instr);
        }
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
    StreamDPOpIpv4Fix   *lpIpv4Fix;
    StreamDPOpPktWr8     *lpw8;
    StreamDPOpPktWr16    *lpw16;
    StreamDPOpPktWr32    *lpw32;
    StreamDPOpPktWr64    *lpw64;
    StreamDPOpClientsLimit   *lp_client;
    StreamDPOpClientsUnLimit  *lp_client_unlimited;


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


        default:
            assert(0);
        }
    };
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


void StreamDPOpIpv4Fix::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, offset: %lu \n",  opt.c_str(),(ulong)m_op,(ulong)m_offset);
}


void StreamDPOpClientsLimit::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flow_offset: %lu (%x-%x) (%x-%x) flow_limit :%lu flags:%x \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,m_min_ip,m_max_ip,m_min_port,m_max_port,(ulong)m_limit_flows,m_flags);
}

void StreamDPOpClientsUnLimit::dump(FILE *fd,std::string opt){
    fprintf(fd," %10s  op:%lu, flow_offset: %lu (%x-%x) flags:%x \n",  opt.c_str(),(ulong)m_op,(ulong)m_flow_offset,m_min_ip,m_max_ip,m_flags);
}


