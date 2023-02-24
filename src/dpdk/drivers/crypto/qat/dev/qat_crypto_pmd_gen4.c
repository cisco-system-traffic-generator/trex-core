/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen4[] = {
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA256_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_XCBC_MAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 12, 12, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_CMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_DOCSISBPI,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(digest_size),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA256,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AEAD_CAP(AES_GCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AEAD_CAP(AES_CCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 2),
		CAP_RNG(aad_size, 0, 224, 1), CAP_RNG(iv_size, 7, 13, 1)),
	QAT_SYM_AUTH_CAP(AES_GMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AEAD_CAP(CHACHA20_POLY1305,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 32, 32, 0),
		CAP_RNG(digest_size, 16, 16, 0),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static struct qat_capabilities_info
qat_sym_crypto_cap_get_gen4(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_capabilities_info capa_info;
	capa_info.data = qat_sym_crypto_caps_gen4;
	capa_info.size = sizeof(qat_sym_crypto_caps_gen4);
	return capa_info;
}

static __rte_always_inline void
enqueue_one_aead_job_gen4(struct qat_sym_session *ctx,
	struct icp_qat_fw_la_bulk_req *req,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	union rte_crypto_sym_ofs ofs, uint32_t data_len)
{
	if (ctx->is_single_pass && ctx->is_ucs) {
		struct icp_qat_fw_la_cipher_20_req_params *cipher_param_20 =
			(void *)&req->serv_specif_rqpars;
		struct icp_qat_fw_la_cipher_req_params *cipher_param =
			(void *)&req->serv_specif_rqpars;

		/* QAT GEN4 uses single pass to treat AEAD as cipher
		 * operation
		 */
		qat_set_cipher_iv(cipher_param, iv, ctx->cipher_iv.length,
				req);
		cipher_param->cipher_offset = ofs.ofs.cipher.head;
		cipher_param->cipher_length = data_len -
				ofs.ofs.cipher.head - ofs.ofs.cipher.tail;

		cipher_param_20->spc_aad_addr = aad->iova;
		cipher_param_20->spc_auth_res_addr = digest->iova;

		return;
	}

	enqueue_one_aead_job_gen1(ctx, req, iv, digest, aad, ofs, data_len);
}

static int
qat_sym_build_op_aead_gen4(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie)
{
	register struct icp_qat_fw_la_bulk_req *qat_req;
	struct rte_crypto_op *op = in_op;
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_sgl in_sgl, out_sgl;
	struct rte_crypto_vec in_vec[QAT_SYM_SGL_MAX_NUMBER],
			out_vec[QAT_SYM_SGL_MAX_NUMBER];
	struct rte_crypto_va_iova_ptr cipher_iv;
	struct rte_crypto_va_iova_ptr aad;
	struct rte_crypto_va_iova_ptr digest;
	union rte_crypto_sym_ofs ofs;
	int32_t total_len;

	in_sgl.vec = in_vec;
	out_sgl.vec = out_vec;

	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)qat_req, (const uint8_t *)&(ctx->fw_req));

	ofs.raw = qat_sym_convert_op_to_vec_aead(op, ctx, &in_sgl, &out_sgl,
			&cipher_iv, &aad, &digest);
	if (unlikely(ofs.raw == UINT64_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	total_len = qat_sym_build_req_set_data(qat_req, in_op, cookie,
			in_sgl.vec, in_sgl.num, out_sgl.vec, out_sgl.num);
	if (unlikely(total_len < 0)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}

	enqueue_one_aead_job_gen4(ctx, qat_req, &cipher_iv, &digest, &aad, ofs,
		total_len);

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	qat_sym_debug_log_dump(qat_req, ctx, in_sgl.vec, in_sgl.num, &cipher_iv,
			NULL, &aad, &digest);
#endif

	return 0;
}

static int
qat_sym_crypto_set_session_gen4(void *cdev, void *session)
{
	struct qat_sym_session *ctx = session;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int ret;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	ret = qat_sym_crypto_set_session_gen1(cdev, session);
	/* special single pass build request for GEN4 */
	if (ctx->is_single_pass && ctx->is_ucs)
		ctx->build_request[proc_type] = qat_sym_build_op_aead_gen4;

	if (ret == -ENOTSUP) {
		/* GEN1 returning -ENOTSUP as it cannot handle some mixed algo,
		 * this is addressed by GEN4
		 */
		if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_3G_128_EIA3 &&
				ctx->qat_cipher_alg !=
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx,
				1 << ICP_QAT_FW_AUTH_HDR_FLAG_ZUC_EIA3_BITPOS);
		} else if (ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2 &&
				ctx->qat_cipher_alg !=
				ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx,
				1 << ICP_QAT_FW_AUTH_HDR_FLAG_SNOW3G_UIA2_BITPOS);
		} else if ((ctx->aes_cmac ||
				ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) &&
				(ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
				ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3)) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx, 0);
		}

		ret = 0;
	}

	return ret;
}

