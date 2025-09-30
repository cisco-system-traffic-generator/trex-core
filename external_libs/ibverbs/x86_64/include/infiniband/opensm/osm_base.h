/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2011 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009 Sun Microsystems, Inc. All rights reserved.
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
 *
 */

/*
 * Abstract:
 * 	Basic OpenSM definitions.
 */

#pragma once

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#ifdef __WIN__
#include <vendor/winosm_common.h>
#endif

#include <iba/ib_types.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Constants
* NAME
*	Constants
*
* DESCRIPTION
*	The following constants are used throughout the OpenSM.
*
* AUTHOR
*	Steve King, Intel
*
*********/
/****d* OpenSM: OSM_DEFAULT_M_KEY
* NAME
*	OSM_DEFAULT_M_KEY
*
* DESCRIPTION
*	Management key value used by the OpenSM.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_M_KEY 0
/********/
/****d* OpenSM: OSM_DEFAULT_SM_KEY
* NAME
*	OSM_DEFAULT_SM_KEY
*
* DESCRIPTION
*	Subnet Manager key value used by the OpenSM.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SM_KEY CL_HTON64(1)
/********/
/****d* OpenSM: OSM_DEFAULT_SA_KEY
* NAME
*	OSM_DEFAULT_SA_KEY
*
* DESCRIPTION
*	Subnet Administration key value.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SA_KEY OSM_DEFAULT_SM_KEY
/********/
/****d* OpenSM: OSM_DEFAULT_LMC
* NAME
*	OSM_DEFAULT_LMC
*
* DESCRIPTION
*	Default LMC value used by the OpenSM.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_LMC 0
/********/
/****d* OpenSM: OSM_DEFAULT_MAX_OP_VLS
* NAME
*	OSM_DEFAULT_MAX_OP_VLS
*
* DESCRIPTION
*	Default Maximal Operational VLs to be initialized on
*  the link ports PortInfo by the OpenSM.
*  Default value provides backward compatibility.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_OP_VLS 2
/********/
/****d* OpenSM: OSM_DEFAULT_SL
* NAME
*	OSM_DEFAULT_SL
*
* DESCRIPTION
*	Default SL value used by the OpenSM.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SL 0
/********/
/****d* OpenSM: OSM_DEFAULT_SCATTER_PORTS
* NAME
*	OSM_DEFAULT_SCATTER_PORTS
*
* DESCRIPTION
*	Default Scatter Ports value used by OpenSM.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SCATTER_PORTS 8
/********/
/****d* OpenSM: OSM_DEFAULT_MAX_SEQ_REDISC
* NAME
*	OSM_DEFAULT_MAX_SEQ_REDISC
* DESCRIPTION
* 	Default SM maximum sequential failed rediscoveries,
* 	before SM skips the rediscovery loop and compete the whole
* 	heavy sweep cycle. 0 is never skip the rediscovery loop.
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_SEQ_REDISC 4
/********/
/****d* OpenSM: OSM_DEFAULT_SM_PRIORITY
* NAME
*	OSM_DEFAULT_SM_PRIORITY
*
* DESCRIPTION
*	Default SM priority value used by the OpenSM,
*	as defined in the SMInfo attribute.  0 is the lowest priority.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SM_PRIORITY 0
/********/
/****d* OpenSM: OSM_DEFAULT_SM_MASTER_PRIORITY
* NAME
*	OSM_DEFAULT_SM_MASTER_PRIORITY
*
* DESCRIPTION
*	Default SM MASTER priority value used by the OpenSM,
*	when entering MASTER state.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SM_MASTER_PRIORITY 14
/********/
/****d* OpenSM: OSM_DEFAULT_TMP_DIR
* NAME
*	OSM_DEFAULT_TMP_DIR
*
* DESCRIPTION
*	Specifies the default temporary directory for the log file,
*  osm-subnet.lst, and other log files.
*
* SYNOPSIS
*/
#ifdef __WIN__
#define OSM_DEFAULT_TMP_DIR "%TEMP%\\"
#else
#define OSM_DEFAULT_TMP_DIR "/var/log/"
#endif
/***********/
/****d* OpenSM: OSM_DEFAULT_CACHE_DIR
* NAME
*	OSM_DEFAULT_CACHE_DIR
*
* DESCRIPTION
*	Specifies the default cache directory for the db files.
*
* SYNOPSIS
*/
#ifdef __WIN__
#define OSM_DEFAULT_CACHE_DIR "%TEMP%"
#elif defined (OPENSM_CACHE_DIR)
#define OSM_DEFAULT_CACHE_DIR OPENSM_CACHE_DIR
#else
#define OSM_DEFAULT_CACHE_DIR "/var/cache/opensm"
#endif
/***********/
/****d* OpenSM: OSM_DEFAULT_LOG_FILE
* NAME
*	OSM_DEFAULT_LOG_FILE
*
* DESCRIPTION
*	Specifies the default log file name
*
* SYNOPSIS
*/
#ifdef __WIN__
#define OSM_DEFAULT_LOG_FILE OSM_DEFAULT_TMP_DIR "osm.log"
#else
#define OSM_DEFAULT_LOG_FILE "/var/log/opensm.log"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_CONFIG_FILE
* NAME
*	OSM_DEFAULT_CONFIG_FILE
*
* DESCRIPTION
*	Specifies the default OpenSM config file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_OPENSM_CONFIG_FILE)
#define OSM_DEFAULT_CONFIG_FILE HAVE_DEFAULT_OPENSM_CONFIG_FILE
#elif defined (OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_CONFIG_FILE OPENSM_CONFIG_DIR "/opensm.conf"
#else
#define OSM_DEFAULT_CONFIG_FILE "/etc/opensm/opensm.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_PARTITION_CONFIG_FILE
* NAME
*	OSM_DEFAULT_PARTITION_CONFIG_FILE
*
* DESCRIPTION
*	Specifies the default partition config file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_PARTITION_CONFIG_FILE)
#define OSM_DEFAULT_PARTITION_CONFIG_FILE HAVE_DEFAULT_PARTITION_CONFIG_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_PARTITION_CONFIG_FILE OPENSM_CONFIG_DIR "/partitions.conf"
#else
#define OSM_DEFAULT_PARTITION_CONFIG_FILE "/etc/opensm/partitions.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_QOS_POLICY_FILE
* NAME
*	OSM_DEFAULT_QOS_POLICY_FILE
*
* DESCRIPTION
*	Specifies the default QoS policy file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_QOS_POLICY_FILE)
#define OSM_DEFAULT_QOS_POLICY_FILE HAVE_DEFAULT_QOS_POLICY_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_QOS_POLICY_FILE OPENSM_CONFIG_DIR "/qos-policy.conf"
#else
#define OSM_DEFAULT_QOS_POLICY_FILE "/etc/opensm/qos-policy.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_CC_POLICY_FILE
* NAME
*	OSM_DEFAULT_CC_POLICY_FILE
*
* DESCRIPTION
*	Specifies the default Congestion Control policy file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_CC_POLICY_FILE)
#define OSM_DEFAULT_CC_POLICY_FILE HAVE_DEFAULT_CC_POLICY_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_CC_POLICY_FILE OPENSM_CONFIG_DIR "/congestion-control-policy.conf"
#else
#define OSM_DEFAULT_CC_POLICY_FILE "/etc/opensm/congestion-control-policy.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_PPCC_ALGO_DIR
* NAME
*	OSM_DEFAULT_PPCC_ALGO_DIR
*
* DESCRIPTION
*	Specifies the default PPCC algoritns dirp name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_PPCC_ALGO_DIR)
#define OSM_DEFAULT_PPCC_ALGO_DIR HAVE_DEFAULT_PPCC_ALGO_DIR
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_PPCC_ALGO_DIR OPENSM_CONFIG_DIR "/ppcc_algo_dir"
#else
#define OSM_DEFAULT_PPCC_ALGO_DIR "/etc/opensm/ppcc_algo_dir"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_TORUS_CONF_FILE
* NAME
*	OSM_DEFAULT_TORUS_CONF_FILE
*
* DESCRIPTION
*	Specifies the default file name for extra torus-2QoS configuration
*
* SYNOPSIS
*/
#ifdef __WIN__
#define OSM_DEFAULT_TORUS_CONF_FILE strcat(GetOsmCachePath(), "osm-torus-2QoS.conf")
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_TORUS_CONF_FILE OPENSM_CONFIG_DIR "/torus-2QoS.conf"
#else
#define OSM_DEFAULT_TORUS_CONF_FILE "/etc/opensm/torus-2QoS.conf"
#endif /* __WIN__ */
/***********/

