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

/**
 * interface for stream VM instruction
 * 
 */
class StreamVmInstruction {
public:

    virtual ~StreamVmInstruction();

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

private:
    uint16_t m_pkt_offset;
};

/**
 * flow manipulation instruction
 * 
 * @author imarom (07-Sep-15)
 */
class StreamVmInstructionFlowMan : public StreamVmInstruction {

public:

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

private:


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
private:

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
 * describes a VM program
 * 
 */
class StreamVm {
public:

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

    /**
     * compile the VM 
     * return true of success, o.w false 
     * 
     */
    bool compile();

    ~StreamVm();

private:
    std::vector<StreamVmInstruction *> m_inst_list;
};

#endif /* __TREX_STREAM_VM_API_H__ */
