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
#ifndef __TREX_STREAM_VM_API_H__
#define __TREX_STREAM_VM_API_H__

#include <string>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <assert.h>
#include <common/Network/Packet/IPHeader.h>
#include "pal_utl.h"
#include "mbuf.h"




class StreamVm;


/* in memory program */

struct StreamDPOpFlowVar8 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint8_t m_min_val;
    uint8_t m_max_val;
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint8_t * p=(flow_var+m_flow_offset);
        *p=*p+1;
        if (*p>m_max_val) {
            *p=m_min_val;
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint8_t * p=(flow_var+m_flow_offset);
        *p=*p-1;
        if (*p<m_min_val) {
            *p=m_max_val;
        }
    }

    inline void run_rand(uint8_t * flow_var) {
        uint8_t * p=(flow_var+m_flow_offset);
        *p= m_min_val + (rand() % (int)(m_max_val - m_min_val + 1));
    }


} __attribute__((packed)) ;

struct StreamDPOpFlowVar16 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint16_t m_min_val;
    uint16_t m_max_val;
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint16_t * p=(uint16_t *)(flow_var+m_flow_offset);
        *p=*p+1;
        if (*p>m_max_val) {
            *p=m_min_val;
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint16_t * p=(uint16_t *)(flow_var+m_flow_offset);
        *p=*p-1;
        if (*p<m_min_val) {
            *p=m_max_val;
        }
    }

    inline void run_rand(uint8_t * flow_var) {
        uint16_t * p=(uint16_t *)(flow_var+m_flow_offset);
        *p= m_min_val + (rand() % (int)(m_max_val - m_min_val + 1));
    }



} __attribute__((packed)) ;

struct StreamDPOpFlowVar32 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint32_t m_min_val;
    uint32_t m_max_val;
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint32_t * p=(uint32_t *)(flow_var+m_flow_offset);
        *p=*p+1;
        if (*p>m_max_val) {
            *p=m_min_val;
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint32_t * p=(uint32_t *)(flow_var+m_flow_offset);
        *p=*p-1;
        if (*p<m_min_val) {
            *p=m_max_val;
        }
    }

    inline void run_rand(uint8_t * flow_var) {
        uint32_t * p=(uint32_t *)(flow_var+m_flow_offset);
        *p= m_min_val + (rand() % (int)(m_max_val - m_min_val + 1));
    }

} __attribute__((packed)) ;

struct StreamDPOpFlowVar64 {
    uint8_t m_op;
    uint8_t m_flow_offset;
    uint64_t m_min_val;
    uint64_t m_max_val;
public:
    void dump(FILE *fd,std::string opt);

    inline void run_inc(uint8_t * flow_var) {
        uint64_t * p=(uint64_t *)(flow_var+m_flow_offset);
        *p=*p+1;
        if (*p>m_max_val) {
            *p=m_min_val;
        }
    }

    inline void run_dec(uint8_t * flow_var) {
        uint64_t * p=(uint64_t *)(flow_var+m_flow_offset);
        *p=*p-1;
        if (*p<m_min_val) {
            *p=m_max_val;
        }
    }

    inline void run_rand(uint8_t * flow_var) {
        uint64_t * p=(uint64_t *)(flow_var+m_flow_offset);
        *p= m_min_val + (rand() % (int)(m_max_val - m_min_val + 1));
    }


} __attribute__((packed)) ;


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