/****d* OpenSM: OSM_DEFAULT_PREFIX_ROUTES_FILE
* NAME
*	OSM_DEFAULT_PREFIX_ROUTES_FILE
*
* DESCRIPTION
*	Specifies the default prefix routes file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_PREFIX_ROUTES_FILE)
#define OSM_DEFAULT_PREFIX_ROUTES_FILE HAVE_DEFAULT_PREFIX_ROUTES_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_PREFIX_ROUTES_FILE OPENSM_CONFIG_DIR "/prefix-routes.conf"
#else
#define OSM_DEFAULT_PREFIX_ROUTES_FILE "/etc/opensm/prefix-routes.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_PER_MOD_LOGGING_CONF_FILE
* NAME
*	OSM_DEFAULT_PER_MOD_LOGGING_CONF_FILE
*
* DESCRIPTION
*	Specifies the default file name for per module logging configuration
*
* SYNOPSIS
*/
#ifdef __WIN__
#define OSM_DEFAULT_PER_MOD_LOGGING_CONF_FILE strcat(GetOsmCachePath(), "per-module-logging.conf")
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_PER_MOD_LOGGING_CONF_FILE OPENSM_CONFIG_DIR "/per-module-logging.conf"
#else
#define OSM_DEFAULT_PER_MOD_LOGGING_CONF_FILE "/etc/opensm/per-module-logging.conf"
#endif /* __WIN__ */
/***********/

/****d* OpenSM: OSM_DEFAULT_DEVICE_CONF_FILE
* NAME
*	OSM_DEFAULT_DEVICE_CONF_FILE
*
* DESCRIPTION
*	Specifies the default device configuration file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_DEVICE_CONF_FILE)
#define OSM_DEFAULT_DEVICE_CONF_FILE HAVE_DEFAULT_DEVICE_CONF_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_DEVICE_CONF_FILE OPENSM_CONFIG_DIR "/device-configuration.conf"
#else
#define OSM_DEFAULT_DEVICE_CONF_FILE "/etc/opensm/device-configuration.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_FAST_RECOVERY_CONF_FILE
* NAME
*	OSM_DEFAULT_FAST_RECOVERY_CONF_FILE
*
* DESCRIPTION
*	Specifies the default fast recovery configuration file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_FAST_RECOVERY_CONF_FILE)
#define OSM_DEFAULT_FAST_RECOVERY_CONF_FILE HAVE_DEFAULT_FAST_RECOVERY_CONF_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_FAST_RECOVERY_CONF_FILE OPENSM_CONFIG_DIR "/fast_recovery.conf"
#else
#define OSM_DEFAULT_FAST_RECOVERY_CONF_FILE "/etc/opensm/fast_recovery.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_TENANTS_POLICY_CONF_FILE
* NAME
*     OSM_DEFAULT_TENANTS_POLICY_CONF_FILE
*
* DESCRIPTION
*     Specifies the default TENANTS configuration file name
*
* SYNOPSIS
*/
#if defined(HAVE_OSM_DEFAULT_TENANTS_POLICY_CONF_FILE)
#define OSM_DEFAULT_TENANTS_POLICY_CONF_FILE HAVE_OSM_DEFAULT_TENANTS_POLICY_CONF_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_TENANTS_POLICY_CONF_FILE OPENSM_CONFIG_DIR "/tenants-policy.conf"
#else
#define OSM_DEFAULT_TENANTS_POLICY_CONF_FILE "/etc/opensm/tenants-policy.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE
* NAME
*     OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE
*
* DESCRIPTION
*     Specifies the default FABRIC_MODE configuration file name
*
* SYNOPSIS
*/
#if defined(HAVE_OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE)
#define OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE HAVE_OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE
#elif defined(OPENSM_CONFIG_DIR)
#define OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE OPENSM_CONFIG_DIR "/fabric-mode-policy.conf"
#else
#define OSM_DEFAULT_FABRIC_MODE_POLICY_CONF_FILE "/etc/opensm/fabric-mode-policy.conf"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_PID_FILE
* NAME
*	OSM_DEFAULT_PID_FILE
*
* DESCRIPTION
*	Specifies the default pid file name
*
* SYNOPSIS
*/
#if defined(HAVE_DEFAULT_PID_FILE)
#define OSM_DEFAULT_PID_FILE HAVE_DEFAULT_PID_FILE
#elif defined(OPENSM_PID_DIR)
#define OSM_DEFAULT_PID_FILE OPENSM_PID_DIR "/opensm.pid"
#else
#define OSM_DEFAULT_PID_FILE "/var/run/opensm.pid"
#endif
/***********/

/****d* OpenSM: OSM_DEFAULT_SWITCH_NAME
* NAME
*	OSM_DEFAULT_SWITCH_NAME
*
* DESCRIPTION
*	Specifies the default switch name for FiT
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SWITCH_NAME "infiniband-default"
/***********/

