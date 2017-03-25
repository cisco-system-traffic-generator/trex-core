/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
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
 */

#ifndef INFINIBAND_DRIVER_EXP_H
#define INFINIBAND_DRIVER_EXP_H

#include <infiniband/verbs_exp.h>
#include <infiniband/driver.h>
#include <infiniband/kern-abi_exp.h>

int ibv_exp_cmd_query_device(struct ibv_context *context,
			     struct ibv_exp_device_attr *device_attr,
			     uint64_t *raw_fw_ver,
			     struct ibv_exp_query_device *cmd, size_t cmd_size);
int ibv_exp_cmd_create_qp(struct ibv_context *context,
			  struct verbs_qp *qp, int vqp_sz,
			  struct ibv_exp_qp_init_attr *attr_exp,
			  void *cmd_buf, size_t lib_cmd_size, size_t drv_cmd_size,
			  void *resp_buf, size_t lib_resp_size, size_t drv_resp_size,
			  int force_exp);
int ibv_exp_cmd_create_dct(struct ibv_context *context,
			   struct ibv_exp_dct *dct,
			   struct ibv_exp_dct_init_attr *attr,
			   struct ibv_exp_create_dct *cmd,
			   size_t lib_cmd_sz, size_t drv_cmd_sz,
			   struct ibv_exp_create_dct_resp *resp,
			   size_t lib_resp_sz, size_t drv_resp_sz);
int ibv_exp_cmd_destroy_dct(struct ibv_context *context,
			    struct ibv_exp_dct *dct,
			    struct ibv_exp_destroy_dct *cmd,
			    size_t lib_cmd_sz, size_t drv_cmd_sz,
			    struct ibv_exp_destroy_dct_resp *resp,
			    size_t lib_resp_sz, size_t drv_resp_sz);
int ibv_exp_cmd_query_dct(struct ibv_context *context,
			  struct ibv_exp_query_dct *cmd,
			  size_t lib_cmd_sz, size_t drv_cmd_sz,
			  struct ibv_exp_query_dct_resp *resp,
			  size_t lib_resp_sz, size_t drv_resp_sz,
			  struct ibv_exp_dct_attr *attr);
int ibv_exp_cmd_arm_dct(struct ibv_context *context,
			struct ibv_exp_arm_attr *attr,
			struct ibv_exp_arm_dct *cmd,
			size_t lib_cmd_sz, size_t drv_cmd_sz,
			struct ibv_exp_arm_dct_resp *resp,
			size_t lib_resp_sz, size_t drv_resp_sz);
int ibv_exp_cmd_modify_cq(struct ibv_cq *cq,
			  struct ibv_exp_cq_attr *attr,
			  int attr_mask,
			  struct ibv_exp_modify_cq *cmd, size_t cmd_size);
int ibv_exp_cmd_create_cq(struct ibv_context *context, int cqe,
			  struct ibv_comp_channel *channel,
			  int comp_vector, struct ibv_cq *cq,
			  struct ibv_exp_create_cq *cmd, size_t lib_cmd_sz, size_t drv_cmd_sz,
			  struct ibv_create_cq_resp *resp, size_t lib_resp_sz, size_t drv_resp_sz,
			  struct ibv_exp_cq_init_attr *attr);
int ibv_exp_cmd_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
			  uint64_t attr_mask, struct ibv_exp_modify_qp *cmd,
			  size_t cmd_size);
int ibv_exp_cmd_create_mr(struct ibv_exp_create_mr_in *in, struct ibv_mr *mr,
			  struct ibv_exp_create_mr *cmd, size_t lib_cmd_sz, size_t drv_cmd_sz,
			  struct ibv_exp_create_mr_resp *resp, size_t lib_resp_sz, size_t drv_resp_sz);
int ibv_exp_cmd_query_mkey(struct ibv_context *context,
			   struct ibv_mr *mr,
			   struct ibv_exp_mkey_attr *mkey_attr,
			   struct ibv_exp_query_mkey *cmd, size_t lib_cmd_sz, size_t drv_cmd_sz,
			   struct ibv_exp_query_mkey_resp *resp, size_t lib_resp_sz, size_t drv_resp_sz);
int ibv_cmd_exp_reg_mr(const struct ibv_exp_reg_mr_in *mr_init_attr,
		       uint64_t hca_va, struct ibv_mr *mr,
		       struct ibv_exp_reg_mr *cmd,
		       size_t cmd_size,
		       struct ibv_exp_reg_mr_resp *resp,
		       size_t resp_size);
int ibv_cmd_exp_prefetch_mr(struct ibv_mr *mr,
			    struct ibv_exp_prefetch_attr *attr);
int ibv_exp_cmd_create_wq(struct ibv_context *context,
			  struct ibv_exp_wq_init_attr *wq_init_attr,
			  struct ibv_exp_wq *wq,
			  struct ibv_exp_create_wq *cmd,
			  size_t cmd_core_size,
			  size_t cmd_size,
			  struct ibv_exp_create_wq_resp *resp,
			  size_t resp_core_size,
			  size_t resp_size);
int ibv_exp_cmd_destroy_wq(struct ibv_exp_wq *wq);
int ibv_exp_cmd_modify_wq(struct ibv_exp_wq *wq, struct ibv_exp_wq_attr *attr,
			  struct ib_exp_modify_wq *cmd, size_t cmd_size);
int ibv_exp_cmd_create_rwq_ind_table(struct ibv_context *context,
				     struct ibv_exp_rwq_ind_table_init_attr *init_attr,
				     struct ibv_exp_rwq_ind_table *rwq_ind_table,
				     struct ibv_exp_create_rwq_ind_table *cmd,
				     size_t cmd_core_size,
				     size_t cmd_size,
				     struct ibv_exp_create_rwq_ind_table_resp *resp,
				     size_t resp_core_size,
				     size_t resp_size);
int ibv_exp_cmd_destroy_rwq_ind_table(struct ibv_exp_rwq_ind_table *rwq_ind_table);
/*
 * ibv_exp_cmd_getenv
 *
 * @context: context to the device
 * @name: the name of the variable to read
 * @value: pointer where the value of the variable will be written
 * @n: number of bytes pointed to by val
 *
 * return: 0 success
 *         < 0 varaible was not found
	   > 0 variable found but not enuogh space provided. requied space is the value returned.
 */
int ibv_exp_cmd_getenv(struct ibv_context *context,
		       const char *name, char *value, size_t n);


#endif /* INFINIBAND_DRIVER_EXP_H */
