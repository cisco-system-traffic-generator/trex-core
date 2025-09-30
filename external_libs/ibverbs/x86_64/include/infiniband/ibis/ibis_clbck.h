/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef IBIS_CLBCK_H_
#define IBIS_CLBCK_H_

//The definitions in this file can't be changed without changing
//ibdiagnet plugin's interface version
typedef void (*pack_data_func_t)(void *data_to_pack,
                                u_int8_t *packed_buffer);
typedef void (*unpack_data_func_t)(void *data_to_unpack,
                                    u_int8_t *unpacked_buffer);
typedef void (*dump_data_func_t)(void *data_to_dump,
                                    FILE * out_port);
typedef struct func_set {
    pack_data_func_t      pack;
    unpack_data_func_t    unpack;
    dump_data_func_t      dump;

    template<typename T>
        func_set(
            void (*pack_func)(const T *, u_int8_t *),
            void (*unpack_func)(T *, const u_int8_t *),
            void (*dump_func)(const T *, FILE*) )
            : pack(reinterpret_cast<pack_data_func_t>(pack_func)),
              unpack(reinterpret_cast<unpack_data_func_t>(unpack_func)),
              dump(reinterpret_cast<dump_data_func_t>(dump_func))
        {
        }

} func_set_t;

typedef struct data_func_set : func_set_t  {
    mutable void* data;

    template<typename T>
        data_func_set(
            T * data_ptr,
            void (*pack_func)(const T *, u_int8_t *),
            void (*unpack_func)(T *, const u_int8_t *),
            void (*dump_func)(const T *, FILE*) )
            : func_set_t(pack_func, unpack_func, dump_func), data(data_ptr)
        {
        }

} data_func_set_t;

#define IBIS_FUNC_LST(type) type ## _pack, type ## _unpack, type ## _dump

typedef struct clbck_data clbck_data_t;
typedef void (*handle_data_func_t)(const clbck_data_t &clbck_data,
                                   int rec_status,
                                   void *p_attribute_data);

class IBNode;
class IBPort;
typedef struct clbck_data {
    handle_data_func_t m_handle_data_func;
    void*              m_p_obj; //obj containing handle func
    union {
        struct{
            void*       m_data1;
            void*       m_data2;
            void*       m_data3;
            void*       m_data4;
            void*       m_p_progress_bar;
        };
        struct {
            IBNode*     m_p_node;
            IBPort*     m_p_port;
            void*       m_ptr;
            struct {
                struct {
                    uint64_t u8a:8;
                    uint64_t u8b:8;
                    uint64_t u16:16;
                    uint64_t u32:32;
                };
                uint64_t u64;
            } m_index;
        };
    };

    // Value of AdditionalStatus field in received MAD Common Header
    // This value is filled by IBIS and only in case of receiving a response
    uint16_t additional_status;

    struct {
        struct timespec m_send_timestamp;
        struct timespec m_recv_timestamp;
        bool            m_is_smp;
    } m_stat;

} clbck_data_t;

#endif //IBIS_CLBCK_H_