/****d* OpenSM: OSM_DEFAULT_FTREE_CA_ORDER_DUMP_FILE
* NAME
*	OSM_DEFAULT_FTREE_CA_ORDER_DUMP_FILE
*
* DESCRIPTION
*	Specifies the default name of ftree ca order file name.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_FTREE_CA_ORDER_DUMP_FILE OSM_DUMP_FILES_PREFIX "-ftree-ca-order"
/***********/
/****d* OpenSM: OSM_DEFAULT_STATISTICS_DUMP_FILE
* NAME
*       OSM_DEFAULT_STATISTICS_DUMP_FILE
*
* DESCRIPTION
*       Specifies the default name of statistic dump file name.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_STATISTICS_DUMP_FILE OSM_DUMP_FILES_PREFIX "-statistics"
/***********/
/****d* OpenSM: OSM_DEFAULT_SWEEP_INTERVAL_SECS
* NAME
*	OSM_DEFAULT_SWEEP_INTERVAL_SECS
*
* DESCRIPTION
*	Specifies the default number of seconds between subnet sweeps.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SWEEP_INTERVAL_SECS 10
/***********/
/****d* OpenSM: OSM_DEFAULT_TRANS_TIMEOUT_MILLISEC
* NAME
*	OSM_DEFAULT_TRANS_TIMEOUT_MILLISEC
*
* DESCRIPTION
*	Specifies the default transaction timeout in milliseconds.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_TRANS_TIMEOUT_MILLISEC 100
/***********/
/****d* OpenSM: OSM_DEFAULT_LONG_TRANS_TIMEOUT_MILLISEC
* NAME
*       OSM_DEFAULT_LONG_TRANS_TIMEOUT_MILLISEC
*
* DESCRIPTION
*       Specifies the default "long" transaction timeout in milliseconds.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_LONG_TRANS_TIMEOUT_MILLISEC 1000
/***********/
/****d* OpenSM: OSM_DEFAULT_SUBNET_TIMEOUT
* NAME
*	OSM_DEFAULT_SUBNET_TIMEOUT
*
* DESCRIPTION
*	Specifies the default subnet timeout.
*	timeout time = 4us * 2^timeout.
*  We use here ~1sec.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SUBNET_TIMEOUT 0x12
/***********/
/****d* OpenSM: OSM_DEFAULT_MAX_SA_GET_FIFO_SIZE
* NAME
*	OSM_DEFAULT_MAX_SA_GET_FIFO_SIZE
*
* DESCRIPTION
*	Specifies the default maximum number of packets that might reside
*	in the queue, before SubnAdmGet/SubnAdmGetTbl requests would
*	be dropped.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_SA_GET_FIFO_SIZE 20000
/***********/
/****d* OpenSM: OSM_DEFAULT_SWITCH_PACKET_LIFE
* NAME
*	OSM_DEFAULT_SWITCH_PACKET_LIFE
*
* DESCRIPTION
*	Specifies the default max life time for a pcket on the switch.
*	timeout time = 4us * 2^timeout.
*  We use here the value of ~1sec
*  A Value > 19dec disables this mechanism.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SWITCH_PACKET_LIFE 0x12
/***********/
/****d* OpenSM: OSM_DEFAULT_HEAD_OF_QUEUE_LIFE
* NAME
*	OSM_DEFAULT_HEAD_OF_QUEUE_LIFE
*
* DESCRIPTION
*	Sets the time a packet can live in the head of the VL Queue
*  We use here the value of ~1sec
*  A Value > 19dec disables this mechanism.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HEAD_OF_QUEUE_LIFE 0x12
/***********/
/****d* OpenSM: OSM_DEFAULT_LEAF_HEAD_OF_QUEUE_LIFE
* NAME
*	OSM_DEFAULT_LEAF_HEAD_OF_QUEUE_LIFE
*
* DESCRIPTION
*	Sets the time a packet can live in the head of the VL Queue
*  of a port that drives a CA port.
*  We use here the value of ~256msec
*
* SYNOPSIS
*/
#define OSM_DEFAULT_LEAF_HEAD_OF_QUEUE_LIFE 0x10
/***********/
/****d* OpenSM: OSM_DEFAULT_VL_STALL_COUNT
* NAME
*	OSM_DEFAULT_LEAF_VL_COUNT
*
* DESCRIPTION
*	Sets the number of consecutive head of queue life time drops that
*  puts the VL into stalled state. In stalled state, the port is supposed
*  to drop everything for 8*(head of queue lifetime)
*
* SYNOPSIS
*/
#define OSM_DEFAULT_VL_STALL_COUNT 0x7
/***********/
/****d* OpenSM: OSM_DEFAULT_LEAF_VL_STALL_COUNT
* NAME
*	OSM_DEFAULT_LEAF_VL_STALL_COUNT
*
* DESCRIPTION
*	Sets the number of consecutive head of queue life time drops that
*  puts the VL into stalled state. In stalled state, the port is supposed
*  to drop everything for 8*(head of queue lifetime). This value is for
*  switch ports driving a CA port.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_LEAF_VL_STALL_COUNT 0x7
/***********/
/****d* OpenSM: OSM_DEFAULT_TRAP_SUPPRESSION_TIMEOUT
* NAME
*	OSM_DEFAULT_TRAP_SUPPRESSION_TIMEOUT
*
* DESCRIPTION
*	Specifies the default timeout for ignoring same trap.
*	timeout time = 5000000us
*  We use here ~5sec.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_TRAP_SUPPRESSION_TIMEOUT 5000000
/***********/
/****d* OpenSM: OSM_DEFAULT_UNHEALTHY_TIMEOUT
* NAME
*	OSM_DEFAULT_UNHEALTHY_TIMEOUT
*
* DESCRIPTION
*	Specifies the default timeout for setting port as unhealthy.
*	timeout time = 60000000us
*  We use here ~60sec.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_UNHEALTHY_TIMEOUT 60000000
/***********/
/****d* OpenSM: OSM_DEFAULT_ERROR_THRESHOLD
* NAME
*	OSM_DEFAULT_ERROR_THRESHOLD
*
* DESCRIPTION
*	Specifies default link error threshold to be set by SubnSet(PortInfo).
*
* SYNOPSIS
*/
#define OSM_DEFAULT_ERROR_THRESHOLD 0x08
/***********/
/****d* OpenSM: OSM_DEFAULT_SMP_MAX_ON_WIRE
* NAME
*	OSM_DEFAULT_SMP_MAX_ON_WIRE
*
* DESCRIPTION
*	Specifies the default number of VL15 SMP MADs allowed on
*	the wire at any one time.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_SMP_MAX_ON_WIRE 32
/***********/
/****d* OpenSM: OSM_SM_DEFAULT_QP0_RCV_SIZE
* NAME
*	OSM_SM_DEFAULT_QP0_RCV_SIZE
*
* DESCRIPTION
*	Specifies the default size (in MADs) of the QP0 receive queue
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_QP0_RCV_SIZE 256
/***********/
/****d* OpenSM: OSM_SM_DEFAULT_QP0_SEND_SIZE
* NAME
*	OSM_SM_DEFAULT_QP0_SEND_SIZE
*
* DESCRIPTION
*	Specifies the default size (in MADs) of the QP0 send queue
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_QP0_SEND_SIZE 256
/***********/
/****d* OpenSM: OSM_SM_DEFAULT_QP1_RCV_SIZE
* NAME
*   OSM_SM_DEFAULT_QP1_RCV_SIZE
*
* DESCRIPTION
*   Specifies the default size (in MADs) of the QP1 receive queue
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_QP1_RCV_SIZE 256
/***********/
/****d* OpenSM: OSM_SM_DEFAULT_QP1_SEND_SIZE
* NAME
*   OSM_SM_DEFAULT_QP1_SEND_SIZE
*
* DESCRIPTION
*   Specifies the default size (in MADs) of the QP1 send queue
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_QP1_SEND_SIZE 256
/****d* OpenSM: OSM_PM_DEFAULT_QP1_RCV_SIZE
* NAME
*   OSM_PM_DEFAULT_QP1_RCV_SIZE
*
* DESCRIPTION
*   Specifies the default size (in MADs) of the QP1 receive queue
*
* SYNOPSIS
*/
#define OSM_PM_DEFAULT_QP1_RCV_SIZE 256
/***********/
/****d* OpenSM: OSM_PM_DEFAULT_QP1_SEND_SIZE
* NAME
*   OSM_PM_DEFAULT_QP1_SEND_SIZE
*
* DESCRIPTION
*   Specifies the default size (in MADs) of the QP1 send queue
*
* SYNOPSIS
*/
#define OSM_PM_DEFAULT_QP1_SEND_SIZE 256
/****d* OpenSM: OSM_SM_DEFAULT_POLLING_TIMEOUT_MILLISECS
* NAME
*   OSM_SM_DEFAULT_POLLING_TIMEOUT_MILLISECS
*
* DESCRIPTION
*   Specifies the polling timeout (in milliseconds) - the timeout
*   between one poll to another.
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_POLLING_TIMEOUT_MILLISECS 10000
/**********/
/****d* OpenSM: OSM_SM_DEFAULT_POLLING_RETRY_NUMBER
* NAME
*   OSM_SM_DEFAULT_POLLING_RETRY_NUMBER
*
* DESCRIPTION
*   Specifies the number of polling retries before the SM goes back
*   to DISCOVERY stage. So the default total time for handoff is 40 sec.
*
* SYNOPSIS
*/
#define OSM_SM_DEFAULT_POLLING_RETRY_NUMBER 4
/**********/
/****d* OpenSM: MC Member Record Receiver/OSM_DEFAULT_MGRP_MTU
* Name
*	OSM_DEFAULT_MGRP_MTU
*
* DESCRIPTION
*	Default MTU used for new MGRP creation (2048 bytes)
*  Note it includes the MTUSelector which is set to "Greater Than"
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MGRP_MTU 0x04
/***********/
/****d* OpenSM: MC Member Record Receiver/OSM_DEFAULT_MGRP_RATE
* Name
*	OSM_DEFAULT_MGRP_RATE
*
* DESCRIPTION
*	Default RATE used for new MGRP creation (2.5Gb/sec)
*  Note it includes the RateSelector which is set to "Greater Than"
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MGRP_RATE 0x02
/***********/
/****d* OpenSM: MC Member Record Receiver/OSM_DEFAULT_MGRP_SCOPE
* Name
*	OSM_DEFAULT_MGRP_SCOPE
*
* DESCRIPTION
*	Default SCOPE used for new MGRP creation (link local)
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MGRP_SCOPE IB_MC_SCOPE_LINK_LOCAL
/***********/
/****d* OpenSM: OSM_DEFAULT_QOS_MAX_VLS
 * Name
 *       OSM_DEFAULT_QOS_MAX_VLS
 *
 * DESCRIPTION
 *       Default Maximum VLs used by the OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_QOS_MAX_VLS 15
/***********/
/****d* OpenSM: OSM_DEFAULT_QOS_HIGH_LIMIT
 * Name
 *       OSM_DEFAULT_QOS_HIGH_LIMIT
 *
 * DESCRIPTION
 *       Default Limit of High Priority in VL Arbitration used by OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_QOS_HIGH_LIMIT 0
/***********/
/****d* OpenSM: OSM_DEFAULT_QOS_VLARB_HIGH
 * Name
 *       OSM_DEFAULT_QOS_VLARB_HIGH
 *
 * DESCRIPTION
 *       Default High Priority VL Arbitration table used by the OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_QOS_VLARB_HIGH "0:4,1:0,2:0,3:0,4:0,5:0,6:0,7:0,8:0,9:0,10:0,11:0,12:0,13:0,14:0"
/***********/
/****d* OpenSM: OSM_DEFAULT_QOS_VLARB_LOW
 * Name
 *       OSM_DEFAULT_QOS_VLARB_LOW
 *
 * DESCRIPTION
 *       Default Low Priority VL Arbitration table used by the OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_QOS_VLARB_LOW "0:0,1:4,2:4,3:4,4:4,5:4,6:4,7:4,8:4,9:4,10:4,11:4,12:4,13:4,14:4"
/***********/
/****d* OpenSM: OSM_DEFAULT_QOS_SL2VL
 * Name
 *       OSM_DEFAULT_QOS_SL2VL
 *
 * DESCRIPTION
 *       Default QoS SL2VL Mapping Table used by the OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_QOS_SL2VL "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,7"
/***********/
/****d* OpenSM: OSM_MCMR_USER_DEFINED_SL
 * Name
 *       OSM_MCMR_USER_DEFINED_SL
 *
 * DESCRIPTION
 *       Value for default_mcg_sl parameter which indicates that OpenSM should
 *       use SL provided in multicast join/create requests.
 *
 * SYNOPSIS
 */
