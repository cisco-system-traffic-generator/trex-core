#ifndef INFINIBAND_OFA_VERBS_H
#define INFINIBAND_OFA_VERBS_H

struct ibv_srq_init_attr;
struct ibv_cq;
struct ibv_pd;
struct ibv_qp_init_attr;
struct ibv_qp_attr;


#ifdef __GNUC__
#define DEPRECATED  __attribute__((deprecated))
#else
#define DEPRECATED
#endif

/* XRC compatability layer */
#define LEGACY_XRC_SRQ_HANDLE 0xffffffff

struct ibv_xrc_domain {
	struct ibv_context     *context;
	uint32_t		handle;
};

struct ibv_srq_legacy {
	struct ibv_context     *context;
	void		       *srq_context;
	struct ibv_pd	       *pd;
	uint32_t		handle;

	uint32_t		events_completed;

	uint32_t		xrc_srq_num_bin_compat;
	struct ibv_xrc_domain  *xrc_domain_bin_compat;
	struct ibv_cq	       *xrc_cq_bin_compat;

	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	/* Here we hook the new one from OFED 2.0 */
	void 			*ibv_srq;
	/* Below 3 fields are for legacy source compatibility, reside
	  * on same offset as of those fields in struct ibv_srq.
	*/
	uint32_t	    xrc_srq_num;
	struct ibv_xrc_domain  *xrc_domain;
	struct ibv_cq		  *xrc_cq;
};

/**
 * ibv_open_xrc_domain - open an XRC domain
 * Returns a reference to an XRC domain.
 *
 * @context: Device context
 * @fd: descriptor for inode associated with the domain
 *     If fd == -1, no inode is associated with the domain; in this ca= se,
 *     the only legal value for oflag is O_CREAT
 *
 * @oflag: oflag values are constructed by OR-ing flags from the following list
 *
 * O_CREAT
 *     If a domain belonging to device named by context is already associated
 *     with the inode, this flag has no effect, except as noted under O_EXCL
 *     below. Otherwise, a new XRC domain is created and is associated with
 *     inode specified by fd.
 *
 * O_EXCL
 *     If O_EXCL and O_CREAT are set, open will fail if a domain associated with
 *     the inode exists. The check for the existence of the domain and creation
 *     of the domain if it does not exist is atomic with respect to other
 *     processes executing open with fd naming the same inode.
 */
struct ibv_xrc_domain *ibv_open_xrc_domain(struct ibv_context *context,
					   int fd, int oflag) DEPRECATED;

/**
 * ibv_create_xrc_srq - Creates a SRQ associated with the specified protection
 *   domain and xrc domain.
 * @pd: The protection domain associated with the SRQ.
 * @xrc_domain: The XRC domain associated with the SRQ.
 * @xrc_cq: CQ to report completions for XRC packets on.
 *
 * @srq_init_attr: A list of initial attributes required to create the SRQ.
 *
 * srq_attr->max_wr and srq_attr->max_sge are read the determine the
 * requested size of the SRQ, and set to the actual values allocated
 * on return.  If ibv_create_srq() succeeds, then max_wr and max_sge
 * will always be at least as large as the requested values.
 */
struct ibv_srq *ibv_create_xrc_srq(struct ibv_pd *pd,
				   struct ibv_xrc_domain *xrc_domain,
				   struct ibv_cq *xrc_cq,
				   struct ibv_srq_init_attr *srq_init_attr) DEPRECATED;

/**
 * ibv_close_xrc_domain - close an XRC domain
 * If this is the last reference, destroys the domain.
 *
 * @d: reference to XRC domain to close
 *
 * close is implicitly performed at process exit.
 */
int ibv_close_xrc_domain(struct ibv_xrc_domain *d) DEPRECATED;

/**
 * ibv_create_xrc_rcv_qp - creates an XRC QP for serving as a receive-side-only QP,
 *
 * This QP is created in kernel space, and persists until the last process
 * registered for the QP calls ibv_unreg_xrc_rcv_qp() (at which time the QP
 * is destroyed).
 *
 * @init_attr: init attributes to use for QP. xrc domain MUST be included here.
 *	       All other fields are ignored.
 *
 * @xrc_rcv_qpn: qp_num of created QP (if success). To be passed to the
 *		 remote node (sender). The remote node will use xrc_rcv_qpn
 *		 in ibv_post_send when sending to XRC SRQ's on this host
 *		 in the same xrc domain.
 *
 * RETURNS: success (0), or a (negative) error value.
 *
 * NOTE: this verb also registers the calling user-process with the QP at its
 *	 creation time (implicit call to ibv_reg_xrc_rcv_qp), to avoid race
 *	 conditions. The creating process will need to call ibv_unreg_xrc_qp()
 *	 for the QP to release it from this process.
 */
