/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __NT_COMPAT_H__
#define __NT_COMPAT_H__

/**
 * Unsupported RTE types
 */
#define RTE_FLOW_ITEM_TYPE_IPinIP 254

/**
	 * Insert a NTPL string into the NTPL filtercode.
	 *
	 * See struct rte_flow_item_ntpl.
	 */
#define	RTE_FLOW_ITEM_TYPE_NTPL 255
/**
 * RTE_FLOW_ITEM_TYPE_NTPL
 *
 * Insert a NTPL string into the NTPL filtercode.
 *
 * tunnel defines how following commands are treated.
 * Setting tunnel=RTE_FLOW_NTPL_TUNNEL makes the following commands to be treated
 * as inner tunnel commands
 *
 * 1. The string is inserted into the filter string with tunnel=RTE_FLOW_NTPL_NO_TUNNEL:
 *
 *    assign[xxxxxxxx]=(Layer3Protocol==IPV4) and and "Here is the ntpl item" and port==0 and Key(KDEF4)==4
 *
 * 2. The string is inserted into the filter string with tunnel=RTE_FLOW_NTPL_TUNNEL:
 *
 *    assign[xxxxxxxx]=(InnerLayer3Protocol==IPV4) and and "Here is the ntpl item" and port==0 and Key(KDEF4)==4
 *
 * Note: When setting tunnel=RTE_FLOW_NTPL_TUNNEL the Layer3Protocol command is changed to InnerLayer3Protocol,
 * 			 now matching the inner layers in stead of the outer layers.
 *
 * Note: When setting tunnel=RTE_FLOW_NTPL_TUNNEL, the commands used before the RTE_FLOW_ITEM_TYPE_NTPL will
 *       match the outer layers and commands used after will match the inner layers.
 *
 * The ntpl item string must have the right syntax in order to prevent a
 * syntax error and it must break the rest of the ntpl string in order
 * to prevent a ntpl error.
 */
struct rte_flow_item_ntpl {
	const char *ntpl_str;
	int tunnel;
};

enum {
	RTE_FLOW_NTPL_NO_TUNNEL,
	RTE_FLOW_NTPL_TUNNEL,
};

#ifndef __cplusplus
static const struct rte_flow_item_ntpl rte_flow_item_ntpl_mask = {
	.ntpl_str = NULL,
	.tunnel   = RTE_FLOW_NTPL_NO_TUNNEL,
};
#endif

#endif