#define OSM_MCMR_USER_DEFINED_SL 0xff
/***********/
/****d* OpenSM: OSM_NO_PATH
* NAME
*	OSM_NO_PATH
*
* DESCRIPTION
*	Value indicating there is no path to the given LID.
*
* SYNOPSIS
*/
#define OSM_NO_PATH			0xFF
/**********/
/****d* OpenSM: OSM_INVALID_GUID
* NAME
*	OSM_INVALID_GUID
*
* DESCRIPTION
*	Value indicating invalid GUID.
*
* SYNOPSIS
*/
#define OSM_INVALID_GUID (0xFFFFFFFFFFFFFFFFULL)
/**********/
/****d* OpenSM: OSM_NODE_DESC_UNKNOWN
* NAME
*	OSM_NODE_DESC_UNKNOWN
*
* DESCRIPTION
*	Value indicating the Node Description is not set and is "unknown"
*
* SYNOPSIS
*/
#define OSM_NODE_DESC_UNKNOWN "<unknown>"
/**********/
/****d* OpenSM: OSM_DEFAULT_MAX_SA_REPORTS
* NAME
*	OSM_DEFAULT_MAX_SA_REPORTS
*
* DESCRIPTION
*	Default value for max_sa_reports option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_SA_REPORTS 512
/**********/
/****d* OpenSM: OSM_DEFAULT_MAX_SA_REPORTS_ON_WIRE
* NAME
*	OSM_DEFAULT_MAX_SA_REPORTS_ON_WIRE
*
* DESCRIPTION
*	Default value for max_sa_reports_on_wire option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_SA_REPORTS_ON_WIRE 256
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_HEALTH_POLICY_FILE
* NAME
*	OSM_DEFAULT_HM_HEALTH_POLICY_FILE
*
* DESCRIPTION
*	Default value for hm_health_policy_file option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_HEALTH_POLICY_FILE NULL
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_REBOOTS
* NAME
*	OSM_DEFAULT_HM_NUM_REBOOTS
*
* DESCRIPTION
*	Default value for hm_num_reboots option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_REBOOTS 10
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_REBOOTS_PERIOD_SECS
* NAME
*	OSM_DEFAULT_HM_REBOOTS_PERIOD_SECS
*
* DESCRIPTION
*	Default value for hm_reboots_period_secs option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_REBOOTS_PERIOD_SECS 900
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_SET_ERR_SWEEPS
* NAME
*	OSM_DEFAULT_HM_NUM_SET_ERR_SWEEPS
*
* DESCRIPTION
*	Default value for hm_num_set_err_sweeps option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_SET_ERR_SWEEPS 5
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_ERR_SWEEPS_WINDOW
* NAME
*	OSM_DEFAULT_HM_NUM_ERR_SWEEPS_WINDOW
*
* DESCRIPTION
*	Default value for hm_num_set_err_sweeps_window option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_ERR_SWEEPS_WINDOW 7
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS
* NAME
*	OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS
*
* DESCRIPTION
*	Default value for hm_num_no_resp_sweeps option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS 5
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS_WINDOW
* NAME
*	OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS_WINDOW
*
* DESCRIPTION
*	Default value for hm_num_no_resp_sweeps option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_NO_RESP_SWEEPS_WINDOW 7
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_TRAPS
* NAME
*	OSM_DEFAULT_HM_NUM_TRAPS
*
* DESCRIPTION
*	Default value for hm_num_traps option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_TRAPS 60
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_TRAPS_PERIOD_SECS
* NAME
*	OSM_DEFAULT_HM_NUM_TRAPS_PERIOD_SECS
*
* DESCRIPTION
*	Default value for hm_num_traps_period_secs option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_TRAPS_PERIOD_SECS 90
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS
* NAME
*	OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS
*
* DESCRIPTION
*	Default value for hm_num_flapping_sweeps option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS 5
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS_WINDOW
* NAME
*	OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS_WINDOW
*
* DESCRIPTION
*	Default value for hm_num_flapping_sweeps_window option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_HM_NUM_FLAPPING_SWEEPS_WINDOW 30
/**********/
/****d* OpenSM: OSM_DEFAULT_HM_NUM_ILLEGAL
* NAME
*	OSM_DEFAULT_HM_NUM_ILLEGAL
*
* DESCRIPTION
*	Default value for hm_num_illegal option
*
* SYNOPSIS
*/

