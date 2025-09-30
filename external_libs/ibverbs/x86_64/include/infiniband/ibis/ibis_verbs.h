/*
 * Copyright (c) 2022-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBIS_VERBS_H_
#define IBIS_VERBS_H_

#include "ibis_verbs_supported.h"

#include <map>
#include <queue>

#if VERBS_SUPPORTED == 1
    #include <infiniband/sa.h>

    class VerbsPort
    {
        public:
            const char *dev_name;
            int port_num;
            uint64_t queue_size;
            struct ibv_context *context;
            struct ibv_pd *pd;
            struct ibv_cq *send_cq;
            struct ibv_cq *recv_cq;
            struct ibv_comp_channel *recv_comp_ch;
            struct ibv_qp *qp;
            uint8_t *mad_buff;
            struct ibv_mr *mr;
            std::map<uint32_t, struct ibv_ah *> ah_map;
            std::queue<uint64_t> send_queue;

            VerbsPort()
                : dev_name(NULL), port_num(0), queue_size(0), context(NULL),
                  pd(NULL), send_cq(NULL), recv_cq(NULL), recv_comp_ch(NULL),
                  qp(NULL), mad_buff(NULL), mr(NULL)
            {}
    };
#else
    class VerbsPort {};
#endif

#endif	/* IBIS_VERBS_H_ */