int ibv_create_xrc_rcv_qp(struct ibv_qp_init_attr *init_attr,
			  uint32_t *xrc_rcv_qpn) DEPRECATED;

/**
 * ibv_modify_xrc_rcv_qp - modifies an xrc_rcv qp.
 *
 * @xrc_domain: xrc domain the QP belongs to (for verification).
 * @xrc_qp_num: The (24 bit) number of the XRC QP.
 * @attr: modify-qp attributes. The following fields must be specified:
 *		for RESET_2_INIT: qp_state, pkey_index , port, qp_access_flags
 *		for INIT_2_RTR:   qp_state, path_mtu, dest_qp_num, rq_psn,
 *				  max_dest_rd_atomic, min_rnr_timer, ah_attr
 *		The QP need not be brought to RTS for the QP to operate as a
 *		receive-only QP.
 * @attr_mask:  bitmap indicating which attributes are provided in the attr
 *		struct.	Used for validity checking.
 *		The following bits must be set:
 *		for RESET_2_INIT: IBV_QP_PKEY_INDEX, IBV_QP_PORT,
 *				  IBV_QP_ACCESS_FLAGS, IBV_QP_STATE
 *		for INIT_2_RTR: IBV_QP_AV, IBV_QP_PATH_MTU, IBV_QP_DEST_QPN,
 *				IBV_QP_RQ_PSN, IBV_QP_MAX_DEST_RD_ATOMIC,
 *				IBV_QP_MIN_RNR_TIMER, IBV_QP_STATE
 *
 * RETURNS: success (0), or a (positive) error value.
 *
 */
int ibv_modify_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			  uint32_t xrc_qp_num,
			  struct ibv_qp_attr *attr, int attr_mask) DEPRECATED;

/**
 * ibv_query_xrc_rcv_qp - queries an xrc_rcv qp.
 *
 * @xrc_domain: xrc domain the QP belongs to (for verification).
 * @xrc_qp_num: The (24 bit) number of the XRC QP.
 * @attr: for returning qp attributes.
 * @attr_mask:  bitmap indicating which attributes to return.
 * @init_attr: for returning the init attributes
 *
 * RETURNS: success (0), or a (positive) error value.
 *
 */
int ibv_query_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain, uint32_t xrc_qp_num,
			 struct ibv_qp_attr *attr, int attr_mask,
			 struct ibv_qp_init_attr *init_attr) DEPRECATED;

/**
 * ibv_reg_xrc_rcv_qp: registers a user process with an XRC QP which serves as
 *         a receive-side only QP.
 *
 * @xrc_domain: xrc domain the QP belongs to (for verification).
 * @xrc_qp_num: The (24 bit) number of the XRC QP.
 *
 * RETURNS: success (0),
 *	or error (EINVAL), if:
 *		1. There is no such QP_num allocated.
 *		2. The QP is allocated, but is not an receive XRC QP
 *		3. The XRC QP does not belong to the given domain.
 */
int ibv_reg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
				uint32_t xrc_qp_num) DEPRECATED;

/**
 * ibv_unreg_xrc_rcv_qp: detaches a user process from an XRC QP serving as
 *         a receive-side only QP. If as a result, there are no remaining
 *	   userspace processes registered for this XRC QP, it is destroyed.
 *
 * @xrc_domain: xrc domain the QP belongs to (for verification).
 * @xrc_qp_num: The (24 bit) number of the XRC QP.
 *
 * RETURNS: success (0),
 *	    or error (EINVAL), if:
 *		1. There is no such QP_num allocated.
 *		2. The QP is allocated, but is not an XRC QP
 *		3. The XRC QP does not belong to the given domain.
 * NOTE: There is no reason to return a special code if the QP is destroyed.
 *	 The unregister simply succeeds.
 */
int ibv_unreg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			 uint32_t xrc_qp_num) DEPRECATED;


#endif