#define OSM_DEFAULT_HM_NUM_ILLEGAL 1
/**********/

/**********/
/****d* OpenSM: OSM_DEFAULT_TOPOLOGIES_PER_SW
* NAME
*	OSM_DEFAULT_TOPOLOGIES_PER_SW
*
* DESCRIPTION
*	Default value for max number of topologies switch may be part of
*
* SYNOPSIS
*/
#define OSM_DEFAULT_TOPOLOGIES_PER_SW 4

/****d* OpenSM: OSM_DEFAULT_FORCE_HEAVY_SWEEP_WINDOW
* NAME
*	OSM_DEFAULT_FORCE_HEAVY_SWEEP_WINDOW
*
* DESCRIPTION
*	Default value for force_heavy_sweep_window
*
* SYNOPSIS
*/
#define OSM_DEFAULT_FORCE_HEAVY_SWEEP_WINDOW -1
/**********/
/****d* OpenSM: OSM_MAX_HM_WINDOW
* NAME
*	OSM_MAX_HM_WINDOW
*
* DESCRIPTION
*	Maximum value of HM window size
*
* SYNOPSIS
*/

#define OSM_MAX_HM_WINDOW 31
/**********/
/****d* OpenSM: osm_thread_state_t
* NAME
*	osm_thread_state_t
*
* DESCRIPTION
*	Enumerates the possible states of worker threads, such
*	as the subnet sweeper.
*
* SYNOPSIS
*/
typedef enum _osm_thread_state {
	OSM_THREAD_STATE_NONE = 0,
	OSM_THREAD_STATE_INIT,
	OSM_THREAD_STATE_RUN,
	OSM_THREAD_STATE_EXIT
} osm_thread_state_t;
/***********/
/****d* OpenSM: OSM_DEFAULT_ROUTING_ENGINE_NAMES
* NAME
*	OSM_DEFAULT_ROUTING_ENGINE_NAMES
*
* DESCRIPTION
*	Default value for the routing engine
*
* SYNOPSIS
*/
#define OSM_DEFAULT_ROUTING_ENGINE_NAMES "ar_updn"
/***********/
/****d* OpenSM: OSM_DEFAULT_ROUTING_THREADS_NUM
* NAME
*	OSM_DEFAULT_ROUTING_THREADS_NUM
*
* DESCRIPTION
*	Default value for routing_threads_num option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_ROUTING_THREADS_NUM  0
/***********/
/****d* OpenSM: OSM_DEFAULT_MAX_THREADS_PER_CORE
* NAME
*
* DESCRIPTION
*	Default value for max_threads_per_core option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_THREADS_PER_CORE  0
/***********/
/****d* OpenSM: OSM_DEFAULT_GMP_THREADS_NUM
* NAME
*
* DESCRIPTION
*	Default value for gmp_traps_threads_num option
*
* SYNOPSIS
*/
#define OSM_DEFAULT_GMP_TRAPS_THREADS_NUM  1
/***********/
/****d* OpenSM: OSM_DEFAULT_OFFSWEEP_BALANCING_WINDOW
* NAME
*
* DESCRIPTION
*	Default value for offsweep_balancing_window option
*
* SYNOPSIS
*/

#define OSM_DEFAULT_OFFSWEEP_BALANCING_WINDOW 180
/***********/
/****d* OpenSM: OSM_DEFAULT_MEPI_ENABLED_SPEED
* NAME
*
* DESCRIPTION
*	Default enabled speeds in Mlnx extended port info
*
* SYNOPSIS
*/
#define OSM_DEFAULT_MEPI_ENABLED_SPEED  FDR10_BIT
/***********/

/****d* OpenSM: OSM_DEFAULT_PFRN_SL
* NAME
*
* DESCRIPTION
*	Default SL for pFRN communication
*
* SYNOPSIS
*/
#define OSM_DEFAULT_PFRN_SL				0x0
/***********/

/****d* OpenSM: OSM_DEFAULT_PFRN_MASK_CLEAR
* NAME
*
* DESCRIPTION
*	Default time (in seconds) since latest pFRN for a specific sub group
*	was received, after which the entire mask must be cleared
*
* SYNOPSIS
*/
#define OSM_DEFAULT_PFRN_MASK_CLEAR			180
/***********/

/****d* OpenSM: OSM_DEFAULT_PFRN_MASK_FORCE_CLEAR
* NAME
*
* DESCRIPTION
*	Default maximal time (in seconds) since last mask clear, after which
*	mask must be cleared. During that period, mask bits may have been set.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_PFRN_MASK_FORCE_CLEAR		720
/***********/

/*
 * OSM_CAP are from IBA 1.2.1 Table 117 and Table 188
 */

/****d* OpenSM: OSM_CAP_IS_TRAP_SUP
* Name
*	OSM_CAP_IS_SUBN_TRAP_SUP
*
* DESCRIPTION
*	Management class generates Trap() MADs
*
* SYNOPSIS
*/
#define OSM_CAP_IS_SUBN_TRAP_SUP (1 << 0)
/***********/

/****d* OpenSM: OSM_CAP_IS_GET_SET_NOTICE_SUP
* Name
*	OSM_CAP_IS_GET_SET_NOTICE_SUP
*
* DESCRIPTION
*       Management class supports Get/Set(Notice)
*
* SYNOPSIS
*/
#define OSM_CAP_IS_SUBN_GET_SET_NOTICE_SUP (1 << 1)
/***********/

/****d* OpenSM: OSM_CAP_IS_SUBN_OPT_RECS_SUP
* Name
*	OSM_CAP_IS_SUBN_OPT_RECS_SUP
*
* DESCRIPTION
*	Support all optional attributes except:
*  MCMemberRecord, TraceRecord, MultiPathRecord
*
* SYNOPSIS
*/
#define OSM_CAP_IS_SUBN_OPT_RECS_SUP (1 << 8)
/***********/