struct StreamDPOpIpv4Fix {
    uint8_t m_op;
    uint16_t  m_offset;
public:
    void dump(FILE *fd,std::string opt);
    void run(uint8_t * pkt_base){
        IPHeader *      ipv4=  (IPHeader *)(pkt_base+m_offset);
        ipv4->updateCheckSum();
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

    uint32_t    m_min_ip;
    uint32_t    m_max_ip;
    uint32_t    m_limit_flows; /* limit the number of flows */

public:
    void dump(FILE *fd,std::string opt);
    inline void run(uint8_t * flow_var_base) {
        StreamDPFlowClient * lp= (StreamDPFlowClient *)(flow_var_base+m_flow_offset);
        lp->cur_ip++;
        if (lp->cur_ip > m_max_ip ) {
            lp->cur_ip= m_min_ip;
            lp->cur_port++;
            if (lp->cur_port > m_max_port) {
                lp->cur_port = m_min_port;
            }
        }

        if (m_limit_flows) {
            lp->cur_flow_id++;
            if ( lp->cur_flow_id > m_limit_flows ){
                /* reset to the first flow */
                lp->cur_flow_id = 1;
                lp->cur_ip      =  m_min_ip;
                lp->cur_port    =  m_min_port;
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


/* datapath instructions */
class StreamDPVmInstructions {
public:
    enum INS_TYPE {
        ditINC8         ,
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

        itPKT_WR8       ,
        itPKT_WR16       ,
        itPKT_WR32       ,
        itPKT_WR64       ,
        itCLIENT_VAR       ,
        itCLIENT_VAR_UNLIMIT      
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
    inline void run(uint32_t program_size,
                    uint8_t * program,  /* program */
                    uint8_t * flow_var, /* flow var */
                    uint8_t * pkt);      /* pkt */

};


inline void StreamDPVmInstructionsRunner::run(uint32_t program_size,
                                              uint8_t * program,  /* program */
                                              uint8_t * flow_var, /* flow var */
                                              uint8_t * pkt){


    uint8_t * p=program;
    uint8_t * p_end=p+program_size;


    union  ua_ {
        StreamDPOpFlowVar8  *lpv8;
        StreamDPOpFlowVar16 *lpv16;
        StreamDPOpFlowVar32 *lpv32;
        StreamDPOpFlowVar64 *lpv64;
        StreamDPOpIpv4Fix   *lpIpv4Fix;
        StreamDPOpPktWr8     *lpw8;
        StreamDPOpPktWr16    *lpw16;
        StreamDPOpPktWr32    *lpw32;
        StreamDPOpPktWr64    *lpw64;
        StreamDPOpClientsLimit      *lpcl;
        StreamDPOpClientsUnLimit    *lpclu;
    } ua ;

    while ( p < p_end) {
        uint8_t op_code=*p;
        switch (op_code) {

        case  StreamDPVmInstructions::itCLIENT_VAR :      
            ua.lpcl =(StreamDPOpClientsLimit *)p;
            ua.lpcl->run(flow_var);
            p+=sizeof(StreamDPOpClientsLimit);
            break;

        case  StreamDPVmInstructions::itCLIENT_VAR_UNLIMIT :      
            ua.lpclu =(StreamDPOpClientsUnLimit *)p;
            ua.lpclu->run(flow_var);
            p+=sizeof(StreamDPOpClientsUnLimit);
            break;

        case   StreamDPVmInstructions::ditINC8  :
            ua.lpv8 =(StreamDPOpFlowVar8 *)p;
            ua.lpv8->run_inc(flow_var);
            p+=sizeof(StreamDPOpFlowVar8);
            break;

        case  StreamDPVmInstructions::ditINC16  :
            ua.lpv16 =(StreamDPOpFlowVar16 *)p;
            ua.lpv16->run_inc(flow_var);
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  StreamDPVmInstructions::ditINC32 :
            ua.lpv32 =(StreamDPOpFlowVar32 *)p;
            ua.lpv32->run_inc(flow_var);
            p+=sizeof(StreamDPOpFlowVar32);
             break;
        case  StreamDPVmInstructions::ditINC64 :
            ua.lpv64 =(StreamDPOpFlowVar64 *)p;
            ua.lpv64->run_inc(flow_var);
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  StreamDPVmInstructions::ditDEC8 :
            ua.lpv8 =(StreamDPOpFlowVar8 *)p;
            ua.lpv8->run_dec(flow_var);
            p+=sizeof(StreamDPOpFlowVar8);
            break;
        case  StreamDPVmInstructions::ditDEC16 :
            ua.lpv16 =(StreamDPOpFlowVar16 *)p;
            ua.lpv16->run_dec(flow_var);
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  StreamDPVmInstructions::ditDEC32 :
            ua.lpv32 =(StreamDPOpFlowVar32 *)p;
            ua.lpv32->run_dec(flow_var);
            p+=sizeof(StreamDPOpFlowVar32);
            break;
        case  StreamDPVmInstructions::ditDEC64 :
            ua.lpv64 =(StreamDPOpFlowVar64 *)p;
            ua.lpv64->run_dec(flow_var);
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  StreamDPVmInstructions::ditRANDOM8 :
            ua.lpv8 =(StreamDPOpFlowVar8 *)p;
            ua.lpv8->run_rand(flow_var);
            p+=sizeof(StreamDPOpFlowVar8);
            break;
        case  StreamDPVmInstructions::ditRANDOM16 :
            ua.lpv16 =(StreamDPOpFlowVar16 *)p;
            ua.lpv16->run_rand(flow_var);
            p+=sizeof(StreamDPOpFlowVar16);
            break;
        case  StreamDPVmInstructions::ditRANDOM32 :
            ua.lpv32 =(StreamDPOpFlowVar32 *)p;
            ua.lpv32->run_rand(flow_var);
            p+=sizeof(StreamDPOpFlowVar32);
            break;
        case  StreamDPVmInstructions::ditRANDOM64 :
            ua.lpv64 =(StreamDPOpFlowVar64 *)p;
            ua.lpv64->run_rand(flow_var);
            p+=sizeof(StreamDPOpFlowVar64);
            break;

        case  StreamDPVmInstructions::ditFIX_IPV4_CS :
            ua.lpIpv4Fix =(StreamDPOpIpv4Fix *)p;
            ua.lpIpv4Fix->run(pkt);
            p+=sizeof(StreamDPOpIpv4Fix);
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
        default:
            assert(0);
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
        itFLOW_CLIENT  = 7 

    };

    typedef uint8_t instruction_type_t ;

    virtual instruction_type_t get_instruction_type() const = 0;

    virtual ~StreamVmInstruction();

    virtual void Dump(FILE *fd)=0;
    
    virtual StreamVmInstruction * clone() = 0;

    /**
     * by default an instruction is not a splitable field 
     */
    virtual bool is_splitable() const {
        return false;
    }

private:
    static const std::string m_name;
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
 * flow manipulation instruction
 * 
 * @author imarom (07-Sep-15)
 */
class StreamVmInstructionFlowMan : public StreamVmInstruction {

public:

    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFLOW_MAN);
    }

    uint64_t get_range() const {
        return (m_max_value - m_min_value + 1);
    }

    virtual bool is_splitable() const {
        return true;
    }

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
                               uint64_t max_value) : 
                                                     m_var_name(var_name),
                                                     m_size_bytes(size),
                                                     m_op(op),
                                                     m_init_value(init_value),
                                                     m_min_value(min_value),
                                                     m_max_value(max_value) {

    }

    virtual void Dump(FILE *fd);

    void sanity_check(uint32_t ins_id,StreamVm *lp);

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFlowMan(m_var_name,
                                              m_size_bytes,
                                              m_op,
                                              m_init_value,
                                              m_min_value,
                                              m_max_value);
    }

private:
    void sanity_check_valid_range(uint32_t ins_id,StreamVm *lp);
    void sanity_check_valid_size(uint32_t ins_id,StreamVm *lp);
    void sanity_check_valid_opt(uint32_t ins_id,StreamVm *lp);

public:


    /* flow var name */
    std::string   m_var_name;

    /* flow var size */
    uint8_t       m_size_bytes;

    /* type of op */
    flow_var_op_e m_op;

    /* range */
    uint64_t m_init_value;
    uint64_t m_min_value;
    uint64_t m_max_value;

};


/**
 * flow client instruction - save state for client range and port range 
 * 
 * @author hhaim
 */
class StreamVmInstructionFlowClient : public StreamVmInstruction {

public:
    enum client_flags_e {
        CLIENT_F_UNLIMITED_FLOWS=1, /* unlimited number of flow, don't care about ports  */
    };


    virtual instruction_type_t get_instruction_type() const {
        return ( StreamVmInstruction::itFLOW_CLIENT);
    }

    virtual bool is_splitable() const {
        return true;
    }

    StreamVmInstructionFlowClient(const std::string &var_name,
                               uint32_t client_min_value,
                               uint32_t client_max_value,
                               uint16_t port_min,
                               uint16_t port_max,
                               uint32_t limit_num_flows, /* zero means don't limit */
                               uint16_t flags
                               ) { 
        m_var_name   = var_name;
        m_client_min = client_min_value;
        m_client_max = client_max_value;

        m_port_min   = port_min;
        m_port_max   = port_max;

        m_limit_num_flows = limit_num_flows;
        m_flags = flags;
    }

    virtual void Dump(FILE *fd);

    static uint8_t get_flow_var_size(){
        return  (4+2+4);
    }

    bool is_unlimited_flows(){
        return ( (m_flags &   StreamVmInstructionFlowClient::CLIENT_F_UNLIMITED_FLOWS ) == 
                  StreamVmInstructionFlowClient::CLIENT_F_UNLIMITED_FLOWS );
    }

    virtual StreamVmInstruction * clone() {
        return new StreamVmInstructionFlowClient(m_var_name,
                                                 m_client_min,
                                                 m_client_max,
                                                 m_port_min,
                                                 m_port_max,
                                                 m_limit_num_flows,
                                                 m_flags);
    }

public:


    /* flow var name */
    std::string   m_var_name;

    uint32_t m_client_min;  // min ip 
    uint32_t m_client_max;  // max ip 
    uint16_t m_port_min;  // start port 
    uint16_t m_port_max;  // start port 
    uint32_t m_limit_num_flows;   // number of flows
    uint16_t m_flags;
};



class VmFlowVarRec {
public:
    uint32_t    m_offset;
    union {
        StreamVmInstructionFlowMan * m_ins_flowv;
        StreamVmInstructionFlowClient * m_ins_flow_client;
    } m_ins;
    uint8_t    m_size_bytes;
};




/**
 * write flow var to packet
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
    }

    StreamVmDp( uint8_t * bss,
                uint16_t bss_size,
                uint8_t * prog,
                uint16_t prog_size,
                uint16_t max_pkt_offset,
                uint16_t prefix_size
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
                                         m_prefix_size
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

private:
    uint8_t *  m_bss_ptr; /* pointer to the data section */
    uint8_t *  m_program_ptr; /* pointer to the program */
    uint16_t   m_bss_size;
    uint16_t   m_program_size; /* program size*/
    uint16_t   m_max_pkt_offset_change;
    uint16_t   m_prefix_size;

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
        m_split_instr=NULL;
        m_is_compiled = false;
    }


    uint16_t get_packet_size() const {
        return ( m_pkt_size);
    }


    void set_split_instruction(StreamVmInstruction *instr);

    StreamVmInstruction * get_split_instruction() {
        return m_split_instr;
    }

    StreamVmDp * generate_dp_object(){

        if (!m_is_compiled) {
            return NULL;
        }

        StreamVmDp * lp= new StreamVmDp(get_bss_ptr(),
                                        get_bss_size(),
                                        get_dp_instruction_buffer()->get_program(),
                                        get_dp_instruction_buffer()->get_program_size(),
                                        get_max_packet_update_offset(),
                                        get_prefix_size()
                                        );
        assert(lp);
        return (lp);
        
    }

    /**
     * clone VM instructions
     * 
     */
    void copy_instructions(StreamVm &other) const;

    
    bool is_vm_empty() {
        return (m_inst_list.size() == 0);
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

    bool is_compiled() {
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
    bool                               m_is_compiled;
    uint16_t                           m_prefix_size;
    uint16_t                           m_pkt_size;
    uint16_t                           m_cur_var_offset;
    uint16_t                           m_max_field_update; /* the location of the last byte that is going to be changed in the packet */ 

    std::vector<StreamVmInstruction *> m_inst_list;
    std::unordered_map<std::string, VmFlowVarRec> m_flow_var_offset;
    uint8_t *                          m_bss;

    StreamDPVmInstructions             m_instructions;
    
    StreamVmInstruction               *m_split_instr;
    
};

#endif /* __TREX_STREAM_VM_API_H__ */