static int
qat_sym_dp_enqueue_single_aead_gen4(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad,
	void *user_data)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_op_cookie *cookie;
	struct qat_sym_session *ctx = dp_ctx->session;
	struct icp_qat_fw_la_bulk_req *req;

	int32_t data_len;
	uint32_t tail = dp_ctx->tail;

	req = (struct icp_qat_fw_la_bulk_req *)(
		(uint8_t *)tx_queue->base_addr + tail);
	cookie = qp->op_cookies[tail >> tx_queue->trailz];
	tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;
	rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));
	rte_prefetch0((uint8_t *)tx_queue->base_addr + tail);
	data_len = qat_sym_build_req_set_data(req, user_data, cookie,
			data, n_data_vecs, NULL, 0);
	if (unlikely(data_len < 0))
		return -1;

	enqueue_one_aead_job_gen4(ctx, req, iv, digest, aad, ofs,
		(uint32_t)data_len);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue++;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	qat_sym_debug_log_dump(req, ctx, data, n_data_vecs, iv,
			NULL, aad, digest);
#endif
	return 0;
}

static uint32_t
qat_sym_dp_enqueue_aead_jobs_gen4(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status)
{
	struct qat_qp *qp = qp_data;
	struct qat_sym_dp_ctx *dp_ctx = (void *)drv_ctx;
	struct qat_queue *tx_queue = &qp->tx_q;
	struct qat_sym_session *ctx = dp_ctx->session;
	uint32_t i, n;
	uint32_t tail;
	struct icp_qat_fw_la_bulk_req *req;
	int32_t data_len;

	n = QAT_SYM_DP_GET_MAX_ENQ(qp, dp_ctx->cached_enqueue, vec->num);
	if (unlikely(n == 0)) {
		qat_sym_dp_fill_vec_status(vec->status, -1, vec->num);
		*status = 0;
		return 0;
	}

	tail = dp_ctx->tail;

	for (i = 0; i < n; i++) {
		struct qat_sym_op_cookie *cookie =
			qp->op_cookies[tail >> tx_queue->trailz];

		req  = (struct icp_qat_fw_la_bulk_req *)(
			(uint8_t *)tx_queue->base_addr + tail);
		rte_mov128((uint8_t *)req, (const uint8_t *)&(ctx->fw_req));

		if (vec->dest_sgl) {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec, vec->src_sgl[i].num,
				vec->dest_sgl[i].vec, vec->dest_sgl[i].num);
		} else {
			data_len = qat_sym_build_req_set_data(req,
				user_data[i], cookie,
				vec->src_sgl[i].vec,
				vec->src_sgl[i].num, NULL, 0);
		}

		if (unlikely(data_len < 0))
			break;

		enqueue_one_aead_job_gen4(ctx, req, &vec->iv[i],
				&vec->digest[i], &vec->aad[i], ofs,
				(uint32_t)data_len);

		tail = (tail + tx_queue->msg_size) & tx_queue->modulo_mask;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		qat_sym_debug_log_dump(req, ctx, vec->src_sgl[i].vec,
				vec->src_sgl[i].num, &vec->iv[i], NULL,
				&vec->aad[i], &vec->digest[i]);
#endif
	}

	if (unlikely(i < n))
		qat_sym_dp_fill_vec_status(vec->status + i, -1, n - i);

	dp_ctx->tail = tail;
	dp_ctx->cached_enqueue += i;
	*status = 0;
	return i;
}

static int
qat_sym_configure_raw_dp_ctx_gen4(void *_raw_dp_ctx, void *_ctx)
{
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx = _raw_dp_ctx;
	struct qat_sym_session *ctx = _ctx;
	int ret;

	ret = qat_sym_configure_raw_dp_ctx_gen1(_raw_dp_ctx, _ctx);
	if (ret < 0)
		return ret;

	if (ctx->is_single_pass && ctx->is_ucs) {
		raw_dp_ctx->enqueue_burst = qat_sym_dp_enqueue_aead_jobs_gen4;
		raw_dp_ctx->enqueue = qat_sym_dp_enqueue_single_aead_gen4;
	}

	return 0;
}

RTE_INIT(qat_sym_crypto_gen4_init)
{
	qat_sym_gen_dev_ops[QAT_GEN4].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN4].get_capabilities =
			qat_sym_crypto_cap_get_gen4;
	qat_sym_gen_dev_ops[QAT_GEN4].set_session =
			qat_sym_crypto_set_session_gen4;
	qat_sym_gen_dev_ops[QAT_GEN4].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen4;
	qat_sym_gen_dev_ops[QAT_GEN4].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
#ifdef RTE_LIB_SECURITY
	qat_sym_gen_dev_ops[QAT_GEN4].create_security_ctx =
			qat_sym_create_security_gen1;
#endif
}

RTE_INIT(qat_asym_crypto_gen4_init)
{
	qat_asym_gen_dev_ops[QAT_GEN4].cryptodev_ops = NULL;
	qat_asym_gen_dev_ops[QAT_GEN4].get_capabilities = NULL;
	qat_asym_gen_dev_ops[QAT_GEN4].get_feature_flags = NULL;
	qat_asym_gen_dev_ops[QAT_GEN4].set_session = NULL;
}