/****d* OpenSM: OSM_CAP_IS_UD_MCAST_SUP
* Name
*	OSM_CAP_IS_UD_MCAST_SUP
*
* DESCRIPTION
*	Multicast is supported
*
* SYNOPSIS
*/
#define OSM_CAP_IS_UD_MCAST_SUP (1 << 9)
/***********/

/****d* OpenSM: OSM_CAP_IS_MULTIPATH_SUP
* Name
*	OSM_CAP_IS_MULTIPATH_SUP
*
* DESCRIPTION
*	MultiPathRecord and TraceRecord are supported
*
* SYNOPSIS
*/
#define OSM_CAP_IS_MULTIPATH_SUP (1 << 10)
/***********/

/****d* OpenSM: OSM_CAP_IS_REINIT_SUP
* Name
*	OSM_CAP_IS_REINIT_SUP
*
* DESCRIPTION
*	SM/SA supports re-initialization supported
*
* SYNOPSIS
*/
#define OSM_CAP_IS_REINIT_SUP (1 << 11)
/***********/

/****d* OpenSM: OSM_CAP_IS_PORT_INFO_CAPMASK_MATCH_SUPPORTED
* Name
*	OSM_CAP_IS_PORT_INFO_CAPMASK_MATCH_SUPPORTED
*
* DESCRIPTION
*	SM/SA supports enhanced SA PortInfoRecord searches per 1.2 Errata:
*  ClassPortInfo:CapabilityMask.IsPortInfoCapMaskMatchSupported is 1,
*  then the AttributeModifier of the SubnAdmGet() and SubnAdmGetTable()
*  methods affects the matching behavior on the PortInfo:CapabilityMask
*  component. If the high-order bit (bit 31) of the AttributeModifier
*  is set to 1, matching on the CapabilityMask component will not be an
*  exact bitwise match as described in <ref to 15.4.4>.  Instead,
*  matching will only be performed on those bits which are set to 1 in
*  the PortInfo:CapabilityMask embedded in the query.
*
* SYNOPSIS
*/
#define OSM_CAP_IS_PORT_INFO_CAPMASK_MATCH_SUPPORTED (1 << 13)
/***********/

/****d* OpenSM: OSM_CAP2_IS_QOS_SUPPORTED
* Name
*	OSM_CAP2_IS_QOS_SUPPORTED
*
* DESCRIPTION
*	QoS is supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_QOS_SUPPORTED (1 << 1)
/***********/

/****d* OpenSM: OSM_CAP2_IS_REVERSE_PATH_PKEY_SUPPORTED
* Name
*	OSM_CAP2_IS_REVERSE_PATH_PKEY_SUPPORTED
*
* DESCRIPTION
*	Reverse path PKeys indicate in PathRecord responses
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_REVERSE_PATH_PKEY_SUPPORTED (1 << 2)
/***********/

/****d* OpenSM: OSM_CAP2_IS_MCAST_TOP_SUPPORTED
* Name
*	OSM_CAP2_IS_MCAST_TOP_SUPPORTED
*
* DESCRIPTION
*       SwitchInfo.MulticastFDBTop is supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_MCAST_TOP_SUPPORTED (1 << 3)
/***********/

/****d* OpenSM: OSM_CAP2_IS_HIERARCHY_SUPPORTED
* Name
*
* DESCRIPTION
*	Hierarchy info supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_HIERARCHY_SUPPORTED (1 << 4)
/***********/

/****d* OpenSM: OSM_CAP2_IS_ALIAS_GUIDS_SUPPORTED
* Name
*
* DESCRIPTION
*	Alias GUIDs supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_ALIAS_GUIDS_SUPPORTED (1 << 5)
/***********/

/****d* OpenSM: OSM_CAP2_IS_FULL_PORTINFO_REC_SUPPORTED
* Name
*	OSM_CAP2_IS_FULL_PORTINFO_REC_SUPPORTED
*
* DESCRIPTION
*	Full PortInfoRecords supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_FULL_PORTINFO_REC_SUPPORTED (1 << 6)
/***********/

/****d* OpenSM: OSM_CAP2_IS_EXTENDED_SPEEDS_SUPPORTED
* Name
*	OSM_CAP2_IS_EXTENDED_SPEEDS_SUPPORTED
*
* DESCRIPTION
*	Extended Link Speeds supported
*
* SYNOPSIS
*/
#define OSM_CAP2_IS_EXTENDED_SPEEDS_SUPPORTED (1 << 7)
/***********/

/****d* OpenSM: OSM_CAP2_IS_MULTICAST_SERVICE_RECS_SUPPORTED
 * Name
 *	OSM_CAP2_IS_MULTICAST_SERVICE_RECS_SUPPORTED
 *
 * DESCRIPTION
 *	Multicast Service Records supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_MULTICAST_SERVICE_RECS_SUPPORTED (1 << 8)

/****d* OpenSM: OSM_CAP2_IS_PORT_INFO_CAPMASK2_MATCH_SUPPORTED
 * Name
 *	OSM_CAP2_IS_PORT_INFO_CAPMASK2_MATCH_SUPPORTED
 *
 * DESCRIPTION
 *	CapMask2 matching for PortInfoRecord supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_PORT_INFO_CAPMASK2_MATCH_SUPPORTED (1 << 10)

/****d* OpenSM: OSM_CAP2_IS_SEND_ONLY_FULL_MEM_SUPPORTED
 * Name
 *	OSM_CAP2_IS_SEND_ONLY_FULL_MEM_SUPPORTED
 *
 * DESCRIPTION
 *	CapMask2 is send only full member supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_SEND_ONLY_FULL_MEM_SUPPORTED (1 << 12)

/****d* OpenSM: OSM_DEFAULT_FDR10
 * Name
 *	OSM_DEFAULT_FDR10
 *
 * DESCRIPTION
 *	Default value for fdr10 parameter
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_FDR10 0xFF

/****d* OpenSM: OSM_DEFAULT_VIRT_MAX_PORTS_IN_PROCESS
 * Name
 *	OSM_DEFAULT_VIRT_MAX_PORTS_IN_PROCESS
 *
 * DESCRIPTION
 *	Default value for virt_max_ports_in_process, which controls maximum
 *	number of ports to be processed simultaneously by Virtualization
 *	Manager.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_VIRT_MAX_PORTS_IN_PROCESS 64

/****d* OpenSM: OSM_DEFAULT_VIRT_HOP_LIMIT
 * Name
 *
 *
 * DESCRIPTION
 *	Default hop limit value for virtual ports.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_VIRT_HOP_LIMIT 2

/****d* OpenSM: OSM_VIRT_REVISION
* NAME
*	OSM_VIRT_REVISION
*
* DESCRIPTION
*	Virtualization revision supported by the OpenSM.
*
* SYNOPSIS
*/
#define OSM_VIRT_REVISION 0

