/*
 * Copyright (c) 2020-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef _IBDIAGNET_FORWARD_CLBCK_H_
#define _IBDIAGNET_FORWARD_CLBCK_H_


#include <infiniband/ibis/ibis_clbck.h>
//the clbck_data_t is define in the above h file

template<typename _Type, void (_Type::*_Method)(const clbck_data_t &, int , void*)>
    void forwardClbck(const clbck_data_t &clbck_data,
                      int rec_status,
                      void *p_attribute_data)
{
    (reinterpret_cast<_Type*>(clbck_data.m_p_obj)->*_Method)
        (clbck_data, rec_status, p_attribute_data);
}


template<typename _Type, int (_Type::*_Method)(const clbck_data_t &, int , void*)>
    void forwardClbck(const clbck_data_t &clbck_data,
                      int rec_status,
                      void *p_attribute_data)
{
    (reinterpret_cast<_Type*>(clbck_data.m_p_obj)->*_Method)
        (clbck_data, rec_status, p_attribute_data);
}

#endif