/****d* OpenSM: OSM_CAP2_IS_LINK_WIDTH_2X_SUPPORTED
 * Name
 *	OSM_CAP2_IS_LINK_WIDTH_2X_SUPPORTED
 *
 * DESCRIPTION
 *	2x link widths supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_LINK_WIDTH_2X_SUPPORTED (1 << 13)

/****d* OpenSM: OSM_CAP2_IS_LINK_SPEED_HDR_SUPPORTED
 * Name
 *	OSM_CAP2_IS_LINK_SPEED_HDR_SUPPORTED
 *
 * DESCRIPTION
 *	HDR link speed supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_LINK_SPEED_HDR_SUPPORTED (1 << 15)

/****d* OpenSM: OSM_CAP2_IS_LINK_SPEED_NDR_SUPPORTED
 * Name
 *	OSM_CAP2_IS_LINK_SPEED_NDR_SUPPORTED
 *
 * DESCRIPTION
 *	NDR link speed supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_LINK_SPEED_NDR_SUPPORTED (1 << 17)

/****d* OpenSM: OSM_CAP2_IS_EXTENDED_SPEEDS2_SUPPORTED
 * Name
 *	OSM_CAP2_IS_EXTENDED_SPEEDS2_SUPPORTED
 *
 * DESCRIPTION
 *	Extended Link Speeds 2 supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_EXTENDED_SPEEDS2_SUPPORTED (1 << 19)

/****d* OpenSM: OSM_CAP2_IS_LINK_SPEED_XDR_SUPPORTED
 * Name
 *	OSM_CAP2_IS_LINK_SPEED_XDR_SUPPORTED
 *
 * DESCRIPTION
 *	XDR link speed supported
 *
 * SYNOPSIS
 */
#define OSM_CAP2_IS_LINK_SPEED_XDR_SUPPORTED (1 << 20)

/****d* OpenSM: osm_signal_t
* NAME
*	osm_signal_t
*
* DESCRIPTION
*	Enumerates the possible signal codes used by the OSM managers
*	This cannot be an enum type, since conversion to and from
*	integral types is necessary when passing signals through
*	the dispatcher.
*
* SYNOPSIS
*/
#define OSM_SIGNAL_NONE				        0
#define OSM_SIGNAL_SWEEP			        1
#define OSM_SIGNAL_IDLE_TIME_PROCESS_REQUEST	        2
#define OSM_SIGNAL_PERFMGR_SWEEP		        3
#define OSM_SIGNAL_GUID_PROCESS_REQUEST		        4
#define OSM_SIGNAL_VIRTMGR_PROCESS_REQUEST	        5
#define OSM_SIGNAL_INFORM_PROCESS_REQUEST	        6
#define OSM_SIGNAL_DUMP_STATISTIC		        7
#define OSM_SIGNAL_IDLE_TIME_REBALANCE_ROUTING          8
#define OSM_SIGNAL_FLID_ROUTING_SWEEP		        9
#define OSM_SIGNAL_PKEY_SWEEP_REQUEST                   10
#define OSM_SIGNAL_IDLE_TIME_EXTERNAL_PROCESS_REQUEST	11
#define OSM_SIGNAL_UPDATE_KEYS				12
#define OSM_SIGNAL_MAX				        13

typedef unsigned int osm_signal_t;
/***********/

/****d* OpenSM: osm_sm_signal_t
* NAME
*	osm_sm_signal_t
*
* DESCRIPTION
*	Enumerates the possible signals used by the OSM_SM_MGR
*
* SYNOPSIS
*/
typedef enum _osm_sm_signal {
	OSM_SM_SIGNAL_NONE = 0,
	OSM_SM_SIGNAL_DISCOVERY_COMPLETED,
	OSM_SM_SIGNAL_POLLING_TIMEOUT,
	OSM_SM_SIGNAL_DISCOVER,
	OSM_SM_SIGNAL_DISABLE,
	OSM_SM_SIGNAL_HANDOVER,
	OSM_SM_SIGNAL_HANDOVER_SENT,
	OSM_SM_SIGNAL_ACKNOWLEDGE,
	OSM_SM_SIGNAL_STANDBY,
	OSM_SM_SIGNAL_MASTER_OR_HIGHER_SM_DETECTED,
	OSM_SM_SIGNAL_WAIT_FOR_HANDOVER,
	OSM_SM_SIGNAL_MAX
} osm_sm_signal_t;
/***********/

/****d* OpenSM: MAX_GUID_FILE_LINE_LENGTH
* NAME
*	MAX_GUID_FILE_LINE_LENGTH
*
* DESCRIPTION
*	The maximum line number when reading guid file
*
* SYNOPSIS
*/
#define MAX_GUID_FILE_LINE_LENGTH 120
/**********/

/****d* OpenSM: VendorOUIs
* NAME
*	VendorOUIs
*
* DESCRIPTION
*	Known device vendor ID and GUID OUIs
*
* SYNOPSIS
*/
#define OSM_VENDOR_ID_INTEL         0x00D0B7
#define OSM_VENDOR_ID_MELLANOX      0x0002C9
#define OSM_VENDOR_ID_REDSWITCH     0x000617
#define OSM_VENDOR_ID_SILVERSTORM   0x00066A
#define OSM_VENDOR_ID_TOPSPIN       0x0005AD
#define OSM_VENDOR_ID_FUJITSU       0x00E000
#define OSM_VENDOR_ID_FUJITSU2      0x000B5D
#define OSM_VENDOR_ID_VOLTAIRE      0x0008F1
#define OSM_VENDOR_ID_YOTTAYOTTA    0x000453
#define OSM_VENDOR_ID_PATHSCALE     0x001175
#define OSM_VENDOR_ID_IBM           0x000255
#define OSM_VENDOR_ID_DIVERGENET    0x00084E
#define OSM_VENDOR_ID_FLEXTRONICS   0x000B8C
#define OSM_VENDOR_ID_AGILENT       0x0030D3
#define OSM_VENDOR_ID_OBSIDIAN      0x001777
#define OSM_VENDOR_ID_BAYMICRO      0x000BC1
#define OSM_VENDOR_ID_LSILOGIC      0x00A0B8
#define OSM_VENDOR_ID_DDN           0x0001FF
#define OSM_VENDOR_ID_PANTA         0x001393
#define OSM_VENDOR_ID_HP            0x001708
#define OSM_VENDOR_ID_RIOWORKS      0x005045
#define OSM_VENDOR_ID_SUN           0x0003BA
#define OSM_VENDOR_ID_SUN2          0x002128
#define OSM_VENDOR_ID_3LEAFNTWKS    0x0016A1
#define OSM_VENDOR_ID_XSIGO         0x001397
#define OSM_VENDOR_ID_HP2           0x0018FE
#define OSM_VENDOR_ID_DELL          0x00188B
#define OSM_VENDOR_ID_SUPERMICRO    0x003048
#define OSM_VENDOR_ID_HP3           0x0019BB
#define OSM_VENDOR_ID_HP4           0x00237D
#define OSM_VENDOR_ID_OPENIB        0x001405
#define OSM_VENDOR_ID_IBM2	    0x5CF3FC
#define OSM_VENDOR_ID_MELLANOX2     0xF45214
#define OSM_VENDOR_ID_MELLANOX3     0x00258B
#define OSM_VENDOR_ID_MELLANOX4     0xE41D2D
#define OSM_VENDOR_ID_MELLANOX5     0x7CFE90
#define OSM_VENDOR_ID_MELLANOX6     0xEC0D9A
#define OSM_VENDOR_ID_MELLANOX7     0x248A07
#define OSM_VENDOR_ID_MELLANOX8     0x506B4B
#define OSM_VENDOR_ID_MELLANOX9     0x98039B
#define OSM_VENDOR_ID_BULL          0x080038

/* IPoIB Broadcast Defaults */
#define OSM_IPOIB_BROADCAST_MGRP_QKEY 0x0b1b
extern const ib_gid_t osm_ipoib_broadcast_mgid;

/**********/
/****d* OpenSM: osm_rtr_aguid_mode_t
* NAME
*	osm_rtr_aguid_mode_t
*
* DESCRIPTION
*	Enumerates the possible modes of router alias guid enabling.
*
* SYNOPSIS
*/
typedef enum _osm_rtr_aguid_mode {
	OSM_RTR_AGUID_IGNORE = 0,
	OSM_RTR_AGUID_ENABLE,
	OSM_RTR_AGUID_DISABLE,
	OSM_RTR_AGUID_DEFAULT = OSM_RTR_AGUID_IGNORE
} osm_rtr_aguid_mode_t;
/**********/

/****d* OpenSM: OSM_RTR_PR_MTU_USE_MTU_ON_PATH
* NAME
*	OSM_RTR_PR_MTU_USE_MTU_ON_PATH
*
* DESCRIPTION
*	A special value for mtu configuration for taking maximal common available MTU
*	along the path in the path record query from source to destination.
*
* SYNOPSIS
*/
#define OSM_RTR_PR_MTU_USE_MTU_ON_PATH	255

/****d* OpenSM: MC Member Record Receiver/OSM_DEFAULT_RTR_PR_MTU
* Name
*	OSM_DEFAULT_RTR_PR_MTU
*
* DESCRIPTION
*	Default inter subnet PathRecord MTU is a maximal common available MTU
*	on the path from source to the router, that leads to destination on another subnet.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_RTR_PR_MTU OSM_RTR_PR_MTU_USE_MTU_ON_PATH
/***********/

/****d* OpenSM: OSM_RTR_PR_RATE_USE_RATE_ON_PATH
* NAME
*	OSM_RTR_PR_RATE_USE_RATE_ON_PATH
*
* DESCRIPTION
*	A special value for rate configuration for taking maximal common available rate
*	along the path in the path record query from source to destination.
*
* SYNOPSIS
*/
#define OSM_RTR_PR_RATE_USE_RATE_ON_PATH	255

/****d* OpenSM: MC Member Record Receiver/OSM_DEFAULT_RTR_PR_RATE
* Name
*	OSM_DEFAULT_RTR_PR_RATE
*
* DESCRIPTION
*	Default inter subnet PathRecord Rate is a maximal common available rate
*	on the path from source to the router, that leads to destination on another subnet.
*
* SYNOPSIS
*/
#define OSM_DEFAULT_RTR_PR_RATE OSM_RTR_PR_RATE_USE_RATE_ON_PATH
/***********/
/****d* OpenSM: OSM_DEFAULT_RTR_PR_FLOWLABEL
* NAME
*	OSM_DEFAULT_RTR_PR_FLOWLABEL
*
* DESCRIPTION
*	Default inter subnet PathRecord FlowLabel
*
* SYNOPSIS
*/
#define OSM_DEFAULT_RTR_PR_FLOWLABEL 0
/********/
/****d* OpenSM: OSM_DEFAULT_RTR_PR_TCLASS
* NAME
*	OSM_DEFAULT_RTR_PR_TCLASS
*
* DESCRIPTION
*	Default inter subnet PathRecord TClass
*
* SYNOPSIS
*/
#define OSM_DEFAULT_RTR_PR_TCLASS 0
/********/
/****d* OpenSM: OSM_DEFAULT_MAX_CAS_ON_SPINE
* NAME
* 	OSM_DEFAULT_MAX_CAS_ON_SPINE
* DESCRIPTION
* 	Default maximum number of CAs on switch, for DF+ routing algorithm
* 	to consider the switch as spine.
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_CAS_ON_SPINE 2
/********/

/****d* OpenSM: OSM_DEFAULT_MAX_WIRE_SMPS_PER_DEVICE
* NAME
* 	OSM_DEFAULT_MAX_WIRE_SMPS_PER_DEVICE
*
* DESCRIPTION
* 	The default number of SMPs sent in parallel to the same port.
* 	Currently, the supported MADs are:
* 	portInfo/Extended portInfo, LFTs, AR LFTs,
* 	AR group table, AR copy group table, RN sub group direction.
* SYNOPSIS
*/
#define OSM_DEFAULT_MAX_WIRE_SMPS_PER_DEVICE 2
/********/

/****d* OpenSM: OSM_DEFAULT_ADAPTIVE_TIMEOUT_SL_MASK
* NAME
* 	OSM_DEFAULT_ADAPTIVE_TIMEOUT_SL_MASK
*
* DESCRIPTION
* 	The default SL mask for Adaptive Timeout feature.
* 	When actiated on a device that supports the feature, it allows
* 	QPs on enbled SLs to implement adaptive timeout.
* SYNOPSIS
*/
#define OSM_DEFAULT_ADAPTIVE_TIMEOUT_SL_MASK 0xFFFF
/********/

/****d* OpenSM: OSM_DEFAULT_SUPPRESS_MC_PKEY_TRAPS
* NAME
* 	OSM_DEFAULT_SUPPRESS_MC_PKEY_TRAPS
*
* DESCRIPTION
* 	The default parameter for multicast partition key trap suppression.
* 	When bit 0 is activated these traps will be suppressed.
* SYNOPSIS
*/
#define OSM_DEFAULT_SUPPRESS_MC_PKEY_TRAPS 0x01
/********/


/****d* OpenSM: osm_sm_standalone_t
* NAME
*       osm_sm_standalone_t
*
* DESCRIPTION
*       Enumerates the possible modes for the SM standalone mode
*
* SYNOPSIS
*/
typedef enum _osm_sm_standalone_state {
	OSM_STANDALONE_SA = 0,
	OSM_STANDALONE_NA,
	OSM_STANDALONE_BOTH,
	OSM_STANDALONE_MAX
} osm_sm_standalone_t;

/****d* OpenSM: OSM_DEFAULT_FLID_COMPRESSION
* NAME
* 	OSM_DEFAULT_FLID_COMPRESSION
*
* DESCRIPTION
* 	The default parameter for FLID compression ratio - a maximal number of leaf switches
* 	that can share the same FLID.
* SYNOPSIS
*/
#define OSM_DEFAULT_FLID_COMPRESSION	2
/********/

/* Some hardcoded values for Single Node (SN) case of NVLink */
#define OSM_NVLINK_MIN_PLANES			1
#define OSM_NVLINK_MAX_PLANES			18

#define OSM_FABRIC_MODE_PROFILE_IGNORE		"none"
#define OSM_DEFAULT_FABRIC_MODE_PROFILE		OSM_FABRIC_MODE_PROFILE_IGNORE

/* HOQ life time is 5bits field, hence any larger number can be used to mark invalid value */
#define OSM_HOQ_LIFE_MAX_VALUE			0x1F
#define OSM_HOQ_LIFE_INVALID			OSM_HOQ_LIFE_MAX_VALUE + 1

#define OSM_STO_MILLISEC			500

END_C_DECLS
