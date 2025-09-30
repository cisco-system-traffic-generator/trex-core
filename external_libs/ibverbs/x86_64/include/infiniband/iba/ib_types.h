/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2011 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009 HNR Consulting. All rights reserved.
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

#pragma once

#include <string.h>
#include <complib/cl_types.h>
#include <complib/cl_byteswap.h>
#include <complib/cl_math.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

#ifndef cl_hton8
#define	cl_hton8(x)	(x)
#endif

/*
 * Internal macros for getting/setting field values.
 */
#define _IBTYPES_GET_ATTR(FUNC,NAME,MASK,SHIFT)         (FUNC(NAME) & MASK) >> SHIFT
#define _IBTYPES_SET_ATTR(FUNC,NAME,MASK,SHIFT,VALUE)   NAME = FUNC((FUNC(NAME) & ~MASK) | ((VALUE << SHIFT) & MASK))

/*
 * The following macros allow for getting/setting field values from network order structs.
 *
 * Parameters:
 *	NAME	field name - for example 'p_mad->field'.
 *	MASK	mask in the data type which corresponds to the NAME
 *	SHIFT	shift in the data type which corresponds to the NAME
 *	VALUE	new value for the field.
 *
 * Example:
 *	typedef struct {
 *		uint16_t	a_b_c;		// a is 5 bits, b is 7 bits, c is 4 bits
 *	} ib_fake_mad_t;
 *
 *	ib_fake_mad_t		*p_mad;
 *
 *	IBTYPES_GET_ATTR16(p_mad->a_b_c, 0xf800, 12) would return the value of 'a'.
 *	IBTYPES_GET_ATTR16(p_mad->a_b_c, 0x07f0,  4) would return the value of 'b'.
 *	IBTYPES_GET_ATTR16(p_mad->a_b_c, 0x000f,  0) would return the value of 'c'.
 *
 *	IBTYPES_SET_ATTR16(p_mad->a_b_c, 0xf800, 12, 7) would set the value of 'a' to 7.
 *	IBTYPES_SET_ATTR16(p_mad->a_b_c, 0x07f0,  4, 9) would set the value of 'b' to 9.
 *	IBTYPES_SET_ATTR16(p_mad->a_b_c, 0x000f,  0, 3) would set the value of 'c' to 3.
 */
#define IBTYPES_GET_ATTR8(NAME,MASK,SHIFT)		_IBTYPES_GET_ATTR(cl_hton8,  NAME, MASK, SHIFT)
#define IBTYPES_GET_ATTR16(NAME,MASK,SHIFT)		_IBTYPES_GET_ATTR(cl_hton16, NAME, MASK, SHIFT)
#define IBTYPES_GET_ATTR32(NAME,MASK,SHIFT)		_IBTYPES_GET_ATTR(cl_hton32, NAME, MASK, SHIFT)
#define IBTYPES_GET_ATTR64(NAME,MASK,SHIFT)		_IBTYPES_GET_ATTR(cl_hton64, NAME, MASK, SHIFT)

#define IBTYPES_SET_ATTR8(NAME,MASK,SHIFT,VALUE)	_IBTYPES_SET_ATTR(cl_hton8,  NAME, MASK, SHIFT, VALUE)
#define IBTYPES_SET_ATTR16(NAME,MASK,SHIFT,VALUE)	_IBTYPES_SET_ATTR(cl_hton16, NAME, MASK, SHIFT, VALUE)
#define IBTYPES_SET_ATTR32(NAME,MASK,SHIFT,VALUE)	_IBTYPES_SET_ATTR(cl_hton32, NAME, MASK, SHIFT, VALUE)
#define IBTYPES_SET_ATTR64(NAME,MASK,SHIFT,VALUE)	_IBTYPES_SET_ATTR(cl_hton64, NAME, MASK, SHIFT, VALUE)


#if defined( __WIN__ )
#if defined( EXPORT_AL_SYMBOLS )
#define OSM_EXPORT	__declspec(dllexport)
#else
#define OSM_EXPORT	__declspec(dllimport)
#endif
#define OSM_API __stdcall
#define OSM_CDECL __cdecl
#else
#define OSM_EXPORT	extern
#define OSM_API
#define OSM_CDECL
#define __ptr64
#endif
/****h* IBA Base/Constants
* NAME
*	Constants
*
* DESCRIPTION
*	The following constants are used throughout the IBA code base.
*
*	Definitions are from the InfiniBand Architecture Specification v1.2
*
*********/
/****d* IBA Base: Constants/MAD_BLOCK_SIZE
* NAME
*	MAD_BLOCK_SIZE
*
* DESCRIPTION
*	Size of a non-RMPP MAD datagram.
*
* SOURCE
*/
#define MAD_BLOCK_SIZE						256
/**********/
/****d* IBA Base: Constants/MAD_RMPP_HDR_SIZE
* NAME
*	MAD_RMPP_HDR_SIZE
*
* DESCRIPTION
*	Size of an RMPP header, including the common MAD header.
*
* SOURCE
*/
#define MAD_RMPP_HDR_SIZE					36
/**********/
/****d* IBA Base: Constants/MAD_RMPP_DATA_SIZE
* NAME
*	MAD_RMPP_DATA_SIZE
*
* DESCRIPTION
*	Size of an RMPP transaction data section.
*
* SOURCE
*/
#define MAD_RMPP_DATA_SIZE		(MAD_BLOCK_SIZE - MAD_RMPP_HDR_SIZE)
/**********/
/****d* IBA Base: Constants/MAD_BLOCK_GRH_SIZE
* NAME
*	MAD_BLOCK_GRH_SIZE
*
* DESCRIPTION
*	Size of a MAD datagram, including the GRH.
*
* SOURCE
*/
#define MAD_BLOCK_GRH_SIZE					296
/**********/
/****d* IBA Base: Constants/IB_LID_PERMISSIVE
* NAME
*	IB_LID_PERMISSIVE
*
* DESCRIPTION
*	Permissive LID
*
* SOURCE
*/
#define IB_LID_PERMISSIVE					0xFFFF
/**********/
/****d* IBA Base: Constants/IB_DEFAULT_PKEY
* NAME
*	IB_DEFAULT_PKEY
*
* DESCRIPTION
*	P_Key value for the default partition.
*
* SOURCE
*/
#define IB_DEFAULT_PKEY						0xFFFF
/**********/
/****d* IBA Base: Constants/IB_QP1_WELL_KNOWN_Q_KEY
* NAME
*	IB_QP1_WELL_KNOWN_Q_KEY
*
* DESCRIPTION
*	Well-known Q_Key for QP1 privileged mode access (15.4.2).
*
* SOURCE
*/
#define IB_QP1_WELL_KNOWN_Q_KEY				CL_HTON32(0x80010000)
/*********/
#define IB_QP0								0
#define IB_QP1								CL_HTON32(1)
#define IB_QP_PRIVILEGED_Q_KEY				CL_HTON32(0x80000000)
/****d* IBA Base: Constants/IB_LID_UCAST_START
* NAME
*	IB_LID_UCAST_START
*
* DESCRIPTION
*	Lowest valid unicast LID value.
*
* SOURCE
*/
#define IB_LID_UCAST_START_HO				0x0001
#define IB_LID_UCAST_START					(CL_HTON16(IB_LID_UCAST_START_HO))
/**********/
/****d* IBA Base: Constants/IB_LID_UCAST_END
* NAME
*	IB_LID_UCAST_END
*
* DESCRIPTION
*	Highest valid unicast LID value.
*
* SOURCE
*/
#define IB_LID_UCAST_END_HO					0xBFFF
#define IB_LID_UCAST_END					(CL_HTON16(IB_LID_UCAST_END_HO))
/**********/
/****d* IBA Base: Constants/IB_LID_MCAST_START
* NAME
*	IB_LID_MCAST_START
*
* DESCRIPTION
*	Lowest valid multicast LID value.
*
* SOURCE
*/
#define IB_LID_MCAST_START_HO				0xC000
#define IB_LID_MCAST_START					(CL_HTON16(IB_LID_MCAST_START_HO))
/**********/
/****d* IBA Base: Constants/IB_LID_MCAST_END
* NAME
*	IB_LID_MCAST_END
*
* DESCRIPTION
*	Highest valid multicast LID value.
*
* SOURCE
*/
#define IB_LID_MCAST_END_HO					0xFFFE
#define IB_LID_MCAST_END					(CL_HTON16(IB_LID_MCAST_END_HO))
/**********/
/****d* IBA Base: Constants/IB_DEFAULT_SUBNET_PREFIX
* NAME
*	IB_DEFAULT_SUBNET_PREFIX
*
* DESCRIPTION
*	Default link local subnet GID prefix.
*
* SOURCE
*/
#define IB_DEFAULT_SUBNET_PREFIX			(CL_HTON64(0xFE80000000000000ULL))
#define IB_DEFAULT_SUBNET_PREFIX_HO			(0xFE80000000000000ULL)

/**********/
/****d* IBA Base: Constants/IB_10_MSB_MASK
* NAME
*	IB_10_MSB_MASK
*
* DESCRIPTION
* 10 most significant bits mask
*
* SOURCE
*/
#define IB_10_MSB_MASK					(CL_HTON64(0xFFC0000000000000ULL))

/**********/

/****d* IBA Base: Constants/IB_LOCAL_SUBNET_MASK
* NAME
*	IB_LOCAL_SUBNET_MASK
*
* DESCRIPTION
* A mask to inspect 32 most significant bits, excluding 32 less significant bits,
* that can have subnet prefix and FLID.
*
* SOURCE
*/
#define IB_LOCAL_SUBNET_MASK					(CL_HTON64(0xFFFFFFFF00000000ULL))

/**********/

/****d* IBA Base: Constants/IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK
* NAME
*	IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK
*
* DESCRIPTION
* A mask to inspect 32 most significant bits and 16 less significant bits,
* excluding the 16 bits that can have FLID.
*
* SOURCE
*/
#define IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK			(CL_HTON64(0xFFFFFFFF0000FFFFULL))

/**********/

/****d* IBA Base: Constants/IB_SITE_LOCAL_SUBNET_PREFIX
* NAME
*	IB_SITE_LOCAL_SUBNET_PREFIX
*
* DESCRIPTION
*	Default site local subnet GID prefix.
*
* SOURCE
*/
#define IB_SITE_LOCAL_SUBNET_PREFIX			(CL_HTON64(0xFEC0000000000000ULL))
#define IB_SITE_LOCAL_SUBNET_PREFIX_HO			(0xFEC0000000000000ULL)

/**********/

/****d* IBA Base: Constants/IB_WELL_KNOWN_SM_GUID
* NAME
*	IB_WELL_KNOWN_SM_GUID
*
* DESCRIPTION
*	Port GUID component of the well known SM GID
*
* SOURCE
*/
#define IB_WELL_KNOWN_SM_GUID				(0x0200000000000002ULL)
/**********/


/****d* IBA Base: Constants/IB_NODE_NUM_PORTS_MAX
* NAME
*	IB_NODE_NUM_PORTS_MAX
*
* DESCRIPTION
*	Maximum number of ports in a single node (14.2.5.7).
* SOURCE
*/
#define IB_NODE_NUM_PORTS_MAX				0xFE
/**********/
/****d* IBA Base: Constants/IB_INVALID_PORT_NUM
* NAME
*	IB_INVALID_PORT_NUM
*
* DESCRIPTION
*	Value used to indicate an invalid port number (14.2.5.10).
*
* SOURCE
*/
#define IB_INVALID_PORT_NUM					0xFF
/*********/
/****d* IBA Base: Constants/IB_SUBNET_PATH_HOPS_MAX
* NAME
*	IB_SUBNET_PATH_HOPS_MAX
*
* DESCRIPTION
*	Maximum number of directed route switch hops in a subnet (14.2.1.2).
*
* SOURCE
*/
#define IB_SUBNET_PATH_HOPS_MAX				64
/*********/
/****d* IBA Base: Constants/IB_HOPLIMIT_MAX
* NAME
*	IB_HOPLIMIT_MAX
*
* DESCRIPTION
*       Maximum number of router hops allowed.
*
* SOURCE
*/
#define IB_HOPLIMIT_MAX					255
/*********/
/****d* IBA Base: Constants/IB_MC_SCOPE_*
* NAME
*	IB_MC_SCOPE_*
*
* DESCRIPTION
*	Scope component definitions from IBA 1.2.1 (Table 3 p. 148)
*/
#define IB_MC_SCOPE_LINK_LOCAL		0x2
#define IB_MC_SCOPE_SITE_LOCAL		0x5
#define IB_MC_SCOPE_ORG_LOCAL		0x8
#define IB_MC_SCOPE_GLOBAL		0xE
/*********/
/****d* IBA Base: Constants/IB_PKEY_MAX_BLOCKS
* NAME
*	IB_PKEY_MAX_BLOCKS
*
* DESCRIPTION
*	Maximum number of PKEY blocks (14.2.5.7).
*
* SOURCE
*/
#define IB_PKEY_MAX_BLOCKS					2048
/*********/
/****d* IBA Base: Constants/IB_MCAST_MAX_BLOCK_ID
* NAME
*	IB_MCAST_MAX_BLOCK_ID
*
* DESCRIPTION
*	Maximum number of Multicast port mask blocks
*
* SOURCE
*/
#define IB_MCAST_MAX_BLOCK_ID				511
/*********/
/****d* IBA Base: Constants/IB_MCAST_BLOCK_ID_MASK_HO
* NAME
*	IB_MCAST_BLOCK_ID_MASK_HO
*
* DESCRIPTION
*	Mask (host order) to recover the Multicast block ID.
*
* SOURCE
*/
#define IB_MCAST_BLOCK_ID_MASK_HO			0x000001FF
/*********/
/****d* IBA Base: Constants/IB_MCAST_BLOCK_SIZE
* NAME
*	IB_MCAST_BLOCK_SIZE
*
* DESCRIPTION
*	Number of port mask entries in a multicast forwarding table block.
*
* SOURCE
*/
#define IB_MCAST_BLOCK_SIZE					32
/*********/
/****d* IBA Base: Constants/IB_MCAST_MASK_SIZE
* NAME
*	IB_MCAST_MASK_SIZE
*
* DESCRIPTION
*	Number of port mask bits in each entry in the multicast forwarding table.
*
* SOURCE
*/
#define IB_MCAST_MASK_SIZE					16
/*********/
/****d* IBA Base: Constants/IB_MCAST_POSITION_MASK_HO
* NAME
*	IB_MCAST_POSITION_MASK_HO
*
* DESCRIPTION
*	Mask (host order) to recover the multicast block position.
*
* SOURCE
*/
#define IB_MCAST_POSITION_MASK_HO				0xF0000000
/*********/
/****d* IBA Base: Constants/IB_MCAST_POSITION_MAX
* NAME
*	IB_MCAST_POSITION_MAX
*
* DESCRIPTION
*	Maximum value for the multicast block position.
*
* SOURCE
*/
#define IB_MCAST_POSITION_MAX				0xF
/*********/
/****d* IBA Base: Constants/IB_MCAST_POSITION_SHIFT
* NAME
*	IB_MCAST_POSITION_SHIFT
*
* DESCRIPTION
*	Shift value to normalize the multicast block position value.
*
* SOURCE
*/
#define IB_MCAST_POSITION_SHIFT				28
/*********/
/****d* IBA Base: Constants/IB_PKEY_ENTRIES_MAX
* NAME
*	IB_PKEY_ENTRIES_MAX
*
* DESCRIPTION
*	Maximum number of PKEY entries per port (14.2.5.7).
*
* SOURCE
*/
#define IB_PKEY_ENTRIES_MAX (IB_PKEY_MAX_BLOCKS * IB_NUM_PKEY_ELEMENTS_IN_BLOCK)
/*********/
/****d* IBA Base: Constants/IB_PKEY_BASE_MASK
* NAME
*	IB_PKEY_BASE_MASK
*
* DESCRIPTION
*	Masks for the base P_Key value given a P_Key Entry.
*
* SOURCE
*/
#define IB_PKEY_BASE_MASK					(CL_HTON16(0x7FFF))
/*********/
/****d* IBA Base: Constants/IB_PKEY_TYPE_MASK
* NAME
*	IB_PKEY_TYPE_MASK
*
* DESCRIPTION
*	Masks for the P_Key membership type given a P_Key Entry.
*
* SOURCE
*/
#define IB_PKEY_TYPE_MASK					(CL_HTON16(0x8000))
/*********/
/****d* IBA Base: Constants/IB_DEFAULT_PARTIAL_PKEY
* NAME
*	IB_DEFAULT_PARTIAL_PKEY
*
* DESCRIPTION
*	0x7FFF in network order
*
* SOURCE
*/
#define IB_DEFAULT_PARTIAL_PKEY				       (CL_HTON16(0x7FFF))
/**********/
/****d* IBA Base: Constants/IB_MCLASS_SUBN_LID
* NAME
*	IB_MCLASS_SUBN_LID
*
* DESCRIPTION
*	Subnet Management Class, Subnet Manager LID routed (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_SUBN_LID					0x01
/**********/
/****d* IBA Base: Constants/IB_MCLASS_SUBN_DIR
* NAME
*	IB_MCLASS_SUBN_DIR
*
* DESCRIPTION
*	Subnet Management Class, Subnet Manager directed route (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_SUBN_DIR					0x81
/**********/
/****d* IBA Base: Constants/IB_MCLASS_SUBN_ADM
* NAME
*	IB_MCLASS_SUBN_ADM
*
* DESCRIPTION
*	Management Class, Subnet Administration (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_SUBN_ADM					0x03
/**********/
/****d* IBA Base: Constants/IB_MCLASS_PERF
* NAME
*	IB_MCLASS_PERF
*
* DESCRIPTION
*	Management Class, Performance Management (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_PERF						0x04
/**********/
/****d* IBA Base: Constants/IB_MCLASS_BM
* NAME
*	IB_MCLASS_BM
*
* DESCRIPTION
*	Management Class, Baseboard Management (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_BM						0x05
/**********/
/****d* IBA Base: Constants/IB_MCLASS_DEV_MGMT
* NAME
*	IB_MCLASS_DEV_MGMT
*
* DESCRIPTION
*	Management Class, Device Management (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_DEV_MGMT					0x06
/**********/
/****d* IBA Base: Constants/IB_MCLASS_COMM_MGMT
* NAME
*	IB_MCLASS_COMM_MGMT
*
* DESCRIPTION
*	Management Class, Communication Management (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_COMM_MGMT					0x07
/**********/
/****d* IBA Base: Constants/IB_MCLASS_SNMP
* NAME
*	IB_MCLASS_SNMP
*
* DESCRIPTION
*	Management Class, SNMP Tunneling (13.4.4)
*
* SOURCE
*/
#define IB_MCLASS_SNMP						0x08
/**********/
/****d* IBA Base: Constants/IB_MCLASS_VENDOR_LOW_RANGE_MIN
* NAME
*	IB_MCLASS_VENDOR_LOW_RANGE_MIN
*
* DESCRIPTION
*	Management Class, Vendor Specific Low Range Start
*
* SOURCE
*/
#define IB_MCLASS_VENDOR_LOW_RANGE_MIN 0x09
/**********/
/****d* IBA Base: Constants/IB_MCLASS_VS
* NAME
*	IB_MCLASS_VS
*
* DESCRIPTION
*	Management Class, Vendor Specific
*
* SOURCE
*/
#define IB_MCLASS_VS						0x0A
/**********/
/****d* IBA Base: Constants/IB_MCLASS_N2N
* NAME
*	IB_MCLASS_N2N
*
* DESCRIPTION
*	Management Class, Node to Node
*
* SOURCE
*/
#define IB_MCLASS_N2N						0x0C
/**********/
/****d* IBA Base: Constants/IB_MCLASS_VENDOR_LOW_RANGE_MAX
* NAME
*	IB_MCLASS_VENDOR_LOW_RANGE_MAX
*
* DESCRIPTION
*	Management Class, Vendor Specific Low Range End
*
* SOURCE
*/
#define IB_MCLASS_VENDOR_LOW_RANGE_MAX 0x0f
/**********/
/****d* IBA Base: Constants/IB_MCLASS_DEV_ADM
* NAME
*	IB_MCLASS_DEV_ADM
*
* DESCRIPTION
*	Management Class, Device Administration
*
* SOURCE
*/
#define IB_MCLASS_DEV_ADM 0x10
/**********/
/****d* IBA Base: Constants/IB_MCLASS_BIS
* NAME
*	IB_MCLASS_BIS
*
* DESCRIPTION
*	Management Class, BIS
*
* SOURCE
*/
#define IB_MCLASS_BIS 0x12
/**********/
/****d* IBA Base: Constants/IB_MCLASS_CC
* NAME
*	IB_MCLASS_CC
*
* DESCRIPTION
*	Management Class, Congestion Control (A10.4.1)
*
* SOURCE
*/
#define IB_MCLASS_CC 0x21
/**********/
/****d* IBA Base: Constants/IB_MCLASS_VENDOR_HIGH_RANGE_MIN
* NAME
*	IB_MCLASS_VENDOR_HIGH_RANGE_MIN
*
* DESCRIPTION
*	Management Class, Vendor Specific High Range Start
*
* SOURCE
*/
#define IB_MCLASS_VENDOR_HIGH_RANGE_MIN 0x30
/**********/
/****d* IBA Base: Constants/IB_MCLASS_VENDOR_HIGH_RANGE_MAX
* NAME
*	IB_MCLASS_VENDOR_HIGH_RANGE_MAX
*
* DESCRIPTION
*	Management Class, Vendor Specific High Range End
*
* SOURCE
*/
#define IB_MCLASS_VENDOR_HIGH_RANGE_MAX 0x4f
/**********/
/****f* IBA Base: Types/ib_class_is_vendor_specific_low
* NAME
*	ib_class_is_vendor_specific_low
*
* DESCRIPTION
*	Indicates if the Class Code if a vendor specific class from
*  the low range
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_class_is_vendor_specific_low(IN const uint8_t class_code)
{
	return ((class_code >= IB_MCLASS_VENDOR_LOW_RANGE_MIN) &&
		(class_code <= IB_MCLASS_VENDOR_LOW_RANGE_MAX));
}

/*
* PARAMETERS
*	class_code
*		[in] The Management Datagram Class Code
*
* RETURN VALUE
*	TRUE if the class is in the Low range of Vendor Specific MADs
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
* IB_MCLASS_VENDOR_LOW_RANGE_MIN, IB_MCLASS_VENDOR_LOW_RANGE_MAX
*********/

/****f* IBA Base: Types/ib_class_is_vendor_specific_high
* NAME
*	ib_class_is_vendor_specific_high
*
* DESCRIPTION
*	Indicates if the Class Code if a vendor specific class from
*  the high range
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_class_is_vendor_specific_high(IN const uint8_t class_code)
{
	return ((class_code >= IB_MCLASS_VENDOR_HIGH_RANGE_MIN) &&
		(class_code <= IB_MCLASS_VENDOR_HIGH_RANGE_MAX));
}

/*
* PARAMETERS
*	class_code
*		[in] The Management Datagram Class Code
*
* RETURN VALUE
*	TRUE if the class is in the High range of Vendor Specific MADs
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
* IB_MCLASS_VENDOR_HIGH_RANGE_MIN, IB_MCLASS_VENDOR_HIGH_RANGE_MAX
*********/

/****f* IBA Base: Types/ib_class_is_vendor_specific
* NAME
*	ib_class_is_vendor_specific
*
* DESCRIPTION
*	Indicates if the Class Code if a vendor specific class
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_class_is_vendor_specific(IN const uint8_t class_code)
{
	return (ib_class_is_vendor_specific_low(class_code) ||
		ib_class_is_vendor_specific_high(class_code));
}

/*
* PARAMETERS
*	class_code
*		[in] The Management Datagram Class Code
*
* RETURN VALUE
*	TRUE if the class is a Vendor Specific MAD
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*  ib_class_is_vendor_specific_low, ib_class_is_vendor_specific_high
*********/

/****f* IBA Base: Types/ib_class_is_rmpp
* NAME
*	ib_class_is_rmpp
*
* DESCRIPTION
*	Indicates if the Class Code supports RMPP
*
* SYNOPSIS
*/
static inline boolean_t OSM_API ib_class_is_rmpp(IN const uint8_t class_code)
{
	return ((class_code == IB_MCLASS_SUBN_ADM) ||
		(class_code == IB_MCLASS_DEV_MGMT) ||
		(class_code == IB_MCLASS_DEV_ADM) ||
		(class_code == IB_MCLASS_BIS) ||
		ib_class_is_vendor_specific_high(class_code));
}

/*
* PARAMETERS
*	class_code
*		[in] The Management Datagram Class Code
*
* RETURN VALUE
*	TRUE if the class supports RMPP
*	FALSE otherwise.
*
* NOTES
*
*********/

/*
 *	MAD methods
 */

/****d* IBA Base: Constants/IB_MAX_METHOD
* NAME
*	IB_MAX_METHOD
*
* DESCRIPTION
*	Total number of methods available to a class, not including the R-bit.
*
* SOURCE
*/
#define IB_MAX_METHODS						128
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_RESP_MASK
* NAME
*	IB_MAD_METHOD_RESP_MASK
*
* DESCRIPTION
*	Response mask to extract 'R' bit from the method field. (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_RESP_MASK				0x80
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_GET
* NAME
*	IB_MAD_METHOD_GET
*
* DESCRIPTION
*	Get() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_GET					0x01
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_SET
* NAME
*	IB_MAD_METHOD_SET
*
* DESCRIPTION
*	Set() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_SET					0x02
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_GET_RESP
* NAME
*	IB_MAD_METHOD_GET_RESP
*
* DESCRIPTION
*	GetResp() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_GET_RESP				0x81
/**********/

#define IB_MAD_METHOD_DELETE				0x15

/****d* IBA Base: Constants/IB_MAD_METHOD_GETTABLE
* NAME
*	IB_MAD_METHOD_GETTABLE
*
* DESCRIPTION
*	SubnAdmGetTable() Method (15.2.2)
*
* SOURCE
*/
#define IB_MAD_METHOD_GETTABLE				0x12
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_GETTABLE_RESP
* NAME
*	IB_MAD_METHOD_GETTABLE_RESP
*
* DESCRIPTION
*	SubnAdmGetTableResp() Method (15.2.2)
*
* SOURCE
*/
#define IB_MAD_METHOD_GETTABLE_RESP			0x92

/**********/

#define IB_MAD_METHOD_GETTRACETABLE			0x13
#define IB_MAD_METHOD_GETMULTI				0x14
#define IB_MAD_METHOD_GETMULTI_RESP			0x94

/****d* IBA Base: Constants/IB_MAD_METHOD_SEND
* NAME
*	IB_MAD_METHOD_SEND
*
* DESCRIPTION
*	Send() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_SEND					0x03
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_TRAP
* NAME
*	IB_MAD_METHOD_TRAP
*
* DESCRIPTION
*	Trap() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_TRAP					0x05
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_REPORT
* NAME
*	IB_MAD_METHOD_REPORT
*
* DESCRIPTION
*	Report() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_REPORT				0x06
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_REPORT_RESP
* NAME
*	IB_MAD_METHOD_REPORT_RESP
*
* DESCRIPTION
*	ReportResp() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_REPORT_RESP			0x86
/**********/

/****d* IBA Base: Constants/IB_MAD_METHOD_TRAP_REPRESS
* NAME
*	IB_MAD_METHOD_TRAP_REPRESS
*
* DESCRIPTION
*	TrapRepress() Method (13.4.5)
*
* SOURCE
*/
#define IB_MAD_METHOD_TRAP_REPRESS			0x07
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_BUSY
* NAME
*	IB_MAD_STATUS_BUSY
*
* DESCRIPTION
*	Temporarily busy, MAD discarded (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_BUSY				(CL_HTON16(0x0001))
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_REDIRECT
* NAME
*	IB_MAD_STATUS_REDIRECT
*
* DESCRIPTION
*	QP Redirection required (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_REDIRECT				(CL_HTON16(0x0002))
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_UNSUP_CLASS_VER
* NAME
*	IB_MAD_STATUS_UNSUP_CLASS_VER
*
* DESCRIPTION
*	Unsupported class version (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_UNSUP_CLASS_VER			(CL_HTON16(0x0004))
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_UNSUP_METHOD
* NAME
*	IB_MAD_STATUS_UNSUP_METHOD
*
* DESCRIPTION
*	Unsupported method (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_UNSUP_METHOD			(CL_HTON16(0x0008))
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_UNSUP_METHOD_ATTR
* NAME
*	IB_MAD_STATUS_UNSUP_METHOD_ATTR
*
* DESCRIPTION
*	Unsupported method/attribute combination (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_UNSUP_METHOD_ATTR			(CL_HTON16(0x000C))
/**********/

/****d* IBA Base: Constants/IB_MAD_STATUS_INVALID_FIELD
* NAME
*	IB_MAD_STATUS_INVALID_FIELD
*
* DESCRIPTION
*	Attribute contains one or more invalid fields (13.4.7)
*
* SOURCE
*/
#define IB_MAD_STATUS_INVALID_FIELD			(CL_HTON16(0x001C))
/**********/

#define IB_MAD_STATUS_CLASS_MASK			(CL_HTON16(0xFF00))

#define IB_SA_MAD_STATUS_SUCCESS			(CL_HTON16(0x0000))
#define IB_SA_MAD_STATUS_NO_RESOURCES			(CL_HTON16(0x0100))
#define IB_SA_MAD_STATUS_REQ_INVALID			(CL_HTON16(0x0200))
#define IB_SA_MAD_STATUS_NO_RECORDS			(CL_HTON16(0x0300))
#define IB_SA_MAD_STATUS_TOO_MANY_RECORDS		(CL_HTON16(0x0400))
#define IB_SA_MAD_STATUS_INVALID_GID			(CL_HTON16(0x0500))
#define IB_SA_MAD_STATUS_INSUF_COMPS			(CL_HTON16(0x0600))
#define IB_SA_MAD_STATUS_DENIED				(CL_HTON16(0x0700))
#define IB_SA_MAD_STATUS_PRIO_SUGGESTED			(CL_HTON16(0x0800))

#define IB_DM_MAD_STATUS_NO_IOC_RESP			(CL_HTON16(0x0100))
#define IB_DM_MAD_STATUS_NO_SVC_ENTRIES			(CL_HTON16(0x0200))
#define IB_DM_MAD_STATUS_IOC_FAILURE			(CL_HTON16(0x8000))

/****d* IBA Base: Constants/IB_MAD_ATTR_CLASS_PORT_INFO
* NAME
*	IB_MAD_ATTR_CLASS_PORT_INFO
*
* DESCRIPTION
*	ClassPortInfo attribute (13.4.8)
*
* SOURCE
*/
#define IB_MAD_ATTR_CLASS_PORT_INFO			(CL_HTON16(0x0001))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NOTICE
* NAME
*	IB_MAD_ATTR_NOTICE
*
* DESCRIPTION
*	Notice attribute (13.4.8)
*
* SOURCE
*/
#define IB_MAD_ATTR_NOTICE					(CL_HTON16(0x0002))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_INFORM_INFO
* NAME
*	IB_MAD_ATTR_INFORM_INFO
*
* DESCRIPTION
*	InformInfo attribute (13.4.8)
*
* SOURCE
*/
#define IB_MAD_ATTR_INFORM_INFO				(CL_HTON16(0x0003))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NODE_DESC
* NAME
*	IB_MAD_ATTR_NODE_DESC
*
* DESCRIPTION
*	NodeDescription attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_NODE_DESC				(CL_HTON16(0x0010))

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_SMPL_CTRL
* NAME
*	IB_MAD_ATTR_PORT_SMPL_CTRL
*
* DESCRIPTION
*	PortSamplesControl attribute (16.1.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_SMPL_CTRL			(CL_HTON16(0x0010))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NODE_INFO
* NAME
*	IB_MAD_ATTR_NODE_INFO
*
* DESCRIPTION
*	NodeInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_NODE_INFO				(CL_HTON16(0x0011))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_SMPL_RSLT
* NAME
*	IB_MAD_ATTR_PORT_SMPL_RSLT
*
* DESCRIPTION
*	PortSamplesResult attribute (16.1.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_SMPL_RSLT			(CL_HTON16(0x0011))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SWITCH_INFO
* NAME
*	IB_MAD_ATTR_SWITCH_INFO
*
* DESCRIPTION
*	SwitchInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SWITCH_INFO				(CL_HTON16(0x0012))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_CNTRS
* NAME
*	IB_MAD_ATTR_PORT_CNTRS
*
* DESCRIPTION
*	PortCounters attribute (16.1.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_CNTRS				(CL_HTON16(0x0012))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_CNTRS_EXT
* NAME
*       IB_MAD_ATTR_PORT_CNTRS_EXT
*
* DESCRIPTION
*       PortCountersExtended attribute (16.1.4)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_CNTRS_EXT			(CL_HTON16(0x001D))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_XMIT_DATA_SL
* NAME
*	IB_MAD_ATTR_PORT_XMIT_DATA_SL
*
* DESCRIPTION
*	PortXmitDataSL attribute (A13.6.4)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_XMIT_DATA_SL			(CL_HTON16(0x0036))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_RCV_DATA_SL
* NAME
*	IB_MAD_ATTR_PORT_RCV_DATA_SL
*
* DESCRIPTION
*	PortRcvDataSL attribute (A13.6.4)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_RCV_DATA_SL			(CL_HTON16(0x0037))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_GUID_INFO
* NAME
*	IB_MAD_ATTR_GUID_INFO
*
* DESCRIPTION
*	GUIDInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_GUID_INFO				(CL_HTON16(0x0014))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_INFO
* NAME
*	IB_MAD_ATTR_PORT_INFO
*
* DESCRIPTION
*	PortInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_INFO				(CL_HTON16(0x0015))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_P_KEY_TABLE
* NAME
*	IB_MAD_ATTR_P_KEY_TABLE
*
* DESCRIPTION
*	PartitionTable attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_P_KEY_TABLE				(CL_HTON16(0x0016))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SLVL_TABLE
* NAME
*	IB_MAD_ATTR_SLVL_TABLE
*
* DESCRIPTION
*	SL VL Mapping Table attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SLVL_TABLE				(CL_HTON16(0x0017))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VL_ARBITRATION
* NAME
*	IB_MAD_ATTR_VL_ARBITRATION
*
* DESCRIPTION
*	VL Arbitration Table attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_VL_ARBITRATION			(CL_HTON16(0x0018))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_LIN_FWD_TBL
* NAME
*	IB_MAD_ATTR_LIN_FWD_TBL
*
* DESCRIPTION
*	Switch linear forwarding table
*
* SOURCE
*/
#define IB_MAD_ATTR_LIN_FWD_TBL				(CL_HTON16(0x0019))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RND_FWD_TBL
* NAME
*	IB_MAD_ATTR_RND_FWD_TBL
*
* DESCRIPTION
*	Switch random forwarding table
*
* SOURCE
*/
#define IB_MAD_ATTR_RND_FWD_TBL				(CL_HTON16(0x001A))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MCAST_FWD_TBL
* NAME
*	IB_MAD_ATTR_MCAST_FWD_TBL
*
* DESCRIPTION
*	Switch multicast forwarding table
*
* SOURCE
*/
#define IB_MAD_ATTR_MCAST_FWD_TBL			(CL_HTON16(0x001B))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_HIERARCHY_INFO
* NAME
*	IB_MAD_ATTR_HIERARCHY_INFO
*
* DESCRIPTION
*	Port Hierarchy Information
*
* SOURCE
*/
#define IB_MAD_ATTR_HIERARCHY_INFO			(CL_HTON16(0x001E))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NODE_RECORD
* NAME
*	IB_MAD_ATTR_NODE_RECORD
*
* DESCRIPTION
*	NodeRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_NODE_RECORD				(CL_HTON16(0x0011))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORTINFO_RECORD
* NAME
*	IB_MAD_ATTR_PORTINFO_RECORD
*
* DESCRIPTION
*	PortInfoRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORTINFO_RECORD			(CL_HTON16(0x0012))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SWITCH_INFO_RECORD
* NAME
*       IB_MAD_ATTR_SWITCH_INFO_RECORD
*
* DESCRIPTION
*       SwitchInfoRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SWITCH_INFO_RECORD			(CL_HTON16(0x0014))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_LINK_RECORD
* NAME
*	IB_MAD_ATTR_LINK_RECORD
*
* DESCRIPTION
*	LinkRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_LINK_RECORD				(CL_HTON16(0x0020))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SM_INFO
* NAME
*	IB_MAD_ATTR_SM_INFO
*
* DESCRIPTION
*	SMInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SM_INFO				(CL_HTON16(0x0020))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SMINFO_RECORD
* NAME
*	IB_MAD_ATTR_SMINFO_RECORD
*
* DESCRIPTION
*	SMInfoRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SMINFO_RECORD			(CL_HTON16(0x0018))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_GUIDINFO_RECORD
* NAME
*       IB_MAD_ATTR_GUIDINFO_RECORD
*
* DESCRIPTION
*       GuidInfoRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_GUIDINFO_RECORD			(CL_HTON16(0x0030))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VENDOR_DIAG
* NAME
*	IB_MAD_ATTR_VENDOR_DIAG
*
* DESCRIPTION
*	VendorDiag attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_VENDOR_DIAG				(CL_HTON16(0x0030))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_LED_INFO
* NAME
*	IB_MAD_ATTR_LED_INFO
*
* DESCRIPTION
*	LedInfo attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_LED_INFO				(CL_HTON16(0x0031))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MLNX_EXTENDED_PORT_INFO
* NAME
*	IB_MAD_ATTR_MLNX_EXTENDED_PORT_INFO
*
* DESCRIPTION
*	Vendor specific SM attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_MLNX_EXTENDED_PORT_INFO		(CL_HTON16(0xFF90))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MLNX_EXTENDED_NODE_INFO
* NAME
*	IB_MAD_ATTR_MLNX_EXTENDED_NODE_INFO
*
* DESCRIPTION
*	Vendor specific SM attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_MLNX_EXTENDED_NODE_INFO		(CL_HTON16(0xFF91))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_EXTENDED_SWITCH_INFO
* NAME
*	IB_MAD_ATTR_EXTENDED_SWITCH_INFO
*
* DESCRIPTION
*	Vendor specific SM attribute (VS MAD Spec 2.15)
*
* SOURCE
*/
#define IB_MAD_ATTR_EXTENDED_SWITCH_INFO		(CL_HTON16(0xFF92))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SERVICE_RECORD
* NAME
*	IB_MAD_ATTR_SERVICE_RECORD
*
* DESCRIPTION
*	ServiceRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SERVICE_RECORD			(CL_HTON16(0x0031))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_LFT_RECORD
* NAME
*	IB_MAD_ATTR_LFT_RECORD
*
* DESCRIPTION
*	LinearForwardingTableRecord attribute (15.2.5.6)
*
* SOURCE
*/
#define IB_MAD_ATTR_LFT_RECORD				(CL_HTON16(0x0015))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MFT_RECORD
* NAME
*       IB_MAD_ATTR_MFT_RECORD
*
* DESCRIPTION
*       MulticastForwardingTableRecord attribute (15.2.5.8)
*
* SOURCE
*/
#define IB_MAD_ATTR_MFT_RECORD				(CL_HTON16(0x0017))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PKEYTBL_RECORD
* NAME
*	IB_MAD_ATTR_PKEYTBL_RECORD
*
* DESCRIPTION
*	PKEY Table Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_PKEY_TBL_RECORD			(CL_HTON16(0x0033))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PATH_RECORD
* NAME
*	IB_MAD_ATTR_PATH_RECORD
*
* DESCRIPTION
*	PathRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_PATH_RECORD				(CL_HTON16(0x0035))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VLARB_RECORD
* NAME
*	IB_MAD_ATTR_VLARB_RECORD
*
* DESCRIPTION
*	VL Arbitration Table Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_VLARB_RECORD			(CL_HTON16(0x0036))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SLVL_RECORD
* NAME
*	IB_MAD_ATTR_SLVL_RECORD
*
* DESCRIPTION
*	SLtoVL Mapping Table Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SLVL_RECORD				(CL_HTON16(0x0013))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MCMEMBER_RECORD
* NAME
*	IB_MAD_ATTR_MCMEMBER_RECORD
*
* DESCRIPTION
*	MCMemberRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_MCMEMBER_RECORD			(CL_HTON16(0x0038))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_TRACE_RECORD
* NAME
*	IB_MAD_ATTR_TRACE_RECORD
*
* DESCRIPTION
*	TraceRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_TRACE_RECORD			(CL_HTON16(0x0039))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_MULTIPATH_RECORD
* NAME
*	IB_MAD_ATTR_MULTIPATH_RECORD
*
* DESCRIPTION
*	MultiPathRecord attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_MULTIPATH_RECORD			(CL_HTON16(0x003A))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SVC_ASSOCIATION_RECORD
* NAME
*	IB_MAD_ATTR_SVC_ASSOCIATION_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SVC_ASSOCIATION_RECORD		(CL_HTON16(0x003B))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_QOS_ARBINFO_RECORD
* NAME
*	IB_MAD_ATTR_ENH_QOS_ARBINFO_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_QOS_ARBINFO_RECORD		(CL_HTON16(0x0050))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_VL_GROUPS_ARBITER_RECORD
* NAME
*	IB_MAD_ATTR_ENH_VL_GROUPS_ARBITER_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_VLGROUPS_ARBITER_RECORD		(CL_HTON16(0x0051))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_VL_ARBITER_RECORD
* NAME
*	IB_MAD_ATTR_ENH_VL_ARBITER_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_VL_ARBITER_RECORD		(CL_HTON16(0x0052))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_SL_ARBITER_RECORD
* NAME
*	IB_MAD_ATTR_ENH_SL_ARBITER_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_SL_ARBITER_RECORD		(CL_HTON16(0x0053))

/****d* IBA Base: Constants/IB_MAD_ATTR_INFORM_INFO_RECORD
* NAME
*	IB_MAD_ATTR_INFORM_INFO_RECORD
*
* DESCRIPTION
*	InformInfo Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_INFORM_INFO_RECORD			(CL_HTON16(0x00F3))

/****d* IBA Base: Constants/IB_MAD_ATTR_IO_UNIT_INFO
* NAME
*	IB_MAD_ATTR_IO_UNIT_INFO
*
* DESCRIPTION
*	IOUnitInfo attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_IO_UNIT_INFO			(CL_HTON16(0x0010))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_IO_CONTROLLER_PROFILE
* NAME
*	IB_MAD_ATTR_IO_CONTROLLER_PROFILE
*
* DESCRIPTION
*	IOControllerProfile attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_IO_CONTROLLER_PROFILE	(CL_HTON16(0x0011))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SERVICE_ENTRIES
* NAME
*	IB_MAD_ATTR_SERVICE_ENTRIES
*
* DESCRIPTION
*	ServiceEntries attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_SERVICE_ENTRIES			(CL_HTON16(0x0012))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_DIAGNOSTIC_TIMEOUT
* NAME
*	IB_MAD_ATTR_DIAGNOSTIC_TIMEOUT
*
* DESCRIPTION
*	DiagnosticTimeout attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_DIAGNOSTIC_TIMEOUT		(CL_HTON16(0x0020))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PREPARE_TO_TEST
* NAME
*	IB_MAD_ATTR_PREPARE_TO_TEST
*
* DESCRIPTION
*	PrepareToTest attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_PREPARE_TO_TEST			(CL_HTON16(0x0021))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_TEST_DEVICE_ONCE
* NAME
*	IB_MAD_ATTR_TEST_DEVICE_ONCE
*
* DESCRIPTION
*	TestDeviceOnce attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_TEST_DEVICE_ONCE		(CL_HTON16(0x0022))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_TEST_DEVICE_LOOP
* NAME
*	IB_MAD_ATTR_TEST_DEVICE_LOOP
*
* DESCRIPTION
*	TestDeviceLoop attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_TEST_DEVICE_LOOP		(CL_HTON16(0x0023))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_DIAG_CODE
* NAME
*	IB_MAD_ATTR_DIAG_CODE
*
* DESCRIPTION
*	DiagCode attribute (16.3.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_DIAG_CODE				(CL_HTON16(0x0024))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SVC_ASSOCIATION_RECORD
* NAME
*	IB_MAD_ATTR_SVC_ASSOCIATION_RECORD
*
* DESCRIPTION
*	Service Association Record attribute (15.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SVC_ASSOCIATION_RECORD	(CL_HTON16(0x003B))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CONG_INFO
* NAME
*	IB_MAD_ATTR_CONG_INFO
*
* DESCRIPTION
*	CongestionInfo attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_CONG_INFO				(CL_HTON16(0x0011))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CONG_KEY_INFO
* NAME
*	IB_MAD_ATTR_CONG_KEY_INFO
*
* DESCRIPTION
*	CongestionKeyInfo attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_CONG_KEY_INFO			(CL_HTON16(0x0012))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CONG_LOG
* NAME
*	IB_MAD_ATTR_CONG_LOG
*
* DESCRIPTION
*	CongestionLog attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_CONG_LOG				(CL_HTON16(0x0013))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SW_CONG_SETTING
* NAME
*	IB_MAD_ATTR_SW_CONG_SETTING
*
* DESCRIPTION
*	SwitchCongestionSetting attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_SW_CONG_SETTING			(CL_HTON16(0x0014))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SW_PORT_CONG_SETTING
* NAME
*	IB_MAD_ATTR_SW_PORT_CONG_SETTING
*
* DESCRIPTION
*	SwitchPortCongestionSetting attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_SW_PORT_CONG_SETTING		(CL_HTON16(0x0015))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CA_CONG_SETTING
* NAME
*	IB_MAD_ATTR_CA_CONG_SETTING
*
* DESCRIPTION
*	CACongestionSetting attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_CA_CONG_SETTING			(CL_HTON16(0x0016))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_TBL
* NAME
*	IB_MAD_ATTR_CC_TBL
*
* DESCRIPTION
*	CongestionControlTable attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_TBL				(CL_HTON16(0x0017))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_ENH_INFO
* NAME
*	IB_MAD_ATTR_CC_ENH_INFO
*
* DESCRIPTION
*	EnhancedCongestionInfo attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_ENH_INFO				(CL_HTON16(0xFF00))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_SW_GENERAL_SETTINGS
* NAME
*	IB_MAD_ATTR_CC_SW_GENERAL_SETTINGS
*
* DESCRIPTION
*	CongestionSwitchGeneralSettings attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_SW_GENERAL_SETTINGS		(CL_HTON16(0xFF08))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_PORT_SW_PROFILE_SETTINGS
* NAME
*	IB_MAD_ATTR_CC_PORT_SW_PROFILE_SETTINGS
*
* DESCRIPTION
*	CongestionPortProfileSettings attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_PORT_SW_PROFILE_SETTINGS		(CL_HTON16(0xFF09))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_SL_MAPPING_SETTINGS
* NAME
*	IB_MAD_ATTR_CC_SL_MAPPING_SETTINGS
*
* DESCRIPTION
*	CongestionSLMappingSettings attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_SL_MAPPING_SETTINGS		(CL_HTON16(0xFF10))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_HCA_GENERAL_SETTINGS
* NAME
*	IB_MAD_ATTR_CC_HCA_GENERAL_SETTINGS
*
* DESCRIPTION
*	CongestionHCAGeneralSettings attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_HCA_GENERAL_SETTINGS		(CL_HTON16(0xFF20))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_HCA_RP_PARAMETERS
* NAME
*	IB_MAD_ATTR_CC_HCA_RP_PARAMETERS
*
* DESCRIPTION
*	CongestionHCARPParameters attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_HCA_RP_PARAMETERS		(CL_HTON16(0xFF21))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CC_HCA_NP_PARAMETERS
* NAME
*	IB_MAD_ATTR_CC_HCA_NP_PARAMETERS
*
* DESCRIPTION
*	CongestionHCANPParameters attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_CC_HCA_NP_PARAMETERS		(CL_HTON16(0xFF22))

/****d* IBA Base: Constants/IB_MAD_ATTR_PPCC_HCA_ALGO_CONFIG
* NAME
*	IB_MAD_ATTR_PPCC_HCA_ALGO_CONFIG
*
* DESCRIPTION
*	CongestionHCAAlgoConfig attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_PPCC_HCA_ALGO_CONFIG		(CL_HTON16(0xFF24))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PPCC_HCA_ALGO_PARAMS_CONFIG
* NAME
*	IB_MAD_ATTR_PPCC_HCA_ALGO_PARAMS_CONFIG
*
* DESCRIPTION
*	CongestionHCAConfigParams attribute ()
*
* SOURCE
*/
#define IB_MAD_ATTR_PPCC_HCA_ALGO_PARAMS_CONFIG		(CL_HTON16(0xFF25))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_TIME_STAMP
* NAME
*	IB_MAD_ATTR_TIME_STAMP
*
* DESCRIPTION
*	TimeStamp attribute (A10.4.3)
*
* SOURCE
*/
#define IB_MAD_ATTR_TIME_STAMP				(CL_HTON16(0x0018))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_SWITCH_PORT_STATE_TBL
* NAME
*	IB_MAD_ATTR_SWITCH_PORT_STATE_TBL
*
* DESCRIPTION
*	SwitchPortStateTable attribute (14.2.5)
*
* SOURCE
*/
#define IB_MAD_ATTR_SWITCH_PORT_STATE_TBL		(CL_HTON16(0x0034))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_QOS_ARBINFO
* NAME
*	IB_MAD_ATTR_ENH_QOS_ARBINFO
*
* DESCRIPTION
*	EnqQosArbiterInfo attribute (14.2.6.21)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_QOS_ARBINFO			(CL_HTON16(0x0050))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_PORT_VL_ARBITER
* NAME
*	IB_MAD_ATTR_ENH_PORT_VL_ARBITER
*
* DESCRIPTION
*	EnhPortVLAribiter attribute (14.2.6.22)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_PORT_VL_ARBITER			(CL_HTON16(0x0051))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ENH_SL_ARBITER
* NAME
*	IB_MAD_ATTR_ENH_SL_ARBITER
*
* DESCRIPTION
*	EnhSLArbiter attribute (14.2.6.23)
*
* SOURCE
*/
#define IB_MAD_ATTR_ENH_SL_ARBITER			(CL_HTON16(0x0052))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VPORT_ENH_QOS_ARBITER_INFO
* NAME
*	IB_MAD_ATTR_VPORT_ENH_QOS_ARBITER_INFO
*
* DESCRIPTION
*	VPortEnhQoSArbiterInfo attribute (18.4.2.9)
*
* SOURCE
*/
#define IB_MAD_ATTR_VPORT_ENH_QOS_ARBITER_INFO		(CL_HTON16(0x0053))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VPORT_ENH_SL_ARBITER
* NAME
*	IB_MAD_ATTR_VPORT_ENH_SL_ARBITER
*
* DESCRIPTION
*	VPortEnhSLArbiterInfo attribute (18.4.2.10)
*
* SOURCE
*/
#define IB_MAD_ATTR_VPORT_ENH_SL_ARBITER		(CL_HTON16(0x0054))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VPORT_INFO
* NAME
*	IB_MAD_ATTR_VPORT_INFO
*
* DESCRIPTION
*	VPort info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VPORT_INFO				(CL_HTON16(0xFFB1))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VIRTUALIZATION_INFO
* NAME
*	IB_MAD_ATTR_VIRTUALIZATION_INFO
*
* DESCRIPTION
*	Virtualization info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VIRTUALIZATION_INFO				(CL_HTON16(0xFFB0))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VPORT_STATE
* NAME
*	IB_MAD_ATTR_VPORT_STATE
*
* DESCRIPTION
*	VPort state attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VPORT_STATE				(CL_HTON16(0xFFB3))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VPORT_GUID_INFO
* NAME
*	IB_MAD_ATTR_VPORT_GUID_INFO
*
* DESCRIPTION
*	VPort Guid Info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VPORT_GUID_INFO			(CL_HTON16(0xFFB5))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PLFT_INFO
* NAME
*	IB_MAD_ATTR_PLFT_INFO
*
* DESCRIPTION
*	PrivateLFTInfo attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PLFT_INFO					(CL_HTON16(0xFF10))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PLFT_DEF
* NAME
*	IB_MAD_ATTR_PLFT_DEF
*
* DESCRIPTION
*	PrivateLFTDef attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PLFT_DEF					(CL_HTON16(0xFF11))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PLFT_MAP
* NAME
*	IB_MAD_ATTR_PLFT_MAP
*
* DESCRIPTION
*	PrivateLFTMap attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PLFT_MAP					(CL_HTON16(0xFF12))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_SL_TO_PLFT_MAP
* NAME
*	IB_MAD_ATTR_PORT_SL_TO_PLFT_MAP
*
* DESCRIPTION
*	PortSLToPrivateLFTMap attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_SL_TO_PLFT_MAP				(CL_HTON16(0xFF14))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_GENERAL_INFO_
* NAME
*	IB_MAD_ATTR_GENERAL_INFO
*
* DESCRIPTION
*	General Info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_GENERAL_INFO				(CL_HTON16(0xFF17))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CHASSIS_INFO
* NAME
*	IB_MAD_ATTR_CHASSIS_INFO
*
* DESCRIPTION
*	Chassis Info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_CHASSIS_INFO				(CL_HTON16(0xFF18))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ADAPTIVE_ROUTING_INFO
* NAME
*	IB_MAD_ATTR_ADAPTIVE_ROUTING_INFO
*
* DESCRIPTION
*	Adaptive Routing Info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_ADAPTIVE_ROUTING_INFO		(CL_HTON16(0xFF20))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE
* NAME
*	IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE
*
* DESCRIPTION
*	Adaptive Routing Group Table attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE	(CL_HTON16(0xFF21))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ADAPTIVE_ROUTING_LFT
* NAME
*	IB_MAD_ATTR_ADAPTIVE_ROUTING_LFT
*
* DESCRIPTION
*	Adaptive Routing Group LFT attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_ADAPTIVE_ROUTING_LFT		(CL_HTON16(0xFF23))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_HBF_CONFIG
* NAME
*	IB_MAD_ATTR_HBF_CONFIG
*
* DESCRIPTION
*	HBF Config attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_HBF_CONFIG				(CL_HTON16(0xFF24))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_WHBF_CONFIG
* NAME
*	IB_MAD_ATTR_WHBF_CONFIG
*
* DESCRIPTION
*	Weighted Hash Based Forwrding Config attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_WHBF_CONFIG				(CL_HTON16(0xFF25))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_LFT_SPLIT
* NAME
*	IB_MAD_ATTR_LFT_SPLIT
*
* DESCRIPTION
*	Linear forwarding table split configuration attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_LFT_SPLIT				(CL_HTON16(0xFF26))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NVL_HBF_CONFIG
* NAME
*	IB_MAD_ATTR_NVL_HBF_CONFIG
*
* DESCRIPTION
*	NVLink Hash Based Forwrding Config attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_NVL_HBF_CONFIG			(CL_HTON16(0xFF27))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_END_PORT_PLANE_FILTER_CONFIG
* NAME
*	IB_MAD_ATTR_END_PORT_PLANE_FILTER_CONFIG
*
* DESCRIPTION
*	End-port plane filter config attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_END_PORT_PLANE_FILTER_CONFIG	(CL_HTON16(0xFF54))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PFRN_CONFIG
* NAME
*	IB_MAD_ATTR_PFRN_CONFIG
*
* DESCRIPTION
*	pFRN Configuration attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PFRN_CONFIG				(CL_HTON16(0xFF61))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CREDIT_WATCHDOG_CONFIG
* NAME
*	IB_MAD_ATTR_CREDIT_WATCHDOG_CONFIG
*
* DESCRIPTION
*	Credit Watchdog Configuration attribute
*
* SOURCE
*/
#define IB_MAD_ATTR_CREDIT_WATCHDOG_CONFIG		(CL_HTON16(0xFF71))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_BER_CONFIG
* NAME
*	IB_MAD_ATTR_BER_CONFIG
*
* DESCRIPTION
*	Port BER Configuration attribute
*
* SOURCE
*/
#define IB_MAD_ATTR_BER_CONFIG				(CL_HTON16(0xFF72))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PROFILES_CONFIG
* NAME
*	IB_MAD_ATTR_PROFILES_CONFIG
*
* DESCRIPTION
*	Port Profiles Configuration attribute
*
* SOURCE
*/
#define IB_MAD_ATTR_PROFILES_CONFIG			(CL_HTON16(0xFF73))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CONTAIN_AND_DRAIN_INFO
* NAME
*	IB_MAD_ATTR_CONTAIN_AND_DRAIN_INFO
*
* DESCRIPTION
*	Contain And Drain Configuration attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_CONTAIN_AND_DRAIN_INFO		(CL_HTON16(0xFF87))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_CONTAIN_AND_DRAIN_PORT_STATE
* NAME
*	IB_MAD_ATTR_CONTAIN_AND_DRAIN_PORT_STATE
*
* DESCRIPTION
*	Contain And Drain Configuration attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_CONTAIN_AND_DRAIN_PORT_STATE	(CL_HTON16(0xFF88))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ALID_INFO
* NAME
*	IB_MAD_ATTR_ALID_INFO
*
* DESCRIPTION
*	GPU Anycast LID Configuration attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_ALID_INFO				(CL_HTON16(0xFF89))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RAIL_FILTER_CONFIG
* NAME
*	IB_MAD_ATTR_RAIL_FILTER_CONFIG
*
* DESCRIPTION
*	Rail Filter Configuration attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_RAIL_FILTER_CONFIG			(CL_HTON16(0xFF8A))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_FABRIC_MANAGER_INFO
* NAME
*	IB_MAD_ATTR_FABRIC_MANAGER_INFO
*
* DESCRIPTION
*	Fabric manager info attribute for NVLink
*
* SOURCE
*/
#define IB_MAD_ATTR_FABRIC_MANAGER_INFO			(CL_HTON16(0xFF8E))

/****d* IBA Base: Constants/IB_MAD_ATTR_RN_GEN_STRING_TABLE
* NAME
*	IB_MAD_ATTR_RN_GEN_STRING_TABLE
*
* DESCRIPTION
*	Routing Notification Gen String Table attribute	(vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RN_GEN_STRING_TABLE			(CL_HTON16(0xFFB8))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RN_RCV_STRING_TABLE
* NAME
*	IB_MAD_ATTR_RN_RCV_STRING_TABLE
*
* DESCRIPTION
*	Routing Notification Rcv String Table attribute	(vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RN_RCV_STRING_TABLE			(CL_HTON16(0xFFB9))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RN_SUB_GROUP_DIRECTION_TABLE
* NAME
*	IB_MAD_ATTR_RN_SUB_GROUP_DIRECTION_TABLE
*
* DESCRIPTION
*	Routing Notification Sub Group Direction Table attribute
*	(vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RN_SUB_GROUP_DIRECTION_TABLE	(CL_HTON16(0xFFBA))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RN_GEN_BY_SUB_GROUP_PRIORITY
* NAME
*	IB_MAD_ATTR_RN_GEN_BY_SUB_GROUP_PRIORITY
*
* DESCRIPTION
*	Routing Notification Sub By Group Priority attribute
*	(vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RN_GEN_BY_SUB_GROUP_PRIORITY	(CL_HTON16(0xFFBE))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RN_XMIT_PORT_MASK
* NAME
*	IB_MAD_ATTR_RN_XMIT_PORT_MASK
*
* DESCRIPTION
*	Routing Notification Xmit Port Mask attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RN_XMIT_PORT_MASK	(CL_HTON16(0xFFBC))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE_COPY
* NAME
*	IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE_COPY
*
* DESCRIPTION
*	Adaptive Routing Group Table Copy attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_ADAPTIVE_ROUTING_GROUP_TABLE_COPY	(CL_HTON16(0xFFBD))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VNODE_INFO
* NAME
*	IB_MAD_ATTR_VPORT_INFO
*
* DESCRIPTION
*	VNode Info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VNODE_INFO				(CL_HTON16(0xFFB2))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_V_PKEY_TABLE
* NAME
*	IB_MAD_ATTR_V_PKEY_TABLE
*
* DESCRIPTION
*	VPort PartitionTable attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_V_PKEY_TABLE			(CL_HTON16(0xFFB6))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VNODE_DESC
* NAME
*	IB_MAD_ATTR_VNODE_DESC
*
* DESCRIPTION
*	VPort NodeDescription attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VNODE_DESC				(CL_HTON16(0xFFB4))

/****d* IBA Base: Constants/IB_MAD_ATTR_ROUTER_INFO
* NAME
*	IB_MAD_ATTR_ROUTER_INFO
*
* DESCRIPTION
*	RouterInfo attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_ROUTER_INFO				(CL_HTON16(0xFFD0))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_ROUTER_NEXT_HOP_TBL
* NAME
*	IB_MAD_ATTR_ROUTER_NEXT_HOP_TBL
*
* DESCRIPTION
*	Router next hop table attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RTR_NEXT_HOP_TBL			(CL_HTON16(0xFFD1))

/****d* IBA Base: Constants/IB_MAD_ATTR_RTR_ADJ_SITE_LOCAL_TBL
* NAME
*	IB_MAD_ATTR_RTR_ADJ_SITE_LOCAL_TBL
*
* DESCRIPTION
*	RouterTable attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RTR_ADJ_SITE_LOCAL_TBL		(CL_HTON16(0xFFD2))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RTR_ADJ_ROUTER_LID_INFO
* NAME
*	IB_MAD_ATTR_RTR_ADJ_ROUTER_LID_INFO
*
* DESCRIPTION
*	Adjacent subnets router lid info attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RTR_ADJ_ROUTER_LID_INFO		(CL_HTON16(0xFFD5))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RTR_ROUTER_LID_TBL
* NAME
*	IB_MAD_ATTR_RTR_ROUTER_LID_TBL
*
* DESCRIPTION
*	Router lid table attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RTR_ROUTER_LID_TBL			(CL_HTON16(0xFFD6))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_RTR_AR_GROUP_TO_LID_TBL
* NAME
*	IB_MAD_ATTR_RTR_AR_GROUP_TO_LID_TBL
*
* DESCRIPTION
*	AR group to LID table (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_RTR_AR_GROUP_TO_LID_TBL		(CL_HTON16(0xFFD7))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_PORT_RECOVERY_POLICY_CONFIG
* NAME
*	IB_MAD_ATTR_PORT_RECOVERY_POLICY_CONFIG
*
* DESCRIPTION
*	Port recovery policy config attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_PORT_RECOVERY_POLICY_CONFIG		(CL_HTON16(0xFFDA))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_QOS_CONFIG_SL
* NAME
*	IB_MAD_ATTR_QOS_CONFIG_SL
*
* DESCRIPTION
*	Qos config SL attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_QOS_CONFIG_SL			(CL_HTON16(0xFF82))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_QOS_CONFIG_VL
* NAME
*	IB_MAD_ATTR_QOS_CONFIG_VL
*
* DESCRIPTION
*	Qos config VL attribute (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_QOS_CONFIG_VL			(CL_HTON16(0xFF85))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_VEND_KEY_INFO
* NAME
*	IB_MAD_ATTR_VEND_KEY_INFO
*
* DESCRIPTION
*	KeyInfo attribute for Vendor Specific classes: 0xA, 0xC (vendor specific MAD)
*
* SOURCE
*/
#define IB_MAD_ATTR_VEND_KEY_INFO			(CL_HTON16(0x000F))
/**********/

/****d* IBA Base: Constants/IB_MAD_ATTR_NEIGHBORS_INFO
* NAME
*	IB_MAD_ATTR_NEIGHBORS_INFO
*
* DESCRIPTION
*	NeighborsInfo Configuration attribute for Class 0xC (Node to Node Class)
*
* SOURCE
*/
#define IB_MAD_ATTR_NEIGHBORS_INFO			(CL_HTON16(0x0010))
/**********/

/****d* IBA Base: Constants/IB_NODE_TYPE_CA
* NAME
*	IB_NODE_TYPE_CA
*
* DESCRIPTION
*	Encoded generic node type used in MAD attributes (13.4.8.2)
*
* SOURCE
*/
#define IB_NODE_TYPE_CA						0x01
/**********/

/****d* IBA Base: Constants/IB_NODE_TYPE_SWITCH
* NAME
*	IB_NODE_TYPE_SWITCH
*
* DESCRIPTION
*	Encoded generic node type used in MAD attributes (13.4.8.2)
*
* SOURCE
*/
#define IB_NODE_TYPE_SWITCH					0x02
/**********/

/****d* IBA Base: Constants/IB_NODE_TYPE_ROUTER
* NAME
*	IB_NODE_TYPE_ROUTER
*
* DESCRIPTION
*	Encoded generic node type used in MAD attributes (13.4.8.2)
*
* SOURCE
*/
#define IB_NODE_TYPE_ROUTER					0x03
/**********/

/****d* IBA Base: Constants/IB_EXTENDED_NODE_TYPE_GPU
* NAME
*	IB_EXTENDED_NODE_TYPE_GPU
*
* DESCRIPTION
*	Encoded generic node type extended used in MAD attributes
*
* SOURCE
*/
#define IB_EXTENDED_NODE_TYPE_GPU				0x01
/**********/

/****d* IBA Base: Constants/IB_EXTENDED_NODE_TYPE_MULTI_PLANE_HCA
* NAME
*	IB_EXTENDED_NODE_TYPE_MULTI_PLANE_HCA
*
* DESCRIPTION
*	Encoded generic node type extended used in MAD attributes
*
* SOURCE
*/
#define IB_EXTENDED_NODE_TYPE_MULTI_PLANE_HCA			0x02
/**********/

/****d* IBA Base: Constants/IB_NOTICE_PRODUCER_TYPE_CA
* NAME
*	IB_NOTICE_PRODUCER_TYPE_CA
*
* DESCRIPTION
*	Encoded generic producer type used in Notice attribute (13.4.8.2)
*
* SOURCE
*/
#define IB_NOTICE_PRODUCER_TYPE_CA			(CL_HTON32(0x000001))
/**********/

/****d* IBA Base: Constants/IB_NOTICE_PRODUCER_TYPE_SWITCH
* NAME
*	IB_NOTICE_PRODUCER_TYPE_SWITCH
*
* DESCRIPTION
*	Encoded generic producer type used in Notice attribute (13.4.8.2)
*
* SOURCE
*/
#define IB_NOTICE_PRODUCER_TYPE_SWITCH			(CL_HTON32(0x000002))
/**********/

/****d* IBA Base: Constants/IB_NOTICE_PRODUCER_TYPE_ROUTER
* NAME
*	IB_NOTICE_PRODUCER_TYPE_ROUTER
*
* DESCRIPTION
*	Encoded generic producer type used in Notice attribute (13.4.8.2)
*
* SOURCE
*/
#define IB_NOTICE_PRODUCER_TYPE_ROUTER			(CL_HTON32(0x000003))
/**********/

/****d* IBA Base: Constants/IB_NOTICE_PRODUCER_TYPE_CLASS_MGR
* NAME
*	IB_NOTICE_PRODUCER_TYPE_CLASS_MGR
*
* DESCRIPTION
*	Encoded generic producer type used in Notice attribute (13.4.8.2)
*
* SOURCE
*/
#define IB_NOTICE_PRODUCER_TYPE_CLASS_MGR			(CL_HTON32(0x000004))
/**********/

/****d* IBA Base: Constants/IB_MTU_LEN_TYPE
* NAME
*	IB_MTU_LEN_TYPE
*
* DESCRIPTION
*	Encoded path MTU.
*		1: 256
*		2: 512
*		3: 1024
*		4: 2048
*		5: 4096
*		others: reserved
*
* SOURCE
*/
#define IB_MTU_LEN_256							1
#define IB_MTU_LEN_512							2
#define IB_MTU_LEN_1024							3
#define IB_MTU_LEN_2048							4
#define IB_MTU_LEN_4096							5

#define IB_MIN_MTU    IB_MTU_LEN_256
#define IB_MAX_MTU    IB_MTU_LEN_4096

/**********/

/****d* IBA Base: Constants/IB_VL_TYPE
* NAME
*	IB_VL_TYPE
*
* DESCRIPTION
*	Encoded VL.
*		1: VL0
*		2: VL0, VL1
*		3: VL0 - VL3
*		4: VL0 - VL7
*		5: VL0 - VL14
*		others: reserved
*
* SOURCE
*/
#define IB_VL_VL0							1
#define IB_VL_VL0_VL1							2
#define IB_VL_VL0_VL3							3
#define IB_VL_VL0_VL7							4
#define IB_VL_VL0_VL14							5

#define IB_MIN_VL     IB_VL_VL0
#define IB_MAX_VL     IB_VL_VL0_VL14

/**********/

/****d* IBA Base: Constants/IB_PATH_SELECTOR_TYPE
* NAME
*	IB_PATH_SELECTOR_TYPE
*
* DESCRIPTION
*	Path selector.
*		0: greater than specified
*		1: less than specified
*		2: exactly the specified
*		3: largest available
*
* SOURCE
*/
#define IB_PATH_SELECTOR_GREATER_THAN		0
#define IB_PATH_SELECTOR_LESS_THAN		1
#define IB_PATH_SELECTOR_EXACTLY		2
#define IB_PATH_SELECTOR_LARGEST		3
/**********/

/****d* IBA Base: Constants/IB_SMINFO_STATE_NOTACTIVE
* NAME
*	IB_SMINFO_STATE_NOTACTIVE
*
* DESCRIPTION
*	Encoded state value used in the SMInfo attribute.
*
* SOURCE
*/
#define IB_SMINFO_STATE_NOTACTIVE			0
/**********/

/****d* IBA Base: Constants/IB_SMINFO_STATE_DISCOVERING
* NAME
*	IB_SMINFO_STATE_DISCOVERING
*
* DESCRIPTION
*	Encoded state value used in the SMInfo attribute.
*
* SOURCE
*/
#define IB_SMINFO_STATE_DISCOVERING			1
/**********/

/****d* IBA Base: Constants/IB_SMINFO_STATE_STANDBY
* NAME
*	IB_SMINFO_STATE_STANDBY
*
* DESCRIPTION
*	Encoded state value used in the SMInfo attribute.
*
* SOURCE
*/
#define IB_SMINFO_STATE_STANDBY				2
/**********/

/****d* IBA Base: Constants/IB_SMINFO_STATE_MASTER
* NAME
*	IB_SMINFO_STATE_MASTER
*
* DESCRIPTION
*	Encoded state value used in the SMInfo attribute.
*
* SOURCE
*/
#define IB_SMINFO_STATE_MASTER				3
/**********/

/****d* IBA Base: Constants/IB_PATH_REC_SL_MASK
* NAME
*	IB_PATH_REC_SL_MASK
*
* DESCRIPTION
*	Mask for the sl field for path record
*
* SOURCE
*/
#define IB_PATH_REC_SL_MASK				0x000F

/****d* IBA Base: Constants/IB_PATH_REC_HOPLIMIT_MASK
* NAME
*	IB_PATH_REC_HOPLIMIT_MASK
*
* DESCRIPTION
*	Mask for the hop_limit field for path record
*
* SOURCE
*/
#define IB_PATH_REC_HOPLIMIT_MASK			0x000000FF

/****d* IBA Base: Constants/IB_PATH_REC_FLOW_LABEL_MASK
* NAME
*	IB_PATH_REC_FLOW_LABEL_MASK
*
* DESCRIPTION
*	Mask for the flow_label field for path record
*
* SOURCE
*/
#define IB_PATH_REC_FLOW_LABEL_MASK			0x0FFFFF00

/****d* IBA Base: Constants/IB_MULTIPATH_REC_SL_MASK
* NAME
*	IB_MILTIPATH_REC_SL_MASK
*
* DESCRIPTION*	Mask for the sl field for MultiPath record
*
* SOURCE
*/
#define IB_MULTIPATH_REC_SL_MASK			0x000F

/****d* IBA Base: Constants/IB_PATH_REC_QOS_CLASS_MASK
* NAME
*	IB_PATH_REC_QOS_CLASS_MASK
*
* DESCRIPTION
*	Mask for the QoS class field for path record
*
* SOURCE
*/
#define IB_PATH_REC_QOS_CLASS_MASK			0xFFF0

/****d* IBA Base: Constants/IB_MULTIPATH_REC_QOS_CLASS_MASK
* NAME
*	IB_MULTIPATH_REC_QOS_CLASS_MASK
*
* DESCRIPTION
*	Mask for the QoS class field for MultiPath record
*
* SOURCE
*/
#define IB_MULTIPATH_REC_QOS_CLASS_MASK			0xFFF0

/****d* IBA Base: Constants/IB_PATH_REC_SELECTOR_MASK
* NAME
*	IB_PATH_REC_SELECTOR_MASK
*
* DESCRIPTION
*	Mask for the selector field for path record MTU, rate,
*	and packet lifetime.
*
* SOURCE
*/
#define IB_PATH_REC_SELECTOR_MASK			0xC0

/****d* IBA Base: Constants/IB_MULTIPATH_REC_SELECTOR_MASK
* NAME
*       IB_MULTIPATH_REC_SELECTOR_MASK
*
* DESCRIPTION
*       Mask for the selector field for multipath record MTU, rate,
*       and packet lifetime.
*
* SOURCE
*/
#define IB_MULTIPATH_REC_SELECTOR_MASK                       0xC0
/**********/

/****d* IBA Base: Constants/IB_PATH_REC_BASE_MASK
* NAME
*	IB_PATH_REC_BASE_MASK
*
* DESCRIPTION
*	Mask for the base value field for path record MTU, rate,
*	and packet lifetime.
*
* SOURCE
*/
#define IB_PATH_REC_BASE_MASK				0x3F
/**********/

/****d* IBA Base: Constants/IB_MULTIPATH_REC_BASE_MASK
* NAME
*       IB_MULTIPATH_REC_BASE_MASK
*
* DESCRIPTION
*       Mask for the base value field for multipath record MTU, rate,
*       and packet lifetime.
*
* SOURCE
*/
#define IB_MULTIPATH_REC_BASE_MASK                      0x3F
/**********/

/****h* IBA Base/Type Definitions
* NAME
*	Type Definitions
*
* DESCRIPTION
*	Definitions are from the InfiniBand Architecture Specification v1.2
*
*********/

/****d* IBA Base: Types/ib_net16_t
* NAME
*	ib_net16_t
*
* DESCRIPTION
*	Defines the network ordered type for 16-bit values.
*
* SOURCE
*/
typedef uint16_t ib_net16_t;
/**********/

/****d* IBA Base: Types/ib_net32_t
* NAME
*	ib_net32_t
*
* DESCRIPTION
*	Defines the network ordered type for 32-bit values.
*
* SOURCE
*/
typedef uint32_t ib_net32_t;
/**********/

/****d* IBA Base: Types/ib_net64_t
* NAME
*	ib_net64_t
*
* DESCRIPTION
*	Defines the network ordered type for 64-bit values.
*
* SOURCE
*/
typedef uint64_t ib_net64_t;
/**********/

/****d* IBA Base: Types/ib_gid_prefix_t
* NAME
*	ib_gid_prefix_t
*
* DESCRIPTION
*
* SOURCE
*/
typedef ib_net64_t ib_gid_prefix_t;
/**********/

/****d* IBA Base: Constants/ib_link_states_t
* NAME
*	ib_link_states_t
*
* DESCRIPTION
*	Defines the link states of a port.
*
* SOURCE
*/
#define IB_LINK_NO_CHANGE 0
#define IB_LINK_DOWN      1
#define IB_LINK_INIT	  2
#define IB_LINK_ARMED     3
#define IB_LINK_ACTIVE    4
#define IB_LINK_ACT_DEFER 5
/**********/

static const char *const __ib_node_type_str[] = {
	"UNKNOWN",
	"Channel Adapter",
	"Switch",
	"Router"
};

/****f* IBA Base: Types/ib_get_node_type_str
* NAME
*	ib_get_node_type_str
*
* DESCRIPTION
*	Returns a string for the specified node type.
*	14.2.5.3 NodeInfo
*
* SYNOPSIS
*/
static inline const char *OSM_API ib_get_node_type_str(IN uint8_t node_type)
{
	if (node_type > IB_NODE_TYPE_ROUTER)
		node_type = 0;
	return (__ib_node_type_str[node_type]);
}

/*
* PARAMETERS
*	node_type
*		[in] Encoded node type as returned in the NodeInfo attribute.

* RETURN VALUES
*	Pointer to the node type string.
*
* NOTES
*
* SEE ALSO
* ib_node_info_t
*********/

static const char *const __ib_producer_type_str[] = {
	"UNKNOWN",
	"Channel Adapter",
	"Switch",
	"Router",
	"Class Manager"
};

/****f* IBA Base: Types/ib_get_producer_type_str
* NAME
*	ib_get_producer_type_str
*
* DESCRIPTION
*	Returns a string for the specified producer type
*	13.4.8.2 Notice
*	13.4.8.3 InformInfo
*
* SYNOPSIS
*/
static inline const char *OSM_API
ib_get_producer_type_str(IN ib_net32_t producer_type)
{
	if (cl_ntoh32(producer_type) >
	    CL_NTOH32(IB_NOTICE_PRODUCER_TYPE_CLASS_MGR))
		producer_type = 0;
	return (__ib_producer_type_str[cl_ntoh32(producer_type)]);
}

/*
* PARAMETERS
*	producer_type
*		[in] Encoded producer type from the Notice attribute

* RETURN VALUES
*	Pointer to the producer type string.
*
* NOTES
*
* SEE ALSO
* ib_notice_get_prod_type
*********/

static const char *const __ib_port_state_str[] = {
	"No State Change (NOP)",
	"DOWN",
	"INIT",
	"ARMED",
	"ACTIVE",
	"ACTDEFER",
	"UNKNOWN"
};

/****f* IBA Base: Types/ib_get_port_state_str
* NAME
*	ib_get_port_state_str
*
* DESCRIPTION
*	Returns a string for the specified port state.
*
* SYNOPSIS
*/
static inline const char *OSM_API ib_get_port_state_str(IN uint8_t port_state)
{
	if (port_state > IB_LINK_ACTIVE)
		port_state = IB_LINK_ACTIVE + 1;
	return (__ib_port_state_str[port_state]);
}

/*
* PARAMETERS
*	port_state
*		[in] Encoded port state as returned in the PortInfo attribute.

* RETURN VALUES
*	Pointer to the port state string.
*
* NOTES
*
* SEE ALSO
* ib_port_info_t
*********/

/****f* IBA Base: Types/ib_get_port_state_from_str
* NAME
*	ib_get_port_state_from_str
*
* DESCRIPTION
*	Returns a string for the specified port state.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_get_port_state_from_str(IN char *p_port_state_str)
{
	if (!strncmp(p_port_state_str, "No State Change (NOP)", 12))
		return (0);
	else if (!strncmp(p_port_state_str, "DOWN", 4))
		return (1);
	else if (!strncmp(p_port_state_str, "INIT", 4))
		return (2);
	else if (!strncmp(p_port_state_str, "ARMED", 5))
		return (3);
	else if (!strncmp(p_port_state_str, "ACTIVE", 6))
		return (4);
	else if (!strncmp(p_port_state_str, "ACTDEFER", 8))
		return (5);
	return (6);
}

/*
* PARAMETERS
*	p_port_state_str
*		[in] A string matching one returned by ib_get_port_state_str
*
* RETURN VALUES
*	The appropriate code.
*
* NOTES
*
* SEE ALSO
*	ib_port_info_t
*********/

/****d* IBA Base: Constants/Join States
* NAME
*	Join States
*
* DESCRIPTION
*	Defines the join state flags for multicast group management.
*
* SOURCE
*/
#define IB_JOIN_STATE_FULL		1
#define IB_JOIN_STATE_NON		2
#define IB_JOIN_STATE_SEND_ONLY		4
#define IB_JOIN_STATE_SEND_ONLY_FULL	8
/**********/

/****f* IBA Base: Types/ib_pkey_get_base
* NAME
*	ib_pkey_get_base
*
* DESCRIPTION
*	Returns the base P_Key value with the membership bit stripped.
*
* SYNOPSIS
*/
static inline ib_net16_t OSM_API ib_pkey_get_base(IN const ib_net16_t pkey)
{
	return ((ib_net16_t) (pkey & IB_PKEY_BASE_MASK));
}

/*
* PARAMETERS
*	pkey
*		[in] P_Key value
*
* RETURN VALUE
*	Returns the base P_Key value with the membership bit stripped.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_pkey_is_full_member
* NAME
*	ib_pkey_is_full_member
*
* DESCRIPTION
*	Indicates if the port is a full member of the partition.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API ib_pkey_is_full_member(IN const ib_net16_t pkey)
{
	return ((pkey & IB_PKEY_TYPE_MASK) == IB_PKEY_TYPE_MASK);
}

/*
* PARAMETERS
*	pkey
*		[in] P_Key value
*
* RETURN VALUE
*	TRUE if the port is a full member of the partition.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
* ib_pkey_get_base, ib_net16_t
*********/

/****f* IBA Base: Types/ib_pkey_is_invalid
* NAME
*	ib_pkey_is_invalid
*
* DESCRIPTION
*	Returns TRUE if the given P_Key is an invalid P_Key
*  C10-116: the CI shall regard a P_Key as invalid if its low-order
*           15 bits are all zero...
*
* SYNOPSIS
*/
static inline boolean_t OSM_API ib_pkey_is_invalid(IN const ib_net16_t pkey)
{
	return ib_pkey_get_base(pkey) == 0x0000 ? TRUE : FALSE;
}

/*
* PARAMETERS
*	pkey
*		[in] P_Key value
*
* RETURN VALUE
*	Returns the base P_Key value with the membership bit stripped.
*
* NOTES
*
* SEE ALSO
*********/

/****d* IBA Base: Types/ib_gid_t
* NAME
*	ib_gid_t
*
* DESCRIPTION
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef union _ib_gid {
	uint8_t raw[16];
	struct _ib_gid_unicast {
		ib_gid_prefix_t prefix;
		ib_net64_t interface_id;
	} PACK_SUFFIX unicast;
	struct _ib_gid_multicast {
		uint8_t header[2];
		uint8_t raw_group_id[14];
	} PACK_SUFFIX multicast;
} PACK_SUFFIX ib_gid_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	raw
*		GID represented as an unformatted byte array.
*
*	unicast
*		Typical unicast representation with subnet prefix and
*		port GUID.
*
*	multicast
*		Representation for multicast use.
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_gid_is_multicast
* NAME
*	ib_gid_is_multicast
*
* DESCRIPTION
*       Returns a boolean indicating whether a GID is a multicast GID.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API ib_gid_is_multicast(IN const ib_gid_t * p_gid)
{
	return (p_gid->raw[0] == 0xFF);
}

/****f* IBA Base: Types/ib_mgid_get_scope
* NAME
*	ib_mgid_get_scope
*
* DESCRIPTION
*	Returns scope of (assumed) multicast GID.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API ib_mgid_get_scope(IN const ib_gid_t * p_gid)
{
	return (p_gid->raw[1] & 0x0F);
}

/****f* IBA Base: Types/ib_mgid_set_scope
* NAME
*	ib_mgid_set_scope
*
* DESCRIPTION
*	Sets scope of (assumed) multicast GID.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mgid_set_scope(IN ib_gid_t * const p_gid, IN const uint8_t scope)
{
	p_gid->raw[1] &= 0xF0;
	p_gid->raw[1] |= scope & 0x0F;
}

/****f* IBA Base: Types/ib_gid_set_default
* NAME
*	ib_gid_set_default
*
* DESCRIPTION
*	Sets a GID to the default value.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_gid_set_default(IN ib_gid_t * const p_gid, IN const ib_net64_t interface_id)
{
	p_gid->unicast.prefix = IB_DEFAULT_SUBNET_PREFIX;
	p_gid->unicast.interface_id = interface_id;
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
*	interface_id
*		[in] Manufacturer assigned EUI64 value of a port.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* IBA Base: Types/ib_gid_get_subnet_prefix
* NAME
*	ib_gid_get_subnet_prefix
*
* DESCRIPTION
*	Gets the subnet prefix from a GID.
*
* SYNOPSIS
*/
static inline ib_net64_t OSM_API
ib_gid_get_subnet_prefix(IN const ib_gid_t * const p_gid)
{
	return (p_gid->unicast.prefix);
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
* RETURN VALUES
*	64-bit subnet prefix value.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* IBA Base: Types/ib_gid_is_link_local
* NAME
*	ib_gid_is_link_local
*
* DESCRIPTION
*	Returns TRUE if the unicast GID scoping indicates link local,
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_gid_is_link_local(IN const ib_gid_t * const p_gid)
{
	return ((ib_gid_get_subnet_prefix(p_gid) & IB_10_MSB_MASK) ==
		IB_DEFAULT_SUBNET_PREFIX);
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
* RETURN VALUES
*	Returns TRUE if the unicast GID scoping indicates link local,
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* IBA Base: Types/is_equal_subnet_prefix
* NAME
*	is_equal_subnet_prefix
*
* DESCRIPTION
*	Returns TRUE if the given unicast GID prefixes are equal, FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
is_equal_subnet_prefix(IN ib_gid_prefix_t prefix1, IN ib_gid_prefix_t prefix2)
{
	/* for site local scope compare the whole prefix except the FLID bits */
	if ((prefix1 & IB_10_MSB_MASK) == IB_SITE_LOCAL_SUBNET_PREFIX)
		return (prefix1 & IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK) ==
			(prefix2 & IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK);
	return (prefix1 == prefix2);
}

/*
* PARAMETERS
*	prefix1
*		[in] First gid unicast subnet prefix.
*
*	prefix2
*		[in] Second gid unicast subnet prefix.
* RETURN VALUES
*	Returns TRUE if the given unicast GID prefixes are equal, FALSE otherwise.
*
* NOTES
*
* SEE ALSO
* 	ib_gid_prefix_t
*********/

/****f* IBA Base: Types/ib_gid_is_site_local
* NAME
*	ib_gid_is_site_local
*
* DESCRIPTION
*	Returns TRUE if the unicast GID scoping indicates site local,
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_gid_is_site_local(IN const ib_gid_t * const p_gid)
{
	return ((ib_gid_get_subnet_prefix(p_gid) & IB_LOCAL_SUBNET_MASK) ==
		IB_SITE_LOCAL_SUBNET_PREFIX);
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
* RETURN VALUES
*	Returns TRUE if the unicast GID scoping indicates site local,
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* IBA Base: Types/ib_gid_get_guid
* NAME
*	ib_gid_get_guid
*
* DESCRIPTION
*	Gets the guid from a GID.
*
* SYNOPSIS
*/
static inline ib_net64_t OSM_API
ib_gid_get_guid(IN const ib_gid_t * const p_gid)
{
	return (p_gid->unicast.interface_id);
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
* RETURN VALUES
*	64-bit GUID value.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****s* IBA Base: Types/ib_path_rec_t
* NAME
*	ib_path_rec_t
*
* DESCRIPTION
*	Path records encapsulate the properties of a given
*	route between two end-points on a subnet.
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_path_rec {
	ib_net64_t service_id;
	ib_gid_t dgid;
	ib_gid_t sgid;
	ib_net16_t dlid;
	ib_net16_t slid;
	ib_net32_t hop_flow_raw;
	uint8_t tclass;
	uint8_t num_path;
	ib_net16_t pkey;
	ib_net16_t qos_class_sl;
	uint8_t mtu;
	uint8_t rate;
	uint8_t pkt_life;
	uint8_t preference;
	uint8_t resv2[6];
} PACK_SUFFIX ib_path_rec_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	service_id
*		Service ID for QoS.
*
*	dgid
*		GID of destination port.
*
*	sgid
*		GID of source port.
*
*	dlid
*		LID of destination port.
*
*	slid
*		LID of source port.
*
*	hop_flow_raw
*		Global routing parameters: hop count, flow label and raw bit.
*
*	tclass
*		Another global routing parameter.
*
*	num_path
*		Reversible path - 1 bit to say if path is reversible.
*		num_path [6:0] In queries, maximum number of paths to return.
*		In responses, undefined.
*
*	pkey
*		Partition key (P_Key) to use on this path.
*
*	qos_class_sl
*		QoS class and service level to use on this path.
*
*	mtu
*		MTU and MTU selector fields to use on this path
*
*	rate
*		Rate and rate selector fields to use on this path.
*
*	pkt_life
*		Packet lifetime
*
*	preference
*		Indicates the relative merit of this path versus other path
*		records returned from the SA.  Lower numbers are better.
*
*	resv2
*		Reserved bytes.
* SEE ALSO
*********/

/* Path Record Component Masks */
#define  IB_PR_COMPMASK_SERVICEID_MSB     (CL_HTON64(((uint64_t)1)<<0))
#define  IB_PR_COMPMASK_SERVICEID_LSB     (CL_HTON64(((uint64_t)1)<<1))
#define  IB_PR_COMPMASK_DGID              (CL_HTON64(((uint64_t)1)<<2))
#define  IB_PR_COMPMASK_SGID              (CL_HTON64(((uint64_t)1)<<3))
#define  IB_PR_COMPMASK_DLID              (CL_HTON64(((uint64_t)1)<<4))
#define  IB_PR_COMPMASK_SLID              (CL_HTON64(((uint64_t)1)<<5))
#define  IB_PR_COMPMASK_RAWTRAFFIC        (CL_HTON64(((uint64_t)1)<<6))
#define  IB_PR_COMPMASK_RESV0             (CL_HTON64(((uint64_t)1)<<7))
#define  IB_PR_COMPMASK_FLOWLABEL         (CL_HTON64(((uint64_t)1)<<8))
#define  IB_PR_COMPMASK_HOPLIMIT          (CL_HTON64(((uint64_t)1)<<9))
#define  IB_PR_COMPMASK_TCLASS            (CL_HTON64(((uint64_t)1)<<10))
#define  IB_PR_COMPMASK_REVERSIBLE        (CL_HTON64(((uint64_t)1)<<11))
#define  IB_PR_COMPMASK_NUMBPATH          (CL_HTON64(((uint64_t)1)<<12))
#define  IB_PR_COMPMASK_PKEY              (CL_HTON64(((uint64_t)1)<<13))
#define  IB_PR_COMPMASK_QOS_CLASS         (CL_HTON64(((uint64_t)1)<<14))
#define  IB_PR_COMPMASK_SL                (CL_HTON64(((uint64_t)1)<<15))
#define  IB_PR_COMPMASK_MTUSELEC          (CL_HTON64(((uint64_t)1)<<16))
#define  IB_PR_COMPMASK_MTU               (CL_HTON64(((uint64_t)1)<<17))
#define  IB_PR_COMPMASK_RATESELEC         (CL_HTON64(((uint64_t)1)<<18))
#define  IB_PR_COMPMASK_RATE              (CL_HTON64(((uint64_t)1)<<19))
#define  IB_PR_COMPMASK_PKTLIFETIMESELEC  (CL_HTON64(((uint64_t)1)<<20))
#define  IB_PR_COMPMASK_PKTLIFETIME       (CL_HTON64(((uint64_t)1)<<21))

#define  IB_PR_COMPMASK_SERVICEID (IB_PR_COMPMASK_SERVICEID_MSB | \
				   IB_PR_COMPMASK_SERVICEID_LSB)

/* Link Record Component Masks */
#define IB_LR_COMPMASK_FROM_LID           (CL_HTON64(((uint64_t)1)<<0))
#define IB_LR_COMPMASK_FROM_PORT          (CL_HTON64(((uint64_t)1)<<1))
#define IB_LR_COMPMASK_TO_PORT            (CL_HTON64(((uint64_t)1)<<2))
#define IB_LR_COMPMASK_TO_LID             (CL_HTON64(((uint64_t)1)<<3))

/* VL Arbitration Record Masks */
#define IB_VLA_COMPMASK_LID               (CL_HTON64(((uint64_t)1)<<0))
#define IB_VLA_COMPMASK_OUT_PORT          (CL_HTON64(((uint64_t)1)<<1))
#define IB_VLA_COMPMASK_BLOCK             (CL_HTON64(((uint64_t)1)<<2))

/* SLtoVL Mapping Record Masks */
#define IB_SLVL_COMPMASK_LID              (CL_HTON64(((uint64_t)1)<<0))
#define IB_SLVL_COMPMASK_IN_PORT          (CL_HTON64(((uint64_t)1)<<1))
#define IB_SLVL_COMPMASK_OUT_PORT         (CL_HTON64(((uint64_t)1)<<2))

/* P_Key Table Record Masks */
#define IB_PKEY_COMPMASK_LID              (CL_HTON64(((uint64_t)1)<<0))
#define IB_PKEY_COMPMASK_BLOCK            (CL_HTON64(((uint64_t)1)<<1))
#define IB_PKEY_COMPMASK_PORT             (CL_HTON64(((uint64_t)1)<<2))

/* Switch Info Record Masks */
#define IB_SWIR_COMPMASK_LID		  (CL_HTON64(((uint64_t)1)<<0))
#define IB_SWIR_COMPMASK_RESERVED1	  (CL_HTON64(((uint64_t)1)<<1))

/* LFT Record Masks */
#define IB_LFTR_COMPMASK_LID              (CL_HTON64(((uint64_t)1)<<0))
#define IB_LFTR_COMPMASK_BLOCK            (CL_HTON64(((uint64_t)1)<<1))

/* MFT Record Masks */
#define IB_MFTR_COMPMASK_LID		  (CL_HTON64(((uint64_t)1)<<0))
#define IB_MFTR_COMPMASK_POSITION	  (CL_HTON64(((uint64_t)1)<<1))
#define IB_MFTR_COMPMASK_RESERVED1	  (CL_HTON64(((uint64_t)1)<<2))
#define IB_MFTR_COMPMASK_BLOCK		  (CL_HTON64(((uint64_t)1)<<3))
#define IB_MFTR_COMPMASK_RESERVED2	  (CL_HTON64(((uint64_t)1)<<4))

/* NodeInfo Record Masks */
#define IB_NR_COMPMASK_LID                (CL_HTON64(((uint64_t)1)<<0))
#define IB_NR_COMPMASK_RESERVED1          (CL_HTON64(((uint64_t)1)<<1))
#define IB_NR_COMPMASK_BASEVERSION        (CL_HTON64(((uint64_t)1)<<2))
#define IB_NR_COMPMASK_CLASSVERSION       (CL_HTON64(((uint64_t)1)<<3))
#define IB_NR_COMPMASK_NODETYPE           (CL_HTON64(((uint64_t)1)<<4))
#define IB_NR_COMPMASK_NUMPORTS           (CL_HTON64(((uint64_t)1)<<5))
#define IB_NR_COMPMASK_SYSIMAGEGUID       (CL_HTON64(((uint64_t)1)<<6))
#define IB_NR_COMPMASK_NODEGUID           (CL_HTON64(((uint64_t)1)<<7))
#define IB_NR_COMPMASK_PORTGUID           (CL_HTON64(((uint64_t)1)<<8))
#define IB_NR_COMPMASK_PARTCAP            (CL_HTON64(((uint64_t)1)<<9))
#define IB_NR_COMPMASK_DEVID              (CL_HTON64(((uint64_t)1)<<10))
#define IB_NR_COMPMASK_REV                (CL_HTON64(((uint64_t)1)<<11))
#define IB_NR_COMPMASK_PORTNUM            (CL_HTON64(((uint64_t)1)<<12))
#define IB_NR_COMPMASK_VENDID             (CL_HTON64(((uint64_t)1)<<13))
#define IB_NR_COMPMASK_NODEDESC           (CL_HTON64(((uint64_t)1)<<14))

/* Service Record Component Masks Sec 15.2.5.14 Ver 1.1*/
#define IB_SR_COMPMASK_SID                (CL_HTON64(((uint64_t)1)<<0))
#define IB_SR_COMPMASK_SGID               (CL_HTON64(((uint64_t)1)<<1))
#define IB_SR_COMPMASK_SPKEY              (CL_HTON64(((uint64_t)1)<<2))
#define IB_SR_COMPMASK_RES1               (CL_HTON64(((uint64_t)1)<<3))
#define IB_SR_COMPMASK_SLEASE             (CL_HTON64(((uint64_t)1)<<4))
#define IB_SR_COMPMASK_SKEY               (CL_HTON64(((uint64_t)1)<<5))
#define IB_SR_COMPMASK_SNAME              (CL_HTON64(((uint64_t)1)<<6))
#define IB_SR_COMPMASK_SDATA8_0           (CL_HTON64(((uint64_t)1)<<7))
#define IB_SR_COMPMASK_SDATA8_1           (CL_HTON64(((uint64_t)1)<<8))
#define IB_SR_COMPMASK_SDATA8_2           (CL_HTON64(((uint64_t)1)<<9))
#define IB_SR_COMPMASK_SDATA8_3           (CL_HTON64(((uint64_t)1)<<10))
#define IB_SR_COMPMASK_SDATA8_4           (CL_HTON64(((uint64_t)1)<<11))
#define IB_SR_COMPMASK_SDATA8_5           (CL_HTON64(((uint64_t)1)<<12))
#define IB_SR_COMPMASK_SDATA8_6           (CL_HTON64(((uint64_t)1)<<13))
#define IB_SR_COMPMASK_SDATA8_7           (CL_HTON64(((uint64_t)1)<<14))
#define IB_SR_COMPMASK_SDATA8_8           (CL_HTON64(((uint64_t)1)<<15))
#define IB_SR_COMPMASK_SDATA8_9           (CL_HTON64(((uint64_t)1)<<16))
#define IB_SR_COMPMASK_SDATA8_10       (CL_HTON64(((uint64_t)1)<<17))
#define IB_SR_COMPMASK_SDATA8_11       (CL_HTON64(((uint64_t)1)<<18))
#define IB_SR_COMPMASK_SDATA8_12       (CL_HTON64(((uint64_t)1)<<19))
#define IB_SR_COMPMASK_SDATA8_13       (CL_HTON64(((uint64_t)1)<<20))
#define IB_SR_COMPMASK_SDATA8_14       (CL_HTON64(((uint64_t)1)<<21))
#define IB_SR_COMPMASK_SDATA8_15       (CL_HTON64(((uint64_t)1)<<22))
#define IB_SR_COMPMASK_SDATA16_0       (CL_HTON64(((uint64_t)1)<<23))
#define IB_SR_COMPMASK_SDATA16_1       (CL_HTON64(((uint64_t)1)<<24))
#define IB_SR_COMPMASK_SDATA16_2       (CL_HTON64(((uint64_t)1)<<25))
#define IB_SR_COMPMASK_SDATA16_3       (CL_HTON64(((uint64_t)1)<<26))
#define IB_SR_COMPMASK_SDATA16_4       (CL_HTON64(((uint64_t)1)<<27))
#define IB_SR_COMPMASK_SDATA16_5       (CL_HTON64(((uint64_t)1)<<28))
#define IB_SR_COMPMASK_SDATA16_6       (CL_HTON64(((uint64_t)1)<<29))
#define IB_SR_COMPMASK_SDATA16_7       (CL_HTON64(((uint64_t)1)<<30))
#define IB_SR_COMPMASK_SDATA32_0       (CL_HTON64(((uint64_t)1)<<31))
#define IB_SR_COMPMASK_SDATA32_1       (CL_HTON64(((uint64_t)1)<<32))
#define IB_SR_COMPMASK_SDATA32_2       (CL_HTON64(((uint64_t)1)<<33))
#define IB_SR_COMPMASK_SDATA32_3       (CL_HTON64(((uint64_t)1)<<34))
#define IB_SR_COMPMASK_SDATA64_0       (CL_HTON64(((uint64_t)1)<<35))
#define IB_SR_COMPMASK_SDATA64_1       (CL_HTON64(((uint64_t)1)<<36))

/* Port Info Record Component Masks */
#define IB_PIR_COMPMASK_LID              (CL_HTON64(((uint64_t)1)<<0))
#define IB_PIR_COMPMASK_PORTNUM          (CL_HTON64(((uint64_t)1)<<1))
#define IB_PIR_COMPMASK_OPTIONS		 (CL_HTON64(((uint64_t)1)<<2))
#define IB_PIR_COMPMASK_MKEY             (CL_HTON64(((uint64_t)1)<<3))
#define IB_PIR_COMPMASK_GIDPRE           (CL_HTON64(((uint64_t)1)<<4))
#define IB_PIR_COMPMASK_BASELID          (CL_HTON64(((uint64_t)1)<<5))
#define IB_PIR_COMPMASK_SMLID            (CL_HTON64(((uint64_t)1)<<6))
#define IB_PIR_COMPMASK_CAPMASK          (CL_HTON64(((uint64_t)1)<<7))
#define IB_PIR_COMPMASK_DIAGCODE         (CL_HTON64(((uint64_t)1)<<8))
#define IB_PIR_COMPMASK_MKEYLEASEPRD     (CL_HTON64(((uint64_t)1)<<9))
#define IB_PIR_COMPMASK_LOCALPORTNUM     (CL_HTON64(((uint64_t)1)<<10))
#define IB_PIR_COMPMASK_LINKWIDTHENABLED (CL_HTON64(((uint64_t)1)<<11))
#define IB_PIR_COMPMASK_LNKWIDTHSUPPORT  (CL_HTON64(((uint64_t)1)<<12))
#define IB_PIR_COMPMASK_LNKWIDTHACTIVE   (CL_HTON64(((uint64_t)1)<<13))
#define IB_PIR_COMPMASK_LNKSPEEDSUPPORT  (CL_HTON64(((uint64_t)1)<<14))
#define IB_PIR_COMPMASK_PORTSTATE        (CL_HTON64(((uint64_t)1)<<15))
#define IB_PIR_COMPMASK_PORTPHYSTATE     (CL_HTON64(((uint64_t)1)<<16))
#define IB_PIR_COMPMASK_LINKDWNDFLTSTATE (CL_HTON64(((uint64_t)1)<<17))
#define IB_PIR_COMPMASK_MKEYPROTBITS     (CL_HTON64(((uint64_t)1)<<18))
#define IB_PIR_COMPMASK_RESV2            (CL_HTON64(((uint64_t)1)<<19))
#define IB_PIR_COMPMASK_LMC              (CL_HTON64(((uint64_t)1)<<20))
#define IB_PIR_COMPMASK_LINKSPEEDACTIVE  (CL_HTON64(((uint64_t)1)<<21))
#define IB_PIR_COMPMASK_LINKSPEEDENABLE  (CL_HTON64(((uint64_t)1)<<22))
#define IB_PIR_COMPMASK_NEIGHBORMTU      (CL_HTON64(((uint64_t)1)<<23))
#define IB_PIR_COMPMASK_MASTERSMSL       (CL_HTON64(((uint64_t)1)<<24))
#define IB_PIR_COMPMASK_VLCAP            (CL_HTON64(((uint64_t)1)<<25))
#define IB_PIR_COMPMASK_INITTYPE         (CL_HTON64(((uint64_t)1)<<26))
#define IB_PIR_COMPMASK_VLHIGHLIMIT      (CL_HTON64(((uint64_t)1)<<27))
#define IB_PIR_COMPMASK_VLARBHIGHCAP     (CL_HTON64(((uint64_t)1)<<28))
#define IB_PIR_COMPMASK_VLARBLOWCAP      (CL_HTON64(((uint64_t)1)<<29))
#define IB_PIR_COMPMASK_INITTYPEREPLY    (CL_HTON64(((uint64_t)1)<<30))
#define IB_PIR_COMPMASK_MTUCAP           (CL_HTON64(((uint64_t)1)<<31))
#define IB_PIR_COMPMASK_VLSTALLCNT       (CL_HTON64(((uint64_t)1)<<32))
#define IB_PIR_COMPMASK_HOQLIFE          (CL_HTON64(((uint64_t)1)<<33))
#define IB_PIR_COMPMASK_OPVLS            (CL_HTON64(((uint64_t)1)<<34))
#define IB_PIR_COMPMASK_PARENFIN         (CL_HTON64(((uint64_t)1)<<35))
#define IB_PIR_COMPMASK_PARENFOUT        (CL_HTON64(((uint64_t)1)<<36))
#define IB_PIR_COMPMASK_FILTERRAWIN      (CL_HTON64(((uint64_t)1)<<37))
#define IB_PIR_COMPMASK_FILTERRAWOUT     (CL_HTON64(((uint64_t)1)<<38))
#define IB_PIR_COMPMASK_MKEYVIO          (CL_HTON64(((uint64_t)1)<<39))
#define IB_PIR_COMPMASK_PKEYVIO          (CL_HTON64(((uint64_t)1)<<40))
#define IB_PIR_COMPMASK_QKEYVIO          (CL_HTON64(((uint64_t)1)<<41))
#define IB_PIR_COMPMASK_GUIDCAP          (CL_HTON64(((uint64_t)1)<<42))
#define IB_PIR_COMPMASK_CLIENTREREG	 (CL_HTON64(((uint64_t)1)<<43))
#define IB_PIR_COMPMASK_RESV3            (CL_HTON64(((uint64_t)1)<<44))
#define IB_PIR_COMPMASK_SUBNTO           (CL_HTON64(((uint64_t)1)<<45))
#define IB_PIR_COMPMASK_RESV4            (CL_HTON64(((uint64_t)1)<<46))
#define IB_PIR_COMPMASK_RESPTIME         (CL_HTON64(((uint64_t)1)<<47))
#define IB_PIR_COMPMASK_LOCALPHYERR      (CL_HTON64(((uint64_t)1)<<48))
#define IB_PIR_COMPMASK_OVERRUNERR       (CL_HTON64(((uint64_t)1)<<49))
#define IB_PIR_COMPMASK_MAXCREDHINT	 (CL_HTON64(((uint64_t)1)<<50))
#define IB_PIR_COMPMASK_RESV5		 (CL_HTON64(((uint64_t)1)<<51))
#define IB_PIR_COMPMASK_LINKRTLAT	 (CL_HTON64(((uint64_t)1)<<52))
#define IB_PIR_COMPMASK_CAPMASK2	 (CL_HTON64(((uint64_t)1)<<53))
#define IB_PIR_COMPMASK_LINKSPDEXTACT	 (CL_HTON64(((uint64_t)1)<<54))
#define IB_PIR_COMPMASK_LINKSPDEXTSUPP	 (CL_HTON64(((uint64_t)1)<<55))
#define IB_PIR_COMPMASK_RESV7		 (CL_HTON64(((uint64_t)1)<<56))
#define IB_PIR_COMPMASK_LINKSPDEXTENAB	 (CL_HTON64(((uint64_t)1)<<57))

/* Multicast Member Record Component Masks */
#define IB_MCR_COMPMASK_GID         (CL_HTON64(((uint64_t)1)<<0))
#define IB_MCR_COMPMASK_MGID        (CL_HTON64(((uint64_t)1)<<0))
#define IB_MCR_COMPMASK_PORT_GID    (CL_HTON64(((uint64_t)1)<<1))
#define IB_MCR_COMPMASK_QKEY        (CL_HTON64(((uint64_t)1)<<2))
#define IB_MCR_COMPMASK_MLID        (CL_HTON64(((uint64_t)1)<<3))
#define IB_MCR_COMPMASK_MTU_SEL     (CL_HTON64(((uint64_t)1)<<4))
#define IB_MCR_COMPMASK_MTU         (CL_HTON64(((uint64_t)1)<<5))
#define IB_MCR_COMPMASK_TCLASS      (CL_HTON64(((uint64_t)1)<<6))
#define IB_MCR_COMPMASK_PKEY        (CL_HTON64(((uint64_t)1)<<7))
#define IB_MCR_COMPMASK_RATE_SEL    (CL_HTON64(((uint64_t)1)<<8))
#define IB_MCR_COMPMASK_RATE        (CL_HTON64(((uint64_t)1)<<9))
#define IB_MCR_COMPMASK_LIFE_SEL    (CL_HTON64(((uint64_t)1)<<10))
#define IB_MCR_COMPMASK_LIFE        (CL_HTON64(((uint64_t)1)<<11))
#define IB_MCR_COMPMASK_SL          (CL_HTON64(((uint64_t)1)<<12))
#define IB_MCR_COMPMASK_FLOW        (CL_HTON64(((uint64_t)1)<<13))
#define IB_MCR_COMPMASK_HOP         (CL_HTON64(((uint64_t)1)<<14))
#define IB_MCR_COMPMASK_SCOPE       (CL_HTON64(((uint64_t)1)<<15))
#define IB_MCR_COMPMASK_JOIN_STATE  (CL_HTON64(((uint64_t)1)<<16))
#define IB_MCR_COMPMASK_PROXY       (CL_HTON64(((uint64_t)1)<<17))

/* GUID Info Record Component Masks */
#define IB_GIR_COMPMASK_LID		(CL_HTON64(((uint64_t)1)<<0))
#define IB_GIR_COMPMASK_BLOCKNUM	(CL_HTON64(((uint64_t)1)<<1))
#define IB_GIR_COMPMASK_RESV1		(CL_HTON64(((uint64_t)1)<<2))
#define IB_GIR_COMPMASK_RESV2		(CL_HTON64(((uint64_t)1)<<3))
#define IB_GIR_COMPMASK_GID0		(CL_HTON64(((uint64_t)1)<<4))
#define IB_GIR_COMPMASK_GID1		(CL_HTON64(((uint64_t)1)<<5))
#define IB_GIR_COMPMASK_GID2		(CL_HTON64(((uint64_t)1)<<6))
#define IB_GIR_COMPMASK_GID3		(CL_HTON64(((uint64_t)1)<<7))
#define IB_GIR_COMPMASK_GID4		(CL_HTON64(((uint64_t)1)<<8))
#define IB_GIR_COMPMASK_GID5		(CL_HTON64(((uint64_t)1)<<9))
#define IB_GIR_COMPMASK_GID6		(CL_HTON64(((uint64_t)1)<<10))
#define IB_GIR_COMPMASK_GID7		(CL_HTON64(((uint64_t)1)<<11))

/* MultiPath Record Component Masks */
#define IB_MPR_COMPMASK_RAWTRAFFIC	(CL_HTON64(((uint64_t)1)<<0))
#define IB_MPR_COMPMASK_RESV0		(CL_HTON64(((uint64_t)1)<<1))
#define IB_MPR_COMPMASK_FLOWLABEL	(CL_HTON64(((uint64_t)1)<<2))
#define IB_MPR_COMPMASK_HOPLIMIT	(CL_HTON64(((uint64_t)1)<<3))
#define IB_MPR_COMPMASK_TCLASS		(CL_HTON64(((uint64_t)1)<<4))
#define IB_MPR_COMPMASK_REVERSIBLE	(CL_HTON64(((uint64_t)1)<<5))
#define IB_MPR_COMPMASK_NUMBPATH	(CL_HTON64(((uint64_t)1)<<6))
#define IB_MPR_COMPMASK_PKEY		(CL_HTON64(((uint64_t)1)<<7))
#define IB_MPR_COMPMASK_QOS_CLASS	(CL_HTON64(((uint64_t)1)<<8))
#define IB_MPR_COMPMASK_SL		(CL_HTON64(((uint64_t)1)<<9))
#define IB_MPR_COMPMASK_MTUSELEC	(CL_HTON64(((uint64_t)1)<<10))
#define IB_MPR_COMPMASK_MTU		(CL_HTON64(((uint64_t)1)<<11))
#define IB_MPR_COMPMASK_RATESELEC	(CL_HTON64(((uint64_t)1)<<12))
#define IB_MPR_COMPMASK_RATE		(CL_HTON64(((uint64_t)1)<<13))
#define IB_MPR_COMPMASK_PKTLIFETIMESELEC (CL_HTON64(((uint64_t)1)<<14))
#define IB_MPR_COMPMASK_PKTLIFETIME	(CL_HTON64(((uint64_t)1)<<15))
#define IB_MPR_COMPMASK_SERVICEID_MSB	(CL_HTON64(((uint64_t)1)<<16))
#define IB_MPR_COMPMASK_INDEPSELEC	(CL_HTON64(((uint64_t)1)<<17))
#define IB_MPR_COMPMASK_RESV3		(CL_HTON64(((uint64_t)1)<<18))
#define IB_MPR_COMPMASK_SGIDCOUNT	(CL_HTON64(((uint64_t)1)<<19))
#define IB_MPR_COMPMASK_DGIDCOUNT	(CL_HTON64(((uint64_t)1)<<20))
#define IB_MPR_COMPMASK_SERVICEID_LSB	(CL_HTON64(((uint64_t)1)<<21))

#define IB_MPR_COMPMASK_SERVICEID (IB_MPR_COMPMASK_SERVICEID_MSB | \
				   IB_MPR_COMPMASK_SERVICEID_LSB)

/* SMInfo Record Component Masks */
#define IB_SMIR_COMPMASK_LID		(CL_HTON64(((uint64_t)1)<<0))
#define IB_SMIR_COMPMASK_RESV0		(CL_HTON64(((uint64_t)1)<<1))
#define IB_SMIR_COMPMASK_GUID		(CL_HTON64(((uint64_t)1)<<2))
#define IB_SMIR_COMPMASK_SMKEY		(CL_HTON64(((uint64_t)1)<<3))
#define IB_SMIR_COMPMASK_ACTCOUNT	(CL_HTON64(((uint64_t)1)<<4))
#define IB_SMIR_COMPMASK_PRIORITY	(CL_HTON64(((uint64_t)1)<<5))
#define IB_SMIR_COMPMASK_SMSTATE	(CL_HTON64(((uint64_t)1)<<6))

/* InformInfo Record Component Masks */
#define IB_IIR_COMPMASK_SUBSCRIBERGID	(CL_HTON64(((uint64_t)1)<<0))
#define IB_IIR_COMPMASK_ENUM		(CL_HTON64(((uint64_t)1)<<1))
#define IB_IIR_COMPMASK_RESV0		(CL_HTON64(((uint64_t)1)<<2))
#define IB_IIR_COMPMASK_GID		(CL_HTON64(((uint64_t)1)<<3))
#define IB_IIR_COMPMASK_LIDRANGEBEGIN	(CL_HTON64(((uint64_t)1)<<4))
#define IB_IIR_COMPMASK_LIDRANGEEND	(CL_HTON64(((uint64_t)1)<<5))
#define IB_IIR_COMPMASK_RESV1		(CL_HTON64(((uint64_t)1)<<6))
#define IB_IIR_COMPMASK_ISGENERIC	(CL_HTON64(((uint64_t)1)<<7))
#define IB_IIR_COMPMASK_SUBSCRIBE	(CL_HTON64(((uint64_t)1)<<8))
#define IB_IIR_COMPMASK_TYPE		(CL_HTON64(((uint64_t)1)<<9))
#define IB_IIR_COMPMASK_TRAPNUMB	(CL_HTON64(((uint64_t)1)<<10))
#define IB_IIR_COMPMASK_DEVICEID	(CL_HTON64(((uint64_t)1)<<10))
#define IB_IIR_COMPMASK_QPN		(CL_HTON64(((uint64_t)1)<<11))
#define IB_IIR_COMPMASK_RESV2		(CL_HTON64(((uint64_t)1)<<12))
#define IB_IIR_COMPMASK_RESPTIME	(CL_HTON64(((uint64_t)1)<<13))
#define IB_IIR_COMPMASK_RESV3		(CL_HTON64(((uint64_t)1)<<14))
#define IB_IIR_COMPMASK_PRODTYPE	(CL_HTON64(((uint64_t)1)<<15))
#define IB_IIR_COMPMASK_VENDID		(CL_HTON64(((uint64_t)1)<<15))

/* Number of Service Levels in InfiniBand */
#define IB_NUMBER_OF_SLS 16

/****f* IBA Base: Types/ib_path_rec_init_local
* NAME
*	ib_path_rec_init_local
*
* DESCRIPTION
*	Initializes a subnet local path record.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_path_rec_init_local(IN ib_path_rec_t * const p_rec,
		       IN ib_gid_t * const p_dgid,
		       IN ib_gid_t * const p_sgid,
		       IN ib_net16_t dlid,
		       IN ib_net16_t slid,
		       IN uint8_t num_path,
		       IN ib_net16_t pkey,
		       IN uint8_t sl,
		       IN uint16_t qos_class,
		       IN uint8_t mtu_selector,
		       IN uint8_t mtu,
		       IN uint8_t rate_selector,
		       IN uint8_t rate,
		       IN uint8_t pkt_life_selector,
		       IN uint8_t pkt_life, IN uint8_t preference)
{
	p_rec->dgid = *p_dgid;
	p_rec->sgid = *p_sgid;
	p_rec->dlid = dlid;
	p_rec->slid = slid;
	p_rec->num_path = num_path;
	p_rec->pkey = pkey;
	p_rec->qos_class_sl = cl_hton16((sl & IB_PATH_REC_SL_MASK) |
					(qos_class << 4));
	p_rec->mtu = (uint8_t) ((mtu & IB_PATH_REC_BASE_MASK) |
				(uint8_t) (mtu_selector << 6));
	p_rec->rate = (uint8_t) ((rate & IB_PATH_REC_BASE_MASK) |
				 (uint8_t) (rate_selector << 6));
	p_rec->pkt_life = (uint8_t) ((pkt_life & IB_PATH_REC_BASE_MASK) |
				     (uint8_t) (pkt_life_selector << 6));
	p_rec->preference = preference;

	/* Clear global routing fields for local path records */
	p_rec->hop_flow_raw = 0;
	p_rec->tclass = 0;
	p_rec->service_id = 0;

	memset(p_rec->resv2, 0, sizeof(p_rec->resv2));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
*	dgid
*		[in] GID of destination port.
*
*	sgid
*		[in] GID of source port.
*
*	dlid
*		[in] LID of destination port.
*
*	slid
*		[in] LID of source port.
*
*	num_path
*		[in] Reversible path - 1 bit to say if path is reversible.
*		num_path [6:0] In queries, maximum number of paths to return.
*		In responses, undefined.
*
*	pkey
*		[in] Partition key (P_Key) to use on this path.
*
*	qos_class
*		[in] QoS class to use on this path.  Lower 12-bits are valid.
*
*	sl
*		[in] Service level to use on this path.  Lower 4-bits are valid.
*
*	mtu_selector
*		[in] Encoded MTU selector value to use on this path
*
*	mtu
*		[in] Encoded MTU to use on this path
*
*	rate_selector
*		[in] Encoded rate selector value to use on this path.
*
*	rate
*		[in] Encoded rate to use on this path.
*
*	pkt_life_selector
*		[in] Encoded Packet selector value lifetime for this path.
*
*	pkt_life
*		[in] Encoded Packet lifetime for this path.
*
*	preference
*		[in] Indicates the relative merit of this path versus other path
*		records returned from the SA.  Lower numbers are better.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* IBA Base: Types/ib_path_rec_num_path
* NAME
*	ib_path_rec_num_path
*
* DESCRIPTION
*	Get max number of paths to return.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_num_path(IN const ib_path_rec_t * const p_rec)
{
	return (p_rec->num_path & 0x7F);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Maximum number of paths to return for each unique SGID_DGID combination.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_set_sl
* NAME
*	ib_path_rec_set_sl
*
* DESCRIPTION
*	Set path service level.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_path_rec_set_sl(IN ib_path_rec_t * const p_rec, IN const uint8_t sl)
{
	p_rec->qos_class_sl =
	    (p_rec->qos_class_sl & CL_HTON16(IB_PATH_REC_QOS_CLASS_MASK)) |
	    cl_hton16(sl & IB_PATH_REC_SL_MASK);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
*	sl
*		[in] Service level to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_sl
* NAME
*	ib_path_rec_sl
*
* DESCRIPTION
*	Get path service level.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_sl(IN const ib_path_rec_t * const p_rec)
{
	return (uint8_t)(cl_ntoh16(p_rec->qos_class_sl) & IB_PATH_REC_SL_MASK);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	SL.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_set_qos_class
* NAME
*	ib_path_rec_set_qos_class
*
* DESCRIPTION
*	Set path QoS class.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_path_rec_set_qos_class(IN ib_path_rec_t * const p_rec,
			  IN const uint16_t qos_class)
{
	p_rec->qos_class_sl =
	    (p_rec->qos_class_sl & CL_HTON16(IB_PATH_REC_SL_MASK)) |
	    cl_hton16(qos_class << 4);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
*	qos_class
*		[in] QoS class to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_qos_class
* NAME
*	ib_path_rec_qos_class
*
* DESCRIPTION
*	Get QoS class.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_path_rec_qos_class(IN const ib_path_rec_t * const p_rec)
{
	return (cl_ntoh16(p_rec->qos_class_sl) >> 4);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	QoS class of the path record.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_mtu
* NAME
*	ib_path_rec_mtu
*
* DESCRIPTION
*	Get encoded path MTU.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_mtu(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->mtu & IB_PATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path MTU.
*		1: 256
*		2: 512
*		3: 1024
*		4: 2048
*		5: 4096
*		others: reserved
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_mtu_sel
* NAME
*	ib_path_rec_mtu_sel
*
* DESCRIPTION
*	Get encoded path MTU selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_mtu_sel(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) ((p_rec->mtu & IB_PATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path MTU selector value (for queries).
*		0: greater than MTU specified
*		1: less than MTU specified
*		2: exactly the MTU specified
*		3: largest MTU available
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_rate
* NAME
*	ib_path_rec_rate
*
* DESCRIPTION
*	Get encoded path rate.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_rate(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->rate & IB_PATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path rate.
*		2: 2.5 Gb/sec.
*		3: 10 Gb/sec.
*		4: 30 Gb/sec.
*		5: 5 Gb/sec.
*		6: 20 Gb/sec.
*		7: 40 Gb/sec.
*		8: 60 Gb/sec.
*		9: 80 Gb/sec.
*		10: 120 Gb/sec.
*		11: 14 Gb/sec.
*		12: 56 Gb/sec.
*		13: 112 Gb/sec.
*		14: 168 Gb/sec.
*		15: 25 Gb/sec.
*		16: 100 Gb/sec.
*		17: 200 Gb/sec.
*		18: 300 Gb/sec.
*		19: 28 Gb/sec.
*		20: 50 Gb/sec.
*		21: 400 Gb/sec.
*		22: 600 Gb/sec.
*		23: 800 Gb/sec.
*		24: 1200 Gb/sec.
*		25: 1600 Gb/sec.
*		26: 2400 Gb/sec.
*		others: reserved
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_rate_sel
* NAME
*	ib_path_rec_rate_sel
*
* DESCRIPTION
*	Get encoded path rate selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_rate_sel(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) ((p_rec->rate & IB_PATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path rate selector value (for queries).
*		0: greater than rate specified
*		1: less than rate specified
*		2: exactly the rate specified
*		3: largest rate available
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_pkt_life
* NAME
*	ib_path_rec_pkt_life
*
* DESCRIPTION
*	Get encoded path pkt_life.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_pkt_life(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->pkt_life & IB_PATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path pkt_life = 4.096 usec * 2 ** PacketLifeTime.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_pkt_life_sel
* NAME
*	ib_path_rec_pkt_life_sel
*
* DESCRIPTION
*	Get encoded path pkt_lifetime selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_pkt_life_sel(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) ((p_rec->pkt_life & IB_PATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Encoded path pkt_lifetime selector value (for queries).
*		0: greater than rate specified
*		1: less than rate specified
*		2: exactly the rate specified
*		3: smallest packet lifetime available
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_flow_lbl
* NAME
*	ib_path_rec_flow_lbl
*
* DESCRIPTION
*	Get flow label.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_path_rec_flow_lbl(IN const ib_path_rec_t * const p_rec)
{
	return (((cl_ntoh32(p_rec->hop_flow_raw) >> 8) & 0x000FFFFF));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Flow label of the path record.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_set_flow_lbl
* NAME
*	ib_path_rec_set_flow_lbl
*
* DESCRIPTION
*	Set flow label.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_path_rec_set_flow_lbl(IN OUT ib_path_rec_t * const p_rec,
			 IN const uint32_t flow_label)
{
	p_rec->hop_flow_raw = (p_rec->hop_flow_raw &
				CL_HTON32(~IB_PATH_REC_FLOW_LABEL_MASK)) |
			       (cl_hton32(flow_label << 8));
}

/*
* PARAMETERS
*	p_rec
*		[in][out] Pointer to the path record object.
*
*	flow_label
*		[in] Flow label to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_hop_limit
* NAME
*	ib_path_rec_hop_limit
*
* DESCRIPTION
*	Get hop limit.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_rec_hop_limit(IN const ib_path_rec_t * const p_rec)
{
	return ((uint8_t) (cl_ntoh32(p_rec->hop_flow_raw) & 0x000000FF));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
* RETURN VALUES
*	Hop limit of the path record.
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****f* IBA Base: Types/ib_path_rec_set_hop_limit
* NAME
*	ib_path_rec_set_hop_limit
*
* DESCRIPTION
*	Set hop limit value of path record object.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_path_rec_set_hop_limit(IN ib_path_rec_t * const p_rec,
			  IN const uint8_t hop_limit)
{
	p_rec->hop_flow_raw =
	    (p_rec->hop_flow_raw & CL_HTON32(~IB_PATH_REC_HOPLIMIT_MASK)) |
	    cl_hton32(hop_limit);
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the path record object.
*
*	hop_limit
*		[in] Hop limit to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_path_rec_t
*********/

/****s* IBA Base: Constants/IB_CLASS_CAP_TRAP
* NAME
*	IB_CLASS_CAP_TRAP
*
* DESCRIPTION
*	ClassPortInfo CapabilityMask bits.  This bit will be set
*	if the class supports Trap() MADs (13.4.8.1).
*
* SEE ALSO
*	ib_class_port_info_t, IB_CLASS_CAP_GETSET, IB_CLASS_CAP_CAPMASK2
*
* SOURCE
*/
#define IB_CLASS_CAP_TRAP					0x0001
/*********/

/****s* IBA Base: Constants/IB_CLASS_CAP_GETSET
* NAME
*	IB_CLASS_CAP_GETSET
*
* DESCRIPTION
*	ClassPortInfo CapabilityMask bits.  This bit will be set
*	if the class supports Get(Notice) and Set(Notice) MADs (13.4.8.1).
*
* SEE ALSO
*	ib_class_port_info_t, IB_CLASS_CAP_TRAP, IB_CLASS_CAP_CAPMASK2
*
* SOURCE
*/
#define IB_CLASS_CAP_GETSET					0x0002
/*********/

/****s* IBA Base: Constants/IB_CLASS_CAP_CAPMASK2
* NAME
*	IB_CLASS_CAP_CAPMASK2
*
* DESCRIPTION
*	ClassPortInfo CapabilityMask bits.
*	This bit will be set of the class supports additional class specific
*	capabilities (CapabilityMask2) (13.4.8.1).
*
* SEE ALSO
*	ib_class_port_info_t, IB_CLASS_CAP_TRAP, IB_CLASS_CAP_GETSET
*
* SOURCE
*/
#define IB_CLASS_CAP_CAPMASK2					0x0004
/*********/

/****s* IBA Base: Constants/IB_CLASS_ENH_PORT0_CC_MASK
* NAME
*	IB_CLASS_ENH_PORT0_CC_MASK
*
* DESCRIPTION
*	ClassPortInfo CapabilityMask bits.
*	Switch only: This bit will be set if the EnhancedPort0
*	supports CA Congestion Control (A10.4.3.1).
*
* SEE ALSO
*	ib_class_port_info_t
*
* SOURCE
*/
#define IB_CLASS_ENH_PORT0_CC_MASK			0x0100
/*********/

/****s* IBA Base: Constants/IB_CLASS_RESP_TIME_MASK
* NAME
*	IB_CLASS_RESP_TIME_MASK
*
* DESCRIPTION
*	Mask bits to extract the response time value from the
*	cap_mask2_resp_time field of ib_class_port_info_t.
*
* SEE ALSO
*	ib_class_port_info_t
*
* SOURCE
*/
#define IB_CLASS_RESP_TIME_MASK				0x1F
/*********/

/****s* IBA Base: Constants/IB_CLASS_CAPMASK2_SHIFT
* NAME
*	IB_CLASS_CAPMASK2_SHIFT
*
* DESCRIPTION
*	Number of bits to shift to extract the capability mask2
*	from the cap_mask2_resp_time field of ib_class_port_info_t.
*
* SEE ALSO
*	ib_class_port_info_t
*
* SOURCE
*/
#define IB_CLASS_CAPMASK2_SHIFT				5
/*********/

/****s* IBA Base: Types/ib_class_port_info_t
* NAME
*	ib_class_port_info_t
*
* DESCRIPTION
*	IBA defined ClassPortInfo attribute (13.4.8.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_class_port_info {
	uint8_t base_ver;
	uint8_t class_ver;
	ib_net16_t cap_mask;
	ib_net32_t cap_mask2_resp_time;
	ib_gid_t redir_gid;
	ib_net32_t redir_tc_sl_fl;
	ib_net16_t redir_lid;
	ib_net16_t redir_pkey;
	ib_net32_t redir_qp;
	ib_net32_t redir_qkey;
	ib_gid_t trap_gid;
	ib_net32_t trap_tc_sl_fl;
	ib_net16_t trap_lid;
	ib_net16_t trap_pkey;
	ib_net32_t trap_hop_qp;
	ib_net32_t trap_qkey;
} PACK_SUFFIX ib_class_port_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	base_ver
*		Maximum supported MAD Base Version.
*
*	class_ver
*		Maximum supported management class version.
*
*	cap_mask
*		Supported capabilities of this management class.
*
*	cap_mask2_resp_time
*		Maximum expected response time and additional
*		supported capabilities of this management class.
*
*	redir_gid
*		GID to use for redirection, or zero
*
*	redir_tc_sl_fl
*		Traffic class, service level and flow label the requester
*		should use if the service is redirected.
*
*	redir_lid
*		LID used for redirection, or zero
*
*	redir_pkey
*		P_Key used for redirection
*
*	redir_qp
*		QP number used for redirection
*
*	redir_qkey
*		Q_Key associated with the redirected QP.  This shall be the
*		well known Q_Key value.
*
*	trap_gid
*		GID value used for trap messages from this service.
*
*	trap_tc_sl_fl
*		Traffic class, service level and flow label used for
*		trap messages originated by this service.
*
*	trap_lid
*		LID used for trap messages, or zero
*
*	trap_pkey
*		P_Key used for trap messages
*
*	trap_hop_qp
*		Hop limit (upper 8 bits) and QP number used for trap messages
*
*	trap_qkey
*		Q_Key associated with the trap messages QP.
*
* SEE ALSO
*	IB_CLASS_CAP_GETSET, IB_CLASS_CAP_TRAP
*
*********/

#define IB_PM_ALL_PORT_SELECT			(CL_HTON16(((uint16_t)1)<<8))
#define IB_PM_EXT_WIDTH_SUPPORTED		(CL_HTON16(((uint16_t)1)<<9))
#define IB_PM_EXT_WIDTH_NOIETF_SUP		(CL_HTON16(((uint16_t)1)<<10))
#define IB_PM_SAMPLES_ONLY_SUP			(CL_HTON16(((uint16_t)1)<<11))
#define IB_PM_PC_XMIT_WAIT_SUP			(CL_HTON16(((uint16_t)1)<<12))
#define IS_PM_INH_LMTD_PKEY_MC_CONSTR_ERR	(CL_HTON16(((uint16_t)1)<<13))
#define IS_PM_RSFEC_COUNTERS_SUP		(CL_HTON16(((uint16_t)1)<<14))
#define IB_PM_IS_QP1_DROP_SUP			(CL_HTON16(((uint16_t)1)<<15))
/* CapabilityMask2 */
#define IB_PM_IS_PM_KEY_SUPPORTED		(CL_HTON32(((uint32_t)1)<<0))
#define IB_PM_IS_ADDL_PORT_CTRS_EXT_SUP		(CL_HTON32(((uint32_t)1)<<1))

/****f* IBA Base: Types/ib_class_set_resp_time_val
* NAME
*	ib_class_set_resp_time_val
*
* DESCRIPTION
*	Set maximum expected response time.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_class_set_resp_time_val(IN ib_class_port_info_t * const p_cpi,
			   IN const uint8_t val)
{
	p_cpi->cap_mask2_resp_time =
	    (p_cpi->cap_mask2_resp_time & CL_HTON32(~IB_CLASS_RESP_TIME_MASK)) |
	    cl_hton32(val & IB_CLASS_RESP_TIME_MASK);
}

/*
* PARAMETERS
*	p_cpi
*		[in] Pointer to the class port info object.
*
*	val
*		[in] Response time value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_class_port_info_t
*********/

/****f* IBA Base: Types/ib_class_resp_time_val
* NAME
*	ib_class_resp_time_val
*
* DESCRIPTION
*	Get response time value.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_class_resp_time_val(IN ib_class_port_info_t * const p_cpi)
{
	return (uint8_t)(cl_ntoh32(p_cpi->cap_mask2_resp_time) &
			 IB_CLASS_RESP_TIME_MASK);
}

/*
* PARAMETERS
*	p_cpi
*		[in] Pointer to the class port info object.
*
* RETURN VALUES
*	Response time value.
*
* NOTES
*
* SEE ALSO
*	ib_class_port_info_t
*********/

/****f* IBA Base: Types/ib_class_set_cap_mask2
* NAME
*	ib_class_set_cap_mask2
*
* DESCRIPTION
*	Set ClassPortInfo:CapabilityMask2.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_class_set_cap_mask2(IN ib_class_port_info_t * const p_cpi,
		       IN const uint32_t cap_mask2)
{
	p_cpi->cap_mask2_resp_time = (p_cpi->cap_mask2_resp_time &
		CL_HTON32(IB_CLASS_RESP_TIME_MASK)) |
		cl_hton32(cap_mask2 << IB_CLASS_CAPMASK2_SHIFT);
}

/*
* PARAMETERS
*	p_cpi
*		[in] Pointer to the class port info object.
*
*	cap_mask2
*		[in] CapabilityMask2 value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_class_port_info_t
*********/

/****f* IBA Base: Types/ib_class_cap_mask2
* NAME
*	ib_class_cap_mask2
*
* DESCRIPTION
*	Get ClassPortInfo:CapabilityMask2.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_class_cap_mask2(IN const ib_class_port_info_t * const p_cpi)
{
	return (cl_ntoh32(p_cpi->cap_mask2_resp_time) >> IB_CLASS_CAPMASK2_SHIFT);
}

/*
* PARAMETERS
*	p_cpi
*		[in] Pointer to the class port info object.
*
* RETURN VALUES
*	CapabilityMask2 of the ClassPortInfo.
*
* NOTES
*
* SEE ALSO
*	ib_class_port_info_t
*********/

/****s* IBA Base: Types/ib_sm_info_t
* NAME
*	ib_sm_info_t
*
* DESCRIPTION
*	SMInfo structure (14.2.5.13).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_sm_info {
	ib_net64_t guid;
	ib_net64_t sm_key;
	ib_net32_t act_count;
	uint8_t pri_state;
} PACK_SUFFIX ib_sm_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	guid
*		Port GUID for this SM.
*
*	sm_key
*		SM_Key of this SM.
*
*	act_count
*		Activity counter used as a heartbeat.
*
*	pri_state
*		Priority and State information
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_sminfo_get_priority
* NAME
*	ib_sminfo_get_priority
*
* DESCRIPTION
*	Returns the priority value.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_sminfo_get_priority(IN const ib_sm_info_t * const p_smi)
{
	return ((uint8_t) ((p_smi->pri_state & 0xF0) >> 4));
}

/*
* PARAMETERS
*	p_smi
*		[in] Pointer to the SMInfo Attribute.
*
* RETURN VALUES
*	Returns the priority value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_sminfo_get_state
* NAME
*	ib_sminfo_get_state
*
* DESCRIPTION
*	Returns the state value.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_sminfo_get_state(IN const ib_sm_info_t * const p_smi)
{
	return ((uint8_t) (p_smi->pri_state & 0x0F));
}

/*
* PARAMETERS
*	p_smi
*		[in] Pointer to the SMInfo Attribute.
*
* RETURN VALUES
*	Returns the state value.
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_mad_t
* NAME
*	ib_mad_t
*
* DESCRIPTION
*	IBA defined MAD header (13.4.3)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_mad {
	uint8_t base_ver;
	uint8_t mgmt_class;
	uint8_t class_ver;
	uint8_t method;
	ib_net16_t status;
	ib_net16_t class_spec;
	ib_net64_t trans_id;
	ib_net16_t attr_id;
	ib_net16_t additional_status;
	ib_net32_t attr_mod;
} PACK_SUFFIX ib_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	base_ver
*		MAD base format.
*
*	mgmt_class
*		Class of operation.
*
*	class_ver
*		Version of MAD class-specific format.
*
*	method
*		Method to perform, including 'R' bit.
*
*	status
*		Status of operation.
*
*	class_spec
*		Reserved for subnet management.
*
*	trans_id
*		Transaction ID.
*
*	attr_id
*		Attribute ID.
*
*	additional_status
*		Optional additional status.
*
*	attr_mod
*		Attribute modifier.
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_rmpp_mad_t
* NAME
*	ib_rmpp_mad_t
*
* DESCRIPTION
*	IBA defined MAD RMPP header (13.6.2.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rmpp_mad {
	ib_mad_t common_hdr;
	uint8_t rmpp_version;
	uint8_t rmpp_type;
	uint8_t rmpp_flags;
	uint8_t rmpp_status;
	ib_net32_t seg_num;
	ib_net32_t paylen_newwin;
} PACK_SUFFIX ib_rmpp_mad_t;
#include <complib/cl_packoff.h>
/*
* SEE ALSO
*	ib_mad_t
*********/

/****f* IBA Base: Types/ib_mad_init_new
* NAME
*	ib_mad_init_new
*
* DESCRIPTION
*	Initializes a MAD common header.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mad_init_new(IN ib_mad_t * const p_mad,
		IN const uint8_t mgmt_class,
		IN const uint8_t class_ver,
		IN const uint8_t method,
		IN const ib_net64_t trans_id,
		IN const ib_net16_t attr_id, IN const ib_net32_t attr_mod)
{
	CL_ASSERT(p_mad);
	p_mad->base_ver = 1;
	p_mad->mgmt_class = mgmt_class;
	p_mad->class_ver = class_ver;
	p_mad->method = method;
	p_mad->status = 0;
	p_mad->class_spec = 0;
	p_mad->trans_id = trans_id;
	p_mad->attr_id = attr_id;
	p_mad->additional_status = 0;
	p_mad->attr_mod = attr_mod;
}

/*
* PARAMETERS
*	p_mad
*		[in] Pointer to the MAD common header.
*
*	mgmt_class
*		[in] Class of operation.
*
*	class_ver
*		[in] Version of MAD class-specific format.
*
*	method
*		[in] Method to perform, including 'R' bit.
*
*	trans_Id
*		[in] Transaction ID.
*
*	attr_id
*		[in] Attribute ID.
*
*	attr_mod
*		[in] Attribute modifier.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	ib_mad_t
*********/

/****f* IBA Base: Types/ib_mad_init_response
* NAME
*	ib_mad_init_response
*
* DESCRIPTION
*	Initializes a MAD common header as a response.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mad_init_response(IN const ib_mad_t * const p_req_mad,
		     IN ib_mad_t * const p_mad, IN const ib_net16_t status)
{
	CL_ASSERT(p_req_mad);
	CL_ASSERT(p_mad);
	*p_mad = *p_req_mad;
	p_mad->status = status;
	if (p_mad->method == IB_MAD_METHOD_SET)
		p_mad->method = IB_MAD_METHOD_GET;
	p_mad->method |= IB_MAD_METHOD_RESP_MASK;
}

/*
* PARAMETERS
*	p_req_mad
*		[in] Pointer to the MAD common header in the original request MAD.
*
*	p_mad
*		[in] Pointer to the MAD common header to initialize.
*
*	status
*		[in] MAD Status value to return;
*
* RETURN VALUES
*	None.
*
* NOTES
*	p_req_mad and p_mad may point to the same MAD.
*
* SEE ALSO
*	ib_mad_t
*********/

/****f* IBA Base: Types/ib_mad_is_response
* NAME
*	ib_mad_is_response
*
* DESCRIPTION
*	Returns TRUE if the MAD is a response ('R' bit set)
*	or if the MAD is a TRAP REPRESS,
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mad_is_response(IN const ib_mad_t * const p_mad)
{
	CL_ASSERT(p_mad);
	return (p_mad->method & IB_MAD_METHOD_RESP_MASK ||
		p_mad->method == IB_MAD_METHOD_TRAP_REPRESS);
}

/*
* PARAMETERS
*	p_mad
*		[in] Pointer to the MAD.
*
* RETURN VALUES
*	Returns TRUE if the MAD is a response ('R' bit set),
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	ib_mad_t
*********/

#define IB_RMPP_TYPE_DATA		1
#define IB_RMPP_TYPE_ACK		2
#define IB_RMPP_TYPE_STOP		3
#define IB_RMPP_TYPE_ABORT		4

#define IB_RMPP_NO_RESP_TIME		0x1F
#define IB_RMPP_FLAG_ACTIVE		0x01
#define IB_RMPP_FLAG_FIRST		0x02
#define IB_RMPP_FLAG_LAST		0x04

#define IB_RMPP_STATUS_SUCCESS		0
#define IB_RMPP_STATUS_RESX		1	/* resources exhausted */
#define IB_RMPP_STATUS_T2L		118	/* time too long */
#define IB_RMPP_STATUS_BAD_LEN		119	/* incon. last and payload len */
#define IB_RMPP_STATUS_BAD_SEG		120	/* incon. first and segment no */
#define IB_RMPP_STATUS_BADT		121	/* bad rmpp type */
#define IB_RMPP_STATUS_W2S		122	/* newwindowlast too small */
#define IB_RMPP_STATUS_S2B		123	/* segment no too big */
#define IB_RMPP_STATUS_BAD_STATUS	124	/* illegal status */
#define IB_RMPP_STATUS_UNV		125	/* unsupported version */
#define IB_RMPP_STATUS_TMR		126	/* too many retries */
#define IB_RMPP_STATUS_UNSPEC		127	/* unspecified */

/****f* IBA Base: Types/ib_rmpp_is_flag_set
* NAME
*	ib_rmpp_is_flag_set
*
* DESCRIPTION
*	Returns TRUE if the MAD has the given RMPP flag set.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_rmpp_is_flag_set(IN const ib_rmpp_mad_t * const p_rmpp_mad,
		    IN const uint8_t flag)
{
	CL_ASSERT(p_rmpp_mad);
	return ((p_rmpp_mad->rmpp_flags & flag) == flag);
}

/*
* PARAMETERS
*	ib_rmpp_mad_t
*		[in] Pointer to a MAD with an RMPP header.
*
*	flag
*		[in] The RMPP flag being examined.
*
* RETURN VALUES
*	Returns TRUE if the MAD has the given RMPP flag set.
*
* NOTES
*
* SEE ALSO
*	ib_mad_t, ib_rmpp_mad_t
*********/

static inline void OSM_API
ib_rmpp_set_resp_time(IN ib_rmpp_mad_t * const p_rmpp_mad,
		      IN const uint8_t resp_time)
{
	CL_ASSERT(p_rmpp_mad);
	p_rmpp_mad->rmpp_flags |= (resp_time << 3);
}

static inline uint8_t OSM_API
ib_rmpp_get_resp_time(IN const ib_rmpp_mad_t * const p_rmpp_mad)
{
	CL_ASSERT(p_rmpp_mad);
	return ((uint8_t) (p_rmpp_mad->rmpp_flags >> 3));
}

/****d* IBA Base: Constants/IB_SMP_DIRECTION
* NAME
*	IB_SMP_DIRECTION
*
* DESCRIPTION
*	The Direction bit for directed route SMPs.
*
* SOURCE
*/
#define IB_SMP_DIRECTION_HO		0x8000
#define IB_SMP_DIRECTION		(CL_HTON16(IB_SMP_DIRECTION_HO))
/**********/

/****d* IBA Base: Constants/IB_SMP_STATUS_MASK
* NAME
*	IB_SMP_STATUS_MASK
*
* DESCRIPTION
*	Mask value for extracting status from a directed route SMP.
*
* SOURCE
*/
#define IB_SMP_STATUS_MASK_HO		0x7FFF
#define IB_SMP_STATUS_MASK		(CL_HTON16(IB_SMP_STATUS_MASK_HO))
/**********/

/****s* IBA Base: Types/ib_smp_t
* NAME
*	ib_smp_t
*
* DESCRIPTION
*	IBA defined SMP. (14.2.1.2)
*
* SYNOPSIS
*/
#define IB_SMP_DATA_SIZE 64
#include <complib/cl_packon.h>
typedef struct _ib_smp {
	uint8_t base_ver;
	uint8_t mgmt_class;
	uint8_t class_ver;
	uint8_t method;
	ib_net16_t status;
	uint8_t hop_ptr;
	uint8_t hop_count;
	ib_net64_t trans_id;
	ib_net16_t attr_id;
	ib_net16_t additional_status;
	ib_net32_t attr_mod;
	ib_net64_t m_key;
	ib_net16_t dr_slid;
	ib_net16_t dr_dlid;
	uint32_t resv1[7];
	uint8_t data[IB_SMP_DATA_SIZE];
	uint8_t initial_path[IB_SUBNET_PATH_HOPS_MAX];
	uint8_t return_path[IB_SUBNET_PATH_HOPS_MAX];
} PACK_SUFFIX ib_smp_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	base_ver
*		MAD base format.
*
*	mgmt_class
*		Class of operation.
*
*	class_ver
*		Version of MAD class-specific format.
*
*	method
*		Method to perform, including 'R' bit.
*
*	status
*		Status of operation.
*
*	hop_ptr
*		Hop pointer for directed route MADs.
*
*	hop_count
*		Hop count for directed route MADs.
*
*	trans_Id
*		Transaction ID.
*
*	attr_id
*		Attribute ID.
*
*	additional_status
*		Optional additional status.
*
*	attr_mod
*		Attribute modifier.
*
*	m_key
*		Management key value.
*
*	dr_slid
*		Directed route source LID.
*
*	dr_dlid
*		Directed route destination LID.
*
*	resv0
*		Reserved for 64 byte alignment.
*
*	data
*		MAD data payload.
*
*	initial_path
*		Outbound port list.
*
*	return_path
*		Inbound port list.
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_smp_get_status
* NAME
*	ib_smp_get_status
*
* DESCRIPTION
*	Returns the SMP status value in network order.
*
* SYNOPSIS
*/
static inline ib_net16_t OSM_API
ib_smp_get_status(IN const ib_smp_t * const p_smp)
{
	return ((ib_net16_t) (p_smp->status & IB_SMP_STATUS_MASK));
}

/*
* PARAMETERS
*	p_smp
*		[in] Pointer to the SMP packet.
*
* RETURN VALUES
*	Returns the SMP status value in network order.
*
* NOTES
*
* SEE ALSO
*	ib_smp_t
*********/

/****f* IBA Base: Types/ib_smp_is_response
* NAME
*	ib_smp_is_response
*
* DESCRIPTION
*	Returns TRUE if the SMP is a response MAD, FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_smp_is_response(IN const ib_smp_t * const p_smp)
{
	return (ib_mad_is_response((const ib_mad_t *)p_smp));
}

/*
* PARAMETERS
*	p_smp
*		[in] Pointer to the SMP packet.
*
* RETURN VALUES
*	Returns TRUE if the SMP is a response MAD, FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	ib_smp_t
*********/

/****f* IBA Base: Types/ib_smp_is_d
* NAME
*	ib_smp_is_d
*
* DESCRIPTION
*	Returns TRUE if the SMP 'D' (direction) bit is set.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API ib_smp_is_d(IN const ib_smp_t * const p_smp)
{
	return ((p_smp->status & IB_SMP_DIRECTION) == IB_SMP_DIRECTION);
}

/*
* PARAMETERS
*	p_smp
*		[in] Pointer to the SMP packet.
*
* RETURN VALUES
*	Returns TRUE if the SMP 'D' (direction) bit is set.
*
* NOTES
*
* SEE ALSO
*	ib_smp_t
*********/

/****f* IBA Base: Types/ib_smp_init_new
* NAME
*	ib_smp_init_new
*
* DESCRIPTION
*	Initializes a MAD common header.
*
* TODO
*	This is too big for inlining, but leave it here for now
*	since there is not yet another convenient spot.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_smp_init_new(IN ib_smp_t * const p_smp,
		IN const uint8_t method,
		IN const ib_net64_t trans_id,
		IN const ib_net16_t attr_id,
		IN const ib_net32_t attr_mod,
		IN const uint8_t hop_count,
		IN const ib_net64_t m_key,
		IN const uint8_t * path_out,
		IN const ib_net16_t dr_slid, IN const ib_net16_t dr_dlid)
{
	CL_ASSERT(p_smp);
	CL_ASSERT(hop_count < IB_SUBNET_PATH_HOPS_MAX);
	p_smp->base_ver = 1;
	p_smp->mgmt_class = IB_MCLASS_SUBN_DIR;
	p_smp->class_ver = 1;
	p_smp->method = method;
	p_smp->status = 0;
	p_smp->hop_ptr = 0;
	p_smp->hop_count = hop_count;
	p_smp->trans_id = trans_id;
	p_smp->attr_id = attr_id;
	p_smp->additional_status = 0;
	p_smp->attr_mod = attr_mod;
	p_smp->m_key = m_key;
	p_smp->dr_slid = dr_slid;
	p_smp->dr_dlid = dr_dlid;

	memset(p_smp->resv1, 0,
	       sizeof(p_smp->resv1) +
	       sizeof(p_smp->data) +
	       sizeof(p_smp->initial_path) + sizeof(p_smp->return_path));

	/* copy the path */
	memcpy(&p_smp->initial_path, path_out, sizeof(p_smp->initial_path));
}

/*
* PARAMETERS
*	p_smp
*		[in] Pointer to the SMP packet.
*
*	method
*		[in] Method to perform, including 'R' bit.
*
*	trans_Id
*		[in] Transaction ID.
*
*	attr_id
*		[in] Attribute ID.
*
*	attr_mod
*		[in] Attribute modifier.
*
*	hop_count
*		[in] Number of hops in the path.
*
*	m_key
*		[in] Management key for this SMP.
*
*	path_out
*		[in] Port array for outbound path.
*
*
* RETURN VALUES
*	None.
*
* NOTES
*	Payload area is initialized to zero.
*
*
* SEE ALSO
*	ib_mad_t
*********/

/****f* IBA Base: Types/ib_smp_get_payload_ptr
* NAME
*	ib_smp_get_payload_ptr
*
* DESCRIPTION
*	Gets a pointer to the SMP payload area.
*
* SYNOPSIS
*/
static inline void *OSM_API
ib_smp_get_payload_ptr(IN const ib_smp_t * const p_smp)
{
	return ((void *)p_smp->data);
}

/*
* PARAMETERS
*	p_smp
*		[in] Pointer to the SMP packet.
*
* RETURN VALUES
*	Pointer to SMP payload area.
*
* NOTES
*
* SEE ALSO
*	ib_mad_t
*********/

/****s* IBA Base: Types/ib_node_info_t
* NAME
*	ib_node_info_t
*
* DESCRIPTION
*	IBA defined NodeInfo. (14.2.5.3)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_node_info {
	uint8_t base_version;
	uint8_t class_version;
	uint8_t node_type;
	uint8_t num_ports;
	ib_net64_t sys_guid;
	ib_net64_t node_guid;
	ib_net64_t port_guid;
	ib_net16_t partition_cap;
	ib_net16_t device_id;
	ib_net32_t revision;
	ib_net32_t port_num_vendor_id;
} PACK_SUFFIX ib_node_info_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_sa_mad_t
* NAME
*	ib_sa_mad_t
*
* DESCRIPTION
*	IBA defined SA MAD format. (15.2.1)
*
* SYNOPSIS
*/
#define IB_SA_DATA_SIZE 200

#include <complib/cl_packon.h>
typedef struct _ib_sa_mad {
	uint8_t base_ver;
	uint8_t mgmt_class;
	uint8_t class_ver;
	uint8_t method;
	ib_net16_t status;
	ib_net16_t resv;
	ib_net64_t trans_id;
	ib_net16_t attr_id;
	ib_net16_t additional_status;
	ib_net32_t attr_mod;
	uint8_t rmpp_version;
	uint8_t rmpp_type;
	uint8_t rmpp_flags;
	uint8_t rmpp_status;
	ib_net32_t seg_num;
	ib_net32_t paylen_newwin;
	ib_net64_t sm_key;
	ib_net16_t attr_offset;
	ib_net16_t resv3;
	ib_net64_t comp_mask;
	uint8_t data[IB_SA_DATA_SIZE];
} PACK_SUFFIX ib_sa_mad_t;
#include <complib/cl_packoff.h>
/**********/
#define IB_SA_MAD_HDR_SIZE (sizeof(ib_sa_mad_t) - IB_SA_DATA_SIZE)

static inline uint32_t OSM_API ib_get_attr_size(IN const ib_net16_t attr_offset)
{
	return (((uint32_t) cl_ntoh16(attr_offset)) << 3);
}

static inline ib_net16_t OSM_API ib_get_attr_offset(IN const uint32_t attr_size)
{
	return (cl_hton16((uint16_t) (attr_size >> 3)));
}

/****f* IBA Base: Types/ib_sa_mad_get_payload_ptr
* NAME
*	ib_sa_mad_get_payload_ptr
*
* DESCRIPTION
*	Gets a pointer to the SA MAD's payload area.
*
* SYNOPSIS
*/
static inline void *OSM_API
ib_sa_mad_get_payload_ptr(IN const ib_sa_mad_t * const p_sa_mad)
{
	return ((void *)p_sa_mad->data);
}

/*
* PARAMETERS
*	p_sa_mad
*		[in] Pointer to the SA MAD packet.
*
* RETURN VALUES
*	Pointer to SA MAD payload area.
*
* NOTES
*
* SEE ALSO
*	ib_mad_t
*********/

#define IB_NODE_INFO_PORT_NUM_MASK		(CL_HTON32(0xFF000000))
#define IB_NODE_INFO_VEND_ID_MASK		(CL_HTON32(0x00FFFFFF))
#if CPU_LE
#define IB_NODE_INFO_PORT_NUM_SHIFT 0
#else
#define IB_NODE_INFO_PORT_NUM_SHIFT 24
#endif

/****f* IBA Base: Types/ib_node_info_get_local_port_num
* NAME
*	ib_node_info_get_local_port_num
*
* DESCRIPTION
*	Gets the local port number from the NodeInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_node_info_get_local_port_num(IN const ib_node_info_t * const p_ni)
{
	return ((uint8_t) ((p_ni->port_num_vendor_id &
			    IB_NODE_INFO_PORT_NUM_MASK)
			   >> IB_NODE_INFO_PORT_NUM_SHIFT));
}

/*
* PARAMETERS
*	p_ni
*		[in] Pointer to a NodeInfo attribute.
*
* RETURN VALUES
*	Local port number that returned the attribute.
*
* NOTES
*
* SEE ALSO
*	ib_node_info_t
*********/

/****f* IBA Base: Types/ib_node_info_get_vendor_id
* NAME
*	ib_node_info_get_vendor_id
*
* DESCRIPTION
*	Gets the VendorID from the NodeInfo attribute.
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_node_info_get_vendor_id(IN const ib_node_info_t * const p_ni)
{
	return ((ib_net32_t) (p_ni->port_num_vendor_id &
			      IB_NODE_INFO_VEND_ID_MASK));
}

/*
* PARAMETERS
*	p_ni
*		[in] Pointer to a NodeInfo attribute.
*
* RETURN VALUES
*	VendorID that returned the attribute.
*
* NOTES
*
* SEE ALSO
*	ib_node_info_t
*********/

#define IB_NODE_DESCRIPTION_SIZE 64

#include <complib/cl_packon.h>
typedef struct _ib_node_desc {
	// Node String is an array of UTF-8 characters
	// that describe the node in text format
	// Note that this string is NOT NULL TERMINATED!
	uint8_t description[IB_NODE_DESCRIPTION_SIZE];
} PACK_SUFFIX ib_node_desc_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_node_record_t {
	ib_net16_t lid;
	ib_net16_t resv;
	ib_node_info_t node_info;
	ib_node_desc_t node_desc;
	uint8_t pad[4];
} PACK_SUFFIX ib_node_record_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_port_info_t
* NAME
*	ib_port_info_t
*
* DESCRIPTION
*	IBA defined PortInfo. (14.2.5.6)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_info {
	ib_net64_t m_key;
	ib_net64_t subnet_prefix;
	ib_net16_t base_lid;
	ib_net16_t master_sm_base_lid;
	ib_net32_t capability_mask;
	ib_net16_t diag_code;
	ib_net16_t m_key_lease_period;
	uint8_t local_port_num;
	uint8_t link_width_enabled;
	uint8_t link_width_supported;
	uint8_t link_width_active;
	uint8_t state_info1;	/* LinkSpeedSupported and PortState */
	uint8_t state_info2;	/* PortPhysState and LinkDownDefaultState */
	uint8_t mkey_lmc;	/* M_KeyProtectBits and LMC */
	uint8_t link_speed;	/* LinkSpeedEnabled and LinkSpeedActive */
	uint8_t mtu_smsl;
	uint8_t vl_cap;		/* VLCap and InitType */
	uint8_t vl_high_limit;
	uint8_t vl_arb_high_cap;
	uint8_t vl_arb_low_cap;
	uint8_t mtu_cap;
	uint8_t vl_stall_life;
	uint8_t vl_enforce;
	ib_net16_t m_key_violations;
	ib_net16_t p_key_violations;
	ib_net16_t q_key_violations;
	uint8_t guid_cap;
	uint8_t subnet_timeout;	/* cli_rereg(1b), mcast_pkey_trap_suppr(2b), timeout(5b) */
	uint8_t resp_time_value; /* reserv(3b), rtv(5b) */
	uint8_t error_threshold; /* local phy errors(4b), overrun errors(4b) */
	ib_net16_t max_credit_hint;
	ib_net32_t link_rt_latency; /*
				     * reserv(1b),
				     * LinkSpeedExtActive2(2b),
				     * LinkSpeedExtSupported2(2b),
				     * LinkSpeedExtEnabled2(3b),
				     * link round trip lat(24b)
				     */
	ib_net16_t capability_mask2;
	uint8_t link_speed_ext;	/* LinkSpeedExtActive and LinkSpeedExtSupported */
	uint8_t link_speed_ext_enabled; /* reserv(3b), LinkSpeedExtEnabled(5b) */
} PACK_SUFFIX ib_port_info_t;
#include <complib/cl_packoff.h>
/************/

#define IB_PORT_STATE_MASK			0x0F
#define IB_PORT_LMC_MASK			0x07
#define IB_PORT_LMC_MAX				0x07
#define IB_PORT_MPB_MASK			0xC0
#define IB_PORT_MPB_SHIFT			6
#define IB_PORT_MC_PKEY_TRAP_SUPPRESSION_SHIFT	5
#define IB_PORT_MC_PKEY_TRAP_SUPPRESSION_MASK	0x20
#define IB_PORT_LINK_SPEED_SHIFT		4
#define IB_PORT_LINK_SPEED_SUPPORTED_MASK	0xF0
#define IB_PORT_LINK_SPEED_ACTIVE_MASK		0xF0
#define IB_PORT_LINK_SPEED_ENABLED_MASK		0x0F
#define IB_PORT_PHYS_STATE_MASK			0xF0
#define IB_PORT_PHYS_STATE_SHIFT		4
#define IB_PORT_PHYS_STATE_NO_CHANGE		0
#define IB_PORT_PHYS_STATE_SLEEP		1
#define IB_PORT_PHYS_STATE_POLLING		2
#define IB_PORT_PHYS_STATE_DISABLED		3
#define IB_PORT_PHYS_STATE_PORTCONFTRAIN	4
#define IB_PORT_PHYS_STATE_LINKUP	        5
#define IB_PORT_PHYS_STATE_LINKERRRECOVER	6
#define IB_PORT_PHYS_STATE_PHYTEST	        7
#define IB_PORT_LNKDWNDFTSTATE_MASK		0x0F
#define IB_PORT_VLCAP_MASK			0xF0
#define IB_PORT_OPERATIONAL_VLS_MASK		0xF0
#define IB_PORT_REP_TIME_VALUE_MASK		0x1F
#define IB_PORT_LINK_SPEED_EXT_ENABLE_MASK	0x1F

/*
 * Indicate to the device that the sender supports extended port state reporting,
 * which includes state 5=active_defer.
 */
#define IB_PI_ACT_DEFER_STATE_SUPPORTED_MASK_HO     0x00008000

#define IB_PORT_CAP_RESV0         (CL_HTON32(0x00000001))
#define IB_PORT_CAP_IS_SM         (CL_HTON32(0x00000002))
#define IB_PORT_CAP_HAS_NOTICE    (CL_HTON32(0x00000004))
#define IB_PORT_CAP_HAS_TRAP      (CL_HTON32(0x00000008))
#define IB_PORT_CAP_HAS_IPD       (CL_HTON32(0x00000010))
#define IB_PORT_CAP_HAS_AUTO_MIG  (CL_HTON32(0x00000020))
#define IB_PORT_CAP_HAS_SL_MAP    (CL_HTON32(0x00000040))
#define IB_PORT_CAP_HAS_NV_MKEY   (CL_HTON32(0x00000080))
#define IB_PORT_CAP_HAS_NV_PKEY   (CL_HTON32(0x00000100))
#define IB_PORT_CAP_HAS_LED_INFO  (CL_HTON32(0x00000200))
#define IB_PORT_CAP_SM_DISAB      (CL_HTON32(0x00000400))
#define IB_PORT_CAP_HAS_SYS_IMG_GUID  (CL_HTON32(0x00000800))
#define IB_PORT_CAP_HAS_PKEY_SW_EXT_PORT_TRAP (CL_HTON32(0x00001000))
#define IB_PORT_CAP_HAS_CABLE_INFO  (CL_HTON32(0x00002000))
#define IB_PORT_CAP_HAS_EXT_SPEEDS  (CL_HTON32(0x00004000))
#define IB_PORT_CAP_HAS_CAP_MASK2 (CL_HTON32(0x00008000))
#define IB_PORT_CAP_HAS_COM_MGT   (CL_HTON32(0x00010000))
#define IB_PORT_CAP_HAS_SNMP      (CL_HTON32(0x00020000))
#define IB_PORT_CAP_REINIT        (CL_HTON32(0x00040000))
#define IB_PORT_CAP_HAS_DEV_MGT   (CL_HTON32(0x00080000))
#define IB_PORT_CAP_HAS_VEND_CLS  (CL_HTON32(0x00100000))
#define IB_PORT_CAP_HAS_DR_NTC    (CL_HTON32(0x00200000))
#define IB_PORT_CAP_HAS_CAP_NTC   (CL_HTON32(0x00400000))
#define IB_PORT_CAP_HAS_BM        (CL_HTON32(0x00800000))
#define IB_PORT_CAP_HAS_LINK_RT_LATENCY (CL_HTON32(0x01000000))
#define IB_PORT_CAP_HAS_CLIENT_REREG (CL_HTON32(0x02000000))
#define IB_PORT_CAP_HAS_OTHER_LOCAL_CHANGES_NTC (CL_HTON32(0x04000000))
#define IB_PORT_CAP_HAS_LINK_SPEED_WIDTH_PAIRS_TBL (CL_HTON32(0x08000000))
#define IB_PORT_CAP_HAS_VEND_MADS (CL_HTON32(0x10000000))
#define IB_PORT_CAP_HAS_MCAST_PKEY_TRAP_SUPPRESS (CL_HTON32(0x20000000))
#define IB_PORT_CAP_HAS_MCAST_FDB_TOP (CL_HTON32(0x40000000))
#define IB_PORT_CAP_HAS_HIER_INFO (CL_HTON32(0x80000000))

#define IB_PORT_CAP2_IS_SET_NODE_DESC_SUPPORTED (CL_HTON16(0x0001))
#define IB_PORT_CAP2_IS_PORT_INFO_EXT_SUPPORTED (CL_HTON16(0x0002))
#define IB_PORT_CAP2_IS_VIRT_SUPPORTED (CL_HTON16(0x0004))
#define IB_PORT_CAP2_IS_SWITCH_PORT_STATE_TBL_SUPP (CL_HTON16(0x0008))
#define IB_PORT_CAP2_IS_LINK_WIDTH_2X_SUPPORTED (CL_HTON16(0x0010))
#define IB_PORT_CAP2_IS_LINK_SPEED_HDR_SUPPORTED (CL_HTON16(0x0020))
#define IB_PORT_CAP2_IS_LINK_SPEED_NDR_SUPPORTED (CL_HTON16(0x0400))
#define IB_PORT_CAP2_IS_EXT_SPEEDS2_SUPPORTED (CL_HTON16(0x0800))
#define IB_PORT_CAP2_IS_LINK_SPEED_XDR_SUPPORTED (CL_HTON16(0x1000))

#define IB_PORT_CAP_VIRT_BITS	(IB_PORT_CAP_IS_SM | IB_PORT_CAP_HAS_COM_MGT | \
				 IB_PORT_CAP_HAS_SNMP | IB_PORT_CAP_HAS_DEV_MGT | \
				 IB_PORT_CAP_HAS_VEND_CLS | IB_PORT_CAP_HAS_BM)

/****s* IBA Base: Types/ib_port_info_ext_t
* NAME
*	ib_port_info_ext_t
*
* DESCRIPTION
*	IBA defined PortInfoExtended. (14.2.5.19)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_info_ext {
	ib_net32_t cap_mask;
	ib_net16_t fec_mode_active;
	ib_net16_t fdr_fec_mode_sup;
	ib_net16_t fdr_fec_mode_enable;
	ib_net16_t edr_fec_mode_sup;
	ib_net16_t edr_fec_mode_enable;
	ib_net16_t hdr_fec_mode_sup;
	ib_net16_t hdr_fec_mode_enable;
	ib_net16_t ndr_fec_mode_sup;
	ib_net16_t ndr_fec_mode_enable;
	uint8_t reserved[42];
} PACK_SUFFIX ib_port_info_ext_t;
#include <complib/cl_packoff.h>
/************/

/****f* IBA Base: Types/ib_port_info_get_port_flid
* NAME
*	ib_port_info_get_port_flid
*
* DESCRIPTION
*	Returns the port subnet prefix FLID (Nvidia Vendor specific) part.
*
* SYNOPSIS
*/
static inline ib_net16_t OSM_API
ib_port_info_get_port_flid(IN const ib_port_info_t * const p_pi)
{
	return
	cl_hton16((cl_ntoh64(p_pi->subnet_prefix & ~IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK) >> 16));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Port subnet prefix FLID part.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_port_flid
* NAME
*	ib_port_info_set_port_flid
*
* DESCRIPTION
*	Sets the port FLID (Nvidia Vendor specific) part in the subnet prefix.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_port_flid(IN ib_port_info_t * const p_pi,
			   IN const ib_net16_t flid)
{
	p_pi->subnet_prefix =
		cl_hton64((cl_ntoh64(p_pi->subnet_prefix & IB_SITE_LOCAL_VALID_BITS_SUBNET_MASK) |
		((uint64_t)(cl_ntoh16(flid)) << 16)));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	flid
*		[in] FLID value to set.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/


#define IB_PORT_EXT_NO_FEC_MODE_ACTIVE		    0
#define IB_PORT_EXT_FIRE_CODE_FEC_MODE_ACTIVE	    (CL_HTON16(0x0001))
#define IB_PORT_EXT_RS_FEC_MODE_ACTIVE		    (CL_HTON16(0x0002))
#define IB_PORT_EXT_LOW_LATENCY_RS_FEC_MODE_ACTIVE  (CL_HTON16(0x0003))

#define IB_PORT_EXT_CAP_IS_FEC_MODE_SUPPORTED (CL_HTON32(0x00000001))
/****f* IBA Base: Types/ib_port_info_get_port_state
* NAME
*	ib_port_info_get_port_state
*
* DESCRIPTION
*	Returns the port state.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_port_state(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->state_info1 & IB_PORT_STATE_MASK));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Port state.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_port_state
* NAME
*	ib_port_info_set_port_state
*
* DESCRIPTION
*	Sets the port state.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_port_state(IN ib_port_info_t * const p_pi,
			    IN const uint8_t port_state)
{
	p_pi->state_info1 = (uint8_t) ((p_pi->state_info1 & 0xF0) | port_state);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	port_state
*		[in] Port state value to set.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_vl_cap
* NAME
*	ib_port_info_get_vl_cap
*
* DESCRIPTION
*	Gets the VL Capability of a port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_vl_cap(IN const ib_port_info_t * const p_pi)
{
	return ((p_pi->vl_cap >> 4) & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	VL_CAP field
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_vl_cap
* NAME
*	ib_port_info_set_vl_cap
*
* DESCRIPTION
*	Sets the VL Capability of a port.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_vl_cap(IN ib_port_info_t * p_pi, uint8_t vl_cap)
{
	CL_ASSERT((vl_cap & 0x0F) == vl_cap);
	IBTYPES_SET_ATTR8(p_pi->vl_cap, 0xF0, 4, vl_cap);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	vl_cap
*		[in] VL cap to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_init_type
* NAME
*	ib_port_info_get_init_type
*
* DESCRIPTION
*	Gets the init type of a port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_init_type(IN const ib_port_info_t * const p_pi)
{
	return (uint8_t) (p_pi->vl_cap & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	InitType field
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_op_vls
* NAME
*	ib_port_info_get_op_vls
*
* DESCRIPTION
*	Gets the operational VLs on a port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_op_vls(IN const ib_port_info_t * const p_pi)
{
	return ((p_pi->vl_enforce >> 4) & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	OP_VLS field
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_op_vls
* NAME
*	ib_port_info_set_op_vls
*
* DESCRIPTION
*	Sets the operational VLs on a port.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_op_vls(IN ib_port_info_t * const p_pi, IN const uint8_t op_vls)
{
	p_pi->vl_enforce =
	    (uint8_t) ((p_pi->vl_enforce & 0x0F) | (op_vls << 4));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	op_vls
*		[in] Encoded operation VLs value.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_state_no_change
* NAME
*	ib_port_info_set_state_no_change
*
* DESCRIPTION
*	Sets the port state fields to the value for "no change".
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_state_no_change(IN ib_port_info_t * const p_pi)
{
	ib_port_info_set_port_state(p_pi, IB_LINK_NO_CHANGE);
	p_pi->state_info2 = 0;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_sup
* NAME
*	ib_port_info_get_link_speed_sup
*
* DESCRIPTION
*	Returns the encoded value for the link speed supported.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_sup(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) ((p_pi->state_info1 &
			    IB_PORT_LINK_SPEED_SUPPORTED_MASK) >>
			   IB_PORT_LINK_SPEED_SHIFT));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encoded value for the link speed supported.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_link_speed_sup
* NAME
*	ib_port_info_set_link_speed_sup
*
* DESCRIPTION
*	Given an integer of the supported link speed supported.
*	Set the appropriate bits in state_info1
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_link_speed_sup(IN uint8_t const speed,
				IN ib_port_info_t * p_pi)
{
	p_pi->state_info1 =
	    (~IB_PORT_LINK_SPEED_SUPPORTED_MASK & p_pi->state_info1) |
	    (IB_PORT_LINK_SPEED_SUPPORTED_MASK &
	     (speed << IB_PORT_LINK_SPEED_SHIFT));
}

/*
* PARAMETERS
*	speed
*		[in] Supported Speeds Code.
*
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_port_phys_state
* NAME
*	ib_port_info_get_port_phys_state
*
* DESCRIPTION
*	Returns the encoded value for the port physical state.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_port_phys_state(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) ((p_pi->state_info2 &
			    IB_PORT_PHYS_STATE_MASK) >>
			   IB_PORT_PHYS_STATE_SHIFT));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encoded value for the port physical state.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_port_phys_state
* NAME
*	ib_port_info_set_port_phys_state
*
* DESCRIPTION
*	Given an integer of the port physical state,
*	Set the appropriate bits in state_info2
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_port_phys_state(IN uint8_t const phys_state,
				 IN ib_port_info_t * p_pi)
{
	p_pi->state_info2 =
	    (~IB_PORT_PHYS_STATE_MASK & p_pi->state_info2) |
	    (IB_PORT_PHYS_STATE_MASK &
	     (phys_state << IB_PORT_PHYS_STATE_SHIFT));
}

/*
* PARAMETERS
*	phys_state
*		[in] port physical state.
*
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_down_def_state
* NAME
*	ib_port_info_get_link_down_def_state
*
* DESCRIPTION
*	Returns the link down default state.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_down_def_state(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->state_info2 & IB_PORT_LNKDWNDFTSTATE_MASK));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	link down default state of the port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_link_down_def_state
* NAME
*	ib_port_info_set_link_down_def_state
*
* DESCRIPTION
*	Sets the link down default state of the port.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_link_down_def_state(IN ib_port_info_t * const p_pi,
				     IN const uint8_t link_dwn_state)
{
	p_pi->state_info2 =
	    (uint8_t) ((p_pi->state_info2 & 0xF0) | link_dwn_state);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	link_dwn_state
*		[in] Link down default state of the port.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_active
* NAME
*	ib_port_info_get_link_speed_active
*
* DESCRIPTION
*	Returns the Link Speed Active value assigned to this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_active(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) ((p_pi->link_speed &
			    IB_PORT_LINK_SPEED_ACTIVE_MASK) >>
			   IB_PORT_LINK_SPEED_SHIFT));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the link speed active value assigned to this port.
*
* NOTES
*
* SEE ALSO
*********/

#define IB_LINK_WIDTH_ACTIVE_1X			1
#define IB_LINK_WIDTH_ACTIVE_4X			2
#define IB_LINK_WIDTH_ACTIVE_8X			4
#define IB_LINK_WIDTH_ACTIVE_12X 		8
#define IB_LINK_WIDTH_ACTIVE_2X			16
#define IB_LINK_WIDTH_1X_2X_4X_8X_OR_12X	(IB_LINK_WIDTH_ACTIVE_1X | \
						 IB_LINK_WIDTH_ACTIVE_2X | \
						 IB_LINK_WIDTH_ACTIVE_4X | \
						 IB_LINK_WIDTH_ACTIVE_8X | \
						 IB_LINK_WIDTH_ACTIVE_12X)
#define IB_LINK_WIDTH_SET_LWS			255
#define IB_LINK_SPEED_ACTIVE_EXTENDED		0
#define IB_LINK_SPEED_ACTIVE_2_5		1
#define IB_LINK_SPEED_ACTIVE_5			2
#define IB_LINK_SPEED_ACTIVE_10			4
#define IB_LINK_SPEED_2_5_5_OR_10		(IB_LINK_SPEED_ACTIVE_2_5 | \
						 IB_LINK_SPEED_ACTIVE_5 | \
						 IB_LINK_SPEED_ACTIVE_10)
#define IB_LINK_SPEED_MAX_VALUE			(IB_LINK_SPEED_2_5_5_OR_10)
#define IB_LINK_SPEED_SET_LSS			15
#define IB_LINK_SPEED_EXT_ACTIVE_NONE		0
#define IB_LINK_SPEED_EXT_ACTIVE_14		1
#define IB_LINK_SPEED_EXT_ACTIVE_25		2
#define IB_LINK_SPEED_EXT_ACTIVE_50		4
#define IB_LINK_SPEED_EXT_ACTIVE_100		8
#define IB_LINK_SPEED_EXT_14_25_OR_50		(IB_LINK_SPEED_EXT_ACTIVE_14 | \
						 IB_LINK_SPEED_EXT_ACTIVE_25 | \
						 IB_LINK_SPEED_EXT_ACTIVE_50)
#define IB_LINK_SPEED_EXT_MAX_VALUE		(IB_LINK_SPEED_EXT_ACTIVE_14 | \
						 IB_LINK_SPEED_EXT_ACTIVE_25 | \
						 IB_LINK_SPEED_EXT_ACTIVE_50 | \
						 IB_LINK_SPEED_EXT_ACTIVE_100)
#define IB_LINK_SPEED_EXT_DISABLE		30
#define IB_LINK_SPEED_EXT_SET_LSES		31
#define IB_LINK_SPEED_EXT2_ACTIVE_NONE		0
#define IB_LINK_SPEED_EXT2_ACTIVE_200		1
#define IB_LINK_SPEED_EXT2_MAX_VALUE		(IB_LINK_SPEED_EXT2_ACTIVE_200)
#define IB_LINK_SPEED_EXT2_SET_LSES2		7

/* following v1 ver1.3 p984 */
#define IB_PATH_RECORD_RATE_2_5_GBS		2
#define IB_PATH_RECORD_RATE_10_GBS		3
#define IB_PATH_RECORD_RATE_30_GBS		4
#define IB_PATH_RECORD_RATE_5_GBS		5
#define IB_PATH_RECORD_RATE_20_GBS		6
#define IB_PATH_RECORD_RATE_40_GBS		7
#define IB_PATH_RECORD_RATE_60_GBS		8
#define IB_PATH_RECORD_RATE_80_GBS		9
#define IB_PATH_RECORD_RATE_120_GBS		10
#define IB_PATH_RECORD_RATE_14_GBS		11
#define IB_PATH_RECORD_RATE_56_GBS		12
#define IB_PATH_RECORD_RATE_112_GBS		13
#define IB_PATH_RECORD_RATE_168_GBS		14
#define IB_PATH_RECORD_RATE_25_GBS		15
#define IB_PATH_RECORD_RATE_100_GBS		16
#define IB_PATH_RECORD_RATE_200_GBS		17
#define IB_PATH_RECORD_RATE_300_GBS		18
#define IB_PATH_RECORD_RATE_28_GBS		19
#define IB_PATH_RECORD_RATE_50_GBS		20
#define IB_PATH_RECORD_RATE_400_GBS		21
#define IB_PATH_RECORD_RATE_600_GBS		22
/* following v1 ver1.5 (section 15.2.5.16.1) */
#define IB_PATH_RECORD_RATE_800_GBS		23
#define IB_PATH_RECORD_RATE_1200_GBS		24
/* following v1 ver1.7 (section 15.2.5.16.1) */
#define IB_PATH_RECORD_RATE_1600_GBS		25
#define IB_PATH_RECORD_RATE_2400_GBS		26


#define IB_MIN_RATE    IB_PATH_RECORD_RATE_2_5_GBS
#define IB_MAX_RATE    IB_PATH_RECORD_RATE_2400_GBS

static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_active(IN const ib_port_info_t * const p_pi);
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_active2(IN const ib_port_info_t * const p_pi);

/****f* IBA Base: Types/ib_port_info_compute_rate
* NAME
*	ib_port_info_compute_rate
*
* DESCRIPTION
*	Returns the encoded value for the path rate.
*	This function is deprecated. Use ib_port_info_compute_rate_v2 instead.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_compute_rate(IN const ib_port_info_t * const p_pi,
			  IN const int extended)
{
	uint8_t rate = 0;

	if (extended) {
		switch (ib_port_info_get_link_speed_ext_active(p_pi)) {
		case IB_LINK_SPEED_EXT_ACTIVE_14:
			switch (p_pi->link_width_active) {
			case IB_LINK_WIDTH_ACTIVE_1X:
				rate = IB_PATH_RECORD_RATE_14_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_4X:
				rate = IB_PATH_RECORD_RATE_56_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_8X:
				rate = IB_PATH_RECORD_RATE_112_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_12X:
				rate = IB_PATH_RECORD_RATE_168_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_2X:
				rate = IB_PATH_RECORD_RATE_28_GBS;
				break;

			default:
				rate = IB_PATH_RECORD_RATE_14_GBS;
				break;
			}
			break;
		case IB_LINK_SPEED_EXT_ACTIVE_25:
			switch (p_pi->link_width_active) {
			case IB_LINK_WIDTH_ACTIVE_1X:
				rate = IB_PATH_RECORD_RATE_25_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_4X:
				rate = IB_PATH_RECORD_RATE_100_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_8X:
				rate = IB_PATH_RECORD_RATE_200_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_12X:
				rate = IB_PATH_RECORD_RATE_300_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_2X:
				rate = IB_PATH_RECORD_RATE_50_GBS;
				break;

			default:
				rate = IB_PATH_RECORD_RATE_25_GBS;
				break;
			}
			break;
		case IB_LINK_SPEED_EXT_ACTIVE_50:
			switch (p_pi->link_width_active) {
			case IB_LINK_WIDTH_ACTIVE_1X:
				rate = IB_PATH_RECORD_RATE_50_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_4X:
				rate = IB_PATH_RECORD_RATE_200_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_8X:
				rate = IB_PATH_RECORD_RATE_400_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_12X:
				rate = IB_PATH_RECORD_RATE_600_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_2X:
				rate = IB_PATH_RECORD_RATE_100_GBS;
				break;

			default:
				rate = IB_PATH_RECORD_RATE_50_GBS;
				break;
			}
			break;
		case IB_LINK_SPEED_EXT_ACTIVE_100:
			switch (p_pi->link_width_active) {
			case IB_LINK_WIDTH_ACTIVE_1X:
				rate = IB_PATH_RECORD_RATE_100_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_4X:
				rate = IB_PATH_RECORD_RATE_400_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_8X:
				rate = IB_PATH_RECORD_RATE_800_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_12X:
				rate = IB_PATH_RECORD_RATE_1200_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_2X:
				rate = IB_PATH_RECORD_RATE_200_GBS;
				break;

			default:
				rate = IB_PATH_RECORD_RATE_100_GBS;
				break;
			}
			break;
		/* IB_LINK_SPEED_EXT_ACTIVE_NONE and any others */
		default:
			break;
		}
		if (rate)
			return rate;
	}

	switch (ib_port_info_get_link_speed_active(p_pi)) {
	case IB_LINK_SPEED_ACTIVE_2_5:
		switch (p_pi->link_width_active) {
		case IB_LINK_WIDTH_ACTIVE_1X:
			rate = IB_PATH_RECORD_RATE_2_5_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_4X:
			rate = IB_PATH_RECORD_RATE_10_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_8X:
			rate = IB_PATH_RECORD_RATE_20_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_12X:
			rate = IB_PATH_RECORD_RATE_30_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_2X:
			rate = IB_PATH_RECORD_RATE_5_GBS;
			break;

		default:
			rate = IB_PATH_RECORD_RATE_2_5_GBS;
			break;
		}
		break;
	case IB_LINK_SPEED_ACTIVE_5:
		switch (p_pi->link_width_active) {
		case IB_LINK_WIDTH_ACTIVE_1X:
			rate = IB_PATH_RECORD_RATE_5_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_4X:
			rate = IB_PATH_RECORD_RATE_20_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_8X:
			rate = IB_PATH_RECORD_RATE_40_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_12X:
			rate = IB_PATH_RECORD_RATE_60_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_2X:
			rate = IB_PATH_RECORD_RATE_10_GBS;
			break;

		default:
			rate = IB_PATH_RECORD_RATE_5_GBS;
			break;
		}
		break;
	case IB_LINK_SPEED_ACTIVE_10:
		switch (p_pi->link_width_active) {
		case IB_LINK_WIDTH_ACTIVE_1X:
			rate = IB_PATH_RECORD_RATE_10_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_4X:
			rate = IB_PATH_RECORD_RATE_40_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_8X:
			rate = IB_PATH_RECORD_RATE_80_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_12X:
			rate = IB_PATH_RECORD_RATE_120_GBS;
			break;

		case IB_LINK_WIDTH_ACTIVE_2X:
			rate = IB_PATH_RECORD_RATE_20_GBS;
			break;

		default:
			rate = IB_PATH_RECORD_RATE_10_GBS;
			break;
		}
		break;
	default:
		rate = IB_PATH_RECORD_RATE_2_5_GBS;
		break;
	}

	return rate;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	extended
*		[in] Indicates whether or not to use extended link speeds.
*
* RETURN VALUES
*	Returns the encoded value for the link speed supported.
*
* NOTES
*	This function is deprecated. Use ib_port_info_compute_rate_v2 instead.
*
* SEE ALSO
*	ib_port_info_compute_rate_v2
*********/

/****f* IBA Base: Types/ib_port_info_compute_rate_v2
* NAME
*	ib_port_info_compute_rate_v2
*
* DESCRIPTION
*	Returns the encoded value for the path rate.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_compute_rate_v2(IN const ib_port_info_t * const p_pi,
			     IN const ib_port_info_t * const p_cap_pi)
{
	uint8_t rate = 0;

	if ((p_cap_pi->capability_mask & IB_PORT_CAP_HAS_CAP_MASK2) &&
	    (p_cap_pi->capability_mask2 & IB_PORT_CAP2_IS_EXT_SPEEDS2_SUPPORTED)) {
		switch (ib_port_info_get_link_speed_ext_active2(p_pi)) {
		case IB_LINK_SPEED_EXT2_ACTIVE_200:
			switch (p_pi->link_width_active) {
			case IB_LINK_WIDTH_ACTIVE_1X:
				rate = IB_PATH_RECORD_RATE_200_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_2X:
				rate = IB_PATH_RECORD_RATE_400_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_4X:
				rate = IB_PATH_RECORD_RATE_800_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_8X:
				rate = IB_PATH_RECORD_RATE_1600_GBS;
				break;

			case IB_LINK_WIDTH_ACTIVE_12X:
				rate = IB_PATH_RECORD_RATE_2400_GBS;
				break;

			default:
				rate = IB_PATH_RECORD_RATE_200_GBS;
				break;
			}
			break;

		default:
			break;
		}
		if (rate)
			return rate;
	}
	return ib_port_info_compute_rate(p_pi,
					 p_cap_pi->capability_mask & IB_PORT_CAP_HAS_EXT_SPEEDS);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	p_cap_pi
*		[in] Pointer to PortInfo whose capability masks are checked to
*		decide whether to use extended/extended2 link speeds.
*		For HCAs this should be the same as p_pi. For switches this
*		should be the PortInfo of port 0.
*
* RETURN VALUES
*	Returns the encoded value for the link speed supported.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_get_ipd
* NAME
*	ib_path_get_ipd
*
* DESCRIPTION
*	Returns the encoded value for the inter packet delay.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_path_get_ipd(IN uint8_t local_link_width_supported, IN uint8_t path_rec_rate)
{
	uint8_t ipd = 0;

	switch (local_link_width_supported) {
		/* link_width_supported = 1: 1x */
	case 1:
		break;

		/* link_width_supported = 3: 1x or 4x */
	case 3:
		switch (path_rec_rate & 0x3F) {
		case IB_PATH_RECORD_RATE_2_5_GBS:
			ipd = 3;
			break;
		default:
			break;
		}
		break;

		/* link_width_supported = 11: 1x or 4x or 12x */
	case 11:
		switch (path_rec_rate & 0x3F) {
		case IB_PATH_RECORD_RATE_2_5_GBS:
			ipd = 11;
			break;
		case IB_PATH_RECORD_RATE_10_GBS:
			ipd = 2;
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return ipd;
}

/*
* PARAMETERS
*	local_link_width_supported
*		[in] link with supported for this port
*
*	path_rec_rate
*		[in] rate field of the path record
*
* RETURN VALUES
*	Returns the ipd
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_mtu_cap
* NAME
*	ib_port_info_get_mtu_cap
*
* DESCRIPTION
*	Returns the encoded value for the maximum MTU supported by this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_mtu_cap(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->mtu_cap & 0x0F));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encooded value for the maximum MTU supported by this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_mtu_cap
* NAME
*	ib_port_info_set_mtu_cap
*
* DESCRIPTION
*	Sets the encoded value for the maximum MTU supported by this port.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_mtu_cap(IN ib_port_info_t * p_pi, uint8_t mtu_cap)
{
	CL_ASSERT((mtu_cap & 0x0F) == mtu_cap);
	IBTYPES_SET_ATTR8(p_pi->mtu_cap, 0x0F, 0, mtu_cap);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	mtu_cap
*		[in] The encoded value for the maximum MTU supported by this port.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_neighbor_mtu
* NAME
*	ib_port_info_get_neighbor_mtu
*
* DESCRIPTION
*	Returns the encoded value for the neighbor MTU supported by this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_neighbor_mtu(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) ((p_pi->mtu_smsl & 0xF0) >> 4));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encoded value for the neighbor MTU at this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_neighbor_mtu
* NAME
*	ib_port_info_set_neighbor_mtu
*
* DESCRIPTION
*	Sets the Neighbor MTU value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_neighbor_mtu(IN ib_port_info_t * const p_pi,
			      IN const uint8_t mtu)
{
	CL_ASSERT(mtu <= 5);
	CL_ASSERT(mtu != 0);
	p_pi->mtu_smsl = (uint8_t) ((p_pi->mtu_smsl & 0x0F) | (mtu << 4));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	mtu
*		[in] Encoded MTU value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_master_smsl
* NAME
*	ib_port_info_get_master_smsl
*
* DESCRIPTION
*	Returns the encoded value for the Master SMSL at this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_master_smsl(IN const ib_port_info_t * const p_pi)
{
	return (uint8_t) (p_pi->mtu_smsl & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the encoded value for the Master SMSL at this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_master_smsl
* NAME
*	ib_port_info_set_master_smsl
*
* DESCRIPTION
*	Sets the Master SMSL value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_master_smsl(IN ib_port_info_t * const p_pi,
			     IN const uint8_t smsl)
{
	p_pi->mtu_smsl = (uint8_t) ((p_pi->mtu_smsl & 0xF0) | smsl);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	mtu
*		[in] Encoded Master SMSL value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_timeout
* NAME
*	ib_port_info_set_timeout
*
* DESCRIPTION
*	Sets the encoded subnet timeout value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_timeout(IN ib_port_info_t * const p_pi,
			 IN const uint8_t timeout)
{
	CL_ASSERT(timeout <= 0x1F);
	p_pi->subnet_timeout =
	    (uint8_t) ((p_pi->subnet_timeout & 0xE0) | (timeout & 0x1F));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	timeout
*		[in] Encoded timeout value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_client_rereg
* NAME
*	ib_port_info_set_client_rereg
*
* DESCRIPTION
*	Sets the encoded client reregistration bit value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_client_rereg(IN ib_port_info_t * const p_pi,
			      IN const uint8_t client_rereg)
{
	CL_ASSERT(client_rereg <= 0x1);
	p_pi->subnet_timeout =
	    (uint8_t) ((p_pi->subnet_timeout & 0x7F) | (client_rereg << 7));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	client_rereg
*		[in] Client reregistration value to set (either 1 or 0).
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_mcast_pkey_trap_suppress
* NAME
*	ib_port_info_set_mcast_pkey_trap_suppress
*
* DESCRIPTION
*	Sets the encoded multicast pkey trap suppression enabled bits value
*	in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_mcast_pkey_trap_suppress(IN ib_port_info_t * const p_pi,
					  IN const uint8_t trap_suppress)
{
	CL_ASSERT(trap_suppress <= 0x1);
	p_pi->subnet_timeout =
	    (uint8_t) ((p_pi->subnet_timeout & 0x9F) | (trap_suppress << 5));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	trap_suppress
*		[in] Multicast pkey trap suppression enabled value to set
*		     (either 1 or 0).
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_timeout
* NAME
*	ib_port_info_get_timeout
*
* DESCRIPTION
*	Gets the encoded subnet timeout value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_timeout(IN ib_port_info_t const *p_pi)
{
	return (p_pi->subnet_timeout & 0x1F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded timeout value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_guid_cap
* NAME
*	ib_port_info_get_guid_cap
*
* DESCRIPTION
*	Gets the encoded GUIDCap value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_guid_cap(IN ib_port_info_t const *p_pi)
{
	return (p_pi->guid_cap);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded GuidCapability value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_active
* NAME
*	ib_port_info_get_link_speed_ext_active
*
* DESCRIPTION
*	Gets the encoded LinkSpeedExtActive value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_active(IN const ib_port_info_t * const p_pi)
{
	return ((p_pi->link_speed_ext & 0xF0) >> 4);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExtActive value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_sup
* NAME
*	ib_port_info_get_link_speed_ext_sup
*
* DESCRIPTION
*	Returns the encoded value for the link speed extended supported.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_sup(IN const ib_port_info_t * const p_pi)
{
	return (p_pi->link_speed_ext & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExtSupported value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_enabled
* NAME
*	ib_port_info_get_link_speed_ext_enabled
*
* DESCRIPTION
*	Gets the encoded LinkSpeedExtEnabled value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_enabled(IN const ib_port_info_t * const p_pi)
{
        return (p_pi->link_speed_ext_enabled & 0x1F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExtEnabled value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_link_speed_ext_enabled
* NAME
*	ib_port_info_set_link_speed_ext_enabled
*
* DESCRIPTION
*	Sets the link speed extended enabled value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_link_speed_ext_enabled(IN ib_port_info_t * const p_pi,
					IN const uint8_t link_speed_ext_enabled)
{
	CL_ASSERT(link_speed_ext_enabled <= 0x1F);
	p_pi->link_speed_ext_enabled = link_speed_ext_enabled & 0x1F;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	link_speed_ext_enabled
*		[in] link speed extended enabled value to set.
*
* RETURN VALUES
*	The encoded LinkSpeedExtEnabled value
*
* NOTES
*
* SEE ALSO
*********/

#define IB_PI_LINK_SPEED_EXT_ACTIVE2_MASK (CL_HTON32(0x60000000))
#define IB_PI_LINK_SPEED_EXT_ACTIVE2_SHIFT 5 /* network order */
#define IB_PI_LINK_SPEED_EXT_SUPPORTED2_MASK (CL_HTON32(0x18000000))
#define IB_PI_LINK_SPEED_EXT_SUPPORTED2_SHIFT 3 /* network order */
#define IB_PI_LINK_SPEED_EXT_ENABLED2_MASK (CL_HTON32(0x07000000))
#define IB_PI_LINK_SPEED_EXT_ENABLED2_SHIFT 0 /* network order */
#define IB_PI_LINK_SPEED_EXT_ENABLED2_BITS 3
#define IB_PI_LINK_RT_LATENCY_MASK (CL_HTON32(0x00FFFFFF))

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_active2
* NAME
*	ib_port_info_get_link_speed_ext_active2
*
* DESCRIPTION
*	Gets the encoded LinkSpeedExtActive2 value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_active2(IN const ib_port_info_t * const p_pi)
{
	return (p_pi->link_rt_latency & IB_PI_LINK_SPEED_EXT_ACTIVE2_MASK) >>
	       IB_PI_LINK_SPEED_EXT_ACTIVE2_SHIFT;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExActive2 value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_supported2
* NAME
*	ib_port_info_get_link_speed_ext_supported2
*
* DESCRIPTION
*	Gets the encoded LinkSpeedExtSupported2 value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_supported2(IN const ib_port_info_t * const p_pi)
{
	return (p_pi->link_rt_latency & IB_PI_LINK_SPEED_EXT_SUPPORTED2_MASK) >>
	       IB_PI_LINK_SPEED_EXT_SUPPORTED2_SHIFT;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExSupported2 value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_ext_enabled2
* NAME
*	ib_port_info_get_link_speed_ext_enabled2
*
* DESCRIPTION
*	Gets the encoded LinkSpeedExtEnabled2 value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_ext_enabled2(IN const ib_port_info_t * const p_pi)
{
	return (p_pi->link_rt_latency & IB_PI_LINK_SPEED_EXT_ENABLED2_MASK) >>
	       IB_PI_LINK_SPEED_EXT_ENABLED2_SHIFT;

}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkSpeedExEnabled2 value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_link_speed_ext_enabled2
* NAME
*	ib_port_info_set_link_speed_ext_enabled2
*
* DESCRIPTION
*	Sets the link speed extended enabled 2 value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_link_speed_ext_enabled2(IN ib_port_info_t * const p_pi,
					 IN const uint8_t link_speed_ext_enabled2)
{
	ib_net32_t shifted_link_speed_ext_en2;

	CL_ASSERT((link_speed_ext_enabled2 >> IB_PI_LINK_SPEED_EXT_ENABLED2_BITS) == 0);
	shifted_link_speed_ext_en2 = (((ib_net32_t) link_speed_ext_enabled2) <<
				      IB_PI_LINK_SPEED_EXT_ENABLED2_SHIFT) &
				     IB_PI_LINK_SPEED_EXT_ENABLED2_MASK;
	p_pi->link_rt_latency = (p_pi->link_rt_latency & ~IB_PI_LINK_SPEED_EXT_ENABLED2_MASK) |
				shifted_link_speed_ext_en2;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	link_speed_ext_enabled
*		[in] link speed extended enabled 2 value to set.
*
* RETURN VALUES
*	The encoded LinkSpeedExtEnabled2 value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_rt_latency
* NAME
*	ib_port_info_get_link_rt_latency
*
* DESCRIPTION
*	Gets the encoded LinkRoundTripLatency value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_port_info_get_link_rt_latency(IN const ib_port_info_t * const p_pi)
{
	return p_pi->link_rt_latency & IB_PI_LINK_RT_LATENCY_MASK;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded LinkRoundTripLatency value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_resp_time_value
* NAME
*	ib_port_info_get_resp_time_value
*
* DESCRIPTION
*	Gets the encoded resp time value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_resp_time_value(IN const ib_port_info_t * const p_pi)
{
	return (p_pi->resp_time_value & 0x1F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	The encoded resp time value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_resp_time_value
* NAME
*	ib_port_info_set_resp_time_value
*
* DESCRIPTION
*	Sets the encoded resp time value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_resp_time_value(IN ib_port_info_t * p_pi, IN uint8_t resp_time_value)
{
	CL_ASSERT((resp_time_value & 0x1f) == resp_time_value);
	IBTYPES_SET_ATTR8(p_pi->resp_time_value, 0x1f, 0, resp_time_value);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	resp_time_value
*		[in] the encoded resp time value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_client_rereg
* NAME
*	ib_port_info_get_client_rereg
*
* DESCRIPTION
*	Gets the encoded client reregistration bit value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_client_rereg(IN ib_port_info_t const *p_pi)
{
	return ((p_pi->subnet_timeout & 0x80) >> 7);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Client reregistration value (either 1 or 0).
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_mcast_pkey_trap_suppress
* NAME
*	ib_port_info_get_mcast_pkey_trap_suppress
*
* DESCRIPTION
*	Gets the encoded multicast pkey trap suppression enabled bits value
*	in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_mcast_pkey_trap_suppress(IN ib_port_info_t const *p_pi)
{
	return ((p_pi->subnet_timeout & 0x60) >> 5);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Multicast PKey trap suppression enabled value (either 1 or 0).
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_hoq_lifetime
* NAME
*	ib_port_info_set_hoq_lifetime
*
* DESCRIPTION
*	Sets the Head of Queue Lifetime for which a packet can live in the head
*  of VL queue
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_hoq_lifetime(IN ib_port_info_t * const p_pi,
			      IN const uint8_t hoq_life)
{
	p_pi->vl_stall_life = (uint8_t) ((hoq_life & 0x1f) |
					 (p_pi->vl_stall_life & 0xe0));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	hoq_life
*		[in] Encoded lifetime value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_hoq_lifetime
* NAME
*	ib_port_info_get_hoq_lifetime
*
* DESCRIPTION
*	Gets the Head of Queue Lifetime for which a packet can live in the head
*  of VL queue
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_hoq_lifetime(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->vl_stall_life & 0x1f));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*     Encoded lifetime value
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_vl_stall_count
* NAME
*	ib_port_info_set_vl_stall_count
*
* DESCRIPTION
*	Sets the VL Stall Count which define the number of contiguous
*  HLL (hoq) drops that will put the VL into stalled mode.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_vl_stall_count(IN ib_port_info_t * const p_pi,
				IN const uint8_t vl_stall_count)
{
	p_pi->vl_stall_life = (uint8_t) ((p_pi->vl_stall_life & 0x1f) |
					 ((vl_stall_count << 5) & 0xe0));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	vl_stall_count
*		[in] value to set
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_vl_stall_count
* NAME
*	ib_port_info_get_vl_stall_count
*
* DESCRIPTION
*	Gets the VL Stall Count which define the number of contiguous
*  HLL (hoq) drops that will put the VL into stalled mode
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_vl_stall_count(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->vl_stall_life & 0xe0) >> 5);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*     vl stall count
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_lmc
* NAME
*	ib_port_info_get_lmc
*
* DESCRIPTION
*	Returns the LMC value assigned to this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_lmc(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->mkey_lmc & IB_PORT_LMC_MASK));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the LMC value assigned to this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_lmc
* NAME
*	ib_port_info_set_lmc
*
* DESCRIPTION
*	Sets the LMC value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_lmc(IN ib_port_info_t * const p_pi, IN const uint8_t lmc)
{
	CL_ASSERT(lmc <= IB_PORT_LMC_MAX);
	p_pi->mkey_lmc = (uint8_t) ((p_pi->mkey_lmc & 0xF8) | lmc);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	lmc
*		[in] LMC value to set, must be less than 7.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_link_speed_enabled
* NAME
*	ib_port_info_get_link_speed_enabled
*
* DESCRIPTION
*	Returns the link speed enabled value assigned to this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_link_speed_enabled(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) (p_pi->link_speed & IB_PORT_LINK_SPEED_ENABLED_MASK));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Port state.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_link_speed_enabled
* NAME
*	ib_port_info_set_link_speed_enabled
*
* DESCRIPTION
*	Sets the link speed enabled value in the PortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_link_speed_enabled(IN ib_port_info_t * const p_pi,
				    IN const uint8_t link_speed_enabled)
{
	p_pi->link_speed =
	    (uint8_t) ((p_pi->link_speed & 0xF0) | link_speed_enabled);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	link_speed_enabled
*		[in] link speed enabled value to set.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_mpb
* NAME
*	ib_port_info_get_mpb
*
* DESCRIPTION
*	Returns the M_Key protect bits assigned to this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_mpb(IN const ib_port_info_t * const p_pi)
{
	return ((uint8_t) ((p_pi->mkey_lmc & IB_PORT_MPB_MASK) >>
			   IB_PORT_MPB_SHIFT));
}

/*
* PARAMETERS
*	p_ni
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the M_Key protect bits assigned to this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_mpb
* NAME
*	ib_port_info_set_mpb
*
* DESCRIPTION
*	Set the M_Key protect bits of this port.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_mpb(IN ib_port_info_t * p_pi, IN uint8_t mpb)
{
	p_pi->mkey_lmc =
	    (~IB_PORT_MPB_MASK & p_pi->mkey_lmc) |
	    (IB_PORT_MPB_MASK & (mpb << IB_PORT_MPB_SHIFT));
}

/*
* PARAMETERS
*	mpb
*		[in] M_Key protect bits
*	p_ni
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_local_phy_err_thd
* NAME
*	ib_port_info_get_local_phy_err_thd
*
* DESCRIPTION
*	Returns the Phy Link Threshold
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_local_phy_err_thd(IN const ib_port_info_t * const p_pi)
{
	return (uint8_t) ((p_pi->error_threshold & 0xF0) >> 4);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the Phy Link error threshold assigned to this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_overrun_err_thd
* NAME
*	ib_port_info_get_local_overrun_err_thd
*
* DESCRIPTION
*	Returns the Credits Overrun Errors Threshold
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_port_info_get_overrun_err_thd(IN const ib_port_info_t * const p_pi)
{
	return (uint8_t) (p_pi->error_threshold & 0x0F);
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	Returns the Credits Overrun errors threshold assigned to this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_phy_and_overrun_err_thd
* NAME
*	ib_port_info_set_phy_and_overrun_err_thd
*
* DESCRIPTION
*	Sets the Phy Link and Credits Overrun Errors Threshold
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_phy_and_overrun_err_thd(IN ib_port_info_t * const p_pi,
					 IN uint8_t phy_threshold,
					 IN uint8_t overrun_threshold)
{
	p_pi->error_threshold =
	    (uint8_t) (((phy_threshold & 0x0F) << 4) |
		       (overrun_threshold & 0x0F));
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
*	phy_threshold
*		[in] Physical Link Errors Threshold above which Trap 129 is generated
*
*  overrun_threshold
*     [in] Credits overrun Errors Threshold above which Trap 129 is generated
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_get_m_key
* NAME
*	ib_port_info_get_m_key
*
* DESCRIPTION
*	Gets the M_Key
*
* SYNOPSIS
*/
static inline ib_net64_t OSM_API
ib_port_info_get_m_key(IN const ib_port_info_t * const p_pi)
{
	return p_pi->m_key;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*
* RETURN VALUES
*	M_Key.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_port_info_set_m_key
* NAME
*	ib_port_info_set_m_key
*
* DESCRIPTION
*	Sets the M_Key value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_port_info_set_m_key(IN ib_port_info_t * const p_pi, IN ib_net64_t m_key)
{
	p_pi->m_key = m_key;
}

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to a PortInfo attribute.
*	m_key
*		[in] M_Key value.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

#define FDR10 0x01
#define ORIGINAL_LINK_SPEEDS 3 /* SDR DDR QDR */
#define NON_EXT_LINK_SPEEDS (ORIGINAL_LINK_SPEEDS + 1) /* + FDR10 */
#define NON_EXT2_LINK_SPEEDS (NON_EXT_LINK_SPEEDS + 4) /* + FDR EDR HDR NDR */

/****f* IBA Base: Types/ib_get_highest_link_speed
* NAME
*	ib_get_highest_link_speed
*
* DESCRIPTION
*	Returns the highest link speed encoded in the given bit field.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API ib_get_highest_link_speed(IN const uint16_t speeds)
{
	uint16_t ret = 0;
	uint8_t extspeeds = (speeds >> NON_EXT_LINK_SPEEDS);
	uint8_t extspeeds2 = (speeds >> NON_EXT2_LINK_SPEEDS);

	if (extspeeds2 & IB_LINK_SPEED_EXT2_ACTIVE_200)
		ret = IB_LINK_SPEED_EXT2_ACTIVE_200 << NON_EXT2_LINK_SPEEDS;
	else if (extspeeds & IB_LINK_SPEED_EXT_ACTIVE_100)
		ret = IB_LINK_SPEED_EXT_ACTIVE_100 << NON_EXT_LINK_SPEEDS;
	else if (extspeeds & IB_LINK_SPEED_EXT_ACTIVE_50)
		ret = IB_LINK_SPEED_EXT_ACTIVE_50 << NON_EXT_LINK_SPEEDS;
	else if (extspeeds & IB_LINK_SPEED_EXT_ACTIVE_25)
		ret = IB_LINK_SPEED_EXT_ACTIVE_25 << NON_EXT_LINK_SPEEDS;
	else if (extspeeds & IB_LINK_SPEED_EXT_ACTIVE_14)
		ret = IB_LINK_SPEED_EXT_ACTIVE_14 << NON_EXT_LINK_SPEEDS;
	else if (speeds & (FDR10 << ORIGINAL_LINK_SPEEDS))
		ret = FDR10 << ORIGINAL_LINK_SPEEDS;
	else if (speeds & IB_LINK_SPEED_ACTIVE_10)
		ret = IB_LINK_SPEED_ACTIVE_10;
	else if (speeds & IB_LINK_SPEED_ACTIVE_5)
		ret = IB_LINK_SPEED_ACTIVE_5;
	else if (speeds & IB_LINK_SPEED_ACTIVE_2_5)
		ret = IB_LINK_SPEED_ACTIVE_2_5;

	return ret;
}

/*
* PARAMETERS
*	speed
*		[in] The bit field for the supported or enabled link speeds, where:
*		     bits 0-2 (LSB) encode the last 3 bits of LinkSpeedSupported
*		     or LinkSpeedEnabled,
*		     bit 3 encodes supported/enabled FDR10,
*		     bits 4-7 encode either all bits of LinkSpeedExtSupported or
*		     the last 4 bits of the 5-bit long LinkSpeedExtEnabled,
*		     bits 8-9 encode either all bits of LinkSpeedExtSupported2
*		     or the last 2 bits of the 3-bit long LinkSpeedExtEnabled2.
*
* RETURN VALUES
*	Returns the highest link speed encoded in the given bit field, e.g.
*	a return value of 2 indicates DDR, 8 indicates FDR10, and a return value
*	of 32 (or 00100000 in bit) would indicate EDR speed.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_get_highest_link_width
* NAME
*	ib_get_highest_link_width
*
* DESCRIPTION
*	Returns the highest link width encoded in the given bit field.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API ib_get_highest_link_width(IN const uint8_t widths)
{
	uint8_t ret = 0;

	if (widths & IB_LINK_WIDTH_ACTIVE_12X)
		ret = IB_LINK_WIDTH_ACTIVE_12X;
	else if (widths & IB_LINK_WIDTH_ACTIVE_8X)
		ret = IB_LINK_WIDTH_ACTIVE_8X;
	else if (widths & IB_LINK_WIDTH_ACTIVE_4X)
		ret = IB_LINK_WIDTH_ACTIVE_4X;
	else if (widths & IB_LINK_WIDTH_ACTIVE_2X)
		ret = IB_LINK_WIDTH_ACTIVE_2X;
	else if (widths & IB_LINK_WIDTH_ACTIVE_1X)
		ret = IB_LINK_WIDTH_ACTIVE_1X;

	return ret;
}

/*
* PARAMETERS
*	widths
*		[in] The bit field for the supported or enabled link widths.
*
* RETURN VALUES
*	Returns the highest link width encoded in the given bit field.
*
* NOTES
*
* SEE ALSO
*********/

/****d* OpenSM: Types/ib_special_port_type_t
* NAME
*       ib_special_port_type_t
*
* DESCRIPTION
*       Enumerates special port types.
*
* SYNOPSIS
*/
typedef enum _ib_special_port_type
{
	IB_SPECIAL_PORT_TYPE_RESERVED,
	IB_SPECIAL_PORT_TYPE_AN,
	IB_SPECIAL_PORT_TYPE_ROUTER,
	IB_SPECIAL_PORT_TYPE_ETHERNET_GATEWAY,
	IB_SPECIAL_PORT_TYPE_MAX,
} ib_special_port_type_t;
/***********/

#define IB_MEPI_CAP_DUAL_MKEY		(CL_HTON16(0x200))

/****d* OpenSM: Types/ib_epi_unhealthy_reason_type
* NAME
*       ib_epi_unhealthy_reason_type
*
* DESCRIPTION
*       Enumerates unhealthy reason types.
*
* SYNOPSIS
*/
typedef enum _ib_epi_unhealthy_reason_type {
	IB_MEPI_UNHEALTHY_REASON_NOT_SUPPORTED = 0,
	IB_MEPI_UNHEALTHY_REASON_NONE,
	IB_MEPI_UNHEALTHY_REASON_CREDIT_WATCHDG,
	IB_MEPI_UNHEALTHY_REASON_SYMMETRY_ENF,
	IB_MEPI_UNHEALTHY_REASON_RAW_BER,
	IB_MEPI_UNHEALTHY_REASON_EFFECTIVE_BER,
	IB_MEPI_UNHEALTHY_REASON_SYMBOL_BER,
	IB_MEPI_UNHEALTHY_REASON_RESERVED,
	IB_MEPI_UNHEALTHY_REASON_CONTAIN_AND_DRAIN,
	IB_MEPI_UNHEALTHY_REASON_MAX,
} ib_epi_unhealthy_reason_type;
/***********/

/****s* IBA Base: Types/ib_mlnx_ext_port_info_t
* NAME
*	ib_mlnx_ext_port_info_t
*
* DESCRIPTION
*	Mellanox ExtendedPortInfo (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_mlnx_ext_port_info {
	uint8_t resvd1[3];
	uint8_t state_change_enable;
	ib_net16_t bw_utilization;
	uint8_t unhealthy_reason;
	uint8_t link_speed_supported;
	uint8_t resvd3[2];
	uint8_t min_bw_utilization;
	uint8_t link_speed_enabled;
	uint8_t resvd4[3];
	uint8_t link_speed_active;
	uint8_t resvd5;
	uint8_t nmx_admin_state;
	ib_net16_t capability_mask;
	uint8_t resvd6[24];
	uint8_t is_fnm_port;
	uint8_t resvd7[2];
	uint8_t special_port_type;
	uint8_t resvd8[4];
	ib_net16_t adaptive_timeout_sl_mask;
	ib_net16_t ooo_sl_mask;
	uint8_t resvd9[8];

} PACK_SUFFIX ib_mlnx_ext_port_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	resvd1[0]
*		bit 7: AME
*		bit 6: SHarPANEn
*		bit 5: RouterLIDEn
*		bit 4: port_recovered - Indicate the port is during a recovery or has gone through
*		       a successful recovery event since the last time this bit was cleared.
*		       Reserved in case IB_GENERAL_INFO_IS_PORT_RECOVERY_POLICY_CONFIG_SUPPORTED_BIT is not set.
*		       On SET, only 0 is valid.
*		bit 3-0: reserved
*
* SEE ALSO
*********/

#define IB_EXT_PORT_INFO_BW_UTIL_ENABLE_MASK_HO			0x0400
#define IB_EXT_PORT_INFO_BW_UTIL_ENABLE_SHIFT			10
#define IB_EXT_PORT_INFO_BW_UTILIZATION_MASK_HO			0x03FF
#define IB_EXT_PORT_INFO_MIN_BW_UTILIZATION_MASK		0x7F


#define IB_IS_SERVICE_PORT_BIT				0x80

#define IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_MASK_HO            0x40
#define IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_SHIFT	        6
#define IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_MASK_HO               0x20
#define IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_SHIFT		        5
#define IB_EXT_PORT_INFO_ACCESS_TRUNK_MASK_HO                   0x10
#define IB_EXT_PORT_INFO_ACCESS_TRUNK_SHIFT		        4

#define IB_EXT_PORT_INFO_STATE_CHANGE_ENABLE_ADMIN_STATE_BIT	0x2

#define IB_EXT_PORT_INFO_NMX_ADMIN_STATE_MASK_HO		0x0E
#define IB_EXT_PORT_INFO_NMX_ADMIN_STATE_SHIFT		        1

typedef enum _ib_nmx_admin_state {
	IB_NMX_ADMINSTATE_NA,
	IB_NMX_ADMINSTATE_UP,
	IB_NMX_ADMINSTATE_DOWN,
	IB_NMX_ADMINSTATE_DIAG,
} ib_nmx_admin_state_t;

/****f* IBA Base: Types/ib_mlnx_epi_set_state_change_enable_nmx_admin_state
* NAME
*	ib_mlnx_epi_set_state_change_enable_nmx_admin_state
*
* DESCRIPTION
*	Turns on NMX admin state bit of state change enable field
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_state_change_enable_nmx_admin_state(IN ib_mlnx_ext_port_info_t * const p_epi)
{
	p_epi->state_change_enable |= IB_EXT_PORT_INFO_STATE_CHANGE_ENABLE_ADMIN_STATE_BIT;
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	None
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_nmx_admin_state
* NAME
*	ib_mlnx_epi_get_nmx_admin_state
*
* DESCRIPTION
*	Returns NMX admin state of the port
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mlnx_epi_get_nmx_admin_state(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return IBTYPES_GET_ATTR8(p_epi->nmx_admin_state, IB_EXT_PORT_INFO_NMX_ADMIN_STATE_MASK_HO,
				 IB_EXT_PORT_INFO_NMX_ADMIN_STATE_SHIFT);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	Returns NMX admin state of the port
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_nmx_admin_state
* NAME
*	ib_mlnx_epi_set_nmx_admin_state
*
* DESCRIPTION
*	Sets NMX admin state of the port
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_nmx_admin_state(IN ib_mlnx_ext_port_info_t * const p_epi, IN uint8_t nmx_admin_state)
{
	IBTYPES_SET_ATTR8(p_epi->nmx_admin_state, IB_EXT_PORT_INFO_NMX_ADMIN_STATE_MASK_HO,
			  IB_EXT_PORT_INFO_NMX_ADMIN_STATE_SHIFT, nmx_admin_state);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	nmx_admin_state
*		[in] Admin state value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_is_fnm_port
* NAME
*	ib_mlnx_epi_get_is_fnm_port
*
* DESCRIPTION
*	Returns if the is_fnm_port bit is set, i.e wether
*	the port is fabric network management (fnm) port.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mlnx_epi_get_is_fnm_port(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return (p_epi->is_fnm_port & IB_IS_SERVICE_PORT_BIT) != 0;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	TRUE if the value of the is_fnm_port bit is set, FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_mission_as_fnm_port
* NAME
*	ib_mlnx_epi_get_mission_as_fnm_port
*
* DESCRIPTION
* 	Returns the value of mission_port_configured_as_fnm_port bit,
*	i.e wether mission port configured as a logical FNM port.
*	Reserved when is_fnm_port=1.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_mission_as_fnm_port(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return IBTYPES_GET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_MASK_HO,
			         IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_SHIFT);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	Returns the value of mission_port_configured_as_fnm_port bit
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_mission_as_fnm_port
* NAME
*	ib_mlnx_epi_set_mission_as_fnm_port
*
* DESCRIPTION
*	Set the mission_port_configured_as_fnm_port value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_mission_as_fnm_port(IN ib_mlnx_ext_port_info_t * const p_epi, IN uint8_t mission_as_fnm_port)
{
	IBTYPES_SET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_MASK_HO,
			  IB_EXT_PORT_INFO_MISSION_AS_FNM_PORT_SHIFT, mission_as_fnm_port);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	mission_as_fnm_port
*		[in] mission_as_fnm_port value to set.
*
* RETURN VALUES
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_qos_config_vl_en
* NAME
*	ib_mlnx_epi_get_qos_config_vl_en
*
* DESCRIPTION
* 	Returns value of qos_config_vl_en enablement bit,
*	i.e wether allow access to the QoSConfigVL attribute.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_qos_config_vl_en(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return IBTYPES_GET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_MASK_HO,
                                 IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_SHIFT);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	Returns value of qos_config_vl_en enablement bit.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_qos_config_vl_en
* NAME
*	ib_mlnx_epi_set_qos_config_vl_en
*
* DESCRIPTION
*	Set the qos_config_vl_en value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_qos_config_vl_en(IN ib_mlnx_ext_port_info_t * const p_epi, IN uint8_t qos_config_vl_en)
{
	IBTYPES_SET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_MASK_HO,
			  IB_EXT_PORT_INFO_QOS_CONFIG_VL_EN_SHIFT, qos_config_vl_en);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	qos_config_vl_en
*		[in] qos_config_vl_en value to set.
*
* RETURN VALUES
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_is_trunk_port
* NAME
*	ib_mlnx_epi_get_is_trunk_port
*
* DESCRIPTION
* 	Returns the value of access_trunk bit
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_is_trunk_port(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return IBTYPES_GET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_ACCESS_TRUNK_MASK_HO,
			         IB_EXT_PORT_INFO_ACCESS_TRUNK_SHIFT);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	Returns the value of access_trunk bit
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_access_trunk
* NAME
*	ib_mlnx_epi_set_access_trunk
*
* DESCRIPTION
*	Set the access_trunk value:
*	    0: Access port
*	    1: Trunk port
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_access_trunk(IN ib_mlnx_ext_port_info_t * const p_epi, IN uint8_t access_trunk)
{
	IBTYPES_SET_ATTR8(p_epi->is_fnm_port, IB_EXT_PORT_INFO_ACCESS_TRUNK_MASK_HO,
			  IB_EXT_PORT_INFO_ACCESS_TRUNK_SHIFT, access_trunk);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	access_trunk
*		[in] access_trunk value to set.
*
* RETURN VALUES
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_bw_util_en
* NAME
*	ib_mlnx_epi_set_bw_util_en
*
* DESCRIPTION
*	Set the bandwidth utilization enable bit according to bw_util_en.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_bw_util_en(IN ib_mlnx_ext_port_info_t * const p_epi, boolean_t bw_util_en)
{
	IBTYPES_SET_ATTR16(p_epi->bw_utilization, IB_EXT_PORT_INFO_BW_UTIL_ENABLE_MASK_HO,
			   IB_EXT_PORT_INFO_BW_UTIL_ENABLE_SHIFT, bw_util_en);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	bw_util_en
*		[in] Boolean indicates whether to enable or disable the bandwidth utilization.
*
* RETURN VALUES
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_set_bw_utilization
* NAME
*	ib_mlnx_epi_set_bw_utilization
*
* DESCRIPTION
*	Set the bandwidth utilization value according to bw_utilization.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_mlnx_epi_set_bw_utilization(IN ib_mlnx_ext_port_info_t * const p_epi, IN uint16_t bw_utilization)
{
	IBTYPES_SET_ATTR16(p_epi->bw_utilization, IB_EXT_PORT_INFO_BW_UTILIZATION_MASK_HO, 0,
			   bw_utilization);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
*	bw_utilization
*		[in] Bandwidth utilization value to set.
*
* RETURN VALUES
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_bw_utilization
* NAME
*	ib_mlnx_epi_get_bw_utilization
*
* DESCRIPTION
*	Return the bandwidth utilization.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_mlnx_epi_get_bw_utilization(IN ib_mlnx_ext_port_info_t * const mepi)
{
	return IBTYPES_GET_ATTR16(mepi->bw_utilization, IB_EXT_PORT_INFO_BW_UTILIZATION_MASK_HO, 0);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	The bandwidth utilization value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_min_bw_utilization
* NAME
*	ib_mlnx_epi_get_min_bw_utilization
*
* DESCRIPTION
*	Return the minimal bandwidth utilization.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_min_bw_utilization(IN const ib_mlnx_ext_port_info_t * const mepi)
{
	return IBTYPES_GET_ATTR8(mepi->min_bw_utilization, IB_EXT_PORT_INFO_MIN_BW_UTILIZATION_MASK, 0);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	The minimal bandwidth utilization value.
*
* NOTES
*
* SEE ALSO
*********/

#define IB_IS_SPECIAL_PORT_TYPE_SHIFT			(7)
#define IB_IS_SPECIAL_PORT_TYPE_BIT			(1 << IB_IS_SPECIAL_PORT_TYPE_SHIFT)
#define IB_SPECIAL_PORT_TYPE_MASK			(0xF)

/****f* IBA Base: Types/ib_mlnx_epi_get_unhealthy_reason
* NAME
*	ib_mlnx_epi_get_unhealthy_reason
*
* DESCRIPTION
*	Returns the unhealthy reason of this port.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_unhealthy_reason(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	if (p_epi->unhealthy_reason < IB_MEPI_UNHEALTHY_REASON_MAX)
		return p_epi->unhealthy_reason;
	return IB_MEPI_UNHEALTHY_REASON_NOT_SUPPORTED;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	Unhealthy reason
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_is_special_port_bit
* NAME
*	ib_mlnx_epi_get_is_special_port_bit
*
* DESCRIPTION
*	Returns the value of the is-special-port bit.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mlnx_epi_get_is_special_port_bit(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return (p_epi->special_port_type & IB_IS_SPECIAL_PORT_TYPE_BIT) != 0;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	The value of the is-special-port bit.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_special_port_type_val
* NAME
*	ib_mlnx_epi_get_is_special_port_type_val
*
* DESCRIPTION
*	Returns the value of the special-port-type field.
*
* SYNOPSIS
*/
static inline ib_special_port_type_t OSM_API
ib_mlnx_epi_get_special_port_type_val(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	ib_special_port_type_t port_type;

	port_type = (ib_special_port_type_t)
			(p_epi->special_port_type & IB_SPECIAL_PORT_TYPE_MASK);

	return (port_type < IB_SPECIAL_PORT_TYPE_MAX) ?
		port_type : IB_SPECIAL_PORT_TYPE_RESERVED;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	The value of the special-port-type field.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/mlnx_epi_get_special_port_type
* NAME
*	ib_mlnx_epi_get_special_port_type
*
* DESCRIPTION
*	Returns the special port type, if marked as special port.
*	NOTE: This function does not check that the special-port-type is within a
*	      known range (not reserved). Such a check should be done by the caller.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_mlnx_epi_get_special_port_type(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return ib_mlnx_epi_get_is_special_port_bit(p_epi) ?
	       ib_mlnx_epi_get_special_port_type_val(p_epi) :
	       IB_SPECIAL_PORT_TYPE_RESERVED;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	The special port type or 0 if not marked as special port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_is_an
* NAME
*	ib_mlnx_epi_is_an
*
* DESCRIPTION
*	Returns if the special port type matches AN (Aggregation Node).
*
* SYNOPSIS
*/
static inline boolean_t
ib_mlnx_epi_is_an(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return ib_mlnx_epi_get_is_special_port_bit(p_epi) ?
		(ib_mlnx_epi_get_special_port_type_val(p_epi) == IB_SPECIAL_PORT_TYPE_AN) :
		FALSE;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	Returns if the special port type is AN (Aggregation Node).
*
* NOTES
*
* SEE ALSO
*********/

#define FDR10_BIT			        0x01
#define AME_BIT				        0x80
#define SHARP_AN_ENABLED_BIT		        0x40
#define ROUTER_LID_ENABLED_BIT		        0x20
#define IB_EXT_PORT_INFO_PORT_RECOVERY_BIT	0x10


/****f* IBA Base: Types/ib_mlnx_extended_get_mepi_resvd1_bit
* NAME
*	ib_mlnx_extended_get_mepi_resvd1_bit
*
* DESCRIPTION
*	Returns if the given bit is set in resvd1[byte_index] field
*
* SYNOPSIS
*/
static inline boolean_t
ib_mlnx_extended_get_mepi_resvd1_bit(IN const ib_mlnx_ext_port_info_t *p_epi,
				     IN uint8_t byte_index, IN uint8_t bit_to_get)
{
	if (p_epi->resvd1[byte_index] & bit_to_get)
		return TRUE;
	else
		return FALSE;
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*	byte_index
*		[in] byte index in the resvd1 field
*	bit_to_get
*		[in] the bit that is checked
*
* RETURN VALUES
*	Returns if the specified bit is set
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_extended_set_mepi_resvd1_bit
* NAME
*	ib_mlnx_extended_set_mepi_resvd1_bit
*
* DESCRIPTION
*	Sets the given bit in resvd1[byte_index] field
*
* SYNOPSIS
*/
static inline void
ib_mlnx_extended_set_mepi_resvd1_bit(IN OUT ib_mlnx_ext_port_info_t *p_epi,
				     IN uint8_t byte_index, IN uint8_t bit_to_set,
				     IN boolean_t enable)
{
	if (enable == TRUE)
		p_epi->resvd1[byte_index] = p_epi->resvd1[byte_index] | bit_to_set;
	else
		p_epi->resvd1[byte_index] = p_epi->resvd1[byte_index] & ~bit_to_set;
}
/*
* PARAMETERS
*	p_epi
*		[in][out] Pointer to a ib_mlnx_ext_port_info_t attribute.
*	byte_index
*		[in] byte index in the resvd1 field
*	bit_to_set
*		[in] the bit that is set
*	enable
*		[in] the boolean value of the bit, enable or disable.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_extended_get_router_lid_bit
* NAME
*	ib_mlnx_extended_get_router_lid_bit
*
* DESCRIPTION
*	Returns if the router bit is set
*
* SYNOPSIS
*/
static inline boolean_t
ib_mlnx_extended_get_router_lid_bit(IN const ib_mlnx_ext_port_info_t *p_epi)
{
	return ib_mlnx_extended_get_mepi_resvd1_bit(p_epi, 0, ROUTER_LID_ENABLED_BIT);
}
/*
* PARAMETERS
*	p_epi
*		[in][out] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	True if the router bit is set, False otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_extended_set_router_lid_bit
* NAME
*	ib_mlnx_extended_set_router_lid_bit
*
* DESCRIPTION
*	Sets the router bit.
*
* SYNOPSIS
*/
static inline void
ib_mlnx_extended_set_router_lid_bit(ib_mlnx_ext_port_info_t *p_epi,
				    boolean_t enable) {
	ib_mlnx_extended_set_mepi_resvd1_bit(p_epi, 0, ROUTER_LID_ENABLED_BIT, enable);
}
/*
* PARAMETERS
*	p_epi
*		[in][out] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_get_port_recovered
* NAME
*	ib_mlnx_epi_get_port_recovered
*
* DESCRIPTION
* 	Returns if port_recovered bit is set,
* 	i.e whether port is during a recovery or has gone through
* 	a successful recovery event since the last time this bit was cleared.
*	Reserved when IB_GENERAL_INFO_IS_PORT_RECOVERY_POLICY_CONFIG_SUPPORTED_BIT is not set.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mlnx_epi_get_port_recovered(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return ib_mlnx_extended_get_mepi_resvd1_bit(p_epi, 0, IB_EXT_PORT_INFO_PORT_RECOVERY_BIT);
}
/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
* 	True if the port_recovered bit is set, False otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_extended_set_port_recovered_bit
* NAME
*	ib_mlnx_extended_set_port_recovered_bit
*
* DESCRIPTION
*	Sets the port_recovered bit.
*
* SYNOPSIS
*/
static inline void
ib_mlnx_extended_set_port_recovered_bit(ib_mlnx_ext_port_info_t *p_epi, boolean_t enable)
{
	ib_mlnx_extended_set_mepi_resvd1_bit(p_epi, 0, IB_EXT_PORT_INFO_PORT_RECOVERY_BIT, enable);
}
/*
* PARAMETERS
*	p_epi
*		[in][out] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mlnx_epi_supports_dual_mkey
* NAME
*	ib_mlnx_epi_supports_dual_mkey
*
* DESCRIPTION
*	Returns value of ExtendedPortInfo.CapabilityMask.Bit9
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_mlnx_epi_supports_dual_mkey(IN const ib_mlnx_ext_port_info_t * p_epi)
{
	return p_epi->capability_mask & IB_MEPI_CAP_DUAL_MKEY;
}

/*
* PARAMETERS
*	p_epi
*		[in] Pointer to a ib_mlnx_ext_port_info_t attribute.
*
* RETURN VALUES
*	The value of ExtendedPortInfo.CapabilityMask.Bit9
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_mlnx_ext_node_info_t
* NAME
*	ib_mlnx_ext_node_info_t
*
* DESCRIPTION
*	Mellanox ExtendedNodeInfo (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_mlnx_ext_node_info {
	uint8_t num_oob;
	uint8_t num_pcie;
	uint8_t sl_2_vl_act;
	uint8_t sl_2_vl_cap;
	/* other fields will be added later */
	uint8_t reserved[9];
	uint8_t anycast_lid_table_top;
	uint8_t anycast_lid_table_cap;
	uint8_t ext_node_type;
	uint8_t enable_bits;
	uint8_t reserved1[2];
	uint8_t asic_max_planes;
	uint8_t reserved2[44];
} PACK_SUFFIX ib_mlnx_ext_node_info_t;
#include <complib/cl_packoff.h>
/************/
/*
* FIELDS
*       enable_bits
*               Enable bits for the node.
*		bit 8: multiplane_as ymmetry_en
*			0: Asymmetry disabled (symmetry).
*			1: Asymmetry enabled.
*		bit 7: link_auto_retrain_en
*			0: Link auto-retrain capability disabled.
*			1: Link auto-retrain capability enabled.
*
* SEE ALSO
*********/

#define IB_MENI_MULTIPLANE_ASYMMETRY_EN_SHIFT		7
#define IB_MENI_LINK_AUTO_RETRAIN_EN_SHIFT		6

#define IB_MENI_MULTIPLANE_ASYMMETRY_EN_MASK_HO		0x80
#define IB_MENI_LINK_AUTO_RETRAIN_EN_MASK_HO		0x40

/****f* IBA Base: Types/ib_mlnx_ext_node_info_set_link_auto_retrain_en
* NAME
*	ib_mlnx_ext_node_info_set_link_auto_retrain_en
*
* DESCRIPTION
*	Sets the link auto-retrain enable bit.
*
*/
static inline void OSM_API
ib_mlnx_ext_node_info_set_link_auto_retrain_en(ib_mlnx_ext_node_info_t *p_eni, uint8_t enable_bit)
{
	IBTYPES_SET_ATTR8(p_eni->enable_bits, IB_MENI_LINK_AUTO_RETRAIN_EN_MASK_HO, IB_MENI_LINK_AUTO_RETRAIN_EN_SHIFT, enable_bit);
}

/****f* IBA Base: Types/ib_mlnx_ext_node_info_get_link_auto_retrain_en
* NAME
*	ib_mlnx_ext_node_info_get_link_auto_retrain_en
*
* DESCRIPTION
*	Gets the link auto-retrain enable bit.
*
*/
static inline uint8_t OSM_API
ib_mlnx_ext_node_info_get_link_auto_retrain_en(ib_mlnx_ext_node_info_t *p_eni)
{
	return IBTYPES_GET_ATTR8(p_eni->enable_bits, IB_MENI_LINK_AUTO_RETRAIN_EN_MASK_HO, IB_MENI_LINK_AUTO_RETRAIN_EN_SHIFT);
}

/****d* IBA Base: Constants/IB_SERVICE_NAME_SIZE
* NAME
*	IB_SERVICE_NAME_SIZE
*
* DESCRIPTION
*	Number of bytes in service name field in service record.
*
* SOURCE
*/
#define IB_SERVICE_NAME_SIZE 64
/*********/

typedef uint8_t ib_svc_name_t[IB_SERVICE_NAME_SIZE];

#include <complib/cl_packon.h>
typedef struct _ib_service_record {
	ib_net64_t service_id;
	ib_gid_t service_gid;
	ib_net16_t service_pkey;
	ib_net16_t resv;
	ib_net32_t service_lease;
	uint8_t service_key[16];
	ib_svc_name_t service_name;
	uint8_t service_data8[16];
	ib_net16_t service_data16[8];
	ib_net32_t service_data32[4];
	ib_net64_t service_data64[2];
} PACK_SUFFIX ib_service_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_portinfo_record {
	ib_net16_t lid;
	uint8_t port_num;
	uint8_t options;
	ib_port_info_t port_info;
	uint8_t pad[4];
} PACK_SUFFIX ib_portinfo_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_portinfoext_record {
	ib_net16_t lid;
	uint8_t port_num;
	uint8_t options;
	ib_port_info_ext_t port_info_ext;
} PACK_SUFFIX ib_portinfoext_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_link_record {
	ib_net16_t from_lid;
	uint8_t from_port_num;
	uint8_t to_port_num;
	ib_net16_t to_lid;
	uint8_t pad[2];
} PACK_SUFFIX ib_link_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_sminfo_record {
	ib_net16_t lid;
	uint16_t resv0;
	ib_sm_info_t sm_info;
	uint8_t pad[7];
} PACK_SUFFIX ib_sminfo_record_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_lft_record_t
* NAME
*	ib_lft_record_t
*
* DESCRIPTION
*	IBA defined LinearForwardingTableRecord (15.2.5.6)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_lft_record {
	ib_net16_t lid;
	ib_net16_t block_num;
	uint32_t resv0;
	uint8_t lft[64];
} PACK_SUFFIX ib_lft_record_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_mft_record_t
* NAME
*	ib_mft_record_t
*
* DESCRIPTION
*	IBA defined MulticastForwardingTableRecord (15.2.5.8)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_mft_record {
	ib_net16_t lid;
	ib_net16_t position_block_num;
	uint32_t resv0;
	ib_net16_t mft[IB_MCAST_BLOCK_SIZE];
} PACK_SUFFIX ib_mft_record_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_switch_info_t
* NAME
*	ib_switch_info_t
*
* DESCRIPTION
*	IBA defined SwitchInfo. (14.2.5.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_switch_info {
	ib_net16_t lin_cap;
	ib_net16_t rand_cap;
	ib_net16_t mcast_cap;
	ib_net16_t lin_top;
	uint8_t def_port;
	uint8_t def_mcast_pri_port;
	uint8_t def_mcast_not_port;
	uint8_t life_state;
	ib_net16_t lids_per_port;
	ib_net16_t enforce_cap;
	uint8_t flags;
	uint8_t resvd;
	ib_net16_t mcast_top;
} PACK_SUFFIX ib_switch_info_t;
#include <complib/cl_packoff.h>
/************/

#include <complib/cl_packon.h>
typedef struct _ib_switch_info_record {
	ib_net16_t lid;
	uint16_t resv0;
	ib_switch_info_t switch_info;
} PACK_SUFFIX ib_switch_info_record_t;
#include <complib/cl_packoff.h>

#define IB_SWITCH_PSC					0x04
#define IB_SWITCH_OPT_SL2VL				0x03
#define IB_SWITCH_INFO_OPT_SL2VL_WILDCARDED_OPTIMIZED	0x01
#define IB_SWITCH_INFO_OPT_SL2VL_MASKED_OPTIMIZED	0x02
#define IB_SWITCH_INFO_LIFE_TIME_MASK			0xF8
#define IB_SWITCH_INFO_LIFE_TIME_SHIFT			3

/****f* IBA Base: Types/ib_switch_info_get_state_change
* NAME
*	ib_switch_info_get_state_change
*
* DESCRIPTION
*	Returns the value of the state change flag.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_switch_info_get_state_change(IN const ib_switch_info_t * const p_si)
{
	return ((p_si->life_state & IB_SWITCH_PSC) == IB_SWITCH_PSC);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*
* RETURN VALUES
*	Returns the value of the state change flag.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_switch_info_clear_state_change
* NAME
*	ib_switch_info_clear_state_change
*
* DESCRIPTION
*	Clears the switch's state change bit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_switch_info_clear_state_change(IN ib_switch_info_t * const p_si)
{
	p_si->life_state = (uint8_t) (p_si->life_state & ~IB_SWITCH_PSC);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_switch_info_set_state_change
* NAME
*	ib_switch_info_set_state_change
*
* DESCRIPTION
*	Clears the switch's state change bit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_switch_info_set_state_change(IN ib_switch_info_t * const p_si)
{
	p_si->life_state = (uint8_t) (p_si->life_state | IB_SWITCH_PSC);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_switch_info_get_opt_sl2vlmapping
* NAME
*	ib_switch_info_get_state_opt_sl2vlmapping
*
* DESCRIPTION
*       Returns the value of the optimized SLtoVLMapping programming field.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_switch_info_get_opt_sl2vlmapping_programming(IN const ib_switch_info_t * const p_si)
{
	return (p_si->life_state & IB_SWITCH_OPT_SL2VL);
}

/****f* IBA Base: Types/ib_switch_info_get_opt_sl2vlmapping_programming
* NAME
*	ib_switch_info_get_state_opt_sl2vlmapping_programming
*
* DESCRIPTION
*       Returns the value of the optimized SLtoVLMapping programming flag
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_switch_info_get_opt_sl2vlmapping(IN const ib_switch_info_t * const p_si)
{
        return ((p_si->life_state & IB_SWITCH_INFO_OPT_SL2VL_WILDCARDED_OPTIMIZED) ==
		IB_SWITCH_INFO_OPT_SL2VL_WILDCARDED_OPTIMIZED);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*
* RETURN VALUES
*	Returns the value of the optimized SLtoVLMapping programming flag.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_switch_info_set_life_time
* NAME
*	ib_switch_info_set_life_time
*
* DESCRIPTION
*	Sets the value of LifeTimeValue.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_switch_info_set_life_time(IN ib_switch_info_t * const p_si,
			     IN const uint8_t life_time_val)
{
	p_si->life_state = (p_si->life_state & ~IB_SWITCH_INFO_LIFE_TIME_MASK) |
			   (life_time_val << IB_SWITCH_INFO_LIFE_TIME_SHIFT);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*	life_time_val
*		[in] LiveTimeValue.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_switch_info_is_enhanced_port0
* NAME
*	ib_switch_info_is_enhanced_port0
*
* DESCRIPTION
*	Returns TRUE if the enhancedPort0 bit is on (meaning the switch
*  port zero supports enhanced functions).
*  Returns FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_switch_info_is_enhanced_port0(IN const ib_switch_info_t * const p_si)
{
	return ((p_si->flags & 0x08) == 0x08);
}

/*
* PARAMETERS
*	p_si
*		[in] Pointer to a SwitchInfo attribute.
*
* RETURN VALUES
*	Returns TRUE if the switch supports enhanced port 0. FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_guid_info_t
* NAME
*	ib_guid_info_t
*
* DESCRIPTION
*	IBA defined GuidInfo. (14.2.5.5)
*
* SYNOPSIS
*/
#define	GUID_TABLE_MAX_ENTRIES		8

#include <complib/cl_packon.h>
typedef struct _ib_guid_info {
	ib_net64_t guid[GUID_TABLE_MAX_ENTRIES];
} PACK_SUFFIX ib_guid_info_t;
#include <complib/cl_packoff.h>
/************/

#include <complib/cl_packon.h>
typedef struct _ib_guidinfo_record {
	ib_net16_t lid;
	uint8_t block_num;
	uint8_t resv;
	uint32_t reserved;
	ib_guid_info_t guid_info;
} PACK_SUFFIX ib_guidinfo_record_t;
#include <complib/cl_packoff.h>

#define IB_MULTIPATH_MAX_GIDS 11	/* Support max that can fit into first MAD (for now) */

#include <complib/cl_packon.h>
typedef struct _ib_multipath_rec_t {
	ib_net32_t hop_flow_raw;
	uint8_t tclass;
	uint8_t num_path;
	ib_net16_t pkey;
	ib_net16_t qos_class_sl;
	uint8_t mtu;
	uint8_t rate;
	uint8_t pkt_life;
	uint8_t service_id_8msb;
	uint8_t independence;	/* formerly resv2 */
	uint8_t sgid_count;
	uint8_t dgid_count;
	uint8_t service_id_56lsb[7];
	ib_gid_t gids[IB_MULTIPATH_MAX_GIDS];
} PACK_SUFFIX ib_multipath_rec_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*       hop_flow_raw
*               Global routing parameters: hop count, flow label and raw bit.
*
*       tclass
*               Another global routing parameter.
*
*       num_path
*     Reversible path - 1 bit to say if path is reversible.
*               num_path [6:0] In queries, maximum number of paths to return.
*               In responses, undefined.
*
*       pkey
*               Partition key (P_Key) to use on this path.
*
*       qos_class_sl
*               QoS class and service level to use on this path.
*
*       mtu
*               MTU and MTU selector fields to use on this path
*       rate
*               Rate and rate selector fields to use on this path.
*
*       pkt_life
*               Packet lifetime
*
*	service_id_8msb
*		8 most significant bits of Service ID
*
*	service_id_56lsb
*		56 least significant bits of Service ID
*
*       preference
*               Indicates the relative merit of this path versus other path
*               records returned from the SA.  Lower numbers are better.
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_multipath_rec_num_path
* NAME
*       ib_multipath_rec_num_path
*
* DESCRIPTION
*       Get max number of paths to return.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_num_path(IN const ib_multipath_rec_t * const p_rec)
{
	return (p_rec->num_path & 0x7F);
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Maximum number of paths to return for each unique SGID_DGID combination.
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_set_sl
* NAME
*	ib_multipath_rec_set_sl
*
* DESCRIPTION
*	Set path service level.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_multipath_rec_set_sl(
	IN ib_multipath_rec_t* const p_rec,
	IN const uint8_t sl )
{
	p_rec->qos_class_sl =
		(p_rec->qos_class_sl & CL_HTON16(IB_MULTIPATH_REC_QOS_CLASS_MASK)) |
			cl_hton16(sl & IB_MULTIPATH_REC_SL_MASK);
}
/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the MultiPath record object.
*
*	sl
*		[in] Service level to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_sl
* NAME
*       ib_multipath_rec_sl
*
* DESCRIPTION
*       Get multipath service level.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_sl(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t) ((cl_ntoh16(p_rec->qos_class_sl)) & IB_MULTIPATH_REC_SL_MASK));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*	SL.
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_set_qos_class
* NAME
*	ib_multipath_rec_set_qos_class
*
* DESCRIPTION
*	Set path QoS class.
*
* SYNOPSIS
*/
static inline void	OSM_API
ib_multipath_rec_set_qos_class(
	IN ib_multipath_rec_t* const p_rec,
	IN const uint16_t qos_class )
{
	p_rec->qos_class_sl =
		(p_rec->qos_class_sl & CL_HTON16(IB_MULTIPATH_REC_SL_MASK)) |
			cl_hton16(qos_class << 4);
}
/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the MultiPath record object.
*
*	qos_class
*		[in] QoS class to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_qos_class
* NAME
*	ib_multipath_rec_qos_class
*
* DESCRIPTION
*	Get QoS class.
*
* SYNOPSIS
*/
static inline uint16_t	OSM_API
ib_multipath_rec_qos_class(
	IN	const	ib_multipath_rec_t* const	p_rec )
{
	return (cl_ntoh16( p_rec->qos_class_sl ) >> 4);
}
/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the MultiPath record object.
*
* RETURN VALUES
*	QoS class of the MultiPath record.
*
* NOTES
*
* SEE ALSO
*	ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_mtu
* NAME
*       ib_multipath_rec_mtu
*
* DESCRIPTION
*       Get encoded path MTU.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_mtu(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->mtu & IB_MULTIPATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Encoded path MTU.
*               1: 256
*               2: 512
*               3: 1024
*               4: 2048
*               5: 4096
*               others: reserved
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_mtu_sel
* NAME
*       ib_multipath_rec_mtu_sel
*
* DESCRIPTION
*       Get encoded multipath MTU selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_mtu_sel(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t) ((p_rec->mtu & IB_MULTIPATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Encoded path MTU selector value (for queries).
*               0: greater than MTU specified
*               1: less than MTU specified
*               2: exactly the MTU specified
*               3: largest MTU available
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_rate
* NAME
*	ib_multipath_rec_rate
*
* DESCRIPTION
*	Get encoded multipath rate.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_rate(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->rate & IB_MULTIPATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the multipath record object.
*
* RETURN VALUES
*	Encoded multipath rate.
*		2: 2.5 Gb/sec.
*		3: 10 Gb/sec.
*		4: 30 Gb/sec.
*		5: 5 Gb/sec.
*		6: 20 Gb/sec.
*		7: 40 Gb/sec.
*		8: 60 Gb/sec.
*		9: 80 Gb/sec.
*		10: 120 Gb/sec.
*		11: 14 Gb/sec.
*		12: 56 Gb/sec.
*		13: 112 Gb/sec.
*		14: 168 Gb/sec.
*		15: 25 Gb/sec.
*		16: 100 Gb/sec.
*		17: 200 Gb/sec.
*		18: 300 Gb/sec.
*		19: 28 Gb/sec.
*		20: 50 Gb/sec.
*		21: 400 Gb/sec.
*		22: 600 Gb/sec.
*               others: reserved
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_rate_sel
* NAME
*       ib_multipath_rec_rate_sel
*
* DESCRIPTION
*       Get encoded multipath rate selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_rate_sel(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t)
		((p_rec->rate & IB_MULTIPATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Encoded path rate selector value (for queries).
*               0: greater than rate specified
*               1: less than rate specified
*               2: exactly the rate specified
*               3: largest rate available
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_pkt_life
* NAME
*       ib_multipath_rec_pkt_life
*
* DESCRIPTION
*       Get encoded multipath pkt_life.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_pkt_life(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t) (p_rec->pkt_life & IB_MULTIPATH_REC_BASE_MASK));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Encoded multipath pkt_life = 4.096 usec * 2 ** PacketLifeTime.
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_pkt_life_sel
* NAME
*       ib_multipath_rec_pkt_life_sel
*
* DESCRIPTION
*       Get encoded multipath pkt_lifetime selector.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_multipath_rec_pkt_life_sel(IN const ib_multipath_rec_t * const p_rec)
{
	return ((uint8_t)
		((p_rec->pkt_life & IB_MULTIPATH_REC_SELECTOR_MASK) >> 6));
}

/*
* PARAMETERS
*       p_rec
*               [in] Pointer to the multipath record object.
*
* RETURN VALUES
*       Encoded path pkt_lifetime selector value (for queries).
*               0: greater than rate specified
*               1: less than rate specified
*               2: exactly the rate specified
*               3: smallest packet lifetime available
*
* NOTES
*
* SEE ALSO
*       ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_service_id
* NAME
*	ib_multipath_rec_service_id
*
* DESCRIPTION
*	Get multipath service id.
*
* SYNOPSIS
*/
static inline ib_net64_t OSM_API
ib_multipath_rec_service_id(IN const ib_multipath_rec_t * const p_rec)
{
	union {
		ib_net64_t sid;
		uint8_t sid_arr[8];
	} sid_union;
	sid_union.sid_arr[0] = p_rec->service_id_8msb;
	memcpy(&sid_union.sid_arr[1], p_rec->service_id_56lsb, 7);
	return sid_union.sid;
}

/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the multipath record object.
*
* RETURN VALUES
*	Service ID
*
* NOTES
*
* SEE ALSO
*	ib_multipath_rec_t
*********/

/****f* IBA Base: Types/ib_multipath_rec_flow_lbl
* NAME
*	ib_multipath_rec_flow_lbl
*
* DESCRIPTION
*	Get flow label.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_multipath_rec_flow_lbl(IN const ib_multipath_rec_t * const p_rec)
{
	return (cl_ntoh32(p_rec->hop_flow_raw) >> 8) & 0x000FFFFF;
}
/*
* PARAMETERS
*	p_rec
*		[in] Pointer to the multipath record object.
*
* RETURN VALUES
*	Flow label
*
* NOTES
*
* SEE ALSO
*	ib_multipath_rec_t
*********/

#define IB_NUM_PKEY_ELEMENTS_IN_BLOCK		32
/****s* IBA Base: Types/ib_pkey_table_t
* NAME
*	ib_pkey_table_t
*
* DESCRIPTION
*	IBA defined PKey table. (14.2.5.7)
*
* SYNOPSIS
*/

#include <complib/cl_packon.h>
typedef struct _ib_pkey_table {
	ib_net16_t pkey_entry[IB_NUM_PKEY_ELEMENTS_IN_BLOCK];
} PACK_SUFFIX ib_pkey_table_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_pkey_table_record_t
* NAME
*	ib_pkey_table_record_t
*
* DESCRIPTION
*	IBA defined P_Key Table Record for SA Query. (15.2.5.11)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_pkey_table_record {
	ib_net16_t lid;		// for CA: lid of port, for switch lid of port 0
	ib_net16_t block_num;
	uint8_t port_num;	// for switch: port number, for CA: reserved
	uint8_t reserved1;
	uint16_t reserved2;
	ib_pkey_table_t pkey_tbl;
} PACK_SUFFIX ib_pkey_table_record_t;
#include <complib/cl_packoff.h>
/************/

#define IB_DROP_VL 15
#define IB_MAX_NUM_VLS 16
/****s* IBA Base: Types/ib_slvl_table_t
* NAME
*	ib_slvl_table_t
*
* DESCRIPTION
*	IBA defined SL2VL Mapping Table Attribute. (14.2.5.8)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_slvl_table {
	uint8_t raw_vl_by_sl[IB_MAX_NUM_VLS / 2];
} PACK_SUFFIX ib_slvl_table_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_slvl_table_record_t
* NAME
*	ib_slvl_table_record_t
*
* DESCRIPTION
*	IBA defined SL to VL Mapping Table Record for SA Query. (15.2.5.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_slvl_table_record {
	ib_net16_t lid;		// for CA: lid of port, for switch lid of port 0
	uint8_t in_port_num;	// reserved for CAs
	uint8_t out_port_num;	// reserved for CAs
	uint32_t resv;
	ib_slvl_table_t slvl_tbl;
} PACK_SUFFIX ib_slvl_table_record_t;
#include <complib/cl_packoff.h>
/************/

/****f* IBA Base: Types/ib_slvl_table_set
* NAME
*	ib_slvl_table_set
*
* DESCRIPTION
*	Set slvl table entry.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_slvl_table_set(IN ib_slvl_table_t * p_slvl_tbl,
		  IN uint8_t sl_index, IN uint8_t vl)
{
	uint8_t idx = sl_index / 2;
	CL_ASSERT(vl <= 15);
	CL_ASSERT(sl_index <= 15);

	if (sl_index % 2)
		/* this is an odd sl. Need to update the ls bits */
		p_slvl_tbl->raw_vl_by_sl[idx] =
		    (p_slvl_tbl->raw_vl_by_sl[idx] & 0xF0) | vl;
	else
		/* this is an even sl. Need to update the ms bits */
		p_slvl_tbl->raw_vl_by_sl[idx] =
		    (vl << 4) | (p_slvl_tbl->raw_vl_by_sl[idx] & 0x0F);
}

/*
* PARAMETERS
*	p_slvl_tbl
*		[in] pointer to ib_slvl_table_t object.
*
*	sl_index
*		[in] the sl index in the table to be updated.
*
*	vl
*		[in] the vl value to update for that sl.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	ib_slvl_table_t
*********/

/****f* IBA Base: Types/ib_slvl_table_get
* NAME
*	ib_slvl_table_get
*
* DESCRIPTION
*	Get slvl table entry.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_slvl_table_get(IN const ib_slvl_table_t * p_slvl_tbl, IN uint8_t sl_index)
{
	uint8_t idx = sl_index / 2;
	CL_ASSERT(sl_index <= 15);

	if (sl_index % 2)
		/* this is an odd sl. Need to return the ls bits. */
		return (p_slvl_tbl->raw_vl_by_sl[idx] & 0x0F);
	else
		/* this is an even sl. Need to return the ms bits. */
		return ((p_slvl_tbl->raw_vl_by_sl[idx] & 0xF0) >> 4);
}

/*
* PARAMETERS
*	p_slvl_tbl
*		[in] pointer to ib_slvl_table_t object.
*
*	sl_index
*		[in] the sl index in the table whose value should be returned.
*
* RETURN VALUES
*	vl for the requested sl_index.
*
* NOTES
*
* SEE ALSO
*	ib_slvl_table_t
*********/

/****s* IBA Base: Types/ib_vl_arb_element_t
* NAME
*	ib_vl_arb_element_t
*
* DESCRIPTION
*	IBA defined VL Arbitration Table Element. (14.2.5.9)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vl_arb_element {
	uint8_t vl;
	uint8_t weight;
} PACK_SUFFIX ib_vl_arb_element_t;
#include <complib/cl_packoff.h>
/************/

#define IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK 32

/****s* IBA Base: Types/ib_vl_arb_table_t
* NAME
*	ib_vl_arb_table_t
*
* DESCRIPTION
*	IBA defined VL Arbitration Table. (14.2.5.9)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vl_arb_table {
	ib_vl_arb_element_t vl_entry[IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK];
} PACK_SUFFIX ib_vl_arb_table_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_vl_arb_table_record_t
* NAME
*	ib_vl_arb_table_record_t
*
* DESCRIPTION
*	IBA defined VL Arbitration Table Record for SA Query. (15.2.5.9)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vl_arb_table_record {
	ib_net16_t lid;		// for CA: lid of port, for switch lid of port 0
	uint8_t port_num;
	uint8_t block_num;
	uint32_t reserved;
	ib_vl_arb_table_t vl_arb_tbl;
} PACK_SUFFIX ib_vl_arb_table_record_t;
#include <complib/cl_packoff.h>
/************/

/*
 *	Global route header information received with unreliable datagram messages
 */
#include <complib/cl_packon.h>
typedef struct _ib_grh {
	ib_net32_t ver_class_flow;
	ib_net16_t resv1;
	uint8_t resv2;
	uint8_t hop_limit;
	ib_gid_t src_gid;
	ib_gid_t dest_gid;
} PACK_SUFFIX ib_grh_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_grh_get_ver_class_flow
* NAME
*	ib_grh_get_ver_class_flow
*
* DESCRIPTION
*	Get encoded version, traffic class and flow label in grh
*
* SYNOPSIS
*/
static inline void OSM_API
ib_grh_get_ver_class_flow(IN const ib_net32_t ver_class_flow,
			  OUT uint8_t * const p_ver,
			  OUT uint8_t * const p_tclass,
			  OUT uint32_t * const p_flow_lbl)
{
	ib_net32_t tmp_ver_class_flow;

	if (p_ver)
		*p_ver = (uint8_t) (ver_class_flow & 0x0f);

	tmp_ver_class_flow = ver_class_flow >> 4;

	if (p_tclass)
		*p_tclass = (uint8_t) (tmp_ver_class_flow & 0xff);

	tmp_ver_class_flow = tmp_ver_class_flow >> 8;

	if (p_flow_lbl)
		*p_flow_lbl = tmp_ver_class_flow & 0xfffff;
}

/*
* PARAMETERS
*	ver_class_flow
*		[in] the version, traffic class and flow label info.
*
* RETURN VALUES
*	p_ver
*		[out] pointer to the version info.
*
*	p_tclass
*		[out] pointer to the traffic class info.
*
*	p_flow_lbl
*		[out] pointer to the flow label info
*
* NOTES
*
* SEE ALSO
*	ib_grh_t
*********/

/****f* IBA Base: Types/ib_grh_set_ver_class_flow
* NAME
*	ib_grh_set_ver_class_flow
*
* DESCRIPTION
*	Set encoded version, traffic class and flow label in grh
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_grh_set_ver_class_flow(IN const uint8_t ver,
			  IN const uint8_t tclass, IN const uint32_t flow_lbl)
{
	ib_net32_t ver_class_flow;

	ver_class_flow = flow_lbl;
	ver_class_flow = ver_class_flow << 8;
	ver_class_flow = ver_class_flow | tclass;
	ver_class_flow = ver_class_flow << 4;
	ver_class_flow = ver_class_flow | ver;
	return (ver_class_flow);
}

/*
* PARAMETERS
*	ver
*		[in] the version info.
*
*	tclass
*		[in] the traffic class info.
*
*	flow_lbl
*		[in] the flow label info
*
* RETURN VALUES
*	ver_class_flow
*		[out] the version, traffic class and flow label info.
*
* NOTES
*
* SEE ALSO
*	ib_grh_t
*********/

/****s* IBA Base: Types/ib_member_rec_t
* NAME
*	ib_member_rec_t
*
* DESCRIPTION
*	Multicast member record, used to create, join, and leave multicast
*	groups.
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_member_rec {
	ib_gid_t mgid;
	ib_gid_t port_gid;
	ib_net32_t qkey;
	ib_net16_t mlid;
	uint8_t mtu;
	uint8_t tclass;
	ib_net16_t pkey;
	uint8_t rate;
	uint8_t pkt_life;
	ib_net32_t sl_flow_hop;
	uint8_t scope_state;
	uint8_t proxy_join:1;
	uint8_t reserved[2];
	uint8_t pad[4];
} PACK_SUFFIX ib_member_rec_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	mgid
*		Multicast GID address for this multicast group.
*
*	port_gid
*		Valid GID of the endpoint joining this multicast group.
*
*	qkey
*		Q_Key to be sued by this multicast group.
*
*	mlid
*		Multicast LID for this multicast group.
*
*	mtu
*		MTU and MTU selector fields to use on this path
*
*	tclass
*		Another global routing parameter.
*
*	pkey
*		Partition key (P_Key) to use for this member.
*
*	rate
*		Rate and rate selector fields to use on this path.
*
*	pkt_life
*		Packet lifetime
*
*	sl_flow_hop
*		Global routing parameters: service level, hop count, and flow label.
*
*	scope_state
*		MGID scope and JoinState of multicast request.
*
*	proxy_join
*		Enables others in the Partition to proxy add/remove from the group
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_member_get_sl_flow_hop
* NAME
*	ib_member_get_sl_flow_hop
*
* DESCRIPTION
*	Get encoded sl, flow label, and hop limit
*
* SYNOPSIS
*/
static inline void OSM_API
ib_member_get_sl_flow_hop(IN const ib_net32_t sl_flow_hop,
			  OUT uint8_t * const p_sl,
			  OUT uint32_t * const p_flow_lbl,
			  OUT uint8_t * const p_hop)
{
	uint32_t tmp;

	tmp = cl_ntoh32(sl_flow_hop);
	if (p_hop)
		*p_hop = (uint8_t) tmp;
	tmp >>= 8;

	if (p_flow_lbl)
		*p_flow_lbl = (uint32_t) (tmp & 0xfffff);
	tmp >>= 20;

	if (p_sl)
		*p_sl = (uint8_t) tmp;
}

/*
* PARAMETERS
*	sl_flow_hop
*		[in] the sl, flow label, and hop limit of MC Group
*
* RETURN VALUES
*	p_sl
*		[out] pointer to the service level
*
*	p_flow_lbl
*		[out] pointer to the flow label info
*
*	p_hop
*		[out] pointer to the hop count limit.
*
* NOTES
*
* SEE ALSO
*	ib_member_rec_t
*********/

/****f* IBA Base: Types/ib_member_set_sl_flow_hop
* NAME
*	ib_member_set_sl_flow_hop
*
* DESCRIPTION
*	Set encoded sl, flow label, and hop limit
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_member_set_sl_flow_hop(IN const uint8_t sl,
			  IN const uint32_t flow_label,
			  IN const uint8_t hop_limit)
{
	uint32_t tmp;

	tmp = (sl << 28) | ((flow_label & 0xfffff) << 8) | hop_limit;
	return cl_hton32(tmp);
}

/*
* PARAMETERS
*	sl
*		[in] the service level.
*
*	flow_lbl
*		[in] the flow label info
*
*	hop_limit
*		[in] the hop limit.
*
* RETURN VALUES
*	sl_flow_hop
*		[out] the encoded sl, flow label, and hop limit
*
* NOTES
*
* SEE ALSO
*	ib_member_rec_t
*********/

/****f* IBA Base: Types/ib_member_get_scope_state
* NAME
*	ib_member_get_scope_state
*
* DESCRIPTION
*	Get encoded MGID scope and JoinState
*
* SYNOPSIS
*/
static inline void OSM_API
ib_member_get_scope_state(IN const uint8_t scope_state,
			  OUT uint8_t * const p_scope,
			  OUT uint8_t * const p_state)
{
	uint8_t tmp_scope_state;

	if (p_state)
		*p_state = (uint8_t) (scope_state & 0x0f);

	tmp_scope_state = scope_state >> 4;

	if (p_scope)
		*p_scope = (uint8_t) (tmp_scope_state & 0x0f);

}

/*
* PARAMETERS
*	scope_state
*		[in] the scope and state
*
* RETURN VALUES
*	p_scope
*		[out] pointer to the MGID scope
*
*	p_state
*		[out] pointer to the join state
*
* NOTES
*
* SEE ALSO
*	ib_member_rec_t
*********/

/****f* IBA Base: Types/ib_member_set_scope_state
* NAME
*	ib_member_set_scope_state
*
* DESCRIPTION
*	Set encoded version, MGID scope and JoinState
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_member_set_scope_state(IN const uint8_t scope, IN const uint8_t state)
{
	uint8_t scope_state;

	scope_state = scope;
	scope_state = scope_state << 4;
	scope_state = scope_state | state;
	return (scope_state);
}

/*
* PARAMETERS
*	scope
*		[in] the MGID scope
*
*	state
*		[in] the JoinState
*
* RETURN VALUES
*	scope_state
*		[out] the encoded one
*
* NOTES
*
* SEE ALSO
*	ib_member_rec_t
*********/

/****f* IBA Base: Types/ib_member_set_join_state
* NAME
*	ib_member_set_join_state
*
* DESCRIPTION
*	Set JoinState
*
* SYNOPSIS
*/
static inline void OSM_API
ib_member_set_join_state(IN OUT ib_member_rec_t * p_mc_rec,
			 IN const uint8_t state)
{
	/* keep the scope as it is */
	p_mc_rec->scope_state = (p_mc_rec->scope_state & 0xF0) | (0x0f & state);
}

/*
* PARAMETERS
*	p_mc_rec
*		[in] pointer to the member record
*
*	state
*		[in] the JoinState
*
* RETURN VALUES
*	NONE
*
* NOTES
*
* SEE ALSO
*	ib_member_rec_t
*********/

/*
 * Join State Codes:
 */
#define IB_MC_REC_STATE_FULL_MEMBER 0x01
#define IB_MC_REC_STATE_NON_MEMBER 0x02
#define IB_MC_REC_STATE_SEND_ONLY_NON_MEMBER 0x04
#define IB_MC_REC_STATE_SEND_ONLY_FULL_MEMBER 0x08

/****d* IBA Base: Constants/IB_MC_REC_SELECTOR_MASK
* NAME
*       IB_MC_REC_SELECTOR_MASK
*
* DESCRIPTION
*       Mask for the selector field for member record MTU, rate,
*       and packet lifetime.
*
* SOURCE
*/
#define IB_MC_REC_SELECTOR_MASK             		0xC0
/*********/

/****d* IBA Base: Constants/IB_MC_REC_BASE_MASK
* NAME
*       IB_MC_REC_BASE_MASK
*
* DESCRIPTION
*       Mask for the base value field for member record MTU, rate,
*       and packet lifetime.
*
* SOURCE
*/
#define IB_MC_REC_BASE_MASK					0x3F
/*********/

/*
 *	Generic MAD notice types
 */
#define IB_NOTICE_TYPE_FATAL				0x00
#define IB_NOTICE_TYPE_URGENT				0x01
#define IB_NOTICE_TYPE_SECURITY				0x02
#define IB_NOTICE_TYPE_SUBN_MGMT			0x03
#define IB_NOTICE_TYPE_INFO				0x04
#define IB_NOTICE_TYPE_EMPTY				0x7F

#define IB_TRAP_UNAUTHORIZED_PACKETS_MAX_PATH_LEN	16

#define IB_CND_PORT_STATE_CHANGE_TRAP_ID_MASK_LEN		256
#define IB_CND_PORT_STATE_CHANGE_TRAP_ID_MASK_BYTES_LEN		IB_CND_PORT_STATE_CHANGE_TRAP_ID_MASK_LEN / \
								CL_BITS_PER_BYTE

#include <complib/cl_packon.h>
typedef struct _ib_mad_notice_attr	// Total Size calc  Accumulated
{
	uint8_t generic_type;	// 1                1
	union _notice_g_or_v {
		struct _notice_generic	// 5                6
		{
			uint8_t prod_type_msb;
			ib_net16_t prod_type_lsb;
			ib_net16_t trap_num;
		} PACK_SUFFIX generic;
		struct _notice_vend {
			uint8_t vend_id_msb;
			ib_net16_t vend_id_lsb;
			ib_net16_t dev_id;
		} PACK_SUFFIX vend;
	} g_or_v;
	ib_net16_t issuer_lid;	// 2                 8
	ib_net16_t toggle_count;	// 2                 10
	union _data_details	// 54                64
	{
		struct _raw_data {
			uint8_t details[54];
		} PACK_SUFFIX raw_data;
		struct _ntc_64_67 {
			uint8_t res[6];
			ib_gid_t gid;	// the Node or Multicast Group that came in/out
		} PACK_SUFFIX ntc_64_67;
		struct _ntc_128 {
			ib_net16_t sw_lid;	// the sw lid of which link state changed
		} PACK_SUFFIX ntc_128;
		struct _ntc_129_131 {
			ib_net16_t pad;
			ib_net16_t lid;	// lid and port number of the violation
			uint8_t port_num;
		} PACK_SUFFIX ntc_129_131;
		struct _ntc_144 {
			ib_net16_t pad1;
			ib_net16_t lid;             // lid where change occurred
			uint8_t    pad2;            // reserved
			uint8_t    local_changes;   // 7b reserved 1b local changes
			ib_net32_t new_cap_mask;    // new capability mask
			ib_net16_t change_flgs;     // 10b reserved 6b change flags
			ib_net16_t cap_mask2;
		} PACK_SUFFIX ntc_144;
		struct _ntc_145 {
			ib_net16_t pad1;
			ib_net16_t lid;	// lid where sys guid changed
			ib_net16_t pad2;
			ib_net64_t new_sys_guid;	// new system image guid
		} PACK_SUFFIX ntc_145;
		struct _ntc_256 {	// total: 54
			ib_net16_t pad1;	// 2
			ib_net16_t lid;	// 2
			ib_net16_t dr_slid;	// 2
			uint8_t method;	// 1
			uint8_t pad2;	// 1
			ib_net16_t attr_id;	// 2
			ib_net32_t attr_mod;	// 4
			ib_net64_t mkey;	// 8
			uint8_t pad3;	// 1
			uint8_t dr_trunc_hop;	// 1
			uint8_t dr_rtn_path[30];	// 30
		} PACK_SUFFIX ntc_256;
		struct _ntc_257_258	// violation of p/q_key // 49
		{
			ib_net16_t pad1;	// 2
			ib_net16_t lid1;	// 2
			ib_net16_t lid2;	// 2
			ib_net32_t key;	// 4
			ib_net32_t qp1;	// 4b sl, 4b pad, 24b qp1
			ib_net32_t qp2;	// 8b pad, 24b qp2
			ib_gid_t gid1;	// 16
			ib_gid_t gid2;	// 16
		} PACK_SUFFIX ntc_257_258;
		struct _ntc_259	// pkey violation from switch 51
		{
			ib_net16_t data_valid;	// 2
			ib_net16_t lid1;	// 2
			ib_net16_t lid2;	// 2
			ib_net16_t pkey;	// 2
			ib_net32_t sl_qp1; // 4b sl, 4b pad, 24b qp1
			ib_net32_t qp2; // 8b pad, 24b qp2
			ib_gid_t gid1;	// 16
			ib_gid_t gid2;	// 16
			ib_net16_t sw_lid;	// 2
			uint8_t port_no;	// 1
		} PACK_SUFFIX ntc_259;
		struct _ntc_bkey_259	// bkey violation
		{
			ib_net16_t lidaddr;
			uint8_t method;
			uint8_t reserved;
			ib_net16_t attribute_id;
			ib_net32_t attribute_modifier;
			ib_net32_t qp;		// qp is low 24 bits
			ib_net64_t bkey;
			ib_gid_t gid;
		} PACK_SUFFIX ntc_bkey_259;
		struct _ntc_cckey_0	// CC key violation
		{
			ib_net16_t slid;     // source LID from offending packet LRH
			uint8_t method;      // method, from common MAD header
			uint8_t resv0;
			ib_net16_t attribute_id; // Attribute ID, from common MAD header
			ib_net16_t resv1;
			ib_net32_t attribute_modifier; // Attribute Modif, from common MAD header
			ib_net32_t qp;       // 8b pad, 24b dest QP from BTH
			ib_net64_t cc_key;   // CC key of the offending packet
			ib_gid_t source_gid; // GID from GRH of the offending packet
			uint8_t padding[14]; // Padding - ignored on read
		} PACK_SUFFIX ntc_cckey_0;
		struct _vendor_data_details{
			union _v_data_details{
				struct _ntc_1144		//VPort state change
				{
					ib_net16_t padding1;
					ib_net16_t lidaddr;	   // lid where change occurred
					uint8_t padding2;	   // reserved
					uint8_t local_changes;     // 7b reserved 1b local changes
					ib_net16_t cap_mask;	   // vport capability mask
					ib_net16_t change_flags;   // 10b reserved 6b change flags
					ib_net16_t padding3;
					ib_net16_t vport_index;
					uint8_t padding4[38];      // Padding - ignored on read
				} PACK_SUFFIX ntc_1144;
				struct _ntc_1146		//VPort state change
				{
					uint8_t padding1[2];	// Padding - ignored on read
					ib_net16_t lidaddr;
					uint8_t padding2[48];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1146;
				struct _ntc_1148		//Router data change
				{
					uint8_t padding1[52];
				} PACK_SUFFIX ntc_1148;
				struct _ntc_1257
				{
					ib_net16_t padding1;
					ib_net16_t lidaddr1;
					ib_net16_t lidaddr2;
					ib_net32_t p_key;	// 16msb set to 0
					ib_net32_t sl_qp1;	// 4b sl 4b reserved 24b qp1
					ib_net32_t qp2;		// 8b reserved 24b qp2
					ib_gid_t gidaddr1;
					ib_gid_t gidaddr2;
					ib_net16_t vport_index;
				} PACK_SUFFIX ntc_1257;
				struct _ntc_1258
				{
					ib_net16_t padding1;
					ib_net16_t lidaddr1;
					ib_net16_t lidaddr2;
					ib_net32_t q_key;
					ib_net32_t sl_qp1;	// 4b sl 4b reserved 24b qp1
					ib_net32_t qp2;		// 8b reserved 24b qp2
					ib_gid_t gidaddr1;
					ib_gid_t gidaddr2;
					ib_net16_t vport_index;
				} PACK_SUFFIX ntc_1258;
				struct _ntc_1300
				{
					ib_net16_t slid;	// 2
					ib_net16_t attr_id;	// 2
					uint8_t method;		// 1
					uint8_t reserved;	// 1
					ib_gid_t sgid;		// 16
					ib_net64_t physp_sguid;	// 8
					ib_net64_t sa_key;	// 8
					uint8_t padding[14];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1300;
				struct _ntc_1301
				{
					ib_net16_t slid;	// 2
					ib_net16_t attr_id;	// 2
					uint8_t method;		// 1
					uint8_t reserved;	// 1
					ib_gid_t spoofed_sgid;	// 16
					ib_net64_t physp_sguid;	// 8
					uint8_t padding[22];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1301;
				struct _ntc_1302
				{
					uint8_t reserved[6];
					ib_gid_t sgid;		// 16
					ib_net64_t physp_sguid;	// 8
					uint8_t padding[22];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1302;
				struct _ntc_1303_1305
				{
					uint8_t reserved[6];
					ib_gid_t sgid;			// 16
					ib_net64_t physp_sguid;		// 8
					ib_net64_t target_guid;		// 8
					ib_net64_t physp_target_guid;	// 8
					uint8_t padding[6];		// Padding - ignored on read
				} PACK_SUFFIX ntc_1303_1305;
				struct _ntc_1306_1307	// Not allowed SM GUID violation
				{
					ib_net16_t paddding1;            // 2
					ib_net16_t lid;                  // 2
					ib_net32_t padding2;             // 4
					ib_net64_t unauthorized_sm_guid; // 8
					uint8_t padding3[3];		 // 3
					uint8_t dr_trunc_hop;	// 1 (bit 8: dr_notice, bit 7: dr_path_truncated, bits 5-0: dr_hop_count)
					uint8_t dr_notice_init_path[24]; // 24
					uint8_t padding4[8];		 // Padding - ignored on read
				} PACK_SUFFIX ntc_1306_1307;
				struct _ntc_1308	// mkey violation on GetResp
				{
					ib_net16_t paddding1;		 // 2
					ib_net16_t lid;			 // 2
					ib_net16_t attr_id;		 // 2
					uint8_t padding2;		 // 1
					uint8_t method;			 // 1
					ib_net64_t physp_sguid;		 // 8
					uint8_t padding3[3];		 // 3
					uint8_t dr_trunc_hop;	// 1 (bit 8: dr_notice, bit 7: dr_path_truncated, bits 5-0: dr_hop_count)
					uint8_t dr_notice_init_path[24]; // 24
					ib_net64_t mkey; // 8
				} PACK_SUFFIX ntc_1308;
				struct _ntc_1309	// SM_key violation on GetResp
				{
					ib_net16_t paddding1;            // 2
					ib_net16_t lid;                  // 2
					ib_net32_t padding2;             // 4
					ib_net64_t physp_sguid;		 // 8
					uint8_t padding3[3];		 // 3
					uint8_t dr_trunc_hop;	// 1 (bit 8: dr_notice, bit 7: dr_path_truncated, bits 5-0: dr_hop_count)
					uint8_t dr_notice_init_path[24]; // 24
					ib_net64_t sm_key;		 // 8
				} PACK_SUFFIX ntc_1309;
				struct _ntc_vskey	// VS/N2N key violation
				{
					ib_net16_t slid;	// source LID from offending packet LRH
					uint8_t method;		// method, from common MAD header
					uint8_t paddding1;
					ib_net16_t attribute_id; // Attribute ID, from common MAD header
					ib_net32_t attribute_modifier; // Attribute Modif, from common MAD header
					ib_net32_t qp;		// 8b pad, 24b dest QP from BTH
					ib_net64_t key;		// key of the offending packet
					ib_gid_t source_gid;	// GID from GRH of the offending packet
					uint8_t padding2[12];	// Padding - ignored on read
				} PACK_SUFFIX ntc_vskey;
				struct _ntc_1310	// Duplicated Node GUID
				{
					uint8_t resv0[2];
					ib_net64_t node_guid;		// Duplicated Node GUID
					ib_net64_t peer_node_guid_1;	// Node GUID of peer node 1
					ib_net64_t peer_node_guid_2;	// Node GUID of peer node 2
					uint8_t peer_port_1;		// Port number in peer node 1
					uint8_t peer_port_2;		// Port number in peer node 2
					uint8_t padding[24];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1310;
				struct _ntc_1311	// Duplicated Port GUID
				{
					uint8_t resv0[2];
					ib_net64_t port_guid;	// Duplicated Port GUID
					ib_net64_t node_guid_1; // Node GUID of device 1
					ib_net64_t node_guid_2; // Node GUID of device 2
					uint8_t port_1;		// Port number in device 1
					uint8_t port_2;		// Port number in device 2
					ib_net16_t index_1;	// Virtual port index in device 1
					ib_net16_t index_2;	// Virtual port index in device 2
					uint8_t padding[20];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1311;
				struct _ntc_1312	// Node reboot
				{
					uint8_t resv0[2];
					ib_net64_t port_guid;	// Port GUID
					ib_net64_t node_guid;   // Node GUID of the device
					uint8_t resv1;
					uint8_t port;		// Port number in the device
					uint8_t padding[32];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1312;
				struct _ntc_1313	// Router LID Assignment Error
				{
					uint8_t syndrome;
					uint8_t comp_ratio;
					ib_net32_t global_router_lid_range_start;
					ib_net32_t global_router_lid_range_end;
					ib_net32_t local_router_lid_range_start;
					ib_net32_t local_router_lid_range_end;
					uint8_t padding[34];
				} PACK_SUFFIX ntc_1313;
				struct _ntc_1314	// Fast recovery trap
				{
					uint8_t resv0;
					uint8_t port;
					uint8_t trigger;
					uint8_t thr_type;
					ib_net16_t fields_select;
					ib_net16_t resv1;
					ib_net16_t counter_overflow;
					ib_net32_t num_errors;
					ib_net32_t num_warnings;
					ib_net32_t num_normals;
					ib_net32_t min_value;
					ib_net32_t max_value;
					ib_net32_t consecutive_normal;
					ib_net32_t last_value[3];
					uint8_t  padding[6];
				} PACK_SUFFIX ntc_1314;
				struct _ntc_1315		// Topoconfig parsing failed
				{
					uint8_t error_type;
					uint8_t padding[51];
				} PACK_SUFFIX ntc_1315;
				struct _ntc_1316		// Topoconfig link validity
				{
					ib_net64_t node_guid;
					ib_net64_t neighbor_node_guid;
					ib_net64_t expected_neighbor_node_guid;
					uint8_t port;
					uint8_t neighbor_port;
					uint8_t neighbor_node_type;
					uint8_t expected_neighbor_port;
					uint8_t expected_neighbor_node_type;
					uint8_t padding[23];
				} PACK_SUFFIX ntc_1316;
				struct _ntc_1317		// ContainAndDrainPortChangeTrap
				{
					ib_net16_t lid_addr;
					uint8_t resv[4];
					uint8_t block_id_mask[IB_CND_PORT_STATE_CHANGE_TRAP_ID_MASK_BYTES_LEN];
					uint8_t padding[14];
				} PACK_SUFFIX ntc_1317;
				struct _ntc_1318		// Unauthorized packets trap
				{
					ib_net16_t data_valid;
					ib_net16_t lid1;
					ib_net16_t lid2;
					ib_net16_t resv1;
					uint8_t resv2;
					uint8_t reason; /* 4b reserved, 4b reason */
					ib_net32_t qp1; /* 8b reserved, 24b qp1 */
					ib_net32_t hop_qp2; /* 8b hop_count, 24b qp2 */
					ib_net16_t dr_path[IB_TRAP_UNAUTHORIZED_PACKETS_MAX_PATH_LEN];
					uint8_t padding[2];
				} PACK_SUFFIX ntc_1318;
				struct _ntc_1319		// ISSU upgrade state
				{
					uint8_t reserved;
					uint8_t state;
					uint8_t padding[50];
				} PACK_SUFFIX ntc_1319;
				struct _ntc_1320		// PortRecovered trap
				{
					uint8_t block_id;
					uint8_t reserved;
					uint8_t recovered_bitmask[32];
					uint8_t padding[18];
				} PACK_SUFFIX ntc_1320;
				struct _ntc_1321	// Link flap
				{
					uint8_t resv0[2];
					ib_net64_t node_guid_1;
					ib_net64_t node_guid_2;
					ib_net16_t port_1;
					ib_net16_t port_2;
					uint8_t padding[30];	// Padding - ignored on read
				} PACK_SUFFIX ntc_1321;
				struct _ntc_1322	// Periodic update cycle finished
				{
					uint8_t resv[52];
				} PACK_SUFFIX ntc_1322;
				struct _ntc_1600	// key violation for UFM report
				{
					ib_net16_t slid;
					ib_net16_t attribute_id;
					uint8_t method;
					uint8_t mclass;
					ib_gid_t sgid;
					ib_net64_t physp_sguid;
					ib_net64_t key;
					uint8_t padding[14]; // Padding - ignored on read
				} PACK_SUFFIX ntc_1600;
				struct _ntc_1601        // tenant violations
				{
					uint64_t physical_group_id;
					ib_net64_t physical_guid;
					uint64_t virtual_group_id;
					ib_net64_t virtual_guid;
					uint16_t vport_index;
					uint8_t tenant_notice_type;
					uint8_t reserved[17];
				} PACK_SUFFIX ntc_1601;
				struct _ntc_1605        // routing engine action removal
				{
					uint64_t guid;
					uint8_t reserved[44];
				} PACK_SUFFIX ntc_1605;
				struct _ntc_1606        // routing engine action recovery
				{
					uint64_t guid;
					uint8_t reserved[44];
				} PACK_SUFFIX ntc_1606;
				struct _ntc_1609	// ISSU switch isolation
				{
					ib_net64_t guid;
					uint8_t state;
					uint8_t reserved[43];
				} PACK_SUFFIX ntc_1609;
				struct _ntc_1610	// ISSU switch restoration
				{
					ib_net64_t guid;
					uint8_t state;
					uint8_t reserved[43];
				} PACK_SUFFIX ntc_1610;
			} PACK_SUFFIX v_data_details;
			ib_net16_t trap_num;
		} PACK_SUFFIX vendor_data_details;
	} data_details;
	ib_gid_t issuer_gid;	// 16          80
} PACK_SUFFIX ib_mad_notice_attr_t;
#include <complib/cl_packoff.h>

/**
 * SA Security Traps
 */
#define SA_VS_TRAP_NUM_KEY_VIOLATION		(CL_HTON16(1300))
#define SA_VS_TRAP_NUM_SGID_SPOOFED		(CL_HTON16(1301))
#define SA_VS_TRAP_NUM_RATE_LIMIT_EXCEEDED	(CL_HTON16(1302))
#define SA_VS_TRAP_NUM_MC_EXCEEDED		(CL_HTON16(1303))
#define SA_VS_TRAP_NUM_SERVICES_EXCEEDED 	(CL_HTON16(1304))
#define SA_VS_TRAP_NUM_EVENT_SUBS_EXCEEDED	(CL_HTON16(1305))
#define SA_VS_TRAP_NUM_MIN			SA_VS_TRAP_NUM_KEY_VIOLATION
#define SA_VS_TRAP_NUM_MAX			SA_VS_TRAP_NUM_EVENT_SUBS_EXCEEDED

#define SM_VS_TRAP_NUM_UNALLOWED_SM_GUID_FOUND			(CL_HTON16(1306))
#define SM_VS_TRAP_NUM_SMINFO_SET_FROM_UNALLOWED_SM_GUID	(CL_HTON16(1307))
#define SM_VS_TRAP_NUM_SM_KEY_VIOLATION				(CL_HTON16(1309))
#define SM_VS_TRAP_NUM_DUPLICATED_NODE_GUID			(CL_HTON16(1310))
#define SM_VS_TRAP_NUM_DUPLICATED_PORT_GUID			(CL_HTON16(1311))
#define SM_VS_TRAP_NUM_NODE_REBOOT				(CL_HTON16(1312))
#define SM_VS_TRAP_NUM_ROUTER_LID_ASSIGNMENT_ERROR		(CL_HTON16(1313))
#define SM_VS_TRAP_NUM_TOPOCONFIG_PARSING_ERROR			(CL_HTON16(1315))
#define SM_VS_TRAP_NUM_TOPOCONFIG_LINK_ERROR			(CL_HTON16(1316))
#define SM_VS_TRAP_NUM_CND_PORT_STATE_CHANGE_TRAP		(CL_HTON16(1317))
#define SM_VS_TRAP_NUM_UNAUTHORIZED_PACKETS_TRAP		(CL_HTON16(1318))
#define SM_VS_TRAP_NUM_LINK_FLAP				(CL_HTON16(1321))
#define SM_VS_TRAP_NUM_PERIODIC_KEY_UPDATE_CYCLE_FINISH		(CL_HTON16(1322))

typedef enum osm_vs_trap_topoconfig_parsing_error_syndrom {
	SM_VS_TRAP_TOPOCONFIG_FAILED_TO_OPEN_FILE,
	SM_VS_TRAP_TOPOCONFIG_BAD_SYNTAX,
	SM_VS_TRAP_TOPOCONFIG_VALIDATION_FAILED,
} osm_vs_trap_topoconfig_parsing_error_syndrom_t;

typedef enum osm_vs_trap_lid_router_error_syndrom {
	SM_VS_TRAP_RTR_LID_SUBNETS_INCONSISTENT_FLID_RANGE = 2,
	SM_VS_TRAP_RTR_LID_INVALID_GLOBAL_FLID_RANGE,
	SM_VS_TRAP_RTR_LID_INVALID_LOCAL_FLID_RANGE,
	SM_VS_TRAP_RTR_LID_NUMBER_LIDS_EXCEEDED,
} osm_vs_trap_lid_router_error_syndrom_t;

/* CC and VS key violation event reported to plugin by trap */
#define CC_VS_TRAP_NUM_EVENT_KEY_VIOLATION	(CL_HTON16(1600))

/* TENANT access violation */
#define TENANT_ACCESS_VIOLATION			(CL_HTON16(1601))

/* Routing action */
#define ROUTING_ENGINE_ACTION_REMOVAL		(CL_HTON16(1605))
#define ROUTING_ENGINE_ACTION_RECOVERY		(CL_HTON16(1606))

/* ISSU upgrade actions */
#define ISSU_ISOLATION				(CL_HTON16(1609))
#define ISSU_RECOVERY				(CL_HTON16(1610))

/**
 * Traps 257-259 masks
 */
#define TRAP_259_MASK_SL (CL_HTON32(0xF0000000))
#define TRAP_259_MASK_QP (CL_HTON32(0x00FFFFFF))
#define TRAP_259_QP_MASK			0x00FFFFFF
#define TRAP_259_SL_SHIFT			28

/**
 * Trap 144 masks
 */
#define TRAP_144_MASK_OTHER_LOCAL_CHANGES      0x01
#define TRAP_144_MASK_LINK_SPEED_EXT_ENABLE_CHG (CL_HTON16(0x0020))
#define TRAP_144_MASK_HIERARCHY_INFO_CHANGE    (CL_HTON16(0x0010))
#define TRAP_144_MASK_SM_PRIORITY_CHANGE       (CL_HTON16(0x0008))
#define TRAP_144_MASK_LINK_SPEED_ENABLE_CHANGE (CL_HTON16(0x0004))
#define TRAP_144_MASK_LINK_WIDTH_ENABLE_CHANGE (CL_HTON16(0x0002))
#define TRAP_144_MASK_NODE_DESCRIPTION_CHANGE  (CL_HTON16(0x0001))

/**
 * Trap 1144 masks
 */
#define TRAP_1144_MASK_OTHER_LOCAL_CHANGES       0x01
#define TRAP_1144_MASK_HIERARCHY_INFO_CHANGE     (CL_HTON16(0x0010))
#define TRAP_1144_MASK_SM_PRIORITY_CHANGE        (CL_HTON16(0x0008))
#define TRAP_1144_MASK_VNODE_DESCRIPTION_CHANGE  (CL_HTON16(0x0001))

/*
 * Traps 1257-1258 masks
 */
#define TRAP_1257_MASK_QP (CL_HTON32(0x00FFFFFF))

/*
 * Trap 1318 masks
 */
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_LID1_BIT	CL_HTON16(0x0001)
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_LID2_BIT	CL_HTON16(0x0002)
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_REASON_BIT	CL_HTON16(0x0004)
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_QP1_BIT		CL_HTON16(0x0010)
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_QP2_BIT		CL_HTON16(0x0020)
#define IB_TRAP_UNAUTHORIZED_PACKETS_DATA_VALID_DR_PATH_BIT	CL_HTON16(0x0040)

/*
 * Trap 1318 getters
 */
static inline uint8_t
ib_trap_unauthorized_packets_get_reason(const ib_mad_notice_attr_t * p_ntci)
{
	return IBTYPES_GET_ATTR8(
		p_ntci->data_details.vendor_data_details.v_data_details.ntc_1318.reason, 0x0F, 0);
}

static inline uint32_t
ib_trap_unauthorized_packets_get_qp1(const ib_mad_notice_attr_t * p_ntci)
{
	return IBTYPES_GET_ATTR32(
		p_ntci->data_details.vendor_data_details.v_data_details.ntc_1318.qp1,
		0x00FFFFFF, 0);
}

static inline uint32_t
ib_trap_unauthorized_packets_get_qp2(const ib_mad_notice_attr_t * p_ntci)
{
	return IBTYPES_GET_ATTR32(
		p_ntci->data_details.vendor_data_details.v_data_details.ntc_1318.hop_qp2,
		0x00FFFFFF, 0);
}

static inline uint8_t
ib_trap_unauthorized_packets_get_hop_count(const ib_mad_notice_attr_t * p_ntci)
{
	return IBTYPES_GET_ATTR32(
		p_ntci->data_details.vendor_data_details.v_data_details.ntc_1318.hop_qp2,
		0xFF000000, 24);
}

/*
 * Trap 1320 masks
 */
#define IB_TRAP_PORT_RECOVERED_NUM_PORTS_IN_BLOCK	(256)

/****f* IBA Base: Types/ib_notice_is_generic
* NAME
*	ib_notice_is_generic
*
* DESCRIPTION
*	Check if the notice is generic
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_notice_is_generic(IN const ib_mad_notice_attr_t * p_ntc)
{
	return (p_ntc->generic_type & 0x80);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
* RETURN VALUES
*	TRUE if notice MAD is generic
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_get_type
* NAME
*	ib_notice_get_type
*
* DESCRIPTION
*	Get the notice type
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_notice_get_type(IN const ib_mad_notice_attr_t * p_ntc)
{
	return p_ntc->generic_type & 0x7f;
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to  the notice MAD attribute
*
* RETURN VALUES
*	TRUE if mad is generic
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_get_prod_type
* NAME
*	ib_notice_get_prod_type
*
* DESCRIPTION
*	Get the notice Producer Type of Generic Notice
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_notice_get_prod_type(IN const ib_mad_notice_attr_t * p_ntc)
{
	uint32_t pt;

	pt = cl_ntoh16(p_ntc->g_or_v.generic.prod_type_lsb) |
	    (p_ntc->g_or_v.generic.prod_type_msb << 16);
	return cl_hton32(pt);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
* RETURN VALUES
*	The producer type
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_set_prod_type
* NAME
*	ib_notice_set_prod_type
*
* DESCRIPTION
*	Set the notice Producer Type of Generic Notice
*
* SYNOPSIS
*/
static inline void OSM_API
ib_notice_set_prod_type(IN ib_mad_notice_attr_t * p_ntc,
			IN ib_net32_t prod_type_val)
{
	uint32_t ptv = cl_ntoh32(prod_type_val);
	p_ntc->g_or_v.generic.prod_type_lsb =
	    cl_hton16((uint16_t) (ptv & 0x0000ffff));
	p_ntc->g_or_v.generic.prod_type_msb =
	    (uint8_t) ((ptv & 0x00ff0000) >> 16);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
*  prod_type
*     [in] The producer Type code
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_set_prod_type_ho
* NAME
*	ib_notice_set_prod_type_ho
*
* DESCRIPTION
*	Set the notice Producer Type of Generic Notice given Host Order
*
* SYNOPSIS
*/
static inline void OSM_API
ib_notice_set_prod_type_ho(IN ib_mad_notice_attr_t * p_ntc,
			   IN uint32_t prod_type_val_ho)
{
	p_ntc->g_or_v.generic.prod_type_lsb =
	    cl_hton16((uint16_t) (prod_type_val_ho & 0x0000ffff));
	p_ntc->g_or_v.generic.prod_type_msb =
	    (uint8_t) ((prod_type_val_ho & 0x00ff0000) >> 16);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
*	prod_type
*		[in] The producer Type code in host order
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_get_vend_id
* NAME
*	ib_notice_get_vend_id
*
* DESCRIPTION
*	Get the Vendor Id of Vendor type Notice
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_notice_get_vend_id(IN const ib_mad_notice_attr_t * p_ntc)
{
	uint32_t vi;

	vi = cl_ntoh16(p_ntc->g_or_v.vend.vend_id_lsb) |
	    (p_ntc->g_or_v.vend.vend_id_msb << 16);
	return cl_hton32(vi);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
* RETURN VALUES
*	The Vendor Id of Vendor type Notice
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_set_vend_id
* NAME
*	ib_notice_set_vend_id
*
* DESCRIPTION
*	Set the notice Producer Type of Generic Notice
*
* SYNOPSIS
*/
static inline void OSM_API
ib_notice_set_vend_id(IN ib_mad_notice_attr_t * p_ntc, IN ib_net32_t vend_id)
{
	uint32_t vi = cl_ntoh32(vend_id);
	p_ntc->g_or_v.vend.vend_id_lsb =
	    cl_hton16((uint16_t) (vi & 0x0000ffff));
	p_ntc->g_or_v.vend.vend_id_msb = (uint8_t) ((vi & 0x00ff0000) >> 16);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
*	vend_id
*		[in] The producer Type code
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

/****f* IBA Base: Types/ib_notice_set_vend_id_ho
* NAME
*	ib_notice_set_vend_id_ho
*
* DESCRIPTION
*	Set the notice Producer Type of Generic Notice given a host order value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_notice_set_vend_id_ho(IN ib_mad_notice_attr_t * p_ntc,
			 IN uint32_t vend_id_ho)
{
	p_ntc->g_or_v.vend.vend_id_lsb =
	    cl_hton16((uint16_t) (vend_id_ho & 0x0000ffff));
	p_ntc->g_or_v.vend.vend_id_msb =
	    (uint8_t) ((vend_id_ho & 0x00ff0000) >> 16);
}

/*
* PARAMETERS
*	p_ntc
*		[in] Pointer to the notice MAD attribute
*
*	vend_id_ho
*		[in] The producer Type code in host order
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_mad_notice_attr_t
*********/

#include <complib/cl_packon.h>
typedef struct _ib_inform_info {
	ib_gid_t gid;
	ib_net16_t lid_range_begin;
	ib_net16_t lid_range_end;
	ib_net16_t reserved1;
	uint8_t is_generic;
	uint8_t subscribe;
	ib_net16_t trap_type;
	union _inform_g_or_v {
		struct _inform_generic {
			ib_net16_t trap_num;
			ib_net32_t qpn_resp_time_val;
			uint8_t reserved2;
			uint8_t node_type_msb;
			ib_net16_t node_type_lsb;
		} PACK_SUFFIX generic;
		struct _inform_vend {
			ib_net16_t dev_id;
			ib_net32_t qpn_resp_time_val;
			uint8_t reserved2;
			uint8_t vendor_id_msb;
			ib_net16_t vendor_id_lsb;
		} PACK_SUFFIX vend;
	} PACK_SUFFIX g_or_v;
} PACK_SUFFIX ib_inform_info_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_inform_info_get_qpn_resp_time
* NAME
*	ib_inform_info_get_qpn_resp_time
*
* DESCRIPTION
*	Get QPN of the inform info
*
* SYNOPSIS
*/
static inline void OSM_API
ib_inform_info_get_qpn_resp_time(IN const ib_net32_t qpn_resp_time_val,
				 OUT ib_net32_t * const p_qpn,
				 OUT uint8_t * const p_resp_time_val)
{
	uint32_t tmp = cl_ntoh32(qpn_resp_time_val);

	if (p_qpn)
		*p_qpn = cl_hton32((tmp & 0xffffff00) >> 8);
	if (p_resp_time_val)
		*p_resp_time_val = (uint8_t) (tmp & 0x0000001f);
}

/*
* PARAMETERS
*	qpn_resp_time_val
*		[in] the  qpn and resp time val from the mad
*
* RETURN VALUES
*	p_qpn
*		[out] pointer to the qpn
*
*	p_state
*		[out] pointer to the resp time val
*
* NOTES
*
* SEE ALSO
*	ib_inform_info_t
*********/

/****f* IBA Base: Types/ib_inform_info_set_qpn
* NAME
*	ib_inform_info_set_qpn
*
* DESCRIPTION
*	Set the QPN of the inform info
*
* SYNOPSIS
*/
static inline void OSM_API
ib_inform_info_set_qpn(IN ib_inform_info_t * p_ii, IN ib_net32_t const qpn)
{
	uint32_t tmp = cl_ntoh32(p_ii->g_or_v.generic.qpn_resp_time_val);
	uint32_t qpn_h = cl_ntoh32(qpn);

	p_ii->g_or_v.generic.qpn_resp_time_val =
	    cl_hton32((tmp & 0x000000ff) | ((qpn_h << 8) & 0xffffff00)
	    );
}

/*
* PARAMETERS
*
* NOTES
*
* SEE ALSO
*	ib_inform_info_t
*********/

/****f* IBA Base: Types/ib_inform_info_get_prod_type
* NAME
*	ib_inform_info_get_prod_type
*
* DESCRIPTION
*	Get Producer Type of the Inform Info
*	13.4.8.3 InformInfo
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_inform_info_get_prod_type(IN const ib_inform_info_t * p_inf)
{
	uint32_t nt;

	nt = cl_ntoh16(p_inf->g_or_v.generic.node_type_lsb) |
	    (p_inf->g_or_v.generic.node_type_msb << 16);
	return cl_hton32(nt);
}

/*
* PARAMETERS
*	p_inf
*		[in] pointer to an inform info
*
* RETURN VALUES
*     The producer type
*
* NOTES
*
* SEE ALSO
*	ib_inform_info_t
*********/

/****f* IBA Base: Types/ib_inform_info_get_vend_id
* NAME
*	ib_inform_info_get_vend_id
*
* DESCRIPTION
*	Get Node Type of the Inform Info
*
* SYNOPSIS
*/
static inline ib_net32_t OSM_API
ib_inform_info_get_vend_id(IN const ib_inform_info_t * p_inf)
{
	uint32_t vi;

	vi = cl_ntoh16(p_inf->g_or_v.vend.vendor_id_lsb) |
	    (p_inf->g_or_v.vend.vendor_id_msb << 16);
	return cl_hton32(vi);
}

/*
* PARAMETERS
*	p_inf
*		[in] pointer to an inform info
*
* RETURN VALUES
*     The node type
*
* NOTES
*
* SEE ALSO
*	ib_inform_info_t
*********/

/****s* IBA Base: Types/ib_inform_info_record_t
* NAME
*	ib_inform_info_record_t
*
* DESCRIPTION
*	IBA defined InformInfo Record. (15.2.5.12)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_inform_info_record {
	ib_gid_t subscriber_gid;
	ib_net16_t subscriber_enum;
	uint8_t reserved[6];
	ib_inform_info_t inform_info;
	uint8_t pad[4];
} PACK_SUFFIX ib_inform_info_record_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_perfmgt_mad_t
* NAME
*	ib_perfmgt_mad_t
*
* DESCRIPTION
*	IBA defined Perf Management MAD (16.3.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_perfmgt_mad {
	ib_mad_t header;
	uint8_t resv[40];
#define	IB_PM_DATA_SIZE		192
	uint8_t data[IB_PM_DATA_SIZE];
} PACK_SUFFIX ib_perfmgt_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	header
*		Common MAD header.
*
*	resv
*		Reserved.
*
*	data
*		Performance Management payload.  The structure and content of this field
*		depends upon the method, attr_id, and attr_mod fields in the header.
*
* SEE ALSO
* ib_mad_t
*********/

/****s* IBA Base: Types/ib_port_counters
* NAME
*	ib_port_counters_t
*
* DESCRIPTION
*	IBA defined PortCounters Attribute. (16.1.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_counters {
	uint8_t reserved;
	uint8_t port_select;
	ib_net16_t counter_select;
	ib_net16_t symbol_err_cnt;
	uint8_t link_err_recover;
	uint8_t link_downed;
	ib_net16_t rcv_err;
	ib_net16_t rcv_rem_phys_err;
	ib_net16_t rcv_switch_relay_err;
	ib_net16_t xmit_discards;
	uint8_t xmit_constraint_err;
	uint8_t rcv_constraint_err;
	uint8_t counter_select2;
	uint8_t link_int_buffer_overrun;
	ib_net16_t qp1_dropped;
	ib_net16_t vl15_dropped;
	ib_net32_t xmit_data;
	ib_net32_t rcv_data;
	ib_net32_t xmit_pkts;
	ib_net32_t rcv_pkts;
	ib_net32_t xmit_wait;
} PACK_SUFFIX ib_port_counters_t;
#include <complib/cl_packoff.h>

#define PC_LINK_INT(integ_buf_over) ((integ_buf_over & 0xF0) >> 4)
#define PC_BUF_OVERRUN(integ_buf_over) (integ_buf_over & 0x0F)

/****s* IBA Base: Types/ib_port_counters_ext
* NAME
*	ib_port_counters_ext_t
*
* DESCRIPTION
*	IBA defined PortCounters Extended Attribute. (16.1.4.11)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_counters_ext {
	uint8_t reserved;
	uint8_t port_select;
	ib_net16_t counter_select;
	ib_net32_t counter_select2;
	ib_net64_t xmit_data;
	ib_net64_t rcv_data;
	ib_net64_t xmit_pkts;
	ib_net64_t rcv_pkts;
	ib_net64_t unicast_xmit_pkts;
	ib_net64_t unicast_rcv_pkts;
	ib_net64_t multicast_xmit_pkts;
	ib_net64_t multicast_rcv_pkts;
	ib_net64_t symbol_err_cnt;
	ib_net64_t link_err_recover;
	ib_net64_t link_downed;
	ib_net64_t rcv_err;
	ib_net64_t rcv_rem_phys_err;
	ib_net64_t rcv_switch_relay_err;
	ib_net64_t xmit_discards;
	ib_net64_t xmit_constraint_err;
	ib_net64_t rcv_constraint_err;
	ib_net64_t link_integrity_err;
	ib_net64_t buffer_overrun;
	ib_net64_t vl15_dropped;
	ib_net64_t xmit_wait;
	ib_net64_t qp1_dropped;
} PACK_SUFFIX ib_port_counters_ext_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_port_samples_control
* NAME
*	ib_port_samples_control_t
*
* DESCRIPTION
*	IBA defined PortSamplesControl Attribute. (16.1.3.2)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_samples_control {
	uint8_t op_code;
	uint8_t port_select;
	uint8_t tick;
	uint8_t counter_width;	/* 5 bits res : 3bits counter_width */
	ib_net32_t counter_mask;	/* 2 bits res : 3 bits counter_mask : 27 bits counter_masks_1to9 */
	ib_net16_t counter_mask_10to14;	/* 1 bits res : 15 bits counter_masks_10to14 */
	uint8_t sample_mech;
	uint8_t sample_status;	/* 6 bits res : 2 bits sample_status */
	ib_net64_t option_mask;
	ib_net64_t vendor_mask;
	ib_net32_t sample_start;
	ib_net32_t sample_interval;
	ib_net16_t tag;
	ib_net16_t counter_select0;
	ib_net16_t counter_select1;
	ib_net16_t counter_select2;
	ib_net16_t counter_select3;
	ib_net16_t counter_select4;
	ib_net16_t counter_select5;
	ib_net16_t counter_select6;
	ib_net16_t counter_select7;
	ib_net16_t counter_select8;
	ib_net16_t counter_select9;
	ib_net16_t counter_select10;
	ib_net16_t counter_select11;
	ib_net16_t counter_select12;
	ib_net16_t counter_select13;
	ib_net16_t counter_select14;
} PACK_SUFFIX ib_port_samples_control_t;
#include <complib/cl_packoff.h>

/****d* IBA Base: Types/CounterSelect values
* NAME
*       Counter select values
*
* DESCRIPTION
*	Mandatory counter select values (16.1.3.3)
*
* SYNOPSIS
*/
#define IB_CS_PORT_XMIT_DATA (CL_HTON16(0x0001))
#define IB_CS_PORT_RCV_DATA  (CL_HTON16(0x0002))
#define IB_CS_PORT_XMIT_PKTS (CL_HTON16(0x0003))
#define IB_CS_PORT_RCV_PKTS  (CL_HTON16(0x0004))
#define IB_CS_PORT_XMIT_WAIT (CL_HTON16(0x0005))

/****s* IBA Base: Types/ib_port_samples_result
* NAME
*	ib_port_samples_result_t
*
* DESCRIPTION
*	IBA defined PortSamplesControl Attribute. (16.1.3.2)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_samples_result {
	ib_net16_t tag;
	ib_net16_t sample_status;	/* 14 bits res : 2 bits sample_status */
	ib_net32_t counter0;
	ib_net32_t counter1;
	ib_net32_t counter2;
	ib_net32_t counter3;
	ib_net32_t counter4;
	ib_net32_t counter5;
	ib_net32_t counter6;
	ib_net32_t counter7;
	ib_net32_t counter8;
	ib_net32_t counter9;
	ib_net32_t counter10;
	ib_net32_t counter11;
	ib_net32_t counter12;
	ib_net32_t counter13;
	ib_net32_t counter14;
} PACK_SUFFIX ib_port_samples_result_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_port_xmit_data_sl
* NAME
*	ib_port_xmit_data_sl_t
*
* DESCRIPTION
*       IBA defined PortXmitDataSL Attribute. (A13.6.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_xmit_data_sl {
	uint8_t reserved;
	uint8_t port_select;
	ib_net16_t counter_select;
	ib_net32_t port_xmit_data_sl[16];
	uint8_t resv[124];
} PACK_SUFFIX ib_port_xmit_data_sl_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_port_rcv_data_sl
* NAME
*	ib_port_rcv_data_sl_t
*
* DESCRIPTION
*	IBA defined PortRcvDataSL Attribute. (A13.6.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_rcv_data_sl {
	uint8_t reserved;
	uint8_t port_select;
	ib_net16_t counter_select;
	ib_net32_t port_rcv_data_sl[16];
	uint8_t resv[124];
} PACK_SUFFIX ib_port_rcv_data_sl_t;
#include <complib/cl_packoff.h>

/****d* IBA Base: Types/DM_SVC_NAME
* NAME
*	DM_SVC_NAME
*
* DESCRIPTION
*	IBA defined Device Management service name (16.3)
*
* SYNOPSIS
*/
#define	DM_SVC_NAME				"DeviceManager.IBTA"
/*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_dm_mad_t
* NAME
*	ib_dm_mad_t
*
* DESCRIPTION
*	IBA defined Device Management MAD (16.3.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_dm_mad {
	ib_mad_t header;
	uint8_t resv[40];
#define	IB_DM_DATA_SIZE		192
	uint8_t data[IB_DM_DATA_SIZE];
} PACK_SUFFIX ib_dm_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	header
*		Common MAD header.
*
*	resv
*		Reserved.
*
*	data
*		Device Management payload.  The structure and content of this field
*		depend upon the method, attr_id, and attr_mod fields in the header.
*
* SEE ALSO
* ib_mad_t
*********/

/****s* IBA Base: Types/ib_iou_info_t
* NAME
*	ib_iou_info_t
*
* DESCRIPTION
*	IBA defined IO Unit information structure (16.3.3.3)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_iou_info {
	ib_net16_t change_id;
	uint8_t max_controllers;
	uint8_t diag_rom;
#define	IB_DM_CTRL_LIST_SIZE	128
	uint8_t controller_list[IB_DM_CTRL_LIST_SIZE];
#define	IOC_NOT_INSTALLED		0x0
#define	IOC_INSTALLED			0x1
//              Reserved values                         0x02-0xE
#define	SLOT_DOES_NOT_EXIST		0xF
} PACK_SUFFIX ib_iou_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	change_id
*		Value incremented, with rollover, by any change to the controller_list.
*
*	max_controllers
*		Number of slots in controller_list.
*
*	diag_rom
*		A byte containing two fields: DiagDeviceID and OptionROM.
*		These fields may be read using the ib_iou_info_diag_dev_id
*		and ib_iou_info_option_rom functions.
*
*	controller_list
*		A series of 4-bit nibbles, with each nibble representing a slot
*		in the IO Unit.  Individual nibbles may be read using the
*		ioc_at_slot function.
*
* SEE ALSO
* ib_dm_mad_t, ib_iou_info_diag_dev_id, ib_iou_info_option_rom, ioc_at_slot
*********/

/****f* IBA Base: Types/ib_iou_info_diag_dev_id
* NAME
*	ib_iou_info_diag_dev_id
*
* DESCRIPTION
*	Returns the DiagDeviceID.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_iou_info_diag_dev_id(IN const ib_iou_info_t * const p_iou_info)
{
	return ((uint8_t) (p_iou_info->diag_rom >> 6 & 1));
}

/*
* PARAMETERS
*	p_iou_info
*		[in] Pointer to the IO Unit information structure.
*
* RETURN VALUES
*	DiagDeviceID field of the IO Unit information.
*
* NOTES
*
* SEE ALSO
*	ib_iou_info_t
*********/

/****f* IBA Base: Types/ib_iou_info_option_rom
* NAME
*	ib_iou_info_option_rom
*
* DESCRIPTION
*	Returns the OptionROM.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_iou_info_option_rom(IN const ib_iou_info_t * const p_iou_info)
{
	return ((uint8_t) (p_iou_info->diag_rom >> 7));
}

/*
* PARAMETERS
*	p_iou_info
*		[in] Pointer to the IO Unit information structure.
*
* RETURN VALUES
*	OptionROM field of the IO Unit information.
*
* NOTES
*
* SEE ALSO
*	ib_iou_info_t
*********/

/****f* IBA Base: Types/ioc_at_slot
* NAME
*	ioc_at_slot
*
* DESCRIPTION
*	Returns the IOC value at the specified slot.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ioc_at_slot(IN const ib_iou_info_t * const p_iou_info, IN uint8_t slot)
{
	if (slot >= IB_DM_CTRL_LIST_SIZE)
		return SLOT_DOES_NOT_EXIST;
	else
		return (int8_t)
		    ((slot % 2) ?
		     ((p_iou_info->controller_list[slot / 2] & 0xf0) >> 4) :
		     (p_iou_info->controller_list[slot / 2] & 0x0f));
}

/*
* PARAMETERS
*	p_iou_info
*		[in] Pointer to the IO Unit information structure.
*
*	slot
*		[in] Pointer to the IO Unit information structure.
*
* RETURN VALUES
*	OptionROM field of the IO Unit information.
*
* NOTES
*
* SEE ALSO
*	ib_iou_info_t
*********/

/****s* IBA Base: Types/ib_ioc_profile_t
* NAME
*	ib_ioc_profile_t
*
* DESCRIPTION
*	IBA defined IO Controller profile structure (16.3.3.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ioc_profile {
	ib_net64_t ioc_guid;
	ib_net32_t vend_id;
	ib_net32_t dev_id;
	ib_net16_t dev_ver;
	ib_net16_t resv2;
	ib_net32_t subsys_vend_id;
	ib_net32_t subsys_id;
	ib_net16_t io_class;
	ib_net16_t io_subclass;
	ib_net16_t protocol;
	ib_net16_t protocol_ver;
	ib_net32_t resv3;
	ib_net16_t send_msg_depth;
	uint8_t resv4;
	uint8_t rdma_read_depth;
	ib_net32_t send_msg_size;
	ib_net32_t rdma_size;
	uint8_t ctrl_ops_cap;
#define	CTRL_OPS_CAP_ST		0x01
#define	CTRL_OPS_CAP_SF		0x02
#define	CTRL_OPS_CAP_RT		0x04
#define	CTRL_OPS_CAP_RF		0x08
#define	CTRL_OPS_CAP_WT		0x10
#define	CTRL_OPS_CAP_WF		0x20
#define	CTRL_OPS_CAP_AT		0x40
#define	CTRL_OPS_CAP_AF		0x80
	uint8_t resv5;
	uint8_t num_svc_entries;
#define	MAX_NUM_SVC_ENTRIES	0xff
	uint8_t resv6[9];
#define	CTRL_ID_STRING_LEN	64
	char id_string[CTRL_ID_STRING_LEN];
} PACK_SUFFIX ib_ioc_profile_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	ioc_guid
*		An EUI-64 GUID used to uniquely identify the IO controller.
*
*	vend_id
*		IO controller vendor ID, IEEE format.
*
*	dev_id
*		A number assigned by the vendor to identify the type of controller.
*
*	dev_ver
*		A number assigned by the vendor to identify the device version.
*
*	subsys_vend_id
*		ID of the vendor of the enclosure, if any, in which the IO controller
*		resides in IEEE format; otherwise zero.
*
*	subsys_id
*		A number identifying the subsystem where the controller resides.
*
*	io_class
*		0x0000 - 0xfffe = reserved for IO classes encompased by InfiniBand
*		Architecture.  0xffff = Vendor specific.
*
*	io_subclass
*		0x0000 - 0xfffe = reserved for IO subclasses encompased by InfiniBand
*		Architecture.  0xffff = Vendor specific.  This shall be set to 0xfff
*		if the io_class component is 0xffff.
*
*	protocol
*		0x0000 - 0xfffe = reserved for IO subclasses encompased by InfiniBand
*		Architecture.  0xffff = Vendor specific.  This shall be set to 0xfff
*		if the io_class component is 0xffff.
*
*	protocol_ver
*		Protocol specific.
*
*	send_msg_depth
*		Maximum depth of the send message queue.
*
*	rdma_read_depth
*		Maximum depth of the per-channel RDMA read queue.
*
*	send_msg_size
*		Maximum size of send messages.
*
*	ctrl_ops_cap
*		Supported operation types of this IO controller.  A bit set to one
*		for affirmation of supported capability.
*
*	num_svc_entries
*		Number of entries in the service entries table.
*
*	id_string
*		UTF-8 encoded string for identifying the controller to an operator.
*
* SEE ALSO
* ib_dm_mad_t
*********/

static inline uint32_t OSM_API
ib_ioc_profile_get_vend_id(IN const ib_ioc_profile_t * const p_ioc_profile)
{
	return (cl_ntoh32(p_ioc_profile->vend_id) >> 8);
}

static inline void OSM_API
ib_ioc_profile_set_vend_id(IN ib_ioc_profile_t * const p_ioc_profile,
			   IN const uint32_t vend_id)
{
	p_ioc_profile->vend_id = (cl_hton32(vend_id) << 8);
}

/****s* IBA Base: Types/ib_svc_entry_t
* NAME
*	ib_svc_entry_t
*
* DESCRIPTION
*	IBA defined IO Controller service entry structure (16.3.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_svc_entry {
#define	MAX_SVC_ENTRY_NAME_LEN		40
	char name[MAX_SVC_ENTRY_NAME_LEN];
	ib_net64_t id;
} PACK_SUFFIX ib_svc_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	name
*		UTF-8 encoded, null-terminated name of the service.
*
*	id
*		An identifier of the associated Service.
*
* SEE ALSO
* ib_svc_entries_t
*********/

/****s* IBA Base: Types/ib_svc_entries_t
* NAME
*	ib_svc_entries_t
*
* DESCRIPTION
*	IBA defined IO Controller service entry array (16.3.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_svc_entries {
#define	SVC_ENTRY_COUNT			4
	ib_svc_entry_t service_entry[SVC_ENTRY_COUNT];
} PACK_SUFFIX ib_svc_entries_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	service_entry
*		An array of IO controller service entries.
*
* SEE ALSO
* ib_dm_mad_t, ib_svc_entry_t
*********/

static inline void OSM_API
ib_dm_get_slot_lo_hi(IN const ib_net32_t slot_lo_hi,
		     OUT uint8_t * const p_slot,
		     OUT uint8_t * const p_lo, OUT uint8_t * const p_hi)
{
	ib_net32_t tmp_slot_lo_hi = CL_NTOH32(slot_lo_hi);

	if (p_slot)
		*p_slot = (uint8_t) ((tmp_slot_lo_hi >> 16) & 0x0f);
	if (p_hi)
		*p_hi = (uint8_t) ((tmp_slot_lo_hi >> 8) & 0xff);
	if (p_lo)
		*p_lo = (uint8_t) ((tmp_slot_lo_hi >> 0) & 0xff);
}

/*
 *	IBA defined information describing an I/O controller
 */
#include <complib/cl_packon.h>
typedef struct _ib_ioc_info {
	ib_net64_t module_guid;
	ib_net64_t iou_guid;
	ib_ioc_profile_t ioc_profile;
	ib_net64_t access_key;
	uint16_t initiators_conf;
	uint8_t resv[38];
} PACK_SUFFIX ib_ioc_info_t;
#include <complib/cl_packoff.h>

/*
 *	The following definitions are shared between the Access Layer and VPD
 */
typedef struct _ib_ca *__ptr64 ib_ca_handle_t;
typedef struct _ib_pd *__ptr64 ib_pd_handle_t;
typedef struct _ib_rdd *__ptr64 ib_rdd_handle_t;
typedef struct _ib_mr *__ptr64 ib_mr_handle_t;
typedef struct _ib_mw *__ptr64 ib_mw_handle_t;
typedef struct _ib_qp *__ptr64 ib_qp_handle_t;
typedef struct _ib_eec *__ptr64 ib_eec_handle_t;
typedef struct _ib_cq *__ptr64 ib_cq_handle_t;
typedef struct _ib_av *__ptr64 ib_av_handle_t;
typedef struct _ib_mcast *__ptr64 ib_mcast_handle_t;

/* Currently for windows branch, use the extended version of ib special verbs struct
	in order to be compliant with Infinicon ib_types; later we'll change it to support
	OpenSM ib_types.h */

#ifndef __WIN__
/****d* Access Layer/ib_api_status_t
* NAME
*	ib_api_status_t
*
* DESCRIPTION
*	Function return codes indicating the success or failure of an API call.
*	Note that success is indicated by the return value IB_SUCCESS, which
*	is always zero.
*
* NOTES
*	IB_VERBS_PROCESSING_DONE is used by UVP library to terminate a verbs call
*	in the pre-ioctl step itself.
*
* SYNOPSIS
*/
typedef enum _ib_api_status_t {
	IB_SUCCESS,
	IB_INSUFFICIENT_RESOURCES,
	IB_INSUFFICIENT_MEMORY,
	IB_INVALID_PARAMETER,
	IB_INVALID_SETTING,
	IB_NOT_FOUND,
	IB_TIMEOUT,
	IB_CANCELED,
	IB_INTERRUPTED,
	IB_INVALID_PERMISSION,
	IB_UNSUPPORTED,
	IB_OVERFLOW,
	IB_MAX_MCAST_QPS_REACHED,
	IB_INVALID_QP_STATE,
	IB_INVALID_EEC_STATE,
	IB_INVALID_APM_STATE,
	IB_INVALID_PORT_STATE,
	IB_INVALID_STATE,
	IB_RESOURCE_BUSY,
	IB_INVALID_PKEY,
	IB_INVALID_LKEY,
	IB_INVALID_RKEY,
	IB_INVALID_MAX_WRS,
	IB_INVALID_MAX_SGE,
	IB_INVALID_CQ_SIZE,
	IB_INVALID_SERVICE_TYPE,
	IB_INVALID_GID,
	IB_INVALID_LID,
	IB_INVALID_GUID,
	IB_INVALID_CA_HANDLE,
	IB_INVALID_AV_HANDLE,
	IB_INVALID_CQ_HANDLE,
	IB_INVALID_EEC_HANDLE,
	IB_INVALID_QP_HANDLE,
	IB_INVALID_PD_HANDLE,
	IB_INVALID_MR_HANDLE,
	IB_INVALID_MW_HANDLE,
	IB_INVALID_RDD_HANDLE,
	IB_INVALID_MCAST_HANDLE,
	IB_INVALID_CALLBACK,
	IB_INVALID_AL_HANDLE,	/* InfiniBand Access Layer */
	IB_INVALID_HANDLE,	/* InfiniBand Access Layer */
	IB_ERROR,		/* InfiniBand Access Layer */
	IB_REMOTE_ERROR,	/* Infiniband Access Layer */
	IB_VERBS_PROCESSING_DONE,	/* See Notes above         */
	IB_INVALID_WR_TYPE,
	IB_QP_IN_TIMEWAIT,
	IB_EE_IN_TIMEWAIT,
	IB_INVALID_PORT,
	IB_NOT_DONE,
	IB_UNKNOWN_ERROR	/* ALWAYS LAST ENUM VALUE! */
} ib_api_status_t;
/*****/

OSM_EXPORT const char *ib_error_str[];

/****f* IBA Base: Types/ib_get_err_str
* NAME
*	ib_get_err_str
*
* DESCRIPTION
*	Returns a string for the specified status value.
*
* SYNOPSIS
*/
static inline const char *OSM_API ib_get_err_str(IN ib_api_status_t status)
{
	if (status > IB_UNKNOWN_ERROR)
		status = IB_UNKNOWN_ERROR;
	return (ib_error_str[status]);
}

/*
* PARAMETERS
*	status
*		[in] status value
*
* RETURN VALUES
*	Pointer to the status description string.
*
* NOTES
*
* SEE ALSO
*********/

/****d* Verbs/ib_async_event_t
* NAME
*	ib_async_event_t -- Async event types
*
* DESCRIPTION
*	This type indicates the reason the async callback was called.
*	The context in the ib_event_rec_t indicates the resource context
*	that associated with the callback.  For example, for IB_AE_CQ_ERROR
*	the context provided during the ib_create_cq is returned in the event.
*
* SYNOPSIS
*/
typedef enum _ib_async_event_t {
	IB_AE_SQ_ERROR = 1,
	IB_AE_SQ_DRAINED,
	IB_AE_RQ_ERROR,
	IB_AE_CQ_ERROR,
	IB_AE_QP_FATAL,
	IB_AE_QP_COMM,
	IB_AE_QP_APM,
	IB_AE_EEC_FATAL,
	IB_AE_EEC_COMM,
	IB_AE_EEC_APM,
	IB_AE_LOCAL_FATAL,
	IB_AE_PKEY_TRAP,
	IB_AE_QKEY_TRAP,
	IB_AE_MKEY_TRAP,
	IB_AE_PORT_TRAP,
	IB_AE_SYSIMG_GUID_TRAP,
	IB_AE_BUF_OVERRUN,
	IB_AE_LINK_INTEGRITY,
	IB_AE_FLOW_CTRL_ERROR,
	IB_AE_BKEY_TRAP,
	IB_AE_QP_APM_ERROR,
	IB_AE_EEC_APM_ERROR,
	IB_AE_WQ_REQ_ERROR,
	IB_AE_WQ_ACCESS_ERROR,
	IB_AE_PORT_ACTIVE,
	IB_AE_PORT_DOWN,
	IB_AE_UNKNOWN		/* ALWAYS LAST ENUM VALUE */
} ib_async_event_t;
/*
* VALUES
*	IB_AE_SQ_ERROR
*		An error occurred when accessing the send queue of the QP or EEC.
*		This event is optional.
*
*	IB_AE_SQ_DRAINED
*		The send queue of the specified QP has completed the outstanding
*		messages in progress when the state change was requested and, if
*		applicable, has received all acknowledgements for those messages.
*
*	IB_AE_RQ_ERROR
*		An error occurred when accessing the receive queue of the QP or EEC.
*		This event is optional.
*
*	IB_AE_CQ_ERROR
*		An error occurred when writing an entry to the CQ.
*
*	IB_AE_QP_FATAL
*		A catastrophic error occurred while accessing or processing the
*		work queue that prevents reporting of completions.
*
*	IB_AE_QP_COMM
*		The first packet has arrived for the receive work queue where the
*		QP is still in the RTR state.
*
*	IB_AE_QP_APM
*		If alternate path migration is supported, this event indicates that
*		the QP connection has migrated to the alternate path.
*
*	IB_AE_EEC_FATAL
*		If reliable datagram service is supported, this event indicates that
*		a catastrophic error occurred while accessing or processing the EEC
*		that prevents reporting of completions.
*
*	IB_AE_EEC_COMM
*		If reliable datagram service is supported, this event indicates that
*		the first packet has arrived for the receive work queue where the
*		EEC is still in the RTR state.
*
*	IB_AE_EEC_APM
*		If reliable datagram service and alternate path migration is supported,
*		this event indicates that the EEC connection has migrated to the
*		alternate path.
*
*	IB_AE_LOCAL_FATAL
*		A catastrophic HCA error occurred which cannot be attributed to
*		any resource; behavior is indeterminate.
*
*	IB_AE_PKEY_TRAP
*		A PKEY violation was detected.  This event is optional.
*
*	IB_AE_QKEY_TRAP
*		A QKEY violation was detected.  This event is optional.
*
*	IB_AE_MKEY_TRAP
*		A MKEY violation was detected.  This event is optional.
*
*	IB_AE_PORT_TRAP
*		A port capability change was detected.  This event is optional.
*
*	IB_AE_SYSIMG_GUID_TRAP
*		If the system image GUID is supported, this event indicates that
*		the system image GUID of this HCA has been changed.  This event
*		is optional.
*
*	IB_AE_BUF_OVERRUN
*		The number of consecutive flow control update periods with at least
*		one overrun error in each period has exceeded the threshold specified
*		in the port info attributes.  This event is optional.
*
*	IB_AE_LINK_INTEGRITY
*		The detection of excessively frequent local physical errors has
*		exceeded the threshold specified in the port info attributes.  This
*		event is optional.
*
*	IB_AE_FLOW_CTRL_ERROR
*		An HCA watchdog timer monitoring the arrival of flow control updates
*		has expired without receiving an update.  This event is optional.
*
*	IB_AE_BKEY_TRAP
*		An BKEY violation was detected.  This event is optional.
*
*	IB_AE_QP_APM_ERROR
*		If alternate path migration is supported, this event indicates that
*		an incoming path migration request to this QP was not accepted.
*
*	IB_AE_EEC_APM_ERROR
*		If reliable datagram service and alternate path migration is supported,
*		this event indicates that an incoming path migration request to this
*		EEC was not accepted.
*
*	IB_AE_WQ_REQ_ERROR
*		An OpCode violation was detected at the responder.
*
*	IB_AE_WQ_ACCESS_ERROR
*		An access violation was detected at the responder.
*
*	IB_AE_PORT_ACTIVE
*		If the port active event is supported, this event is generated
*		when the link becomes active: IB_LINK_ACTIVE.
*
*	IB_AE_PORT_DOWN
*		The link is declared unavailable: IB_LINK_INIT, IB_LINK_ARMED,
*		IB_LINK_DOWN.
*
*	IB_AE_UNKNOWN
*		An unknown error occurred which cannot be attributed to any
*		resource; behavior is indeterminate.
*
*****/

OSM_EXPORT const char *ib_async_event_str[];

/****f* IBA Base: Types/ib_get_async_event_str
* NAME
*	ib_get_async_event_str
*
* DESCRIPTION
*	Returns a string for the specified asynchronous event.
*
* SYNOPSIS
*/
static inline const char *OSM_API
ib_get_async_event_str(IN ib_async_event_t event)
{
	if (event > IB_AE_UNKNOWN)
		event = IB_AE_UNKNOWN;
	return (ib_async_event_str[event]);
}

/*
* PARAMETERS
*	event
*		[in] event value
*
* RETURN VALUES
*	Pointer to the asynchronous event description string.
*
* NOTES
*
* SEE ALSO
*********/

/****s* Verbs/ib_event_rec_t
* NAME
*	ib_event_rec_t -- Async event notification record
*
* DESCRIPTION
*	When an async event callback is made, this structure is passed to indicate
*	the type of event, the source of event that caused it, and the context
*	associated with this event.
*
*	context -- Context of the resource that caused the event.
*		-- ca_context if this is a port/adapter event.
*		-- qp_context if the source is a QP event
*		-- cq_context if the source is a CQ event.
*		-- ee_context if the source is an EE event.
*
* SYNOPSIS
*/
typedef struct _ib_event_rec {
	void *context;
	ib_async_event_t type;
	/* HCA vendor specific event information. */
	uint64_t vendor_specific;
	/* The following structures are valid only for trap types. */
	union _trap {
		struct {
			uint16_t lid;
			ib_net64_t port_guid;
			uint8_t port_num;
			/*
			 * The following structure is valid only for
			 * P_KEY, Q_KEY, and M_KEY violation traps.
			 */
			struct {
				uint8_t sl;
				uint16_t src_lid;
				uint16_t dest_lid;
				union _key {
					uint16_t pkey;
					uint32_t qkey;
					uint64_t mkey;
				} key;
				uint32_t src_qp;
				uint32_t dest_qp;
				ib_gid_t src_gid;
				ib_gid_t dest_gid;
			} violation;
		} info;
		ib_net64_t sysimg_guid;
	} trap;
} ib_event_rec_t;
/*******/

/****d* Access Layer/ib_atomic_t
* NAME
*	ib_atomic_t
*
* DESCRIPTION
*	Indicates atomicity levels supported by an adapter.
*
* SYNOPSIS
*/
typedef enum _ib_atomic_t {
	IB_ATOMIC_NONE,
	IB_ATOMIC_LOCAL,
	IB_ATOMIC_GLOBAL
} ib_atomic_t;
/*
* VALUES
*	IB_ATOMIC_NONE
*		Atomic operations not supported.
*
*	IB_ATOMIC_LOCAL
*		Atomic operations guaranteed between QPs of a single CA.
*
*	IB_ATOMIC_GLOBAL
*		Atomic operations are guaranteed between CA and any other entity
*		in the system.
*****/

/****s* Access Layer/ib_port_cap_t
* NAME
*	ib_port_cap_t
*
* DESCRIPTION
*	Indicates which management agents are currently available on the specified
*	port.
*
* SYNOPSIS
*/
typedef struct _ib_port_cap {
	boolean_t cm;
	boolean_t snmp;
	boolean_t dev_mgmt;
	boolean_t vend;
	boolean_t sm;
	boolean_t sm_disable;
	boolean_t qkey_ctr;
	boolean_t pkey_ctr;
	boolean_t notice;
	boolean_t trap;
	boolean_t apm;
	boolean_t slmap;
	boolean_t pkey_nvram;
	boolean_t mkey_nvram;
	boolean_t sysguid;
	boolean_t dr_notice;
	boolean_t boot_mgmt;
	boolean_t capm_notice;
	boolean_t reinit;
	boolean_t ledinfo;
	boolean_t port_active;
} ib_port_cap_t;
/*****/

/****d* Access Layer/ib_init_type_t
* NAME
*	ib_init_type_t
*
* DESCRIPTION
*	If supported by the HCA, the type of initialization requested by
*	this port before SM moves it to the active or armed state.  If the
*	SM implements reinitialization, it shall set these bits to indicate
*	the type of initialization performed prior to activating the port.
*	Otherwise, these bits shall be set to 0.
*
* SYNOPSIS
*/
typedef uint8_t ib_init_type_t;
#define IB_INIT_TYPE_NO_LOAD			0x01
#define IB_INIT_TYPE_PRESERVE_CONTENT		0x02
#define IB_INIT_TYPE_PRESERVE_PRESENCE		0x04
#define IB_INIT_TYPE_DO_NOT_RESUSCITATE		0x08
/*****/

/****s* Access Layer/ib_port_attr_mod_t
* NAME
*	ib_port_attr_mod_t
*
* DESCRIPTION
*	Port attributes that may be modified.
*
* SYNOPSIS
*/
typedef struct _ib_port_attr_mod {
	ib_port_cap_t cap;
	uint16_t pkey_ctr;
	uint16_t qkey_ctr;
	ib_init_type_t init_type;
	ib_net64_t system_image_guid;
} ib_port_attr_mod_t;
/*
* SEE ALSO
*	ib_port_cap_t
*****/

/****s* Access Layer/ib_port_attr_t
* NAME
*	ib_port_attr_t
*
* DESCRIPTION
*	Information about a port on a given channel adapter.
*
* SYNOPSIS
*/
typedef struct _ib_port_attr {
	ib_net64_t port_guid;
	uint8_t port_num;
	uint8_t mtu;
	uint64_t max_msg_size;
	ib_net16_t lid;
	uint8_t lmc;
	/*
	 * LinkWidthSupported as defined in PortInfo.  Required to calculate
	 * inter-packet delay (a.k.a. static rate).
	 */
	uint8_t link_width_supported;
	uint16_t max_vls;
	ib_net16_t sm_lid;
	uint8_t sm_sl;
	uint8_t link_state;
	ib_init_type_t init_type_reply;	/* Optional */
	/*
	 * subnet_timeout:
	 * The maximum expected subnet propagation delay to reach any port on
	 * the subnet.  This value also determines the rate at which traps can
	 * be generated from this node.
	 *
	 * timeout = 4.096 microseconds * 2^subnet_timeout
	 */
	uint8_t subnet_timeout;
	ib_port_cap_t cap;
	uint16_t pkey_ctr;
	uint16_t qkey_ctr;
	uint16_t num_gids;
	uint16_t num_pkeys;
	/*
	 * Pointers at the end of the structure to allow doing a simple
	 * memory comparison of contents up to the first pointer.
	 */
	ib_gid_t *p_gid_table;
	ib_net16_t *p_pkey_table;
} ib_port_attr_t;
/*
* SEE ALSO
*	uint8_t, ib_port_cap_t, ib_link_states_t
*****/

/****s* Access Layer/ib_ca_attr_t
* NAME
*	ib_ca_attr_t
*
* DESCRIPTION
*	Information about a channel adapter.
*
* SYNOPSIS
*/
typedef struct _ib_ca_attr {
	ib_net64_t ca_guid;
	uint32_t vend_id;
	uint16_t dev_id;
	uint16_t revision;
	uint64_t fw_ver;
	/*
	 * Total size of the ca attributes in bytes
	 */
	uint32_t size;
	uint32_t max_qps;
	uint32_t max_wrs;
	uint32_t max_sges;
	uint32_t max_rd_sges;
	uint32_t max_cqs;
	uint32_t max_cqes;
	uint32_t max_pds;
	uint32_t init_regions;
	uint64_t init_region_size;
	uint32_t init_windows;
	uint32_t max_addr_handles;
	uint32_t max_partitions;
	ib_atomic_t atomicity;
	uint8_t max_qp_resp_res;
	uint8_t max_eec_resp_res;
	uint8_t max_resp_res;
	uint8_t max_qp_init_depth;
	uint8_t max_eec_init_depth;
	uint32_t max_eecs;
	uint32_t max_rdds;
	uint32_t max_ipv6_qps;
	uint32_t max_ether_qps;
	uint32_t max_mcast_grps;
	uint32_t max_mcast_qps;
	uint32_t max_qps_per_mcast_grp;
	uint32_t max_fmr;
	uint32_t max_map_per_fmr;
	/*
	 * local_ack_delay:
	 * Specifies the maximum time interval between the local CA receiving
	 * a message and the transmission of the associated ACK or NAK.
	 *
	 * timeout = 4.096 microseconds * 2^local_ack_delay
	 */
	uint8_t local_ack_delay;
	boolean_t bad_pkey_ctr_support;
	boolean_t bad_qkey_ctr_support;
	boolean_t raw_mcast_support;
	boolean_t apm_support;
	boolean_t av_port_check;
	boolean_t change_primary_port;
	boolean_t modify_wr_depth;
	boolean_t current_qp_state_support;
	boolean_t shutdown_port_capability;
	boolean_t init_type_support;
	boolean_t port_active_event_support;
	boolean_t system_image_guid_support;
	boolean_t hw_agents;
	ib_net64_t system_image_guid;
	uint32_t num_page_sizes;
	uint8_t num_ports;
	uint32_t *p_page_size;
	ib_port_attr_t *p_port_attr;
} ib_ca_attr_t;
/*
* FIELDS
*	ca_guid
*		GUID for this adapter.
*
*	vend_id
*		IEEE vendor ID for this adapter
*
*	dev_id
*		Device ID of this adapter. (typically from PCI device ID)
*
*	revision
*		Revision ID of this adapter
*
*	fw_ver
*		Device Firmware version.
*
*	size
*		Total size in bytes for the HCA attributes.  This size includes total
*		size required for all the variable members of the structure.  If a
*		vendor requires to pass vendor specific fields beyond this structure,
*		the HCA vendor can choose to report a larger size.  If a vendor is
*		reporting extended vendor specific features, they should also provide
*		appropriate access functions to aid with the required interpretation.
*
*	max_qps
*		Maximum number of QP's supported by this HCA.
*
*	max_wrs
*		Maximum number of work requests supported by this HCA.
*
*	max_sges
*		Maximum number of scatter gather elements supported per work request.
*
*	max_rd_sges
*		Maximum number of scatter gather elements supported for READ work
*		requests for a Reliable Datagram QP.  This value must be zero if RD
*		service is not supported.
*
*	max_cqs
*		Maximum number of Completion Queues supported.
*
*	max_cqes
*		Maximum number of CQ elements supported per CQ.
*
*	max_pds
*		Maximum number of protection domains supported.
*
*	init_regions
*		Initial number of memory regions supported.  These are only informative
*		values.  HCA vendors can extended and grow these limits on demand.
*
*	init_region_size
*		Initial limit on the size of the registered memory region.
*
*	init_windows
*		Initial number of window entries supported.
*
*	max_addr_handles
*		Maximum number of address handles supported.
*
*	max_partitions
*		Maximum number of partitions supported.
*
*	atomicity
*		Indicates level of atomic operations supported by this HCA.
*
*	max_qp_resp_res
*	max_eec_resp_res
*		Maximum limit on number of responder resources for incoming RDMA
*		operations, on QPs and EEC's respectively.
*
*	max_resp_res
*		Maximum number of responder resources per HCA, with this HCA used as
*		the target.
*
*	max_qp_init_depth
*	max_eec_init_depth
*		Maximum initiator depth per QP or EEC for initiating RDMA reads and
*		atomic operations.
*
*	max_eecs
*		Maximum number of EEC's supported by the HCA.
*
*	max_rdds
*		Maximum number of Reliable datagram domains supported.
*
*	max_ipv6_qps
*	max_ether_qps
*		Maximum number of IPV6 and raw ether QP's supported by this HCA.
*
*	max_mcast_grps
*		Maximum number of multicast groups supported.
*
*	max_mcast_qps
*		Maximum number of QP's that can support multicast operations.
*
*	max_qps_per_mcast_grp
*		Maximum number of multicast QP's per multicast group.
*
*	local_ack_delay
*		Specifies the maximum time interval between the local CA receiving
*		a message and the transmission of the associated ACK or NAK.
*		timeout = 4.096 microseconds * 2^local_ack_delay
*
*	bad_pkey_ctr_support
*	bad_qkey_ctr_support
*		Indicates support for the bad pkey and qkey counters.
*
*	raw_mcast_support
*		Indicates support for raw packet multicast.
*
*	apm_support
*		Indicates support for Automatic Path Migration.
*
*	av_port_check
*		Indicates ability to check port number in address handles.
*
*	change_primary_port
*		Indicates ability to change primary port for a QP or EEC during a
*		SQD->RTS transition.
*
*	modify_wr_depth
*		Indicates ability to modify QP depth during a modify QP operation.
*		Check the verb specification for permitted states.
*
*	current_qp_state_support
*		Indicates ability of the HCA to support the current QP state modifier
*		during a modify QP operation.
*
*	shutdown_port_capability
*		Shutdown port capability support indicator.
*
*	init_type_support
*		Indicates init_type_reply and ability to set init_type is supported.
*
*	port_active_event_support
*		Port active event support indicator.
*
*	system_image_guid_support
*		System image GUID support indicator.
*
*	hw_agents
*		Indicates SMA is implemented in HW.
*
*	system_image_guid
*		Optional system image GUID.  This field is valid only if the
*		system_image_guid_support flag is set.
*
*	num_page_sizes
*		Indicates support for different page sizes supported by the HCA.
*		The variable size array can be obtained from p_page_size.
*
*	num_ports
*		Number of physical ports supported on this HCA.
*
*	p_page_size
*		Array holding different page size supported.
*
*	p_port_attr
*		Array holding port attributes.
*
* NOTES
*	This structure contains the attributes of a channel adapter.  Users must
*	call ib_copy_ca_attr to copy the contents of this structure to a new
*	memory region.
*
* SEE ALSO
*	ib_port_attr_t, ib_atomic_t, ib_copy_ca_attr
*****/

/****f* Access layer/ib_copy_ca_attr
* NAME
*	ib_copy_ca_attr
*
* DESCRIPTION
*	Copies CA attributes.
*
* SYNOPSIS
*/
ib_ca_attr_t *ib_copy_ca_attr(IN ib_ca_attr_t * const p_dest,
			      IN const ib_ca_attr_t * const p_src);
/*
* PARAMETERS
*	p_dest
*		Pointer to the buffer that is the destination of the copy.
*
*	p_src
*		Pointer to the CA attributes to copy.
*
* RETURN VALUE
*	Pointer to the copied CA attributes.
*
* NOTES
*	The buffer pointed to by the p_dest parameter must be at least the size
*	specified in the size field of the buffer pointed to by p_src.
*
* SEE ALSO
*	ib_ca_attr_t, ib_dup_ca_attr, ib_free_ca_attr
*****/

/****s* Access Layer/ib_av_attr_t
* NAME
*	ib_av_attr_t
*
* DESCRIPTION
*	IBA address vector.
*
* SYNOPSIS
*/
typedef struct _ib_av_attr {
	uint8_t port_num;
	uint8_t sl;
	ib_net16_t dlid;
	boolean_t grh_valid;
	ib_grh_t grh;
	uint8_t static_rate;
	uint8_t path_bits;
	struct _av_conn {
		uint8_t path_mtu;
		uint8_t local_ack_timeout;
		uint8_t seq_err_retry_cnt;
		uint8_t rnr_retry_cnt;
	} conn;
} ib_av_attr_t;
/*
* SEE ALSO
*	ib_gid_t
*****/

/****d* Access Layer/ib_qp_type_t
* NAME
*	ib_qp_type_t
*
* DESCRIPTION
*	Indicates the type of queue pair being created.
*
* SYNOPSIS
*/
typedef enum _ib_qp_type {
	IB_QPT_RELIABLE_CONN = 0,	/* Matches CM REQ transport type */
	IB_QPT_UNRELIABLE_CONN = 1,	/* Matches CM REQ transport type */
	IB_QPT_RELIABLE_DGRM = 2,	/* Matches CM REQ transport type */
	IB_QPT_UNRELIABLE_DGRM,
	IB_QPT_QP0,
	IB_QPT_QP1,
	IB_QPT_RAW_IPV6,
	IB_QPT_RAW_ETHER,
	IB_QPT_MAD,		/* InfiniBand Access Layer */
	IB_QPT_QP0_ALIAS,	/* InfiniBand Access Layer */
	IB_QPT_QP1_ALIAS	/* InfiniBand Access Layer */
} ib_qp_type_t;
/*
* VALUES
*	IB_QPT_RELIABLE_CONN
*		Reliable, connected queue pair.
*
*	IB_QPT_UNRELIABLE_CONN
*		Unreliable, connected queue pair.
*
*	IB_QPT_RELIABLE_DGRM
*		Reliable, datagram queue pair.
*
*	IB_QPT_UNRELIABLE_DGRM
*		Unreliable, datagram queue pair.
*
*	IB_QPT_QP0
*		Queue pair 0.
*
*	IB_QPT_QP1
*		Queue pair 1.
*
*	IB_QPT_RAW_DGRM
*		Raw datagram queue pair.
*
*	IB_QPT_RAW_IPV6
*		Raw IP version 6 queue pair.
*
*	IB_QPT_RAW_ETHER
*		Raw Ethernet queue pair.
*
*	IB_QPT_MAD
*		Unreliable, datagram queue pair that will send and receive management
*		datagrams with assistance from the access layer.
*
*	IB_QPT_QP0_ALIAS
*		Alias to queue pair 0.  Aliased QPs can only be created on an aliased
*		protection domain.
*
*	IB_QPT_QP1_ALIAS
*		Alias to queue pair 1.  Aliased QPs can only be created on an aliased
*		protection domain.
*****/

/****d* Access Layer/ib_access_t
* NAME
*	ib_access_t
*
* DESCRIPTION
*	Indicates the type of access is permitted on resources such as QPs,
*	memory regions and memory windows.
*
* SYNOPSIS
*/
typedef uint32_t ib_access_t;
#define IB_AC_RDMA_READ				0x00000001
#define IB_AC_RDMA_WRITE			0x00000002
#define IB_AC_ATOMIC				0x00000004
#define IB_AC_LOCAL_WRITE			0x00000008
#define IB_AC_MW_BIND				0x00000010
/*
* NOTES
*	Users may combine access rights using a bit-wise or operation to specify
*	additional access.  For example: IB_AC_RDMA_READ | IB_AC_RDMA_WRITE grants
*	RDMA read and write access.
*****/

/****d* Access Layer/ib_qp_state_t
* NAME
*	ib_qp_state_t
*
* DESCRIPTION
*	Indicates or sets the state of a queue pair.  The current state of a queue
*	pair is returned through the ib_qp_query call and set via the
*	ib_qp_modify call.
*
* SYNOPSIS
*/
typedef uint32_t ib_qp_state_t;
#define IB_QPS_RESET				0x00000001
#define IB_QPS_INIT				0x00000002
#define IB_QPS_RTR				0x00000004
#define IB_QPS_RTS				0x00000008
#define IB_QPS_SQD				0x00000010
#define IB_QPS_SQD_DRAINING			0x00000030
#define IB_QPS_SQD_DRAINED			0x00000050
#define IB_QPS_SQERR				0x00000080
#define IB_QPS_ERROR				0x00000100
#define IB_QPS_TIME_WAIT			0xDEAD0000	/* InfiniBand Access Layer */
/*****/

/****d* Access Layer/ib_apm_state_t
* NAME
*	ib_apm_state_t
*
* DESCRIPTION
*	The current automatic path migration state of a queue pair
*
* SYNOPSIS
*/
typedef enum _ib_apm_state {
	IB_APM_MIGRATED = 1,
	IB_APM_REARM,
	IB_APM_ARMED
} ib_apm_state_t;
/*****/

/****s* Access Layer/ib_qp_create_t
* NAME
*	ib_qp_create_t
*
* DESCRIPTION
*	Attributes used to initialize a queue pair at creation time.
*
* SYNOPSIS
*/
typedef struct _ib_qp_create {
	ib_qp_type_t qp_type;
	ib_rdd_handle_t h_rdd;
	uint32_t sq_depth;
	uint32_t rq_depth;
	uint32_t sq_sge;
	uint32_t rq_sge;
	ib_cq_handle_t h_sq_cq;
	ib_cq_handle_t h_rq_cq;
	boolean_t sq_signaled;
} ib_qp_create_t;
/*
* FIELDS
*	type
*		Specifies the type of queue pair to create.
*
*	h_rdd
*		A handle to a reliable datagram domain to associate with the queue
*		pair.  This field is ignored if the queue pair is not a reliable
*		datagram type queue pair.
*
*	sq_depth
*		Indicates the requested maximum number of work requests that may be
*		outstanding on the queue pair's send queue.  This value must be less
*		than or equal to the maximum reported by the channel adapter associated
*		with the queue pair.
*
*	rq_depth
*		Indicates the requested maximum number of work requests that may be
*		outstanding on the queue pair's receive queue.  This value must be less
*		than or equal to the maximum reported by the channel adapter associated
*		with the queue pair.
*
*	sq_sge
*		Indicates the maximum number scatter-gather elements that may be
*		given in a send work request.  This value must be less
*		than or equal to the maximum reported by the channel adapter associated
*		with the queue pair.
*
*	rq_sge
*		Indicates the maximum number scatter-gather elements that may be
*		given in a receive work request.  This value must be less
*		than or equal to the maximum reported by the channel adapter associated
*		with the queue pair.
*
*	h_sq_cq
*		A handle to the completion queue that will be used to report send work
*		request completions.  This handle must be NULL if the type is
*		IB_QPT_MAD, IB_QPT_QP0_ALIAS, or IB_QPT_QP1_ALIAS.
*
*	h_rq_cq
*		A handle to the completion queue that will be used to report receive
*		work request completions.  This handle must be NULL if the type is
*		IB_QPT_MAD, IB_QPT_QP0_ALIAS, or IB_QPT_QP1_ALIAS.
*
*	sq_signaled
*		A flag that is used to indicate whether the queue pair will signal
*		an event upon completion of a send work request.  If set to
*		TRUE, send work requests will always generate a completion
*		event.  If set to FALSE, a completion event will only be
*		generated if the send_opt field of the send work request has the
*		IB_SEND_OPT_SIGNALED flag set.
*
* SEE ALSO
*	ib_qp_type_t, ib_qp_attr_t
*****/

/****s* Access Layer/ib_qp_attr_t
* NAME
*	ib_qp_attr_t
*
* DESCRIPTION
*	Queue pair attributes returned through ib_query_qp.
*
* SYNOPSIS
*/
typedef struct _ib_qp_attr {
	ib_pd_handle_t h_pd;
	ib_qp_type_t qp_type;
	ib_access_t access_ctrl;
	uint16_t pkey_index;
	uint32_t sq_depth;
	uint32_t rq_depth;
	uint32_t sq_sge;
	uint32_t rq_sge;
	uint8_t init_depth;
	uint8_t resp_res;
	ib_cq_handle_t h_sq_cq;
	ib_cq_handle_t h_rq_cq;
	ib_rdd_handle_t h_rdd;
	boolean_t sq_signaled;
	ib_qp_state_t state;
	ib_net32_t num;
	ib_net32_t dest_num;
	ib_net32_t qkey;
	ib_net32_t sq_psn;
	ib_net32_t rq_psn;
	uint8_t primary_port;
	uint8_t alternate_port;
	ib_av_attr_t primary_av;
	ib_av_attr_t alternate_av;
	ib_apm_state_t apm_state;
} ib_qp_attr_t;
/*
* FIELDS
*	h_pd
*		This is a handle to a protection domain associated with the queue
*		pair, or NULL if the queue pair is type IB_QPT_RELIABLE_DGRM.
*
* NOTES
*	Other fields are defined by the Infiniband specification.
*
* SEE ALSO
*	ib_qp_type_t, ib_access_t, ib_qp_state_t, ib_av_attr_t, ib_apm_state_t
*****/

/****d* Access Layer/ib_qp_opts_t
* NAME
*	ib_qp_opts_t
*
* DESCRIPTION
*	Optional fields supplied in the modify QP operation.
*
* SYNOPSIS
*/
typedef uint32_t ib_qp_opts_t;
#define IB_MOD_QP_ALTERNATE_AV			0x00000001
#define IB_MOD_QP_PKEY				0x00000002
#define IB_MOD_QP_APM_STATE			0x00000004
#define IB_MOD_QP_PRIMARY_AV			0x00000008
#define IB_MOD_QP_RNR_NAK_TIMEOUT		0x00000010
#define IB_MOD_QP_RESP_RES			0x00000020
#define IB_MOD_QP_INIT_DEPTH			0x00000040
#define IB_MOD_QP_PRIMARY_PORT			0x00000080
#define IB_MOD_QP_ACCESS_CTRL			0x00000100
#define IB_MOD_QP_QKEY				0x00000200
#define IB_MOD_QP_SQ_DEPTH			0x00000400
#define IB_MOD_QP_RQ_DEPTH			0x00000800
#define IB_MOD_QP_CURRENT_STATE			0x00001000
#define IB_MOD_QP_RETRY_CNT			0x00002000
#define IB_MOD_QP_LOCAL_ACK_TIMEOUT		0x00004000
#define IB_MOD_QP_RNR_RETRY_CNT			0x00008000
/*
* SEE ALSO
*	ib_qp_mod_t
*****/

/****s* Access Layer/ib_qp_mod_t
* NAME
*	ib_qp_mod_t
*
* DESCRIPTION
*	Information needed to change the state of a queue pair through the
*	ib_modify_qp call.
*
* SYNOPSIS
*/
typedef struct _ib_qp_mod {
	ib_qp_state_t req_state;
	union _qp_state {
		struct _qp_reset {
			/*
			 * Time, in milliseconds, that the QP needs to spend in
			 * the time wait state before being reused.
			 */
			uint32_t timewait;
		} reset;
		struct _qp_init {
			ib_qp_opts_t opts;
			uint8_t primary_port;
			ib_net32_t qkey;
			uint16_t pkey_index;
			ib_access_t access_ctrl;
		} init;
		struct _qp_rtr {
			ib_net32_t rq_psn;
			ib_net32_t dest_qp;
			ib_av_attr_t primary_av;
			uint8_t resp_res;
			ib_qp_opts_t opts;
			ib_av_attr_t alternate_av;
			ib_net32_t qkey;
			uint16_t pkey_index;
			ib_access_t access_ctrl;
			uint32_t sq_depth;
			uint32_t rq_depth;
			uint8_t rnr_nak_timeout;
		} rtr;
		struct _qp_rts {
			ib_net32_t sq_psn;
			uint8_t retry_cnt;
			uint8_t rnr_retry_cnt;
			uint8_t rnr_nak_timeout;
			uint8_t local_ack_timeout;
			uint8_t init_depth;
			ib_qp_opts_t opts;
			ib_qp_state_t current_state;
			ib_net32_t qkey;
			ib_access_t access_ctrl;
			uint8_t resp_res;
			ib_av_attr_t primary_av;
			ib_av_attr_t alternate_av;
			uint32_t sq_depth;
			uint32_t rq_depth;
			ib_apm_state_t apm_state;
			uint8_t primary_port;
			uint16_t pkey_index;
		} rts;
		struct _qp_sqd {
			boolean_t sqd_event;
		} sqd;
	} state;
} ib_qp_mod_t;
/*
* SEE ALSO
*	ib_qp_state_t, ib_access_t, ib_av_attr_t, ib_apm_state_t
*****/

/****s* Access Layer/ib_eec_attr_t
* NAME
*	ib_eec_attr_t
*
* DESCRIPTION
*	Information about an end-to-end context.
*
* SYNOPSIS
*/
typedef struct _ib_eec_attr {
	ib_qp_state_t state;
	ib_rdd_handle_t h_rdd;
	ib_net32_t local_eecn;
	ib_net32_t sq_psn;
	ib_net32_t rq_psn;
	uint8_t primary_port;
	uint16_t pkey_index;
	uint32_t resp_res;
	ib_net32_t remote_eecn;
	uint32_t init_depth;
	uint32_t dest_num;	// ??? What is this?
	ib_av_attr_t primary_av;
	ib_av_attr_t alternate_av;
	ib_apm_state_t apm_state;
} ib_eec_attr_t;
/*
* SEE ALSO
*	ib_qp_state_t, ib_av_attr_t, ib_apm_state_t
*****/

/****d* Access Layer/ib_eec_opts_t
* NAME
*	ib_eec_opts_t
*
* DESCRIPTION
*	Optional fields supplied in the modify EEC operation.
*
* SYNOPSIS
*/
typedef uint32_t ib_eec_opts_t;
#define IB_MOD_EEC_ALTERNATE_AV			0x00000001
#define IB_MOD_EEC_PKEY				0x00000002
#define IB_MOD_EEC_APM_STATE			0x00000004
#define IB_MOD_EEC_PRIMARY_AV			0x00000008
#define IB_MOD_EEC_RNR				0x00000010
#define IB_MOD_EEC_RESP_RES			0x00000020
#define IB_MOD_EEC_OUTSTANDING			0x00000040
#define IB_MOD_EEC_PRIMARY_PORT			0x00000080
/*
* NOTES
*
*
*****/

/****s* Access Layer/ib_eec_mod_t
* NAME
*	ib_eec_mod_t
*
* DESCRIPTION
*	Information needed to change the state of an end-to-end context through
*	the ib_modify_eec function.
*
* SYNOPSIS
*/
typedef struct _ib_eec_mod {
	ib_qp_state_t req_state;
	union _eec_state {
		struct _eec_init {
			uint8_t primary_port;
			uint16_t pkey_index;
		} init;
		struct _eec_rtr {
			ib_net32_t rq_psn;
			ib_net32_t remote_eecn;
			ib_av_attr_t primary_av;
			uint8_t resp_res;
			ib_eec_opts_t opts;
			ib_av_attr_t alternate_av;
			uint16_t pkey_index;
		} rtr;
		struct _eec_rts {
			ib_net32_t sq_psn;
			uint8_t retry_cnt;
			uint8_t rnr_retry_cnt;
			uint8_t local_ack_timeout;
			uint8_t init_depth;
			ib_eec_opts_t opts;
			ib_av_attr_t alternate_av;
			ib_apm_state_t apm_state;
			ib_av_attr_t primary_av;
			uint16_t pkey_index;
			uint8_t primary_port;
		} rts;
		struct _eec_sqd {
			boolean_t sqd_event;
		} sqd;
	} state;
} ib_eec_mod_t;
/*
* SEE ALSO
*	ib_qp_state_t, ib_av_attr_t, ib_apm_state_t
*****/

/****d* Access Layer/ib_wr_type_t
* NAME
*	ib_wr_type_t
*
* DESCRIPTION
*	Identifies the type of work request posted to a queue pair.
*
* SYNOPSIS
*/
typedef enum _ib_wr_type_t {
	WR_SEND = 1,
	WR_RDMA_WRITE,
	WR_RDMA_READ,
	WR_COMPARE_SWAP,
	WR_FETCH_ADD
} ib_wr_type_t;
/*****/

/****s* Access Layer/ib_local_ds_t
* NAME
*	ib_local_ds_t
*
* DESCRIPTION
*	Local data segment information referenced by send and receive work
*	requests.  This is used to specify local data buffers used as part of a
*	work request.
*
* SYNOPSIS
*/
typedef struct _ib_local_ds {
	void *vaddr;
	uint32_t length;
	uint32_t lkey;
} ib_local_ds_t;
/*****/

/****d* Access Layer/ib_send_opt_t
* NAME
*	ib_send_opt_t
*
* DESCRIPTION
*	Optional flags used when posting send work requests.  These flags
*	indicate specific processing for the send operation.
*
* SYNOPSIS
*/
typedef uint32_t ib_send_opt_t;
#define IB_SEND_OPT_IMMEDIATE		0x00000001
#define IB_SEND_OPT_FENCE		0x00000002
#define IB_SEND_OPT_SIGNALED		0x00000004
#define IB_SEND_OPT_SOLICITED		0x00000008
#define IB_SEND_OPT_INLINE		0x00000010
#define IB_SEND_OPT_LOCAL		0x00000020
#define IB_SEND_OPT_VEND_MASK		0xFFFF0000
/*
* VALUES
*	The following flags determine the behavior of a work request when
*	posted to the send side.
*
*	IB_SEND_OPT_IMMEDIATE
*		Send immediate data with the given request.
*
*	IB_SEND_OPT_FENCE
*		The operation is fenced.  Complete all pending send operations
*		before processing this request.
*
*	IB_SEND_OPT_SIGNALED
*		If the queue pair is configured for signaled completion, then
*		generate a completion queue entry when this request completes.
*
*	IB_SEND_OPT_SOLICITED
*		Set the solicited bit on the last packet of this request.
*
*	IB_SEND_OPT_INLINE
*		Indicates that the requested send data should be copied into a VPD
*		owned data buffer.  This flag permits the user to issue send operations
*		without first needing to register the buffer(s) associated with the
*		send operation.  Verb providers that support this operation may place
*		vendor specific restrictions on the size of send operation that may
*		be performed as inline.
*
*
*  IB_SEND_OPT_LOCAL
*     Indicates that a sent MAD request should be given to the local VPD for
*     processing.  MADs sent using this option are not placed on the wire.
*     This send option is only valid for MAD send operations.
*
*
*	IB_SEND_OPT_VEND_MASK
*		This mask indicates bits reserved in the send options that may be used
*		by the verbs provider to indicate vendor specific options.  Bits set
*		in this area of the send options are ignored by the Access Layer, but
*		may have specific meaning to the underlying VPD.
*
*****/

/****s* Access Layer/ib_send_wr_t
* NAME
*	ib_send_wr_t
*
* DESCRIPTION
*	Information used to submit a work request to the send queue of a queue
*	pair.
*
* SYNOPSIS
*/
typedef struct _ib_send_wr {
	struct _ib_send_wr *p_next;
	uint64_t wr_id;
	ib_wr_type_t wr_type;
	ib_send_opt_t send_opt;
	uint32_t num_ds;
	ib_local_ds_t *ds_array;
	ib_net32_t immediate_data;
	union _send_dgrm {
		struct _send_ud {
			ib_net32_t remote_qp;
			ib_net32_t remote_qkey;
			ib_av_handle_t h_av;
		} ud;
		struct _send_rd {
			ib_net32_t remote_qp;
			ib_net32_t remote_qkey;
			ib_net32_t eecn;
		} rd;
		struct _send_raw_ether {
			ib_net16_t dest_lid;
			uint8_t path_bits;
			uint8_t sl;
			uint8_t max_static_rate;
			ib_net16_t ether_type;
		} raw_ether;
		struct _send_raw_ipv6 {
			ib_net16_t dest_lid;
			uint8_t path_bits;
			uint8_t sl;
			uint8_t max_static_rate;
		} raw_ipv6;
	} dgrm;
	struct _send_remote_ops {
		uint64_t vaddr;
		uint32_t rkey;
		ib_net64_t atomic1;
		ib_net64_t atomic2;
	} remote_ops;
} ib_send_wr_t;
/*
* FIELDS
*	p_next
*		A pointer used to chain work requests together.  This permits multiple
*		work requests to be posted to a queue pair through a single function
*		call.  This value is set to NULL to mark the end of the chain.
*
*	wr_id
*		A 64-bit work request identifier that is returned to the consumer
*		as part of the work completion.
*
*	wr_type
*		The type of work request being submitted to the send queue.
*
*	send_opt
*		Optional send control parameters.
*
*	num_ds
*		Number of local data segments specified by this work request.
*
*	ds_array
*		A reference to an array of local data segments used by the send
*		operation.
*
*	immediate_data
*		32-bit field sent as part of a message send or RDMA write operation.
*		This field is only valid if the send_opt flag IB_SEND_OPT_IMMEDIATE
*		has been set.
*
*	dgrm.ud.remote_qp
*		Identifies the destination queue pair of an unreliable datagram send
*		operation.
*
*	dgrm.ud.remote_qkey
*		The qkey for the destination queue pair.
*
*	dgrm.ud.h_av
*		An address vector that specifies the path information used to route
*		the outbound datagram to the destination queue pair.
*
*	dgrm.rd.remote_qp
*		Identifies the destination queue pair of a reliable datagram send
*		operation.
*
*	dgrm.rd.remote_qkey
*		The qkey for the destination queue pair.
*
*	dgrm.rd.eecn
*		The local end-to-end context number to use with the reliable datagram
*		send operation.
*
*	dgrm.raw_ether.dest_lid
*		The destination LID that will receive this raw ether send.
*
*	dgrm.raw_ether.path_bits
*		path bits...
*
*	dgrm.raw_ether.sl
*		service level...
*
*	dgrm.raw_ether.max_static_rate
*		static rate...
*
*	dgrm.raw_ether.ether_type
*		ether type...
*
*	dgrm.raw_ipv6.dest_lid
*		The destination LID that will receive this raw ether send.
*
*	dgrm.raw_ipv6.path_bits
*		path bits...
*
*	dgrm.raw_ipv6.sl
*		service level...
*
*	dgrm.raw_ipv6.max_static_rate
*		static rate...
*
*	remote_ops.vaddr
*		The registered virtual memory address of the remote memory to access
*		with an RDMA or atomic operation.
*
*	remote_ops.rkey
*		The rkey associated with the specified remote vaddr. This data must
*		be presented exactly as obtained from the remote node. No swapping
*		of data must be performed.
*
*	atomic1
*		The first operand for an atomic operation.
*
*	atomic2
*		The second operand for an atomic operation.
*
* NOTES
*	The format of data sent over the fabric is user-defined and is considered
*	opaque to the access layer.  The sole exception to this are MADs posted
*	to a MAD QP service.  MADs are expected to match the format defined by
*	the Infiniband specification and must be in network-byte order when posted
*	to the MAD QP service.
*
* SEE ALSO
*	ib_wr_type_t, ib_local_ds_t, ib_send_opt_t
*****/

/****s* Access Layer/ib_recv_wr_t
* NAME
*	ib_recv_wr_t
*
* DESCRIPTION
*	Information used to submit a work request to the receive queue of a queue
*	pair.
*
* SYNOPSIS
*/
typedef struct _ib_recv_wr {
	struct _ib_recv_wr *p_next;
	uint64_t wr_id;
	uint32_t num_ds;
	ib_local_ds_t *ds_array;
} ib_recv_wr_t;
/*
* FIELDS
*	p_next
*		A pointer used to chain work requests together.  This permits multiple
*		work requests to be posted to a queue pair through a single function
*		call.  This value is set to NULL to mark the end of the chain.
*
*	wr_id
*		A 64-bit work request identifier that is returned to the consumer
*		as part of the work completion.
*
*	num_ds
*		Number of local data segments specified by this work request.
*
*	ds_array
*		A reference to an array of local data segments used by the send
*		operation.
*
* SEE ALSO
*	ib_local_ds_t
*****/

/****s* Access Layer/ib_bind_wr_t
* NAME
*	ib_bind_wr_t
*
* DESCRIPTION
*	Information used to submit a memory window bind work request to the send
*	queue of a queue pair.
*
* SYNOPSIS
*/
typedef struct _ib_bind_wr {
	uint64_t wr_id;
	ib_send_opt_t send_opt;
	ib_mr_handle_t h_mr;
	ib_access_t access_ctrl;
	uint32_t current_rkey;
	ib_local_ds_t local_ds;
} ib_bind_wr_t;
/*
* FIELDS
*	wr_id
*		A 64-bit work request identifier that is returned to the consumer
*		as part of the work completion.
*
*	send_opt
*		Optional send control parameters.
*
*	h_mr
*		Handle to the memory region to which this window is being bound.
*
*	access_ctrl
*		Access rights for this memory window.
*
*	current_rkey
*		The current rkey assigned to this window for remote access.
*
*	local_ds
*		A reference to a local data segment used by the bind operation.
*
* SEE ALSO
*	ib_send_opt_t, ib_access_t, ib_local_ds_t
*****/

/****d* Access Layer/ib_wc_status_t
* NAME
*	ib_wc_status_t
*
* DESCRIPTION
*	Indicates the status of a completed work request.  These VALUES are
*	returned to the user when retrieving completions.  Note that success is
*	identified as IB_WCS_SUCCESS, which is always zero.
*
* SYNOPSIS
*/
typedef enum _ib_wc_status_t {
	IB_WCS_SUCCESS,
	IB_WCS_LOCAL_LEN_ERR,
	IB_WCS_LOCAL_OP_ERR,
	IB_WCS_LOCAL_EEC_OP_ERR,
	IB_WCS_LOCAL_PROTECTION_ERR,
	IB_WCS_WR_FLUSHED_ERR,
	IB_WCS_MEM_WINDOW_BIND_ERR,
	IB_WCS_REM_ACCESS_ERR,
	IB_WCS_REM_OP_ERR,
	IB_WCS_RNR_RETRY_ERR,
	IB_WCS_TIMEOUT_RETRY_ERR,
	IB_WCS_REM_INVALID_REQ_ERR,
	IB_WCS_REM_INVALID_RD_REQ_ERR,
	IB_WCS_INVALID_EECN,
	IB_WCS_INVALID_EEC_STATE,
	IB_WCS_UNMATCHED_RESPONSE,	/* InfiniBand Access Layer */
	IB_WCS_CANCELED,	/* InfiniBand Access Layer */
	IB_WCS_UNKNOWN		/* Must be last. */
} ib_wc_status_t;
/*
* VALUES
*	IB_WCS_SUCCESS
*		Work request completed successfully.
*
*	IB_WCS_MAD
*		The completed work request was associated with a management datagram
*		that requires post processing.  The MAD will be returned to the user
*		through a callback once all post processing has completed.
*
*	IB_WCS_LOCAL_LEN_ERR
*		Generated for a work request posted to the send queue when the
*		total of the data segment lengths exceeds the message length of the
*		channel.  Generated for a work request posted to the receive queue when
*		the total of the data segment lengths is too small for a
*		valid incoming message.
*
*	IB_WCS_LOCAL_OP_ERR
*		An internal QP consistency error was generated while processing this
*		work request.  This may indicate that the QP was in an incorrect state
*		for the requested operation.
*
*	IB_WCS_LOCAL_EEC_OP_ERR
*		An internal EEC consistency error was generated while processing
*		this work request.  This may indicate that the EEC was in an incorrect
*		state for the requested operation.
*
*	IB_WCS_LOCAL_PROTECTION_ERR
*		The data segments of the locally posted work request did not refer to
*		a valid memory region.  The memory may not have been properly
*		registered for the requested operation.
*
*	IB_WCS_WR_FLUSHED_ERR
*		The work request was flushed from the QP before being completed.
*
*	IB_WCS_MEM_WINDOW_BIND_ERR
*		A memory window bind operation failed due to insufficient access
*		rights.
*
*	IB_WCS_REM_ACCESS_ERR,
*		A protection error was detected at the remote node for a RDMA or atomic
*		operation.
*
*	IB_WCS_REM_OP_ERR,
*		The operation could not be successfully completed at the remote node.
*		This may indicate that the remote QP was in an invalid state or
*		contained an invalid work request.
*
*	IB_WCS_RNR_RETRY_ERR,
*		The RNR retry count was exceeded while trying to send this message.
*
*	IB_WCS_TIMEOUT_RETRY_ERR
*		The local transport timeout counter expired while trying to send this
*		message.
*
*	IB_WCS_REM_INVALID_REQ_ERR,
*		The remote node detected an invalid message on the channel.  This error
*		is usually a result of one of the following:
*			- The operation was not supported on receive queue.
*			- There was insufficient buffers to receive a new RDMA request.
*			- There was insufficient buffers to receive a new atomic operation.
*			- An RDMA request was larger than 2^31 bytes.
*
*	IB_WCS_REM_INVALID_RD_REQ_ERR,
*		Responder detected an invalid RD message.  This may be the result of an
*		invalid qkey or an RDD mismatch.
*
*	IB_WCS_INVALID_EECN
*		An invalid EE context number was detected.
*
*	IB_WCS_INVALID_EEC_STATE
*		The EEC was in an invalid state for the specified request.
*
*	IB_WCS_UNMATCHED_RESPONSE
*		A response MAD was received for which there was no matching send.  The
*		send operation may have been canceled by the user or may have timed
*		out.
*
*	IB_WCS_CANCELED
*		The completed work request was canceled by the user.
*****/

OSM_EXPORT const char *ib_wc_status_str[];

/****f* IBA Base: Types/ib_get_wc_status_str
* NAME
*	ib_get_wc_status_str
*
* DESCRIPTION
*	Returns a string for the specified work completion status.
*
* SYNOPSIS
*/
static inline const char *OSM_API
ib_get_wc_status_str(IN ib_wc_status_t wc_status)
{
	if (wc_status > IB_WCS_UNKNOWN)
		wc_status = IB_WCS_UNKNOWN;
	return (ib_wc_status_str[wc_status]);
}

/*
* PARAMETERS
*	wc_status
*		[in] work completion status value
*
* RETURN VALUES
*	Pointer to the work completion status description string.
*
* NOTES
*
* SEE ALSO
*********/

/****d* Access Layer/ib_wc_type_t
* NAME
*	ib_wc_type_t
*
* DESCRIPTION
*	Indicates the type of work completion.
*
* SYNOPSIS
*/
typedef enum _ib_wc_type_t {
	IB_WC_SEND,
	IB_WC_RDMA_WRITE,
	IB_WC_RECV,
	IB_WC_RDMA_READ,
	IB_WC_MW_BIND,
	IB_WC_FETCH_ADD,
	IB_WC_COMPARE_SWAP,
	IB_WC_RECV_RDMA_WRITE
} ib_wc_type_t;
/*****/

/****d* Access Layer/ib_recv_opt_t
* NAME
*	ib_recv_opt_t
*
* DESCRIPTION
*	Indicates optional fields valid in a receive work completion.
*
* SYNOPSIS
*/
typedef uint32_t ib_recv_opt_t;
#define	IB_RECV_OPT_IMMEDIATE		0x00000001
#define IB_RECV_OPT_FORWARD		0x00000002
#define IB_RECV_OPT_GRH_VALID		0x00000004
#define IB_RECV_OPT_VEND_MASK		0xFFFF0000
/*
* VALUES
*	IB_RECV_OPT_IMMEDIATE
*		Indicates that immediate data is valid for this work completion.
*
*	IB_RECV_OPT_FORWARD
*		Indicates that the received trap should be forwarded to the SM.
*
*	IB_RECV_OPT_GRH_VALID
*		Indicates presence of the global route header. When set, the
*		first 40 bytes received are the GRH.
*
*	IB_RECV_OPT_VEND_MASK
*		This mask indicates bits reserved in the receive options that may be
*		used by the verbs provider to indicate vendor specific options.  Bits
*		set in this area of the receive options are ignored by the Access Layer,
*		but may have specific meaning to the underlying VPD.
*****/

/****s* Access Layer/ib_wc_t
* NAME
*	ib_wc_t
*
* DESCRIPTION
*	Work completion information.
*
* SYNOPSIS
*/
typedef struct _ib_wc {
	struct _ib_wc *p_next;
	uint64_t wr_id;
	ib_wc_type_t wc_type;
	uint32_t length;
	ib_wc_status_t status;
	uint64_t vendor_specific;
	union _wc_recv {
		struct _wc_conn {
			ib_recv_opt_t recv_opt;
			ib_net32_t immediate_data;
		} conn;
		struct _wc_ud {
			ib_recv_opt_t recv_opt;
			ib_net32_t immediate_data;
			ib_net32_t remote_qp;
			uint16_t pkey_index;
			ib_net16_t remote_lid;
			uint8_t remote_sl;
			uint8_t path_bits;
		} ud;
		struct _wc_rd {
			ib_net32_t remote_eecn;
			ib_net32_t remote_qp;
			ib_net16_t remote_lid;
			uint8_t remote_sl;
			uint32_t free_cnt;

		} rd;
		struct _wc_raw_ipv6 {
			ib_net16_t remote_lid;
			uint8_t remote_sl;
			uint8_t path_bits;
		} raw_ipv6;
		struct _wc_raw_ether {
			ib_net16_t remote_lid;
			uint8_t remote_sl;
			uint8_t path_bits;
			ib_net16_t ether_type;
		} raw_ether;
	} recv;
} ib_wc_t;
/*
* FIELDS
*	p_next
*		A pointer used to chain work completions.  This permits multiple
*		work completions to be retrieved from a completion queue through a
*		single function call.  This value is set to NULL to mark the end of
*		the chain.
*
*	wr_id
*		The 64-bit work request identifier that was specified when posting the
*		work request.
*
*	wc_type
*		Indicates the type of work completion.
*
*
*	length
*		The total length of the data sent or received with the work request.
*
*	status
*		The result of the work request.
*
*	vendor_specific
*		HCA vendor specific information returned as part of the completion.
*
*	recv.conn.recv_opt
*		Indicates optional fields valid as part of a work request that
*		completed on a connected (reliable or unreliable) queue pair.
*
*	recv.conn.immediate_data
*		32-bit field received as part of an inbound message on a connected
*		queue pair.  This field is only valid if the recv_opt flag
*		IB_RECV_OPT_IMMEDIATE has been set.
*
*	recv.ud.recv_opt
*		Indicates optional fields valid as part of a work request that
*		completed on an unreliable datagram queue pair.
*
*	recv.ud.immediate_data
*		32-bit field received as part of an inbound message on a unreliable
*		datagram queue pair.  This field is only valid if the recv_opt flag
*		IB_RECV_OPT_IMMEDIATE has been set.
*
*	recv.ud.remote_qp
*		Identifies the source queue pair of a received datagram.
*
*	recv.ud.pkey_index
*		The pkey index for the source queue pair. This is valid only for
*		GSI type QP's.
*
*	recv.ud.remote_lid
*		The source LID of the received datagram.
*
*	recv.ud.remote_sl
*		The service level used by the source of the received datagram.
*
*	recv.ud.path_bits
*		path bits...
*
*	recv.rd.remote_eecn
*		The remote end-to-end context number that sent the received message.
*
*	recv.rd.remote_qp
*		Identifies the source queue pair of a received message.
*
*	recv.rd.remote_lid
*		The source LID of the received message.
*
*	recv.rd.remote_sl
*		The service level used by the source of the received message.
*
*	recv.rd.free_cnt
*		The number of available entries in the completion queue.  Reliable
*		datagrams may complete out of order, so this field may be used to
*		determine the number of additional completions that may occur.
*
*	recv.raw_ipv6.remote_lid
*		The source LID of the received message.
*
*	recv.raw_ipv6.remote_sl
*		The service level used by the source of the received message.
*
*	recv.raw_ipv6.path_bits
*		path bits...
*
*	recv.raw_ether.remote_lid
*		The source LID of the received message.
*
*	recv.raw_ether.remote_sl
*		The service level used by the source of the received message.
*
*	recv.raw_ether.path_bits
*		path bits...
*
*	recv.raw_ether.ether_type
*		ether type...
* NOTES
*	When the work request completes with error, the only values that the
*	consumer can depend on are the wr_id field, and the status of the
*	operation.
*
*	If the consumer is using the same CQ for completions from more than
*	one type of QP (i.e Reliable Connected, Datagram etc), then the consumer
*	must have additional information to decide what fields of the union are
*	valid.
* SEE ALSO
*	ib_wc_type_t, ib_qp_type_t, ib_wc_status_t, ib_recv_opt_t
*****/

/****s* Access Layer/ib_mr_create_t
* NAME
*	ib_mr_create_t
*
* DESCRIPTION
*	Information required to create a registered memory region.
*
* SYNOPSIS
*/
typedef struct _ib_mr_create {
	void *vaddr;
	uint64_t length;
	ib_access_t access_ctrl;
} ib_mr_create_t;
/*
* FIELDS
*	vaddr
*		Starting virtual address of the region being registered.
*
*	length
*		Length of the buffer to register.
*
*	access_ctrl
*		Access rights of the registered region.
*
* SEE ALSO
*	ib_access_t
*****/

/****s* Access Layer/ib_phys_create_t
* NAME
*	ib_phys_create_t
*
* DESCRIPTION
*	Information required to create a physical memory region.
*
* SYNOPSIS
*/
typedef struct _ib_phys_create {
	uint64_t length;
	uint32_t num_bufs;
	uint64_t *buf_array;
	uint32_t buf_offset;
	uint32_t page_size;
	ib_access_t access_ctrl;
} ib_phys_create_t;
/*
*	length
*		The length of the memory region in bytes.
*
*	num_bufs
*		Number of buffers listed in the specified buffer array.
*
*	buf_array
*		An array of physical buffers to be registered as a single memory
*		region.
*
*	buf_offset
*		The offset into the first physical page of the specified memory
*		region to start the virtual address.
*
*	page_size
*		The physical page size of the memory being registered.
*
*	access_ctrl
*		Access rights of the registered region.
*
* SEE ALSO
*	ib_access_t
*****/

/****s* Access Layer/ib_mr_attr_t
* NAME
*	ib_mr_attr_t
*
* DESCRIPTION
*	Attributes of a registered memory region.
*
* SYNOPSIS
*/
typedef struct _ib_mr_attr {
	ib_pd_handle_t h_pd;
	void *local_lb;
	void *local_ub;
	void *remote_lb;
	void *remote_ub;
	ib_access_t access_ctrl;
	uint32_t lkey;
	uint32_t rkey;
} ib_mr_attr_t;
/*
* DESCRIPTION
*	h_pd
*		Handle to the protection domain for this memory region.
*
*	local_lb
*		The virtual address of the lower bound of protection for local
*		memory access.
*
*	local_ub
*		The virtual address of the upper bound of protection for local
*		memory access.
*
*	remote_lb
*		The virtual address of the lower bound of protection for remote
*		memory access.
*
*	remote_ub
*		The virtual address of the upper bound of protection for remote
*		memory access.
*
*	access_ctrl
*		Access rights for the specified memory region.
*
*	lkey
*		The lkey associated with this memory region.
*
*	rkey
*		The rkey associated with this memory region.
*
* NOTES
*	The remote_lb, remote_ub, and rkey are only valid if remote memory access
*	is enabled for this memory region.
*
* SEE ALSO
*	ib_access_t
*****/

/****d* Access Layer/ib_ca_mod_t
* NAME
*	ib_ca_mod_t -- Modify port attributes and error counters
*
* DESCRIPTION
*	Specifies modifications to the port attributes of a channel adapter.
*
* SYNOPSIS
*/
typedef uint32_t ib_ca_mod_t;
#define IB_CA_MOD_IS_CM_SUPPORTED		0x00000001
#define IB_CA_MOD_IS_SNMP_SUPPORTED		0x00000002
#define	IB_CA_MOD_IS_DEV_MGMT_SUPPORTED		0x00000004
#define	IB_CA_MOD_IS_VEND_SUPPORTED		0x00000008
#define	IB_CA_MOD_IS_SM				0x00000010
#define IB_CA_MOD_IS_SM_DISABLED		0x00000020
#define IB_CA_MOD_QKEY_CTR			0x00000040
#define IB_CA_MOD_PKEY_CTR			0x00000080
#define IB_CA_MOD_IS_NOTICE_SUPPORTED		0x00000100
#define IB_CA_MOD_IS_TRAP_SUPPORTED		0x00000200
#define IB_CA_MOD_IS_APM_SUPPORTED		0x00000400
#define IB_CA_MOD_IS_SLMAP_SUPPORTED		0x00000800
#define IB_CA_MOD_IS_PKEY_NVRAM_SUPPORTED	0x00001000
#define IB_CA_MOD_IS_MKEY_NVRAM_SUPPORTED	0x00002000
#define IB_CA_MOD_IS_SYSGUID_SUPPORTED		0x00004000
#define IB_CA_MOD_IS_DR_NOTICE_SUPPORTED	0x00008000
#define IB_CA_MOD_IS_BOOT_MGMT_SUPPORTED	0x00010000
#define IB_CA_MOD_IS_CAPM_NOTICE_SUPPORTED	0x00020000
#define IB_CA_MOD_IS_REINIT_SUPPORTED		0x00040000
#define IB_CA_MOD_IS_LEDINFO_SUPPORTED		0x00080000
#define IB_CA_MOD_SHUTDOWN_PORT			0x00100000
#define IB_CA_MOD_INIT_TYPE_VALUE		0x00200000
#define IB_CA_MOD_SYSTEM_IMAGE_GUID		0x00400000
/*
* VALUES
*	IB_CA_MOD_IS_CM_SUPPORTED
*		Indicates if there is a communication manager accessible through
*		the port.
*
*	IB_CA_MOD_IS_SNMP_SUPPORTED
*		Indicates if there is an SNMP agent accessible through the port.
*
*	IB_CA_MOD_IS_DEV_MGMT_SUPPORTED
*		Indicates if there is a device management agent accessible
*		through the port.
*
*	IB_CA_MOD_IS_VEND_SUPPORTED
*		Indicates if there is a vendor supported agent accessible
*		through the port.
*
*	IB_CA_MOD_IS_SM
*		Indicates if there is a subnet manager accessible through
*		the port.
*
*	IB_CA_MOD_IS_SM_DISABLED
*		Indicates if the port has been disabled for configuration by the
*		subnet manager.
*
*	IB_CA_MOD_QKEY_CTR
*		Used to reset the qkey violation counter associated with the
*		port.
*
*	IB_CA_MOD_PKEY_CTR
*		Used to reset the pkey violation counter associated with the
*		port.
*
*	IB_CA_MOD_IS_NOTICE_SUPPORTED
*		Indicates that this CA supports ability to generate Notices for
*		Port State changes. (only applicable to switches)
*
*	IB_CA_MOD_IS_TRAP_SUPPORTED
*		Indicates that this management port supports ability to generate
*		trap messages. (only applicable to switches)
*
*	IB_CA_MOD_IS_APM_SUPPORTED
*		Indicates that this port is capable of performing Automatic
*		Path Migration.
*
*	IB_CA_MOD_IS_SLMAP_SUPPORTED
*		Indicates this port supports SLMAP capability.
*
*	IB_CA_MOD_IS_PKEY_NVRAM_SUPPORTED
*		Indicates that PKEY is supported in NVRAM
*
*	IB_CA_MOD_IS_MKEY_NVRAM_SUPPORTED
*		Indicates that MKEY is supported in NVRAM
*
*	IB_CA_MOD_IS_SYSGUID_SUPPORTED
*		Indicates System Image GUID support.
*
*	IB_CA_MOD_IS_DR_NOTICE_SUPPORTED
*		Indicate support for generating Direct Routed Notices
*
*	IB_CA_MOD_IS_BOOT_MGMT_SUPPORTED
*		Indicates support for Boot Management
*
*	IB_CA_MOD_IS_CAPM_NOTICE_SUPPORTED
*		Indicates capability to generate notices for changes to CAPMASK
*
*	IB_CA_MOD_IS_REINIT_SUPPORTED
*		Indicates type of node init supported. Refer to Chapter 14 for
*		Initialization actions.
*
*	IB_CA_MOD_IS_LEDINFO_SUPPORTED
*		Indicates support for LED info.
*
*	IB_CA_MOD_SHUTDOWN_PORT
*		Used to modify the port active indicator.
*
*	IB_CA_MOD_INIT_TYPE_VALUE
*		Used to modify the init_type value for the port.
*
*	IB_CA_MOD_SYSTEM_IMAGE_GUID
*		Used to modify the system image GUID for the port.
*****/

/****d* Access Layer/ib_mr_mod_t
* NAME
*	ib_mr_mod_t
*
* DESCRIPTION
*	Mask used to specify which attributes of a registered memory region are
*	being modified.
*
* SYNOPSIS
*/
typedef uint32_t ib_mr_mod_t;
#define IB_MR_MOD_ADDR					0x00000001
#define IB_MR_MOD_PD					0x00000002
#define IB_MR_MOD_ACCESS				0x00000004
/*
* PARAMETERS
*	IB_MEM_MOD_ADDR
*		The address of the memory region is being modified.
*
*	IB_MEM_MOD_PD
*		The protection domain associated with the memory region is being
*		modified.
*
*	IB_MEM_MOD_ACCESS
*		The access rights the memory region are being modified.
*****/

/****d* IBA Base: Constants/IB_SMINFO_ATTR_MOD_HANDOVER
* NAME
*	IB_SMINFO_ATTR_MOD_HANDOVER
*
* DESCRIPTION
*	Encoded attribute modifier value used on SubnSet(SMInfo) SMPs.
*
* SOURCE
*/
#define IB_SMINFO_ATTR_MOD_HANDOVER		(CL_HTON32(0x000001))
/**********/

/****d* IBA Base: Constants/IB_SMINFO_ATTR_MOD_ACKNOWLEDGE
* NAME
*	IB_SMINFO_ATTR_MOD_ACKNOWLEDGE
*
* DESCRIPTION
*	Encoded attribute modifier value used on SubnSet(SMInfo) SMPs.
*
* SOURCE
*/
#define IB_SMINFO_ATTR_MOD_ACKNOWLEDGE		(CL_HTON32(0x000002))
/**********/

/****d* IBA Base: Constants/IB_SMINFO_ATTR_MOD_DISABLE
* NAME
*	IB_SMINFO_ATTR_MOD_DISABLE
*
* DESCRIPTION
*	Encoded attribute modifier value used on SubnSet(SMInfo) SMPs.
*
* SOURCE
*/
#define IB_SMINFO_ATTR_MOD_DISABLE			(CL_HTON32(0x000003))
/**********/

/****d* IBA Base: Constants/IB_SMINFO_ATTR_MOD_STANDBY
* NAME
*	IB_SMINFO_ATTR_MOD_STANDBY
*
* DESCRIPTION
*	Encoded attribute modifier value used on SubnSet(SMInfo) SMPs.
*
* SOURCE
*/
#define IB_SMINFO_ATTR_MOD_STANDBY			(CL_HTON32(0x000004))
/**********/

/****d* IBA Base: Constants/IB_SMINFO_ATTR_MOD_DISCOVER
* NAME
*	IB_SMINFO_ATTR_MOD_DISCOVER
*
* DESCRIPTION
*	Encoded attribute modifier value used on SubnSet(SMInfo) SMPs.
*
* SOURCE
*/
#define IB_SMINFO_ATTR_MOD_DISCOVER			(CL_HTON32(0x000005))
/**********/

/****s* Access Layer/ib_ci_op_t
* NAME
*	ib_ci_op_t
*
* DESCRIPTION
*	A structure used for vendor specific CA interface communication.
*
* SYNOPSIS
*/
typedef struct _ib_ci_op {
	IN uint32_t command;
	IN OUT void *p_buf OPTIONAL;
	IN uint32_t buf_size;
	IN OUT uint32_t num_bytes_ret;
	IN OUT int32_t status;
} ib_ci_op_t;
/*
* FIELDS
*	command
*		A command code that is understood by the verbs provider.
*
*	p_buf
*		A reference to a buffer containing vendor specific data.  The verbs
*		provider must not access pointers in the p_buf between user-mode and
*		kernel-mode.  Any pointers embedded in the p_buf are invalidated by
*		the user-mode/kernel-mode transition.
*
*	buf_size
*		The size of the buffer in bytes.
*
*	num_bytes_ret
*		The size in bytes of the vendor specific data returned in the buffer.
*		This field is set by the verbs provider.  The verbs provider should
*		verify that the buffer size is sufficient to hold the data being
*		returned.
*
*	status
*		The completion status from the verbs provider.  This field should be
*		initialize to indicate an error to allow detection and cleanup in
*		case a communication error occurs between user-mode and kernel-mode.
*
* NOTES
*	This structure is provided to allow the exchange of vendor specific
*	data between the originator and the verbs provider.  Users of this
*	structure are expected to know the format of data in the p_buf based
*	on the structure command field or the usage context.
*****/

/****s* IBA Base: Types/ib_cc_mad_t
* NAME
*	ib_cc_mad_t
*
* DESCRIPTION
*	IBA defined Congestion Control MAD format. (A10.4.1)
*
* SYNOPSIS
*/
#define IB_CC_LOG_DATA_SIZE 32
#define IB_CC_MGT_DATA_SIZE 192
#define IB_CC_MAD_HDR_SIZE (sizeof(ib_sa_mad_t) - IB_CC_LOG_DATA_SIZE \
						- IB_CC_MGT_DATA_SIZE)

#include <complib/cl_packon.h>
typedef struct _ib_cc_mad {
	ib_mad_t header;
	ib_net64_t cc_key;
	uint8_t log_data[IB_CC_LOG_DATA_SIZE];
	uint8_t mgt_data[IB_CC_MGT_DATA_SIZE];
} PACK_SUFFIX ib_cc_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	header
*		Common MAD header.
*
*	cc_key
*		CC_Key of the Congestion Control MAD.
*
*	log_data
*		Congestion Control log data of the CC MAD.
*
*	mgt_data
*		Congestion Control management data of the CC MAD.
*
* SEE ALSO
* ib_mad_t
*********/

/****f* IBA Base: Types/ib_cc_mad_get_cc_key
* NAME
*	ib_cc_mad_get_cc_key
*
* DESCRIPTION
*	Gets a CC_Key of the CC MAD.
*
* SYNOPSIS
*/
static inline ib_net64_t OSM_API
ib_cc_mad_get_cc_key(IN const ib_cc_mad_t * const p_cc_mad)
{
	return p_cc_mad->cc_key;
}
/*
* PARAMETERS
*	p_cc_mad
*		[in] Pointer to the CC MAD packet.
*
* RETURN VALUES
*	CC_Key of the provided CC MAD packet.
*
* NOTES
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****f* IBA Base: Types/ib_cc_mad_get_log_data_ptr
* NAME
*	ib_cc_mad_get_log_data_ptr
*
* DESCRIPTION
*	Gets a pointer to the CC MAD's log data area.
*
* SYNOPSIS
*/
static inline void * OSM_API
ib_cc_mad_get_log_data_ptr(IN const ib_cc_mad_t * const p_cc_mad)
{
	return ((void *)p_cc_mad->log_data);
}
/*
* PARAMETERS
*	p_cc_mad
*		[in] Pointer to the CC MAD packet.
*
* RETURN VALUES
*	Pointer to CC MAD log data area.
*
* NOTES
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****f* IBA Base: Types/ib_cc_mad_get_mgt_data_ptr
* NAME
*	ib_cc_mad_get_mgt_data_ptr
*
* DESCRIPTION
*	Gets a pointer to the CC MAD's management data area.
*
* SYNOPSIS
*/
static inline void * OSM_API
ib_cc_mad_get_mgt_data_ptr(IN const ib_cc_mad_t * const p_cc_mad)
{
	return ((void *)p_cc_mad->mgt_data);
}
/*
* PARAMETERS
*	p_cc_mad
*		[in] Pointer to the CC MAD packet.
*
* RETURN VALUES
*	Pointer to CC MAD management data area.
*
* NOTES
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_cong_info_t
* NAME
*	ib_cong_info_t
*
* DESCRIPTION
*	IBA defined CongestionInfo attribute (A10.4.3.3)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cong_info {
	uint8_t cong_info;
	uint8_t resv;
	uint8_t ctrl_table_cap;
} PACK_SUFFIX ib_cong_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	cong_info
*		Congestion control capabilities of the node.
*
*	ctrl_table_cap
*		Number of 64 entry blocks in the CongestionControlTable.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_cong_key_info_t
* NAME
*	ib_cong_key_info_t
*
* DESCRIPTION
*	IBA defined CongestionKeyInfo attribute (A10.4.3.4)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cong_key_info {
	ib_net64_t cc_key;
	ib_net16_t protect_bit;
	ib_net16_t lease_period;
	ib_net16_t violations;
} PACK_SUFFIX ib_cong_key_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	cc_key
*		8-byte CC Key.
*
*	protect_bit
*		Bit 0 is a CC Key Protect Bit, other 15 bits are reserved.
*
*	lease_period
*		How long the CC Key protect bit is to remain non-zero.
*
*	violations
*		Number of received MADs that violated CC Key.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_cong_log_event_sw_t
* NAME
*	ib_cong_log_event_sw_t
*
* DESCRIPTION
*	IBA defined CongestionLogEvent (SW) entry (A10.4.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cong_log_event_sw {
	ib_net16_t slid;
	ib_net16_t dlid;
	ib_net32_t sl;
	ib_net32_t time_stamp;
} PACK_SUFFIX ib_cong_log_event_sw_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	slid
*		Source LID of congestion event.
*
*	dlid
*		Destination LID of congestion event.
*
*	sl
*		4 bits - SL of congestion event.
*		rest of the bits are reserved.
*
*	time_stamp
*		Timestamp of congestion event.
*
* SEE ALSO
*	ib_cc_mad_t, ib_cong_log_t
*********/

/****s* IBA Base: Types/ib_cong_log_event_ca_t
* NAME
*	ib_cong_log_event_ca_t
*
* DESCRIPTION
*	IBA defined CongestionLogEvent (CA) entry (A10.4.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cong_log_event_ca {
	ib_net32_t local_qp_resv0;
	ib_net32_t remote_qp_sl_service_type;
	ib_net16_t remote_lid;
	ib_net16_t resv1;
	ib_net32_t time_stamp;
} PACK_SUFFIX ib_cong_log_event_ca_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	resv0_local_qp
*		bits [31:8] local QP that reached CN threshold.
*		bits [7:0] reserved.
*
*	remote_qp_sl_service_type
*		bits [31:8] remote QP that is connected to local QP.
*		bits [7:4] SL of the local QP.
*		bits [3:0] Service Type of the local QP.
*
*	remote_lid
*		LID of the remote port that is connected to local QP.
*
*	time_stamp
*		Timestamp when threshold reached.
*
* SEE ALSO
*	ib_cc_mad_t, ib_cong_log_t
*********/

/****s* IBA Base: Types/ib_cong_log_t
* NAME
*	ib_cong_log_t
*
* DESCRIPTION
*	IBA defined CongestionLog attribute (A10.4.3.5)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cong_log {
	uint8_t log_type;
	union _log_details
	{
		struct _log_sw {
			uint8_t cong_flags;
			ib_net16_t event_counter;
			ib_net32_t time_stamp;
			uint8_t port_map[32];
			ib_cong_log_event_sw_t entry_list[15];
		} PACK_SUFFIX log_sw;

		struct _log_ca {
			uint8_t cong_flags;
			ib_net16_t event_counter;
			ib_net16_t event_map;
			ib_net16_t resv;
			ib_net32_t time_stamp;
			ib_cong_log_event_ca_t log_event[13];
		} PACK_SUFFIX log_ca;

	} log_details;
} PACK_SUFFIX ib_cong_log_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	log_{sw,ca}.log_type
*		Log type: 0x1 is for Switch, 0x2 is for CA
*
*	log_{sw,ca}.cong_flags
*		Congestion Flags.
*
*	log_{sw,ca}.event_counter
*		Number of events since log last sent.
*
*	log_{sw,ca}.time_stamp
*		Timestamp when log sent.
*
*	log_sw.port_map
*		If a bit set to 1, then the corresponding port
*		has marked packets with a FECN.
*		bits 0 and 255 - reserved
*		bits [254..1] - ports [254..1].
*
*	log_sw.entry_list
*		Array of 13 most recent congestion log events.
*
*	log_ca.event_map
*		array 16 bits, one for each SL.
*
*	log_ca.log_event
*		Array of 13 most recent congestion log events.
*
* SEE ALSO
*	ib_cc_mad_t, ib_cong_log_event_sw_t, ib_cong_log_event_ca_t
*********/

/****s* IBA Base: Types/ib_sw_cong_setting_t
* NAME
*	ib_sw_cong_setting_t
*
* DESCRIPTION
*	IBA defined SwitchCongestionSetting attribute (A10.4.3.6)
*
* SYNOPSIS
*/
#define IB_CC_PORT_MASK_DATA_SIZE 32
#include <complib/cl_packon.h>
typedef struct _ib_sw_cong_setting {
	ib_net32_t control_map;
	uint8_t victim_mask[IB_CC_PORT_MASK_DATA_SIZE];
	uint8_t credit_mask[IB_CC_PORT_MASK_DATA_SIZE];
	uint8_t threshold_resv;
	uint8_t packet_size;
	ib_net16_t cs_threshold_resv;
	ib_net16_t cs_return_delay;
	ib_net16_t marking_rate;
} PACK_SUFFIX ib_sw_cong_setting_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	control_map
*		Indicates which components of this attribute are valid
*
*	victim_mask
*		If the bit set to 1, then the port corresponding to
*		that bit shall mark packets that encounter congestion
*		with a FECN, whether they are the source or victim
*		of congestion. (See A10.2.1.1.1)
*		  bit 0: port 0 (enhanced port 0 only)
*		  bits [254..1]: ports [254..1]
*		  bit 255: reserved
*
*	credit_mask
*		If the bit set to 1, then the port corresponding
*		to that bit shall apply Credit Starvation.
*		  bit 0: port 0 (enhanced port 0 only)
*		  bits [254..1]: ports [254..1]
*		  bit 255: reserved
*
*	threshold_resv
*		bits [7..4] Indicates how aggressive cong. marking should be
*		bits [3..0] Reserved
*
*	packet_size
*		Any packet less than this size won't be marked with FECN
*
*	cs_threshold_resv
*		bits [15..12] How aggressive Credit Starvation should be
*		bits [11..0] Reserved
*
*	cs_return_delay
*		Value that controls credit return rate.
*
*	marking_rate
*		The value that provides the mean number of packets
*		between marking eligible packets with FECN.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_sw_port_cong_setting_element_t
* NAME
*	ib_sw_port_cong_setting_element_t
*
* DESCRIPTION
*	IBA defined SwitchPortCongestionSettingElement (A10.4.3.7)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_sw_port_cong_setting_element {
	uint8_t valid_ctrl_type_res_threshold;
	uint8_t packet_size;
	ib_net16_t cong_param;
} PACK_SUFFIX ib_sw_port_cong_setting_element_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	valid_ctrl_type_res_threshold
*		bit 7: "Valid"
*			when set to 1, indicates this switch
*			port congestion setting element is valid.
*		bit 6: "Control Type"
*			Indicates which type of attribute is being set:
*			0b = Congestion Control parameters are being set.
*			1b = Credit Starvation parameters are being set.
*		bits [5..4]: reserved
*		bits [3..0]: "Threshold"
*			When Control Type is 0, contains the congestion
*			threshold value (Threshold) for this port.
*			When Control Type is 1, contains the credit
*			starvation threshold (CS_Threshold) value for
*			this port.
*
*	packet_size
*		When Control Type is 0, this field contains the minimum
*		size of packets that may be marked with a FECN.
*		When Control Type is 1, this field is reserved.
*
*	cong_parm
*		When Control Type is 0, this field contains the port
*		marking_rate.
*		When Control Type is 1, this field is reserved.
*
* SEE ALSO
*	ib_cc_mad_t, ib_sw_port_cong_setting_t
*********/

/****d* IBA Base: Types/ib_sw_port_cong_setting_block_t
* NAME
*	ib_sw_port_cong_setting_block_t
*
* DESCRIPTION
*	Defines the SwitchPortCongestionSetting Block (A10.4.3.7).
*
* SOURCE
*/
#define IB_CC_SW_PORT_SETTING_ELEMENTS 32
typedef ib_sw_port_cong_setting_element_t ib_sw_port_cong_setting_block_t[IB_CC_SW_PORT_SETTING_ELEMENTS];
/**********/

/****s* IBA Base: Types/ib_sw_port_cong_setting_t
* NAME
*	ib_sw_port_cong_setting_t
*
* DESCRIPTION
*	IBA defined SwitchPortCongestionSetting attribute (A10.4.3.7)
*
* SYNOPSIS
*/

#include <complib/cl_packon.h>
typedef struct _ib_sw_port_cong_setting {
	ib_sw_port_cong_setting_block_t block;
} PACK_SUFFIX ib_sw_port_cong_setting_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	block
*		SwitchPortCongestionSetting block.
*
* SEE ALSO
*	ib_cc_mad_t, ib_sw_port_cong_setting_element_t
*********/

/****s* IBA Base: Types/ib_ca_cong_entry_t
* NAME
*	ib_ca_cong_entry_t
*
* DESCRIPTION
*	IBA defined CACongestionEntry (A10.4.3.8)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ca_cong_entry {
	ib_net16_t ccti_timer;
	uint8_t ccti_increase;
	uint8_t trigger_threshold;
	uint8_t ccti_min;
	uint8_t resv0;
	ib_net16_t resv1;
} PACK_SUFFIX ib_ca_cong_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	ccti_timer
*		When the timer expires it will be reset to its specified
*		value, and 1 will be decremented from the CCTI.
*
*	ccti_increase
*		The number to be added to the table Index (CCTI)
*		on the receipt of a BECN.
*
*	trigger_threshold
*		When the CCTI is equal to this value, an event
*		is logged in the CAs cyclic event log.
*
*	ccti_min
*		The minimum value permitted for the CCTI.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_ca_cong_setting_t
* NAME
*	ib_ca_cong_setting_t
*
* DESCRIPTION
*	IBA defined CACongestionSetting attribute (A10.4.3.8)
*
* SYNOPSIS
*/
#define IB_CA_CONG_ENTRY_DATA_SIZE 16
#include <complib/cl_packon.h>
typedef struct _ib_ca_cong_setting {
	ib_net16_t port_control;
	ib_net16_t control_map;
	ib_ca_cong_entry_t entry_list[IB_CA_CONG_ENTRY_DATA_SIZE];
} PACK_SUFFIX ib_ca_cong_setting_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	port_control
*		Congestion attributes for this port:
*		  bit0 = 0: QP based CC
*		  bit0 = 1: SL/Port based CC
*		All other bits are reserved
*
*	control_map
*		An array of sixteen bits, one for each SL. Each bit indicates
*		whether or not the corresponding entry is to be modified.
*
*	entry_list
*		List of 16 CACongestionEntries, one per SL.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_cc_tbl_entry_t
* NAME
*	ib_cc_tbl_entry_t
*
* DESCRIPTION
*	IBA defined CongestionControlTableEntry (A10.4.3.9)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_tbl_entry {
	ib_net16_t shift_multiplier;
} PACK_SUFFIX ib_cc_tbl_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	shift_multiplier
*		bits [15..14] - CCT Shift
*		  used when calculating the injection rate delay
*		bits [13..0] - CCT Multiplier
*		  used when calculating the injection rate delay
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_cc_tbl_t
* NAME
*	ib_cc_tbl_t
*
* DESCRIPTION
*	IBA defined CongestionControlTable attribute (A10.4.3.9)
*
* SYNOPSIS
*/
#define IB_CC_TBL_ENTRY_LIST_MAX 64
#include <complib/cl_packon.h>
typedef struct _ib_cc_tbl {
	ib_net16_t ccti_limit;
	ib_net16_t resv;
	ib_cc_tbl_entry_t entry_list[IB_CC_TBL_ENTRY_LIST_MAX];
} PACK_SUFFIX ib_cc_tbl_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	ccti_limit
*		Maximum valid CCTI for this table.
*
*	entry_list
*		List of up to 64 CongestionControlTableEntries.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

#define IB_CC_ENH_INFO_VER0_MASK (0x80)
#define IB_CC_ENH_INFO_VER1_MASK (0x40)
#define IB_CC_CAPABILITY_MASK_ALGO_CONFIG_SUPPORTED_BIT 2
#define IB_CC_CAPABILITY_MASK_GRANULARITY_SUPPORTED_BIT 3
#define IB_CC_CAPABILITY_MASK_DUAL_CC_KEY_SUPPORTED_BIT 5
#define IB_CC_CAPABILITY_MASK_PER_PLANE_SUPPORTED_BIT 6

/****s* IBA Base: Types/ib_cc_enh_info_t
* NAME
*	ib_cc_enh_info_t
*
* DESCRIPTION
*	IBA defined EnhancedCongestionInfo attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_enh {
	uint8_t ver_supported;
	uint8_t reserved;
	ib_net16_t reserved1;
	ib_net32_t reserved2;
	ib_net64_t cc_capability_mask;
} PACK_SUFFIX ib_cc_enh_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	ver_supported
*		bit 7: Version 0 of Congestion Control attributes is supported
*
*		bit 6: Version 1 of Congestion Control attributes is supported
*
*	cc_capability_mask
*		bit 0: reserved.
*
*		bit 1: HCA Counters - Congestion HCA Statistics Query - is supported
*
* 		bit 2: Congetion HCA AlgoConfig/ConfigParam/counters - are supported by the device.
*
* 		bit 3: CongestionPortProfileSettings.granularity bit is supported.
*		
*		bit 6: CongestionHCAGeneralSettings.en_cc_per_plane is supported.
*
*		bits [63:7]: Reserved.
*
* SEE ALSO
*
*********/

static inline
boolean_t ib_cc_get_enh_info_ver0(IN ib_cc_enh_info_t * p_cc_enh_info)
{
	return (p_cc_enh_info->ver_supported & IB_CC_ENH_INFO_VER0_MASK);
}

static inline boolean_t OSM_API
ib_cc_get_enh_info_ver1(IN ib_cc_enh_info_t * p_cc_enh_info)
{
	return (p_cc_enh_info->ver_supported & IB_CC_ENH_INFO_VER1_MASK);
}

/****f* IBA Base: Types/ib_cc_enh_info_is_cap_supported
* NAME
*	ib_cc_enh_info_is_cap_supported
*
* DESCRIPTION
*	Check whether a given capability is supported
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_cc_enh_info_is_cap_supported(IN const ib_cc_enh_info_t * const p_cc_enh_info,
				IN const uint8_t cap_bit)
{
	uint64_t bit_in_mask = (uint64_t) 1 << (cap_bit % 64);

	return ((cl_ntoh64(p_cc_enh_info->cc_capability_mask) & bit_in_mask) != 0);
}

/****f* IBA Base: Types/ib_cc_enh_info_is_dual_key_supported
* NAME
*	ib_cc_enh_info_is_dual_key_supported
*
* DESCRIPTION
*	Check whether a dual key capability is supported
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_cc_enh_info_is_dual_key_supported(IN const ib_cc_enh_info_t * const p_cc_enh_info)
{
	return ib_cc_enh_info_is_cap_supported(p_cc_enh_info, IB_CC_CAPABILITY_MASK_DUAL_CC_KEY_SUPPORTED_BIT);
}

/****f* IBA Base: Types/ib_cc_enh_info_is_per_plane_supported
* NAME
*	ib_cc_enh_info_is_per_plane_supported
*
* DESCRIPTION
*	Check whether a cc per plane capability is supported
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_cc_enh_info_is_per_plane_supported(IN const ib_cc_enh_info_t * const p_cc_enh_info)
{
	return ib_cc_enh_info_is_cap_supported(p_cc_enh_info, IB_CC_CAPABILITY_MASK_PER_PLANE_SUPPORTED_BIT);
}

#define IB_CC_SW_SETTINGS_CC_ENABLE_MASK (0x01)
/****s* IBA Base: Types/ib_cc_sw_general_settings_t
* NAME
*	ib_cc_sw_general_settings_t
*
* DESCRIPTION
*	IBA defined CongestionSwitchGeneralSettings attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_sw_general_settings {
	uint8_t enable;
	uint8_t reserved;
	uint8_t aqs_weight;
	uint8_t aqs_time;
	ib_net32_t cap_total_buffer_size;
	ib_net16_t reserved1;
	uint8_t reserved2;
	uint8_t cap_cc_profile_step_size;
} PACK_SUFFIX ib_cc_sw_general_settings_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
* 	enable
* 		bit 2: CC enable
* 		  All other bits are reserved
*
*	aqs_weight
*		Average queue size weigh
*		The average queue size is calculated by:
*			((current_q_size) * 2^-aqs_weight) +
*			((previous_average_q_size) * (1 - 2^-aqs_weight))
*
*	aqs_time
*		Average queue size time.
*		The time is 2^aws_time*64 nsec. Valid range is 0-24
*
*	cap_total_buffer_size
*		Total buffer size in the device in Bytes
*
*	cap_cc_profile_step_size
*		The profile allowed step size in the device in KB.
*		The cap is calculated by:
*			cap_cc_profile_step_size * 1K Bytes.
*
* SEE ALSO
*
*********/

static inline void OSM_API
ib_cc_sw_set_enable_cc(IN ib_cc_sw_general_settings_t * p_cc_sw_settings, IN uint8_t enable)
{
	p_cc_sw_settings->enable =
	    ((p_cc_sw_settings->enable & ~IB_CC_SW_SETTINGS_CC_ENABLE_MASK) | enable);
}

static inline uint8_t OSM_API
ib_cc_sw_get_enable_cc(IN const ib_cc_sw_general_settings_t * const p_cc_sw_settings)
{
	return !!(p_cc_sw_settings->enable & IB_CC_SW_SETTINGS_CC_ENABLE_MASK);
}

static inline ib_net32_t OSM_API
ib_cc_sw_get_cap_total_buffer_size(IN const ib_cc_sw_general_settings_t * const p_cc_sw_settings)
{
	return p_cc_sw_settings->cap_total_buffer_size;
}

static inline uint8_t OSM_API
ib_cc_sw_get_aqs_weight(IN const ib_cc_sw_general_settings_t * const p_cc_sw_settings)
{
	return p_cc_sw_settings->aqs_weight;
}

static inline uint8_t OSM_API
ib_cc_sw_get_cap_cc_profile_step_size(IN const ib_cc_sw_general_settings_t * const p_cc_sw_settings)
{
	return p_cc_sw_settings->cap_cc_profile_step_size;
}

#define IB_CC_PORT_PROFILE_SETTINGS_PERCENT_MASK (0x7f000000)
#define IB_CC_PORT_PROFILE_SETTINGS_MAX_MASK (0x000fffff)
#define IB_CC_PORT_PROFILE_SETTINGS_MAX_BITS (20)
#define IB_CC_PORT_PROFILE_SETTINGS_MAXIMAL_MAX_VALUE ((1 << IB_CC_PORT_PROFILE_SETTINGS_MAX_BITS) - 1)
#define IB_CC_PORT_PROFILE_SETTINGS_PERCENT_SHIFT (24)
#define IB_CC_PORT_PROFILE_SETTINGS_MODE_SHIFT (7)
#define IB_CC_PORT_PROFILE_SETTINGS_MODE_MASK (1 << IB_CC_PORT_PROFILE_SETTINGS_MODE_SHIFT)
#define IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_SHIFT (6)
#define IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_MASK (1 << IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_SHIFT)

/****s* IBA Base: Types/ib_cc_port_profile_t
* NAME
*	Types/ib_cc_port_profile_t
*
* DESCRIPTION
*	IBA defined CongestionPortProfileSettings attribute data member ()

* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_port_profile {
	ib_net32_t profile_min;
	ib_net32_t profile_percent_max;
} PACK_SUFFIX ib_cc_port_profile_t;
#include <complib/cl_packoff.h>
/*
*	profile_min
*		Minimum average queue size to mark from For Fixed mode:
*		Valid values are [0..cap_total_buffer_size]
*		For Percentage mode: Valid values are [0..100] and represent the quota of port.
*
*	profile_percent_max
*		bits [19..0]  - profile_max:
*			Maximum average queue size to mark from For Fixed mode:
*			Valid values are [0..cap_total_buffer_size] For Percentage mode:
*			Valid values are [0..100] and represent the quota of port.
*
*		bits [30..24] - profile_percent:
*			Percentage of marking for profile_max Default value is 0.
*/

/****d* IBA Base: Constants/IB_CC_PORT_PROFILES_BLOCK_SIZE
* NAME
*	IB_CC_PORT_PROFILES_BLOCK_SIZE
*
* DESCRIPTION
*	Number of port profiles entries in a congestion control port profiles
*	settings MAD.
*
* SOURCE
*/
#define IB_CC_PORT_PROFILES_BLOCK_SIZE 3
/*********/

/****s* IBA Base: Types/ib_cc_port_profile_settings_t
* NAME
*	Types/ib_cc_port_profile_settings_t
*
* DESCRIPTION
*	IBA defined CongestionPortProfileSettings attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_port_profile_settings {
	uint8_t specification_bitmap;
	uint8_t reserved;
	ib_net16_t vl_mask;
	ib_cc_port_profile_t profiles[IB_CC_PORT_PROFILES_BLOCK_SIZE];
} PACK_SUFFIX ib_cc_port_profile_settings_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	specification_bitmap
*		bit 7 (mode): 0 - Fixed
*		              1 - Percentage
*		bit 6 (granularity): 0 - Bytes
*		                     1 - KB
*
*	vl_mask
* 		VL bit mask. If set to 0xFFFF, all VLs are chosen.
*
*		All other bits are reserved.
*
* SEE ALSO
*
*********/

static inline
void ib_cc_port_profile_set_mode(IN ib_cc_port_profile_settings_t * p_cc_port_profile_settings,
				 IN uint8_t mode)
{
	p_cc_port_profile_settings->specification_bitmap =
	    ((p_cc_port_profile_settings->specification_bitmap & ~IB_CC_PORT_PROFILE_SETTINGS_MODE_MASK) |
	     (mode << IB_CC_PORT_PROFILE_SETTINGS_MODE_SHIFT));
}

static inline
void ib_cc_port_profile_set_granularity(IN ib_cc_port_profile_settings_t * p_cc_port_profile_settings,
					IN uint8_t granularity)
{
	p_cc_port_profile_settings->specification_bitmap =
	    ((p_cc_port_profile_settings->specification_bitmap & ~IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_MASK) |
	     ((granularity << IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_SHIFT)
	      & IB_CC_PORT_PROFILE_SETTINGS_GRANULARITY_MASK));
}

static inline
void ib_cc_port_profiles_set_profile(IN ib_cc_port_profile_settings_t *p_cc_port_profile_settings,
				     IN uint8_t profile_num, IN uint32_t min,
				     IN uint32_t max, IN uint8_t percent)
{
	uint32_t profile_percent_max_ho =
	    (max & IB_CC_PORT_PROFILE_SETTINGS_MAX_MASK) |
	    ((percent << IB_CC_PORT_PROFILE_SETTINGS_PERCENT_SHIFT) &
	     IB_CC_PORT_PROFILE_SETTINGS_PERCENT_MASK);

	p_cc_port_profile_settings->profiles[profile_num].profile_min =
	    cl_hton32(min);
	p_cc_port_profile_settings->profiles[profile_num].profile_percent_max =
	    cl_hton32(profile_percent_max_ho);
}

/****s* IBA Base: Types/ib_cc_sl_mapping_settings_t
* NAME
*	ib_cc_sl_mapping_settings_t
*
* DESCRIPTION
*	IBA defined CongestionSLMappingSetting attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_sl_mapping_settings {
	ib_net32_t sl_profile_map;
} PACK_SUFFIX ib_cc_sl_mapping_settings_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	sl_profile_map
*		An array of 32 bits, two for each SL.
*		Each 2 bits are a Profile Number for an SL.
*		bits [31-30]: Profile Number for SL number 0.
*			      Valid numbers are 1-3. 0 is reserved.
*
*		bits [29-28]: Profile Number for SL number 1
*		       ...
*		       ...
*		       ...
*		bits [1-0]: Profile Number for SL number 16
*
* SEE ALSO
*
*********/

static inline
void ib_cc_sl_set_profile_num(IN ib_cc_sl_mapping_settings_t *p_sl_mapping,
			      IN uint8_t sl_num, IN uint8_t profile_num)
{
	uint32_t shift = (15 - sl_num) * 2;
	uint32_t sl_profile_map_ho = cl_ntoh32(p_sl_mapping->sl_profile_map);

	sl_profile_map_ho = (sl_profile_map_ho & ~(3 << shift)) | (uint32_t)(profile_num << shift);
	p_sl_mapping->sl_profile_map = cl_hton32(sl_profile_map_ho);
}

/****s* IBA Base: Types/ib_cc_hca_general_settings_t
* NAME
*	ib_cc_hca_general_settings_t
*
* DESCRIPTION
*	IBA defined CongestionHCAGeneralSettings attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_hca_general_settings {
	uint8_t enable_mask;
} PACK_SUFFIX ib_cc_hca_general_settings_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	enable_mask
*		bit 8: en_notify - Enable notifications on HCA Congestion Control features
*
*		bit 7: en_react - Enable reactions of HCA Congestion Control features and MADs
*
*		bit 6: en_per_plane - Enable a seperate cc context per plane
*
*		All other bits are reserved
*
* SEE ALSO
*
*********/

#define IB_CC_HCA_SETTINGS_EN_PER_PLANE_SHIFT (5)
#define IB_CC_HCA_SETTINGS_EN_PER_PLANE_MASK (1 << IB_CC_HCA_SETTINGS_EN_PER_PLANE_SHIFT)
#define IB_CC_HCA_SETTINGS_EN_REACT_SHIFT (6)
#define IB_CC_HCA_SETTINGS_EN_REACT_MASK  (1 << IB_CC_HCA_SETTINGS_EN_REACT_SHIFT)
#define IB_CC_HCA_SETTINGS_EN_NOTIFY_SHIFT (7)
#define IB_CC_HCA_SETTINGS_EN_NOTIFY_MASK (1 << IB_CC_HCA_SETTINGS_EN_NOTIFY_SHIFT)

static inline
uint8_t ib_cc_hca_get_en_notify(IN const ib_cc_hca_general_settings_t * const p_cc_hca_settings)
{
	return !!(p_cc_hca_settings->enable_mask & IB_CC_HCA_SETTINGS_EN_NOTIFY_MASK);
}

static inline
void ib_cc_hca_set_en_notify(IN ib_cc_hca_general_settings_t * p_cc_hca_settings, uint8_t en_notify)
{
	p_cc_hca_settings->enable_mask =
	    ((p_cc_hca_settings->enable_mask & ~(IB_CC_HCA_SETTINGS_EN_NOTIFY_MASK)) |
	     (en_notify << IB_CC_HCA_SETTINGS_EN_NOTIFY_SHIFT));
}

static inline
uint8_t ib_cc_hca_get_en_react(IN const ib_cc_hca_general_settings_t * const p_cc_hca_settings)
{
	return !!(p_cc_hca_settings->enable_mask & IB_CC_HCA_SETTINGS_EN_REACT_MASK);
}

static inline
void ib_cc_hca_set_en_react(IN ib_cc_hca_general_settings_t * p_cc_hca_settings, uint8_t en_react)
{
	p_cc_hca_settings->enable_mask =
	    ((p_cc_hca_settings->enable_mask & ~(IB_CC_HCA_SETTINGS_EN_REACT_MASK)) |
	     (en_react << IB_CC_HCA_SETTINGS_EN_REACT_SHIFT));
}

static inline
uint8_t ib_cc_hca_get_en_per_plane(IN const ib_cc_hca_general_settings_t * const p_cc_hca_settings)
{
	return !!(p_cc_hca_settings->enable_mask & IB_CC_HCA_SETTINGS_EN_PER_PLANE_MASK);
}

static inline
void ib_cc_hca_set_en_per_plane(IN ib_cc_hca_general_settings_t * p_cc_hca_settings, uint8_t en_per_plane)
{
	p_cc_hca_settings->enable_mask =
	    ((p_cc_hca_settings->enable_mask & ~(IB_CC_HCA_SETTINGS_EN_PER_PLANE_MASK)) |
	     (en_per_plane << IB_CC_HCA_SETTINGS_EN_PER_PLANE_SHIFT));
}


/****s* IBA Base: Types/ib_cc_hca_rp_parameters_t
* NAME
*	ib_cc_hca_rp_parameters_t
*
* DESCRIPTION
*	IBA defined CongestionHCARPParameters attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_hca_rp_parameters {
	ib_net16_t clamp_tgt_rate;
	ib_net16_t reserved;
	ib_net32_t rpg_time_reset;
	ib_net32_t rpg_byte_reset;
	uint8_t reserved1[3];
	uint8_t rpg_threshold;
	ib_net32_t rpg_max_rate;
	ib_net16_t reserved3;
	ib_net16_t rpg_ai_rate;
	ib_net16_t reserved4;
	ib_net16_t rpg_hai_rate;
	uint8_t reserved5[3];
	uint8_t rpg_gd;
	uint8_t reserved6[3];
	uint8_t rpg_min_dec_fac;
	ib_net32_t rpg_min_rate;
	ib_net32_t rate_to_set_on_first_cnp;
	ib_net16_t reserved8;
	ib_net16_t dce_tcp_g;
	ib_net32_t dce_tcp_rtt;
	ib_net32_t rate_reduce_monitor_period;
	ib_net16_t reserved9;
	ib_net16_t initial_alpha_value;
} PACK_SUFFIX ib_cc_hca_rp_parameters_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	clamp_tgt_rate
*		bit 16: clamp_tgt_rate - If set, whenever a CNP is processed,
*					 the target rate is updated to be the current rate
*
*		bit 15: clamp_tgt_rate_after_time_in - When receiving a CNP,
*				the target rate should be updated if the transmission
*				rate was increased due to the timer,
*				and not only due to the byte counter
*		All other bits are reserved
*
*	rpg_time_reset
*		bits [16..0]: Time period between rate increase events in usec
*		All other bits are reserved
*
*	rpg_byte_reset
*		bits [18..0]: The sent byte counter between rate increase events
*		All other bits are reserved
*
*	rpg_threshold
*		bits [4..0]: Threshold of rate increase events for moving
*			     to next rate increase phase
*		All other bits are reserved
*
*	rpg_max_rate
*		bits [18..0]: Defines the maximal rate limit for QP in Mbit/sec
*
*	rpg_ai_rate
*		Rate increase value in the Active Increase phase in Mbit/s
*
*	rpg_hai_rate
*		Rate increase value in the Hyper-active Increase phase in Mbit/s
*
*	rpg_gd
*		bits [3..0]: Coefficient between alpha and rate reduction factor (2^rpg_gd)
*		All other bits are reserved
*
*	rpg_min_dec_fac
*		Defines the max ratio of rate decrease in a single event, in percentage
*
*	rpg_min_rate
*		bits [18..0]: Defines the minimal rate limit for QP in Mbit/sec
*
*	rate_to_set_on_first_cnp
*		Rate on receiving the first CNP in Mbit/sec
*
*	dce_tcp_g
*		bits [9..0]: Parameter controlling alpha
*		All other bits are reserved
*
*	dce_tcp_rtt
*		bits [16..0]: Parameter controlling alpha
*		All other bits are reserved
*
*	rate_reduce_monitor_period
*		Time period between rate reductions in usecs
*
*	initial_alpha_value
*		bits [9..0]: Initial value of alpha parameter that should be
*			     used on receiving the first CNP
*			     (expressed in a fixed-point fraction of 2^10)
*		All other bits are reserved
*
* SEE ALSO
*
*********/

#define IB_CC_HCA_RP_PARAMS_TGT_RATE_SHIFT (15)
#define IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_SHIFT (14)
#define IB_CC_HCA_RP_PARAMS_TGT_RATE_MASK CL_HTON16(1 << IB_CC_HCA_RP_PARAMS_TGT_RATE_SHIFT)
#define IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_MASK CL_HTON16(1 << IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_SHIFT)

static inline
void ib_cc_hca_rp_params_set_tgt_rate(IN ib_cc_hca_rp_parameters_t *p_hca_params,
				      IN uint8_t tgt_rate)
{
	p_hca_params->clamp_tgt_rate =
	    (p_hca_params->clamp_tgt_rate & ~(IB_CC_HCA_RP_PARAMS_TGT_RATE_MASK)) |
	    cl_hton16((uint16_t)(tgt_rate << IB_CC_HCA_RP_PARAMS_TGT_RATE_SHIFT));
}

static inline
void ib_cc_hca_rp_params_set_tgt_rate_at(IN ib_cc_hca_rp_parameters_t *p_hca_params,
					 IN uint8_t tgt_rate_after_time)
{
	p_hca_params->clamp_tgt_rate =
	    (p_hca_params->clamp_tgt_rate & ~(IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_MASK)) |
	    cl_hton16((uint16_t)(tgt_rate_after_time << IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_SHIFT));
}

static inline
uint8_t ib_cc_hca_rp_params_get_tgt_rate(IN ib_cc_hca_rp_parameters_t *p_hca_params)
{
	return ((cl_ntoh16(p_hca_params->clamp_tgt_rate) &
		IB_CC_HCA_RP_PARAMS_TGT_RATE_MASK) >> IB_CC_HCA_RP_PARAMS_TGT_RATE_SHIFT);
}

static inline
uint8_t ib_cc_hca_rp_params_get_tgt_rate_at(IN ib_cc_hca_rp_parameters_t *p_hca_params)
{
	return ((cl_ntoh16(p_hca_params->clamp_tgt_rate) &
		IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_MASK) >> IB_CC_HCA_RP_PARAMS_TGT_RATE_AFTER_TIME_SHIFT);
}

/****s* IBA Base: Types/ib_cc_hca_np_parameters_t
* NAME
*	ib_cc_hca_np_parameters_t
*
* DESCRIPTION
*	IBA defined CongestionHCANPParameters attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_cc_hca_np_parameters {
	ib_net16_t reserved;
	ib_net16_t min_time_between_cnps;
	uint8_t reserved1;
	uint8_t sl_mode;
	uint8_t rtt_resp_sl;
	uint8_t cnp_sl;
} PACK_SUFFIX ib_cc_hca_np_parameters_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	min_time_between_cnps
*		Minimum time between sending two adjacent CNPs. Time in usec.
*
*	sl_mode
*		bit 0: CNP SL choose mode.
*		       0: Take SL from the received packet
*		       1: Determine the SL number by cnp_s
*		bit 1: RTT response mode.
*		       0: Take SL from the received RTT packet
*		       1: Determine the SL from the rtt_resp_sl
*
*	rtt_resp__sl
*		bits[3..0]: The SL number of the generated RTT response for this port
*
*	cnp_sl
*		bits[3..0]: SL number
*
* SEE ALSO
*
*********/

#define IB_CC_HCA_NP_PARAMS_CNP_SL_MODE_SHIFT (0)
#define IB_CC_HCA_NP_PARAMS_CNP_SL_MODE_MASK (1 << IB_CC_HCA_NP_PARAMS_CNP_SL_MODE_SHIFT)
#define IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_SHIFT (1)
#define IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_MASK (1 << IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_SHIFT)

static inline
uint8_t ib_cc_hca_get_cnp_sl_mode(IN ib_cc_hca_np_parameters_t *p_hca_np_params)
{
	return !!(p_hca_np_params->sl_mode & IB_CC_HCA_NP_PARAMS_CNP_SL_MODE_MASK);
}

static inline
void ib_cc_hca_set_cnp_sl_mode(IN ib_cc_hca_np_parameters_t *p_hca_np_params, uint8_t cnp_sl_mode)
{
	p_hca_np_params->sl_mode =
	    ((p_hca_np_params->sl_mode & ~(IB_CC_HCA_NP_PARAMS_CNP_SL_MODE_MASK)) | cnp_sl_mode);
}

static inline
uint8_t ib_cc_hca_get_rtt_sl_mode(IN ib_cc_hca_np_parameters_t *p_hca_np_params)
{
	return !!(p_hca_np_params->sl_mode & IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_MASK);
}

static inline
void ib_cc_hca_set_rtt_sl_mode(IN ib_cc_hca_np_parameters_t *p_hca_np_params, uint8_t rtt_sl_mode)
{
	p_hca_np_params->sl_mode =
	    ((p_hca_np_params->sl_mode & ~(IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_MASK)) |
	     (rtt_sl_mode << IB_CC_HCA_NP_PARAMS_RTT_SL_MODE_SHIFT));
}

#define PPCC_ENCAPSULATION_SIZE 	44
#define MAX_PPCC_ALGO_SLOTS		16
#define PPCC_RESERVED_ENCAP_SIZE	((PPCC_ENCAPSULATION_SIZE) - (MAX_PPCC_ALGO_SLOTS))

/****s* IBA Base: Types/ib_ppcc_algo_info_t
* NAME
*	Types/ib_ppcc_algo_info_t
*
* DESCRIPTION
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ppcc_algo_info {
	ib_net16_t algo_id;
	uint8_t algo_major_version;
	uint8_t algo_minor_version;
} PACK_SUFFIX ib_ppcc_algo_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	algo_id
*		Algorithm ID
*
*	algo_major_version
*		Algorithm major version
*
*	algo_minor_version
*		Algorithm minor version
*
* SEE ALSO
*
*********/

/****s* IBA Base: Types/ib_ppcc_hca_algo_config_t
* NAME
*	Types/ib_ppcc_hca_algo_config_t
*
* DESCRIPTION
*	IBA defined CongestionHCAlgoConfig attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ppcc_hca_algo_config {
	uint8_t enable_mask;
	uint8_t resesved;
	ib_net16_t reserved1;
	ib_net16_t sl_bit_mask;
	uint8_t encap_len;
	uint8_t encap_type;
	ib_net64_t resesved2;
	union _encapsualtion {
		struct _encap_ascii {
			uint32_t algo_info_text[PPCC_ENCAPSULATION_SIZE];
		} PACK_SUFFIX ascii;
		struct _encap_algo_info {
			ib_ppcc_algo_info_t algo_info[MAX_PPCC_ALGO_SLOTS];
			uint32_t reserved[PPCC_RESERVED_ENCAP_SIZE];
		} PACK_SUFFIX encap_algo_info;
	} PACK_SUFFIX encapsulation;
} PACK_SUFFIX ib_ppcc_hca_algo_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	enable_bits
*		bit 8: algo_en - When set, enables algorithm defined by algo_slot on defined port, SL.
*		       Else, disables algorithm defined by algo_slot;
*
*		bit 7: algo_status - When set, indicates algorithm is enabled on defined port, SLs.
*
*		bit 6: trace_en - When set, enables algorithm trace mechanism. Else, disables it.
*
*		bit 5: counter_en - When set, enables PCC algorithm specific counters. Else, disables it.
*		       Each port supports one set of counters, e.g. The counters are supported only
*		       on one specific set of [algo_slot, sl_bitmask].
*		       When set, only the latest configuration set will be monitored
*		       and all the latest will be cleared.
*
*	sl_bit_mask
*		SL Bit mask that maps the SLs for the selected algo_slot. This field selects which SLs
*		need to be configured. On Get() method, only one SL shall be queried at a time.
*		Each SL on a port shall be mapped only to one algo_slot.
*
*	encap_len
*		Encapsulation length in Bytes. When encap_type = 2, the granularity shall be of 4 Bytes.
*
*	encap_type
*		Type of encapsulation:
*		0: No encapsulation
*		1: Algorithm Information in Free ASCII text
*		2: Supported Algorithm Information
*
*	encapsulation
*		Encapsulation based on type of encapsulation:
*		0: No encapsulation
*		1: Free ASCII text - Algorithm Information in Free ASCII text
*		2: Supported Algorithm information - Parameters encapsulation
*
* SEE ALSO
*
*********/

#define IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_SHIFT 		(7)
#define IB_PPCC_HCA_ALGO_CONFIG_ALGO_STATUS_SHIFT 	(6)
#define IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_SHIFT		(5)
#define IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_SHIFT 	(4)
#define IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_MASK 		(1 << IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_SHIFT)
#define IB_PPCC_HCA_ALGO_CONFIG_ALGO_STATUS_MASK 	(1 << IB_PPCC_HCA_ALGO_CONFIG_ALGO_STATUS_SHIFT)
#define IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_MASK 		(1 << IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_SHIFT)
#define IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_MASK 	(1 << IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_SHIFT)

static inline
boolean_t ib_ppcc_get_hca_algo_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf)
{
	return !!(p_ppcc_hca_algo_conf->enable_mask & IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_MASK);
}

static inline
boolean_t ib_ppcc_get_hca_algo_status(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf)
{
	return !!(p_ppcc_hca_algo_conf->enable_mask & IB_PPCC_HCA_ALGO_CONFIG_ALGO_STATUS_MASK);
}

static inline
boolean_t ib_ppcc_get_hca_trace_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf)
{
	return !!(p_ppcc_hca_algo_conf->enable_mask & IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_MASK);
}

static inline
boolean_t ib_ppcc_get_hca_counter_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf)
{
	return !!(p_ppcc_hca_algo_conf->enable_mask & IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_MASK);
}

static inline
void ib_ppcc_set_hca_algo_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf, uint8_t algo_en)
{
	p_ppcc_hca_algo_conf->enable_mask =
		((p_ppcc_hca_algo_conf->enable_mask & ~(IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_MASK)) |
		 (algo_en << IB_PPCC_HCA_ALGO_CONFIG_ALGO_EN_SHIFT));
}

static inline
void ib_ppcc_set_hca_trace_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf, uint8_t trace_en)
{
	p_ppcc_hca_algo_conf->enable_mask =
		((p_ppcc_hca_algo_conf->enable_mask & ~(IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_MASK)) |
		 (trace_en << IB_PPCC_HCA_ALGO_CONFIG_TRACE_EN_SHIFT));
}

static inline
void ib_ppcc_set_hca_counter_en(IN ib_ppcc_hca_algo_config_t *p_ppcc_hca_algo_conf, uint8_t counter_en)
{
	p_ppcc_hca_algo_conf->enable_mask =
		((p_ppcc_hca_algo_conf->enable_mask & ~(IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_MASK)) |
		 (counter_en << IB_PPCC_HCA_ALGO_CONFIG_COUNTER_EN_SHIFT));
}

/****s* IBA Base: Types/ib_ppcc_hca_config_param_t
* NAME
*	Types/ib_ppcc_hca_config_param_t
*
* DESCRIPTION
*	IBA defined CongestionHCAConfigParam attribute ()
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ppcc_hca_config_param {
	ib_net32_t reserved;
	ib_net16_t sl_bit_mask;
	uint8_t encap_len;
	uint8_t encap_type;
	ib_net64_t resesved2;
	uint32_t encapsulation[PPCC_ENCAPSULATION_SIZE];
} PACK_SUFFIX ib_ppcc_hca_config_param_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	sl_bit_mask
*		SL Bit mask - This field reflects which SLs are configured by these parameters.
*		Reserved on Set() method.
*
*	encap_len
*		Encapsulation length in Bytes. When encap_type = 2,
*		the granularity shall be of 4 Bytes.
*		Num ber ofparam eters = encap_len /4.
*
*	encap_type
*		Type of encapsulation:
*		0: No encapsulation
*		2: Algorithm configuration parameters
*
*	encapsulation
*		Encapsulation based on type of encapsulation:
*		0: No encapsulation
*		2: Algorithm configuration parameters
*
* SEE ALSO
*
*********/

/****s* IBA Base: Types/ib_time_stamp_t
* NAME
*	ib_time_stamp_t
*
* DESCRIPTION
*	IBA defined TimeStamp attribute (A10.4.3.10)
*
* SOURCE
*/
#include <complib/cl_packon.h>
typedef struct _ib_time_stamp {
	ib_net32_t value;
} PACK_SUFFIX ib_time_stamp_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	value
*		Free running clock that provides relative time info
*		for a device. Time is kept in 1.024 usec units.
*
* SEE ALSO
*	ib_cc_mad_t
*********/

/****s* IBA Base: Types/ib_vport_info_t
* NAME
*	ib_vport_info_t
*
* DESCRIPTION
*	IBA defined VPortInfo. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vport_info {
	uint8_t state_rereg;
	uint8_t guid_cap;
	ib_net16_t cap_mask;
	ib_net16_t p_key_violations;
	ib_net16_t q_key_violations;
	ib_net16_t vport_lid;
	ib_net16_t lid_by_vport_index;
	ib_net64_t port_guid;
	ib_net64_t port_profile;
	uint8_t resvd2[36];
} PACK_SUFFIX ib_vport_info_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	state_rereg
*		bits [3..0] - the VPort state.
*		bit 4 - Used by SM to request VPort endnode clients reregistration of SA subscriptions.
*		bit 5 - Is Lid required indication
*		bits [7..6] - reserved.
*
*	guid_cap
*		Number of GUID entries supported in the VCAPortGUIDInfo attribute for this virtual VCAPort.
*
*	cap_mask
*		Supported capabilities of this port. A bit set to 1 for affirmation of supported capability.
*
*	p_key_violations
*		the number of packets that have been received at this VPort that have had
*		invalid P_Keys, since power-on or reset.
*
*	q_key_violations
*		the number of packets that have been received at this port that have had
*		invalid Q_Keys, since power-on or reset.
*
*	vport_lid
*		The lid assigned for this vport by SM.
*
*	lid_by_vport_index
*		The index of the vport which its lid is being used by this vport
*		(in case that lid is not required)
*
*	port_guid
*		GUID of this VPort itself.
*
*	port_profile
*		VCAPort profile.
*
*/


#define IB_VPORT_INFO_CLIENT_REREGISTER_SHIFT		3
#define IB_VPORT_INFO_STATE_MASK			0xF0
#define IB_VPORT_INFO_STATE_SHIFT			4
#define IB_VPORT_INFO_LID_REQUIRED_SHIFT		2
#define IB_VPORT_INFO_CLIENT_REREGISTER_BIT		(1 << IB_VPORT_INFO_CLIENT_REREGISTER_SHIFT)
#define IB_VPORT_INFO_LID_REQUIRED_BIT			(1 << IB_VPORT_INFO_LID_REQUIRED_SHIFT)
#define IB_VPORT_INFO_CAP_IS_COMMUNICATION_MANAGEMENT_SUPPORTED		(CL_HTON16(0x0001))
#define IB_VPORT_INFO_CAP_IS_SNMP_TUNNELING_SUPPORTED			(CL_HTON16(0x0002))
#define IB_VPORT_INFO_CAP_IS_DEVICE_MANAGEMENT_SUPPORTED		(CL_HTON16(0x0004))
#define IB_VPORT_INFO_CAP_IS_VENDOR_CLASS_SUPPORTED			(CL_HTON16(0x0008))
#define IB_VPORT_INFO_CAP_IS_BOOT_MANAGEMENT_SUPPORTED			(CL_HTON16(0x0010))
#define IB_VPORT_INFO_CAP_IS_SM						(CL_HTON16(0x0020))
#define IB_VPORT_INFO_CAP_IS_SM_DISABLED				(CL_HTON16(0x0040))

/****d* IBA Base: Constants/IB_VPS_STATE_SIZE
* NAME
*	IB_VPS_STATE_SIZE
*
* DESCRIPTION
*	VPS (vport State) mad includes the state of block of vports
*	This constant defines what is the size in bits for each vport state
*
* SOURCE
*/
#define IB_VPS_STATE_SIZE 4

/****d* OpenSM: Constants/OSM_VPS_BLOCK_SIZE
 * Name
 *	OSM_VPS_BLOCK_SIZE
 *
 * DESCRIPTION
 *	VPS (VPort State) mad describes the state of a block of vports.
 *	This constant defines the number of vports in a vport state mad block.
 *	Note: IB_SMP_DATA_SIZE is given in bytes (need to convert to bits)
 *
 * SYNOPSIS
 */
#define OSM_VPS_BLOCK_SIZE ((IB_SMP_DATA_SIZE * 8)/IB_VPS_STATE_SIZE)

/****f* IBA Base: Types/ib_vport_info_get_vport_state
* NAME
*	ib_vport_info_get_vport_state
*
* DESCRIPTION
*	Returns the vport state.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_vport_info_get_vport_state(IN const ib_vport_info_t * const p_vpi)
{
	return ((uint8_t) (p_vpi->state_rereg) >> IB_VPORT_INFO_STATE_SHIFT);
}

/*
* PARAMETERS
*	p_vpi
*		[in] Pointer to a VPortInfo attribute.
*
* RETURN VALUES
*	VPort state.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_vport_info_set_vport_state
* NAME
*	ib_vport_info_set_vport_state
*
* DESCRIPTION
*	Sets the vport state.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vport_info_set_vport_state(IN ib_vport_info_t * const p_vpi,
			    IN const uint8_t vport_state)
{
	p_vpi->state_rereg = (uint8_t) ((p_vpi->state_rereg & ~IB_VPORT_INFO_STATE_MASK) |
				        (vport_state << IB_VPORT_INFO_STATE_SHIFT));
}

/****f* IBA Base: Types/ib_vport_info_get_lid_required
* NAME
*	ib_vport_info_get_lid_required
*
* DESCRIPTION
*	Returns the lid required.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_vport_info_get_lid_required(IN const ib_vport_info_t * const p_vpi)
{
    return ((uint8_t) (p_vpi->state_rereg & IB_VPORT_INFO_LID_REQUIRED_BIT) >>
          IB_VPORT_INFO_LID_REQUIRED_SHIFT);
}

/*
* PARAMETERS
*	p_vpi
*		[in] Pointer to a VPortInfo attribute.
*
* RETURN VALUES
*	Lid required.
*
* NOTES
*
* SEE ALSO
*********/

/*
* PARAMETERS
*	p_vpi
*		[in] Pointer to a VPortInfo attribute.
*
*	vport_state
*		[in] VPort state value to set (should be 0-4) .
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_vport_info_set_client_rereg
* NAME
*	ib_vport_info_set_client_rereg
*
* DESCRIPTION
*	Sets the encoded client reregistration bit value in the VPortInfo attribute.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vport_info_set_client_rereg(IN ib_vport_info_t * const p_vpi,
			      IN const uint8_t client_rereg)
{
	p_vpi->state_rereg =
	    (uint8_t) ((p_vpi->state_rereg & ~IB_VPORT_INFO_CLIENT_REREGISTER_BIT) |
		      (client_rereg << IB_VPORT_INFO_CLIENT_REREGISTER_SHIFT));
}

/*
* PARAMETERS
*	p_vpi
*		[in] Pointer to a VPortInfo attribute.
*
*	client_rereg
*		[in] Client reregistration value to set (either 1 or 0).
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_virtualization_info_t
* NAME
*	ib_virtualization_info_t
*
* DESCRIPTION
*	IBA defined VirtualizationInfo. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_virtualization_info {
	ib_net16_t vport_cap;
	ib_net16_t vport_index_top;
	uint8_t virt_enable_rereg;
	uint8_t resvd1;
	uint8_t virt_revision;
	uint8_t cap_mask;
	uint8_t resvd2[55];
} PACK_SUFFIX ib_virtualization_info_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	vport_cap
*		maximal supported vports on the port.
*
*	vport_index_top
* 		The highest vport index which is currently enabled.
*
*	virt_enable_rereg
*		bit 0 - VirtualizationEnable, support of optional Virtualization.
*		bit 1 - VClient_Reregistration, Used by SM to request endnode clients
*			on all VPorts of that physical port reregistration of SA subscriptions.
*		bit 2 - VPortStateChange, indicate whether the state of a vport that belongs to the
*			port has changed.
*		bits [7..2] - reserved.
*
*	resvd1
*		reserved.
*
*	virt_revision
*		Local SMA virtualization revision.
*
*	cap_mask
*		Optional Virtualization capabilities.
*
*	resvd2
*		reserved.
*
*
*/

#define IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_SHIFT	7
#define IB_VIRTUALIZATION_INFO_VCLIENT_REREGISTER_SHIFT		6
#define IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_SHIFT		5
#define IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_BIT 	0x80
#define IB_VIRTUALIZATION_INFO_VCLIENT_REREGISTER_BIT		0x40
#define IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_BIT		0x20


/****f* IBA Base: Types/ib_virtualization_info_set_virtualization_enabled
* NAME
*	ib_virtualization_info_set_virtualization_enabled
*
* DESCRIPTION
*	sets virtualization enabled bit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_virtualization_info_set_virtualization_enabled(IN ib_virtualization_info_t * const p_vi,
						  uint8_t virt_enabled)
{
	p_vi->virt_enable_rereg = ((p_vi->virt_enable_rereg & ~IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_BIT) |
				   virt_enabled << IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_SHIFT);
}

/*
* PARAMETERS
*	p_vi
*		[in] Pointer to a VirtualizationInfo attribute.
*
*	virt_enabled
*		[in] Virtualization enabled value to set (either 1 or 0).
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_virtualization_info_get_virtualization_enabled
* NAME
*	ib_virtualization_info_get_virtualization_enabled
*
* DESCRIPTION
*	Return the value of Virtualization Enabled bit.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_virtualization_info_get_virtualization_enabled(IN const ib_virtualization_info_t * const p_vi)
{
	return ((p_vi->virt_enable_rereg & IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_BIT) >>
		IB_VIRTUALIZATION_INFO_VIRTUALIZATION_ENABLE_SHIFT);
}

/*
* PARAMETERS
*	p_vi
*		[in] Pointer to a VirtualizationInfo attribute.
*
*
* RETURN VALUES
*	the value of Virtualization Enabled bit (either 1 or 0).
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_virtualization_info_set_vclient_rereg
* NAME
*	ib_virtualization_info_set_vclient_rereg
*
* DESCRIPTION
*	sets VClient reregistration bit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_virtualization_info_set_vclient_rereg(IN ib_virtualization_info_t * const p_vi,
					 uint8_t vclient_rereg)
{
	p_vi->virt_enable_rereg =
		((p_vi->virt_enable_rereg & ~IB_VIRTUALIZATION_INFO_VCLIENT_REREGISTER_BIT) |
		(vclient_rereg << IB_VIRTUALIZATION_INFO_VCLIENT_REREGISTER_SHIFT));
}

/*
* PARAMETERS
*	p_vi
*		[in] Pointer to a VirtualizationInfo attribute.
*
*	virt_enabled
*		[in] VClient reregistration value to set (either 1 or 0).
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_virtualization_info_set_vport_state_change
 * * NAME
 * *       ib_virtualization_info_set_vport_state_change
 * *
 * * DESCRIPTION
 * *       sets VPort State Change bit.
 * *
 * * SYNOPSIS
 * */
static inline void OSM_API
ib_virtualization_info_set_vport_state_change(IN ib_virtualization_info_t * const p_vi,
	                                      uint8_t vport_state_change)
{
        p_vi->virt_enable_rereg =
                ((p_vi->virt_enable_rereg & ~IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_BIT) |
                (vport_state_change << IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_SHIFT));
}

/*
 * * PARAMETERS
 * *       p_vi
 * *               [in] Pointer to a VirtualizationInfo attribute.
 * *
 * *       virt_enabled
 * *               [in] VPort State Change value to set (either 1 or 0).
 * *
 * * RETURN VALUES
 * *       None.
 * *
 * * NOTES
 * *
 * * SEE ALSO
 * *********/

/****f* IBA Base: Types/ib_virtualization_info_get_vport_state_change
 * * NAME
 * *       ib_virtualization_info_get_vport_state_change
 * *
 * * DESCRIPTION
 * *       Return the value of VPort State Change bit.
 * *
 * * SYNOPSIS
 * */
static inline uint8_t OSM_API
ib_virtualization_info_get_vport_state_change(IN const ib_virtualization_info_t * const p_vi)
{
        return ((p_vi->virt_enable_rereg & IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_BIT) >>
                IB_VIRTUALIZATION_INFO_VPORT_STATE_CHANGE_SHIFT);
}

/*
 * * PARAMETERS
 * *       p_vi
 * *               [in] Pointer to a VirtualizationInfo attribute.
 * *
 * *
 * * RETURN VALUES
 * *       the value of VPort State Change bit (either 1 or 0).
 * *
 * * NOTES
 * *
 * * SEE ALSO
 * *********/

/****s* IBA Base: Types/ib_vport_state_t
* NAME
*	ib_vport_state_t
*
* DESCRIPTION
*	IBA defined VPortState. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vport_state {
	uint8_t vports_states[64];
} PACK_SUFFIX ib_vport_state_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	vports_states
*		each entry contains the state of 2 vports
*		the first 4 bits of each entry would be considered as even state
*		the last 4 bits of each entry would be considered as odd state
*
*/

#define IB_VPORT_STATE_ODD_STATE_SHIFT		0
#define IB_VPORT_STATE_EVEN_STATE_SHIFT		4
#define IB_VPORT_STATE_ODD_STATE_MASK		0x0F
#define IB_VPORT_STATE_EVEN_STATE_MASK		0xF0

/****s* IBA Base: Types/ib_plft_bank_entry_t
* NAME
*	ib_plft_bank_entry_t
*
* DESCRIPTION
*	bank entry describes bank details in Private LFT Info MAD
*	(vendor specific MAD)
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _ib_plft_bank_entry {
	uint8_t size;
	uint8_t num_banks;
} PACK_SUFFIX ib_plft_bank_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	size
*		Maximum size of a bank in the plft e. Indicates the limits on the LID space used.
*               Bank size is specified in units of 1024 LID entries.
*	num_banks
*		Number of independent banks in a plft mode.
*		Number of supported LFT modes.
*
* SEE ALSO
* 	ib_plft_info_t
*********/

#define IB_PLFT_INFO_MODES_NUM			7

/****s* IBA Base: Types/ib_plft_info_t
* NAME
*	ib_plft_info_t
*
* DESCRIPTION
*	PrivateLFTInfo. (vendor specific MAD)
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _ib_plft_info {
	uint8_t resvd1;
	uint8_t num_plfts;
	uint8_t mode_cap;
	uint8_t active_mode;
	ib_plft_bank_entry_t modes[IB_PLFT_INFO_MODES_NUM];
} PACK_SUFFIX ib_plft_info_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*	num_plfts
*		Total number of supported pLFTs in all banks.
*		For backward compatibility a value of 0 means 36 pLFTs.
*
*	mode_cap
*		Number of supported LFT modes.
*
*	active_mode
*		The currently selected mode out of the available ModeCap
*		possible pairs of {NumOfBanks<mode>, BankSize<mode>}.
*		ActiveMode = 0 means PLFT is not enabled
*	modes
*		each entry i holds bank data in mode<i>.
*
* SEE ALSO
* 	ib_plft_bank_entry_t
*********/

/****d* IBA Base: Constants/IB_PLFT_DEF_BLOCK_SIZE
* NAME
*	Constants/IB_PLFT_DEF_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in PLFTDef block.
*
* SOURCE
*/
#define IB_PLFT_DEF_BLOCK_SIZE					16
/*********/

/****d* IBA Base: Constants/IB_PORT_SL_TO_PLFT_MAP_BLOCK_SIZE
* NAME
*	Constants/IB_PORT_SL_TO_PLFT_MAP_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in PortSlToPLFTMap block.
*
* SOURCE
*/
#define IB_PORT_SL_TO_PLFT_MAP_BLOCK_SIZE			4
/*********/

/****s* ib_plft_def_element_t
* NAME
*	ib_plft_def_element
*
* DESCRIPTION
*	An element that defines pLFT table
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _ib_plft_def_element {
	uint8_t force_plft_sub_group0_bank;
	uint8_t plft_lid_space;
	uint8_t reserved;
	uint8_t plft_lid_offset;
} PACK_SUFFIX ib_plft_def_element_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	force_plft_sub_group0_bank
*		Force PLFT Sub Group0 and bank index
*		(0 .. number_of_banks-1 as defined in PrivateLFTInfo).
*
*	plft_lid_space
*		pLFT LID Space[0-15] - The size of the LID space which is
*		allocated to this pLFT in the units of 512-table-entries.
*		Valid values are 1 to 96 and are further reduced by the size
*		of the Bank.
*
*	plft_lid_offset
*		pLFT LID Offset [0-15] - Start offset of the pLFT in
*		the Bank table in units of 512-table-entries
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_plft_def_t
* NAME
*	ib_plft_def_t
*
* DESCRIPTION
*	PrivateLFTDef. (vendor specific MAD)
*	The PrivateLFTDef attribute defines multiple pLFT tables
*	in a single switch device.
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_plft_def {
	ib_plft_def_element_t plfts[IB_PLFT_DEF_BLOCK_SIZE];
} ib_plft_def_t;
#include <complib/cl_packoff.h>
/************/

/****s* IBA Base: Types/ib_plft_map_t
* NAME
*	ib_plft_map_t
*
* DESCRIPTION
*	PrivateLFTMap. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_plft_map {
	uint8_t reserved1[3];
	uint8_t control_map;
	uint8_t fdb_port_group_mask[32];
	uint8_t reserved2[2];
	ib_net16_t lft_top;
	uint8_t reserved3[24];
} ib_plft_map_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	control_map
*		enables the following fields
*		bit 0 - PortMaskEnable - enable bit for FDB Port Group Mask
*		bit 1 - LFTTopEnable - enable bit for Top Value.
*
*	fdb_port_group_mask
*		256 bits. Each bit provides a mask for each port
*		which shares the Private FDB.
*
*	lft_top
*		Indicates the top of the pLFT.
*
* SEE ALSO
*********/

/* PlftMap ControlMap field masks */
#define IB_PLFT_MAP_CONTROL_MAP_PORT_MASK_ENABLE	(0x1)
#define IB_PLFT_MAP_CONTROL_MAP_LFT_TOP_ENABLE		(0x2)

/****s* IBA Base: Types/ib_port_sl_to_plft_map_entry_t
* NAME
*	ib_port_sl_to_plft_map_entry_t
*
* DESCRIPTION
*	Mellanox PortSLToPrivateLFTMap element. (Vendor specific SM class attribute)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_sl_to_plft_map_entry {
	uint8_t plft_port_sl[IB_NUMBER_OF_SLS];
} ib_port_sl_to_plft_map_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	plft_port_sl
*		an array where each entry holds pLFT ID assigned to
*		port and SL=index in array.
*
* SEE ALSO
* 	ib_port_sl_to_plft_map_t
*********/

/****s* IBA Base: Types/ib_port_sl_to_plft_map_t
* NAME
*	ib_port_sl_to_plft_map_t
*
* DESCRIPTION
*	PortSLToPrivateLFTMap. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_sl_to_plft_map {
	ib_port_sl_to_plft_map_entry_t port[IB_PORT_SL_TO_PLFT_MAP_BLOCK_SIZE];
} ib_port_sl_to_plft_map_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	port
*		an array where each entry holds pLFT ID assigned per port and SL
*
* SEE ALSO
* 	ib_port_sl_to_plft_map_entry_t
*********/


/****s* IBA Base: Types/ib_general_info_smp_t
* NAME
*	ib_general_info_smp_t
*
* DESCRIPTION
*	IBA defined GeneralInfo. (vendor specific MAD)
*
* SYNOPSIS
*
* REMARKS
* 	we use union because general info can contain several possibilities
* 	(depends of attribute modifier): HW info, FW info, SW info, CapabilityMask.
* 	for now we only support CapabilityMask, the union is for future use.
*
*/
#include <complib/cl_packon.h>
typedef struct _ib_general_info_smp {
	union _pay_load{
		ib_net32_t cap_mask[4];
	} PACK_SUFFIX payload;
} PACK_SUFFIX ib_general_info_smp_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	cap_mask
*		Optional Capabilities
*
*/

#define IB_GENERAL_INFO_PRIVATE_LINEAR_SUPPORTED_BIT			0
#define IB_GENERAL_INFO_ADAPTIVE_ROUTING_SUPPORTED_BIT			1
#define IB_GENERAL_INFO_ADAPTIVE_ROUTING_REV_SUPPORTED_BIT		2
#define IB_GENERAL_INFO_REMOT_PORT_MIRRORING_SUPPORTED_BIT		3
#define IB_GENERAL_INFO_TEMPERATURE_SENSING_SUPPORTED_BIT		4
#define IB_GENERAL_INFO_CONFIG_SPACE_ACCESS_SUPPORTED_BIT		5
#define IB_GENERAL_INFO_CABLE_INFO_SUPPORTED_BIT			6
#define IB_GENERAL_INFO_SMP_EYE_OPEN_SUPPORTED_BIT			7
#define IB_GENERAL_INFO_LOSSY_VL_CONFIG_SUPPORTED_BIT			8
#define IB_GENERAL_INFO_EXTENDED_PORT_INFO_SUPPORTED_BIT		9
#define IB_GENERAL_INFO_ACCESS_REGISTER_SUPPORTED_BIT			10
#define IB_GENERAL_INFO_INTER_PROC_COMMUNICATION_SUPPORTED_BIT		11
#define IB_GENERAL_INFO_PORT_SL_TO_PRIVATE_LFT_SUPPORTED_BIT		12
#define IB_GENERAL_INFO_EXTENDED_NODE_INFO_SUPPORTED_BIT		13
#define IB_GENERAL_INFO_IS_A_SLAVE_VIRTUAL_SWITCH_BIT			14
#define IB_GENERAL_INFO_VIRTUALIZATION_SUPPORTED_BIT			15
#define IB_GENERAL_INFO_AME_SUPPORTED_BIT				16 // IsAdvancedRetransAndFECModesSupported
#define IB_GENERAL_INFO_IS_SHARP_AGGR_NODE_SUPPORTED_BIT		18
#define IB_GENERAL_INFO_QOS_CONFIG_SL_RATE_LIMIT_SUPPORTED_BIT		24
#define IB_GENERAL_INFO_QOS_CONFIG_SL_VPORT_RATE_LIMIT_SUPPORTED_BIT	25
#define IB_GENERAL_INFO_IS_GLOBAL_OOO_SUPPORTED_BIT			30
/* cap_mask[1] Capabilities */
#define IB_GENERAL_INFO_IS_CC_SUPPORTED_BIT				44 // IsDCQCNSupported
#define IB_GENERAL_INFO_IS_ADAPTIVE_TO_SUPPORTED_BIT			45 // IsAdaptiveTimeoutSupported
#define IB_GENERAL_INFO_IS_ROUTER_LID_SUPPORTED_BIT			48
#define IB_GENERAL_INFO_IS_LFT_SPLIT_SUPPORTED_BIT			49
#define IB_GENERAL_INFO_IS_QOS_CONFIG_VL_SUPPORTED_BIT			51
#define IB_GENERAL_INFO_IS_FAST_RECOVERY_SUPPORTED_BIT			52
#define IB_GENERAL_INFO_IS_CREDIT_WATCHDOG_SUPPORTED_BIT		53
#define IB_GENERAL_INFO_IS_NVL_REDUCTION_MANAGEMENT_SUPPORTED_BIT	55
#define IB_GENERAL_INFO_IS_RAIL_FIILTER_SUPPORTED_BIT			56
#define IB_GENERAL_INFO_IS_NVL_HBF_CONFIG_SUPPORTED_BIT			57
#define IB_GENERAL_INFO_IS_CONTAIN_AND_DRAIN_SUPPORTED_BIT		59
#define IB_GENERAL_INFO_IS_BER_CONFIG_SUPPORTED_BIT			61
#define IB_GENERAL_INFO_IS_VL_BUFFER_WEIGHT_SUPPORTED_BIT 		63
#define IB_GENERAL_INFO_IS_END_PORT_PLANE_FILTER_CONFIG_SUPPORTED_BIT	64
#define IB_GENERAL_INFO_IS_BW_UTILIZAION_SUPPORTED_BIT			66
#define IB_GENERAL_INFO_IS_EXTENDED_SWITCH_INFO_SUPPORTED_BIT		67
#define IB_GENERAL_INFO_IS_FABRIC_MANAGER_INFO_SUPPORTED_BIT		68
#define IB_GENERAL_INFO_IS_CHASSIS_INFO_SUPPORTED_BIT		        72
#define IB_GENERAL_INFO_IS_ISSU_SUPPORTED				74
#define IB_GENERAL_INFO_IS_PORT_RECOVERY_POLICY_CONFIG_SUPPORTED_BIT	76
#define IB_GENERAL_INFO_IS_NMX_ADMINSTATE_SUPPORTED			77
#define IB_GENERAL_INFO_IS_LINK_AUTO_RETRAIN_SUPPORTED_BIT		81
#define IB_GENERAL_INFO_CAP_ATTRIBUTE_MODIFIER				CL_HTON32(0x00000004)

/****f* IBA Base: Types/ib_general_info_is_cap_enabled
* NAME
*	ib_general_info_is_cap_enabled
*
* DESCRIPTION
*	Check whether a given capability is enabled
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_general_info_is_cap_enabled(IN const ib_general_info_smp_t * const p_gi,
			       IN const uint8_t cap_bit) {

	uint8_t dword_idx = cap_bit / 32;
	uint32_t bit_in_dword_mask = 1 << (cap_bit % 32);

	return (cl_ntoh32(p_gi->payload.cap_mask[dword_idx]) & bit_in_dword_mask);
}

/****f* IBA Base: Types/ib_general_info_is_issu_enabled
* NAME
*	ib_general_info_is_issu_enabled
*
* DESCRIPTION
*	Check if a switch is ISSU enabled
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_general_info_is_issu_enabled(IN const ib_general_info_smp_t * const p_gi)
{
       return (ib_general_info_is_cap_enabled(p_gi, IB_GENERAL_INFO_IS_ISSU_SUPPORTED) != 0) ? TRUE : FALSE;
}

/****f* IBA Base: Types/ib_general_info_is_extended_switchinfo_enabled
* NAME
*	ib_general_info_is_extended_switchinfo_enabled
*
* DESCRIPTION
*	Check if a switch is ESI enabled
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_general_info_is_extended_switchinfo_enabled(IN const ib_general_info_smp_t * const p_gi)
{
       return (ib_general_info_is_cap_enabled(p_gi, IB_GENERAL_INFO_IS_EXTENDED_SWITCH_INFO_SUPPORTED_BIT) != 0) ? TRUE : FALSE;
}

/****s* IBA Base: Types/ib_router_info_t
* NAME
*	ib_router_info_t
*
* DESCRIPTION
*	IBA defined RouterInfo. (vendor specific MAD)
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _ib_router_info {
	ib_net32_t cap_mask;
	ib_net32_t next_hop_table_cap;
	ib_net32_t next_hop_table_top;
	ib_net16_t table_changes_bitmask;
	uint8_t    adjacent_site_local_subnets_tbl_top;
	uint8_t    adjacent_site_local_subnets_tbl_cap;
	uint8_t    cap_supported_subnets;
	uint8_t    reserved[1];
	ib_net16_t ar_group_to_router_lid_table_cap;
	uint8_t    pfrn_rtr_en;
	uint8_t    adjacent_subnets_router_lid_info_table_top;
	ib_net16_t router_lid_cap; /* TODO: check if needed */
	ib_net32_t global_router_lid_start; /* 12 bits max_ar_group_cap, 20 bits global router lid start */
	ib_net32_t global_router_lid_end;
	ib_net32_t local_router_lid_start;
	ib_net32_t local_router_lid_end;
} PACK_SUFFIX ib_router_info_t;
#include <complib/cl_packoff.h>

#define IB_ROUTER_ATC		0x8000	/* ATC - Adjacent Table Change */
#define IB_ROUTER_NHTC		0x4000	/* NHTC - Next Hop Table Change */
#define IB_ROUTER_ALTC		0x2000	/* ASITC - Adjacent Subnet Info Table Change */
#define IB_ROUTER_RLTC		0x1000	/* RLTC - Router LID Table Change */
#define IB_ROUTER_ARLITC	0x800	/* ARLITC - Adjacent Subnets Router LID Info Table Change */

#define IB_ROUTER_INFO_CAP_IS_PFRN_SUPPORTED		(CL_HTON32(((uint32_t)1)<<0))

/****f* IBA Base: Types/ib_router_info_is_tbl_change_set
* NAME
*	ib_router_info_is_tbl_change_set
*
* DESCRIPTION
*	Returns router table change flag.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_router_info_is_tbl_change_set(IN const ib_router_info_t * const p_ri, IN uint16_t bitmask) {

	return (p_ri->table_changes_bitmask & cl_hton16(bitmask));
}

/****f* IBA Base: Types/ib_router_info_get_pfrn_rtr_en
* NAME
*	ib_router_info_get_pfrn_rtr_en
*
* DESCRIPTION
* 	Returns value of router's PFRN enablement bit
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_router_info_get_pfrn_rtr_en(IN const ib_router_info_t * const p_ri)
{
	return IBTYPES_GET_ATTR8(p_ri->pfrn_rtr_en, 0x1, 0);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
* 	Returns value of router's PFRN enablement bit
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_pfrn_rtr_en
* NAME
*	ib_router_info_set_pfrn_rtr_en
*
* DESCRIPTION
*	Sets router's PFRN enablement bit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_router_info_set_pfrn_rtr_en(IN ib_router_info_t * const p_ri, IN const uint8_t value) {
	IBTYPES_SET_ATTR8(p_ri->pfrn_rtr_en, 0x1, 0, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_global_router_lid_start
* NAME
*	ib_router_info_set_global_router_lid_start
*
* DESCRIPTION
*	Sets router's global router lids start.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_router_info_set_global_router_lid_start(IN ib_router_info_t * const p_ri, IN const ib_net32_t value)
{
	IBTYPES_SET_ATTR32(p_ri->global_router_lid_start, 0x000FFFFF, 0, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_get_global_router_lid_start
* NAME
*	ib_router_info_get_global_router_lid_start
*
* DESCRIPTION
*	Returns router's global router lids start.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_router_info_get_global_router_lid_start(IN const ib_router_info_t * const p_ri)
{
	return IBTYPES_GET_ATTR32(p_ri->global_router_lid_start, 0x000FFFFF, 0);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
*	Returns router's global router lids start.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_global_router_lid_end
* NAME
*	ib_router_info_set_global_router_lid_end
*
* DESCRIPTION
*	Sets router's global router lids end.
*
* SendNOPSIS
*/
static inline void OSM_API
ib_router_info_set_global_router_lid_end(IN ib_router_info_t * const p_ri, IN const uint32_t value)
{
	IBTYPES_SET_ATTR32(p_ri->global_router_lid_end, 0x000FFFFF, 0, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_get_global_router_lid_end
* NAME
*	ib_router_info_get_global_router_lid_end
*
* DESCRIPTION
*	Returns router's global router lids end.
*
* SendNOPSIS
*/
static inline uint32_t OSM_API
ib_router_info_get_global_router_lid_end(IN const ib_router_info_t * const p_ri)
{
	return IBTYPES_GET_ATTR32(p_ri->global_router_lid_end, 0x000FFFFF, 0);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
*	Returns router's global router lids end.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_local_router_lid_start
* NAME
*	ib_router_info_set_local_router_lid_start
*
* DESCRIPTION
*	Sets router's local router lids start.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_router_info_set_local_router_lid_start(IN ib_router_info_t * const p_ri, IN const uint32_t value)
{
	IBTYPES_SET_ATTR32(p_ri->local_router_lid_start, 0x000FFFFF, 0, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_get_local_router_lid_start
* NAME
*	ib_router_info_get_local_router_lid_start
*
* DESCRIPTION
*	Returns router's local router lids start.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_router_info_get_local_router_lid_start(IN const ib_router_info_t * const p_ri)
{
	return IBTYPES_GET_ATTR32(p_ri->local_router_lid_start, 0x000FFFFF, 0);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
*	Returns router's local router lids start.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_local_router_lid_end
* NAME
*	ib_router_info_set_local_router_lid_end
*
* DESCRIPTION
*	Sets router's local router lids end.
*
* SendNOPSIS
*/
static inline void OSM_API
ib_router_info_set_local_router_lid_end(IN ib_router_info_t * const p_ri, IN const uint32_t value)
{
	IBTYPES_SET_ATTR32(p_ri->local_router_lid_end, 0x000FFFFF, 0, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_get_local_router_lid_end
* NAME
*	ib_router_info_get_local_router_lid_end
*
* DESCRIPTION
*	Returns router's local router lids end.
*
* SendNOPSIS
*/
static inline uint32_t OSM_API
ib_router_info_get_local_router_lid_end(IN const ib_router_info_t * const p_ri)
{
	return IBTYPES_GET_ATTR32(p_ri->local_router_lid_end, 0x000FFFFF, 0);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
*	Returns router's local router lids end.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_set_max_ar_group_id
* NAME
*	ib_router_info_set_max_ar_group_id
*
* DESCRIPTION
*	Sets router's max AR group ID.
*
* SendNOPSIS
*/
static inline void OSM_API
ib_router_info_set_max_ar_group_id(IN ib_router_info_t * const p_ri, IN const uint32_t value)
{
	/* max_ar_group_id is the left 12 bits */
	IBTYPES_SET_ATTR32(p_ri->global_router_lid_start, 0xFFF00000, 20, value);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*	value
*		[in] Value to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_router_info_get_max_ar_group_id
* NAME
*	ib_router_info_get_max_ar_group_id
*
* DESCRIPTION
*	Returns router's max AR group ID.
*
* SendNOPSIS
*/
static inline uint32_t OSM_API
ib_router_info_get_max_ar_group_id(IN const ib_router_info_t * const p_ri)
{
	/* max_ar_group_id is the left 12 bits */
	return IBTYPES_GET_ATTR32(p_ri->global_router_lid_start, 0xFFF00000, 20);
}
/*
* PARAMETERS
*	p_ri
*		[in] Pointer to a RouterInfo attribute.
*
* RETURN VALUES
*	Returns router's max AR group ID.
*
* NOTES
*
* SEE ALSO
*********/

/****s* IBA Base: Types/rtr_adj_site_local_subn_record_t
* NAME
*	rtr_adj_site_local_subn_record_t
*
* DESCRIPTION
*	RouterTable record
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _rtr_adj_site_local_subn_record {
	ib_net16_t pkey;
	ib_net16_t subnet_prefix;
	ib_net16_t pfrn_rtr_en_max_group_id;
	ib_net16_t master_sm_lid;
} PACK_SUFFIX rtr_adj_site_local_subn_record_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_rtr_adj_table_block_t
* NAME
*	ib_rtr_adj_table_block_t
*
* DESCRIPTION
*	IBA defined adjacent RouterTable block.
*
* SYNOPSIS
*
*/
#define IB_ROUTER_ELEMENTS_IN_ADJACENT_BLOCK 8

#include <complib/cl_packon.h>
typedef struct _ib_rtr_adj_table_block {
	rtr_adj_site_local_subn_record_t
		record[IB_ROUTER_ELEMENTS_IN_ADJACENT_BLOCK];
} PACK_SUFFIX ib_rtr_adj_table_block_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/rtr_next_hop_record_t
* NAME
*	rtr_next_hop_record_t
*
* DESCRIPTION
*	Next hop RouterTable record element
*
* SYNOPSIS
*
*/
#include <complib/cl_packon.h>
typedef struct _rtr_next_hop_record {
	ib_net64_t subnet_prefix;
	uint8_t    weight;
	uint8_t    resvd1;
	ib_net16_t pkey;
	uint8_t    resvd2[4];
} PACK_SUFFIX rtr_next_hop_record_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_rtr_next_hop_table_block_t
* NAME
*	ib_rtr_next_hop_table_block_t
*
* DESCRIPTION
*	IBA defined next hop RouterTable block.
*
* SYNOPSIS
*
*/
#define IB_ROUTER_ELEMENTS_IN_NEXT_HOP_BLOCK 4

#include <complib/cl_packon.h>
typedef struct _ib_rtr_next_hop_table_block {
	rtr_next_hop_record_t
		record[IB_ROUTER_ELEMENTS_IN_NEXT_HOP_BLOCK];
} PACK_SUFFIX ib_rtr_next_hop_table_block_t;
#include <complib/cl_packoff.h>

/****s* IBA Base: Types/ib_rtr_adj_subnets_lid_info_block_t
* NAME
*	ib_rtr_adj_subnets_lid_info_block_t
*
* DESCRIPTION
*	Adjacent subnets router lid info block
*
* SYNOPSIS
*
*/
#define IB_ROUTER_ADJ_SUBNETS_LID_INFO_ELEMENTS		8
#include <complib/cl_packon.h>
typedef struct _ib_rtr_adj_subnets_lid_info_block {
	ib_net64_t record[IB_ROUTER_ADJ_SUBNETS_LID_INFO_ELEMENTS];
} PACK_SUFFIX ib_rtr_adj_subnets_lid_info_block_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	record
*		A 64 bit field that holds SubnetPrefixID, Local router LID start,
*		Local router LID end
*
*/

static inline
uint16_t ib_rtr_adj_subnets_lid_info_tbl_get_subnet_prefix(
		IN ib_net64_t * const p_rtr_lid_info_tbl_record) {
	/* subnet prefix id sits on 16 MSB bits of 64 bits fields */
	return ((cl_ntoh64(*p_rtr_lid_info_tbl_record) >> 48) & 0xFFFF);
}

static inline
uint32_t ib_rtr_adj_subnets_lid_info_tbl_get_local_lid_start(
		IN ib_net64_t * const p_rtr_lid_info_tbl_record) {
	/*
	 * local router lid start sits on 20: 15-0 bits of first dword,
	 * continues on bits 31-28 of the second dword.
	 */
	return ((cl_ntoh64(*p_rtr_lid_info_tbl_record) >> 28) & 0xFFFFF);
}

static inline
uint32_t ib_rtr_adj_subnets_lid_info_tbl_get_local_lid_end(
		IN ib_net64_t * const p_rtr_lid_info_tbl_record) {
	/* local router lid end sits on 20: 19-0 bits of second dword */
	return (cl_ntoh64(*p_rtr_lid_info_tbl_record) & 0xFFFFF);
}


/****s* IBA Base: Types/ib_rtr_router_lid_tbl_block_t
* NAME
*	ib_rtr_router_lid_tbl_block_t
*
* DESCRIPTION
*	Router lid table block
*
* SYNOPSIS
*
*/
#define IB_ROUTER_LID_TBL_ELEMENTS			16
#include <complib/cl_packon.h>
typedef struct _ib_rtr_router_lid_tbl_block {
	ib_net32_t lids[IB_ROUTER_LID_TBL_ELEMENTS];
} PACK_SUFFIX ib_rtr_router_lid_tbl_block_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	lids
*		A 32 bit field with a bit for each lid.
*		A set bit means that the lid is enabled
*
*/
#define IB_ROUTER_LID_TBL_BLOCK_RECORD_LIDS_NUM		32
#define IB_ROUTER_LID_TBL_BLOCK_LIDS_NUM		512

/****s* IBA Base: Types/ib_rtr_ar_group_to_lid_tbl_block
* NAME
*	ib_rtr_ar_group_to_lid_tbl_block
*
* DESCRIPTION
*	AR group to LID table
*
* SYNOPSIS
*
*/
#define IB_ROUTER_AR_GROUP_TO_LID_TBL_ELEMENTS			32
#include <complib/cl_packon.h>
typedef struct _ib_rtr_ar_group_to_lid_tbl_block {
	ib_net16_t lids[IB_ROUTER_AR_GROUP_TO_LID_TBL_ELEMENTS];
} PACK_SUFFIX ib_rtr_ar_group_to_lid_tbl_block_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	lids
*		vector of 32 LIDs.
*/

/****s* IBA Base: Types/ib_vnode_info_t
* NAME
*	ib_vnode_info_t
*
* DESCRIPTION
*	IBA defined VNodeInfo. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vnode_info {
	ib_net16_t partition_cap;
	uint8_t local_port_num;
	uint8_t num_ports;
	ib_net64_t vnode_guid;
	uint8_t resvd[52];
} PACK_SUFFIX ib_vnode_info_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	partition_cap
*		Number of entries in the Partition Table for v_node
*
*	local_port_num
*		The number of the v_node port which received this SMP
*
*	num_ports
*		Number of VPorts on this v_node
*
*	vnode_guid
*		GUID of this v_node.
*
*	resvd
*		Reserved
*
*/

/****s* IBA Base: Types/ib_ar_info_t
* NAME
*	ib_ar_info_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingInfo (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_info {
	uint8_t enabled_and_cap_fields;
	uint8_t dir_num_supp;
	uint8_t grp_tbl_cpy_sub_grp_active;
	uint8_t enable_features;
	ib_net32_t group_info;
	ib_net16_t enable_by_sl_mask;
	uint8_t sub_grps_sup;
	uint8_t ar_nv_version_cap;
	uint8_t reserved1[3];
	uint8_t by_trns_disable;
	ib_net32_t ageing_time_value;
	uint8_t cap_fields1;
	uint8_t reserved2;
	uint8_t enable_features1;
	uint8_t reserved3;
	uint8_t whbf_granularity;
	uint8_t reserved4;
	ib_net16_t enable_by_sl_mask_hbf;
	uint8_t reserved5[35];
} PACK_SUFFIX ib_ar_info_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*	enabled_and_cap_fields
*		bit 0 - no_fallback.
*		bit 1 - IS4 mode.
*		bit 2 - global groups.
*		bit 3 - by SL cap.
*		bit 4 - by SL enabled.
*		bit 5 - by transp cap.
*		bit 6 - dynamic cap calc support.
*
*	dir_num_supp
*		Number of supported directions.
*
* 	grp_tbl_cpy_sub_grp_active
* 		4-7 - ar group table copy supported.
* 		0-3 - sub groups active.
*
* 	enable_features
* 		bit 0 - Enable adaptive routing. (Read/Write).
* 		bit 1 - ARN supported.
* 		bit 2 - FRN supported.
* 		bit 3 - FR supported.
* 		bit 4 - FR enabled.
* 		bit 5 - RN Xmit enabled.
*
* 	group_info
* 		28-31 - StringWidthCap.
*		24-27 - GroupTableCap.
*		12-23 - GroupTop.
*		0-11  - GroupCap.
*
*	enable_by_sl_mask
*		If BySLEn is set to 1, this field provides the set of SLs that
*		AR is applied to.
*
* 	sub_grps_sup
* 		Indicates the support for Primary and Secondary Port
* 		Groups. (Read Only).
*
* 	ar_nv_version_cap
*		4-7 - Indicates the latest Routing Notification Version
*		      supported by the device. (Read Only).
*		0-3 - Indicates the latest Adaptive Routing Version supported
*		      by the device. (Read Only).
*
*	by_trns_disable
*		Transport types that AR is not applied to (when ByTranspCap set
*		to 1):
*		bit 0 - UD.
*		bit 1 - RC.
*		bit 2 - UC.
*		bit 3 - DC.
*
*	ageing_time_value
*		Ageing in one microsecond granularity. (Read/Write)
*
*	cap_fields1
*		Additional capability fields
*		bit 0 - reserved
*		bit 1 - pFRN supported
*		bit 2 - by VL cap.
*		bit 3 - IsBTHDQPHashSupported
*		bit 4 - IsDCETHHashSupported
*		bit 5 - IsSymmetricHashSupported
*		bit 6 - IsWHBFSupported
*		bit 7 - IsHBFSupported
*
*	enable_features1
*		Additional features
*		bit 0 - reserved
*		bit 1 - reserved
*		bit 2 - reserved
*		bit 3 - reserved
*		bit 4 - pFRN enabled
*		bit 5 - by VL enabled.
*		bit 6 - IsWHBFSupported
*		bit 7 - IsHBFSupported
*/

/* Adaptive Routing Info Masks */
#define IB_AR_INFO_CAP_MASK_IS4_MODE			(0x02)
#define IB_AR_INFO_CAP_MASK_GLB_GROUPS			(0x04)
#define IB_AR_INFO_CAP_MASK_BY_SL_CAP			(0x08)
#define IB_AR_INFO_CAP_MASK_BY_SL_EN			(0x10)
#define IB_AR_INFO_CAP_MASK_BY_TRANS_CAP		(0x20)
#define IB_AR_INFO_CAP_MASK_DYN_CAP_CALC_SUP		(0x40)

#define IB_AR_INFO_FEATURE_MASK_AR_ENABLED		(0x01)
#define IB_AR_INFO_FEATURE_MASK_ARN_SUPPORTED		(0x02)
#define IB_AR_INFO_FEATURE_MASK_FRN_SUPPORTED		(0x04)
#define IB_AR_INFO_FEATURE_MASK_FR_SUPPORTED		(0x08)
#define IB_AR_INFO_FEATURE_MASK_FR_ENABLED		(0x10)
#define IB_AR_INFO_FEATURE_MASK_RN_XMIT_ENABLED		(0x20)

#define IB_AR_INFO_TRANSPORT_UD				(0x1)
#define IB_AR_INFO_TRANSPORT_RC				(0x2)
#define IB_AR_INFO_TRANSPORT_UC				(0x4)
#define IB_AR_INFO_TRANSPORT_DC				(0x8)
#define IB_AR_INFO_TRANSPORT_NUM			4

#define IB_AR_INFO_CAP_FEILDS_PFRN_SUPPORTED		(0x02)
#define IB_AR_INFO_CAP_FEILDS_NVL_HBF_SUPPORTED		(0x04)
#define IB_AR_INFO_FEATURE_MASK_PFRN_ENABLED		(0x10)

#define IB_AR_INFO_HBF_CAP_BTHDQP_HASH_SUP		(0x8)
#define IB_AR_INFO_HBF_CAP_DCETH_HASH_SUP		(0x10)
#define IB_AR_INFO_HBF_CAP_HBF_SUP			(0x80)
#define IB_AR_INFO_HBF_CAP_WHBF_SUP			(0x40)
#define IB_AR_INFO_HBF_EN_BY_SL_HBF_EN			(0x80)
#define IB_AR_INFO_HBF_EN_WHBF_EN			(0x40)

/* bit 20 of HBF hash config specific fields */
#define IB_HBF_CONFIG_HASH_FIELDS_BTH_DESTINATION_QP	(0x100000)
/* bit 22 of HBF hash config specific fields */
#define IB_HBF_CONFIG_HASH_FIELDS_DCETH_V1_SRC_QP	(0x400000)
/* bit 23 of HBF hash config specific fields */
#define IB_HBF_CONFIG_HASH_FIELDS_DCETH_V1_ISID		(0x800000)

static inline
uint16_t ib_ar_info_get_group_cap_ho(IN const ib_ar_info_t * const p_ar_info)
{
	return (cl_ntoh32(p_ar_info->group_info) & 0x00000FFF);
}

static inline
void ib_ar_info_set_group_cap_ho(IN ib_ar_info_t * const p_ar_info, IN uint16_t group_cap)
{
	uint32_t group_info_ho = cl_ntoh32(p_ar_info->group_info);
	group_info_ho &= 0xFFFFF000;
	group_info_ho |= (uint32_t)group_cap & 0x00000FFF;
	p_ar_info->group_info = cl_hton32(group_info_ho);
}

static inline
uint16_t ib_ar_info_get_group_top_ho(IN const ib_ar_info_t * const p_ar_info)
{
	return ((cl_ntoh32(p_ar_info->group_info) >> 12) & 0x00000FFF);
}

static inline
void ib_ar_info_set_group_top_ho(IN ib_ar_info_t * const p_ar_info,
						 IN uint16_t group_top)
{
	uint32_t group_info_ho = cl_ntoh32(p_ar_info->group_info);
	group_info_ho &= 0xFF000FFF;
	group_info_ho |= (((uint32_t)group_top) << 12)  & 0x00FFF000;
	p_ar_info->group_info = cl_hton32(group_info_ho);
}

static inline
uint8_t ib_ar_info_get_group_tbl_cap_ho(IN const ib_ar_info_t * const p_ar_info)
{
	return ((cl_ntoh32(p_ar_info->group_info) >> 24) & 0x0000000F);
}

static inline
uint8_t ib_ar_info_get_string_width_cap(IN const ib_ar_info_t * const p_ar_info)
{
	return ((cl_ntoh32(p_ar_info->group_info) >> 28) & 0x0000000F);
}

static inline
uint8_t ib_ar_info_get_sub_grps_active(IN const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->grp_tbl_cpy_sub_grp_active & 0x0F);
}

static inline
uint8_t ib_ar_info_get_grp_tbl_cpy_sup(IN const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->grp_tbl_cpy_sub_grp_active >> 4);
}

static inline
void ib_ar_info_set_sub_grps_active(IN ib_ar_info_t * const p_ar_info,
						    IN uint8_t sub_grps_active)
{
	p_ar_info->grp_tbl_cpy_sub_grp_active &= 0xF0;
	p_ar_info->grp_tbl_cpy_sub_grp_active |= sub_grps_active & 0x0F;
}

static inline
uint8_t ib_ar_info_get_whb_granularity(IN ib_ar_info_t * const p_ar_info)
{
	/* look at the 3 MSB bits */
	return (p_ar_info->whbf_granularity >> 5) & 0xE;
}

static inline const char* ib_ar_info_get_transport_name(IN const uint8_t transport_mask_bit)
{
	switch (transport_mask_bit) {
	case IB_AR_INFO_TRANSPORT_UD:
		return "UD";
	case IB_AR_INFO_TRANSPORT_RC:
		return "RC";
	case IB_AR_INFO_TRANSPORT_UC:
		return "UC";
	case IB_AR_INFO_TRANSPORT_DC:
		return "DC";
	default:
		return "??";
	}
}

/****d* IBA Base: Constants/IB_AR_LFT_BLOCK_SIZE
* NAME
*	IB_AR_LFT_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in an AdaptiveRoutingLinearForwardingTable block.
*
* SOURCE
*/
#define IB_AR_LFT_BLOCK_SIZE					16
/*********/

/****d* IBA Base: Constants/IB_AR_GROUP_COPY_BLOCK_SIZE
* NAME
*	IB_AR_GROUP_COPY_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in an AdaptiveRoutingGroupTableCopy block.
*
* SOURCE
*/
#define IB_AR_GROUP_COPY_BLOCK_SIZE				16
/*********/

/****d* IBA Base: Constants/IB_RN_SUB_GROUP_DIRECTION_BLOCK_SIZE
* NAME
*	IB_RN_SUB_GROUP_DIRECTION_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in RNSubGroupDirectionTable block.
*
* SOURCE
*/
#define IB_RN_SUB_GROUP_DIRECTION_BLOCK_SIZE			64
/*********/

/****d* IBA Base: Constants/IB_RN_GEN_STRING_BLOCK_SIZE
* NAME
*	IB_RN_GEN_STRING_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in RNGenStringTable block.
*
* SOURCE
*/
#define IB_RN_GEN_STRING_BLOCK_SIZE				32
/*********/

/****d* IBA Base: Constants/IB_RN_RCV_STRING_BLOCK_SIZE
* NAME
*	IB_RN_RCV_STRING_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in RNRcvStringTable block.
*
* SOURCE
*/
#define IB_RN_RCV_STRING_BLOCK_SIZE				16
/*********/

/****d* IBA Base: Constants/IB_RN_GEN_BY_SUB_GROUP_PRIORITY_BLOCK_SIZE
* NAME
*	IB_RN_GEN_BY_SUB_GROUP_PRIORITY_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in RNGenBySubGroupPriority block.
*
* SOURCE
*/
#define IB_RN_GEN_BY_SUB_GROUP_PRIORITY_BLOCK_SIZE		16
/*********/

/****d* IBA Base: Constants/IB_RN_XMIT_PORT_MASK_BLOCK_SIZE
* NAME
*	Constants/IB_RN_XMIT_PORT_MASK_BLOCK_SIZE
*
* DESCRIPTION
*	Number of entries in RNXmitPortMask block.
*	Each entry contains two masks.
*
* SOURCE
*/
#define IB_RN_XMIT_PORT_MASK_BLOCK_SIZE				64
/*********/

#define IB_AR_LID_STATE_BOUNDED	0
#define IB_AR_LID_STATE_FREE	1
#define IB_AR_LID_STATE_STATIC	2

/****s* IBA Base: Types/ib_ar_lft_entry_t
* NAME
*	ib_ar_lft_entry_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingLinearForwardingTable element (Vendor specific
*	SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_lft_entry {
	uint8_t state_table;
	uint8_t legacy_port;
	ib_net16_t group_number;
} PACK_SUFFIX ib_ar_lft_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	state_table
*		bits 6-7 - LID state.
*		bits 0-3 - table number.
*
* 	legacy_port
* 		static routing port number.
*
* 	group_number
* 		AR group number.
*/

/****f* IBA Base: Types/ib_ar_lft_entry_get_state
* NAME
*	ib_ar_lft_entry_get_state
*
* DESCRIPTION
*	Returns port state in AR LFT table entry.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_ar_lft_entry_get_state(IN const ib_ar_lft_entry_t * p_ar_lft_entry)
{
	return (p_ar_lft_entry->state_table) >> 6;
}

/****f* IBA Base: Types/ib_ar_lft_entry_set_state
* NAME
*	ib_ar_lft_entry_set_state
*
* DESCRIPTION
*	Sets port state in AR LFT table entry.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ar_lft_entry_set_state(OUT ib_ar_lft_entry_t * p_ar_lft_entry,
			  IN const uint8_t state)
{
	p_ar_lft_entry->state_table = (state << 6);
}

/****f* IBA Base: Types/ib_ar_lft_entry_set_port
* NAME
*	ib_ar_lft_entry_set_port
*
* DESCRIPTION
*	Sets port number in AR LFT table entry.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ar_lft_entry_set_port(OUT ib_ar_lft_entry_t * p_ar_lft_entry,
			 IN const uint8_t port)
{
	p_ar_lft_entry->legacy_port = port;
}

/****f* IBA Base: Types/ib_ar_lft_entry_set_group
* NAME
*	ib_ar_lft_entry_set_group
*
* DESCRIPTION
*	Sets group number in AR LFT table entry.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ar_lft_entry_set_group(OUT ib_ar_lft_entry_t * p_ar_lft_entry,
			  IN const uint16_t group_number)
{
	p_ar_lft_entry->group_number = cl_hton16(group_number);
}

/****s* IBA Base: Types/ib_ar_lft_t
* NAME
*	ib_ar_lft_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingLinearForwardingTable block
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_lft {
	ib_ar_lft_entry_t ar_lft_entry[IB_AR_LFT_BLOCK_SIZE];
} PACK_SUFFIX ib_ar_lft_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	ar_lft_entry
* 		Array of 16 Adaptive Routing Linear Forwarding Table elements.
*/

/****s* IBA Base: Types/ib_hbf_config_t
* NAME
*	ib_hbf_config_t
*
* DESCRIPTION
*	Mellanox HbfConfig block*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_hbf_config {
	uint8_t hbf_seed_type;
	uint8_t reserved1[7];
	ib_net32_t seed;
	ib_net64_t fields_enable;
	uint8_t reserved2[34];
} PACK_SUFFIX ib_hbf_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	hbf_seed_type
* 		A seed type for HBF hash. 0 - configurable seed, 1 - random
* 	seed
* 		A seed value for HBF hash, reserved when seed_type = 1
* 	fields_enable
* 		A bitmask with fields for HBF hash.
*/

static inline uint8_t ib_hbf_config_get_hbf_type(IN const ib_hbf_config_t * const p_hbf_config)
{
	return p_hbf_config->hbf_seed_type & 0xF0;
}

static inline uint8_t ib_hbf_config_get_seed_type(IN const ib_hbf_config_t * const p_hbf_config)
{
	return p_hbf_config->hbf_seed_type & 0x3;
}

/****s* IBA Base: Types/ib_ar_group_hbf_weight_t
* NAME
*	ib_ar_group_hbf_weight_t
*
* DESCRIPTION
*	Mellanox Weighted Hash Based Forwarding (HBF) element
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_group_weight {
	uint8_t reserved;
	uint8_t sg0_weight;
	uint8_t sg1_weight;
	uint8_t sg2_weight;
} PACK_SUFFIX ib_ar_group_hbf_weight_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	sg0_weight
* 		An HBF weight of sub group 0 in Adaptive Routing (AR) group
* 	sg1_weight
* 		An HBF weight of sub group 1 in Adaptive Routing (AR) group
* 	sg2_weight
* 		An HBF weight of sub group 2 in Adaptive Routing (AR) group
*/

/****d* OpenSM: Constants/OSM_AR_HBF_WEIGHT_SUBGROUP_NUM
 * Name
 * 	OSM_AR_HBF_WEIGHT_SUBGROUP_NUM
 *
 * DESCRIPTION
 *	The number of sub group weights defined ib_ar_group_hbf_weight_t.
 *
 * SYNOPSIS
 */
#define OSM_AR_HBF_WEIGHT_SUBGROUP_NUM			3

/****s* IBA Base: Types/ib_whbf_config_t
* NAME
*	ib_whbf_config_t
*
* DESCRIPTION
*	Mellanox Weighted Hash Based Forwarding (HBF) block
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#define WHBF_BLOCK_SIZE 16
#include <complib/cl_packon.h>
typedef struct _ib_whbf_config {
	ib_ar_group_hbf_weight_t ar_group_weights[WHBF_BLOCK_SIZE];
} PACK_SUFFIX ib_whbf_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	ar_group_weights
* 		Array of 16 Weighted Hash Based Forwarding elements.
*/

/****s* IBA Base: Types/ib_ar_group_table_t
* NAME
*	ib_ar_group_table_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingGroupTable block
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_grp_tbl {
	uint8_t ar_sub_group_0[32];
	uint8_t ar_sub_group_1[32];
} PACK_SUFFIX ib_ar_group_table_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	ar_sub_group_0
* 		First sub group entry.
*
* 	ar_sub_group_1
* 		Second sub group entry.
*/

/****d* OpenSM: Constants/OSM_AR_GROUP_COPY_ATTR_MOD_SRC_GROUP_MASK
 * Name
 * 	OSM_AR_GROUP_COPY_ATTR_MOD_SRC_GROUP_MASK
 *
 * DESCRIPTION
 *	AttributeModifier[11:0] defines the AdaptiveRoutingGroup index to be copied.
 *
 * SYNOPSIS
 */
#define OSM_AR_GROUP_COPY_ATTR_MOD_SRC_GROUP_MASK	0xfff

/****s* IBA Base: Types/ib_ar_group_copy_entry_t
* NAME
*	ib_ar_group_table_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingGroupTableCopy block
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_group_copy_entry {
	ib_net16_t last_index;
	ib_net16_t first_index;
} PACK_SUFFIX ib_ar_group_copy_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	last_index
* 		First index of the AdapativeRoutingGroupTable to be copied to
* 		(12 LSB).
*
* 	first_index
* 		Last index of the AdapativeRoutingGroupTable to be copied to
* 		(12 LSB).
*/

/****s* IBA Base: Types/ib_ar_group_copy_t
* NAME
*	ib_ar_group_copy_t
*
* DESCRIPTION
*	Mellanox AdaptiveRoutingGroupTableCopy block
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ar_group_copy {
	ib_ar_group_copy_entry_t group_copy_entry[IB_AR_GROUP_COPY_BLOCK_SIZE];
} PACK_SUFFIX ib_ar_group_copy_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	group_copy_entry
* 		Array of 16 AdaptiveRoutingGroupTableCopy elements
*/

/****s* IBA Base: Types/ib_rn_sub_group_direction_table_t
* NAME
*	ib_rn_sub_group_direction_table_t
*
* DESCRIPTION
*	Mellanox RNSubGroupDirectionTable block (Vendor specific SM class
*	attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_sub_group_direction_table {
	uint8_t direction[IB_RN_SUB_GROUP_DIRECTION_BLOCK_SIZE];
} PACK_SUFFIX ib_rn_sub_group_direction_table_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	direction
* 		Array of 64 sub group direction elements.
*/

/****s* IBA Base: Types/ib_rn_gen_string_table_t
* NAME
*	ib_rn_gen_string_table_t
*
* DESCRIPTION
*	Mellanox RNGenStringTable block (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_gen_string_table {
	ib_net16_t rn_gen_string[IB_RN_GEN_STRING_BLOCK_SIZE];
} PACK_SUFFIX ib_rn_gen_string_table_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	rn_gen_string
* 		Array of 32 RNGenStringTable block elements.
*/

/****s* IBA Base: Types/ib_rn_rcv_string_entry_t
* NAME
*	ib_rn_rcv_string_entry_t
*
* DESCRIPTION
*	Mellanox RNRcvStringTable block element (Vendor specific SM class
*	attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_rcv_string_entry {
	ib_net16_t string2string;
	uint8_t plft_id;
	uint8_t decision;
} PACK_SUFFIX ib_rn_rcv_string_entry_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	string2string
* 		New String to be assigned to the RN.
* 		Relevant only if decision is Pass-on.
*
*	plft_id
*		PrivateLFTID to be used for consume or pass-on.
*
* 	decision
* 		RN handling decision:
* 		0 - Discard.
* 		1 - Consume ARN.
* 		2 - Consume ARN/FRN.
* 		3 - Pass-on.
*/

/****s* IBA Base: Types/ib_rn_rcv_string_table_t
* NAME
*	ib_rn_rcv_string_table_t
*
* DESCRIPTION
*	Mellanox RNRcvStringTable block (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_rcv_string_table {
	ib_rn_rcv_string_entry_t rn_rcv_string[IB_RN_RCV_STRING_BLOCK_SIZE];
} PACK_SUFFIX ib_rn_rcv_string_table_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	rn_rcv_string
* 		Array of 16 RNRcvStringTable block elements.
*/

/****s* IBA Base: Types/ib_rn_gen_by_sub_group_priority_t
* NAME
*	ib_rn_gen_by_sub_group_priority_t
*
* DESCRIPTION
*	Mellanox RNGenBySubGroupPriority block (Vendor specific SM class
*	attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_gen_by_sub_group_priority {
	ib_net32_t elements[IB_RN_GEN_BY_SUB_GROUP_PRIORITY_BLOCK_SIZE];
} PACK_SUFFIX ib_rn_gen_by_sub_group_priority_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	elements
* 		Array of 16 RNGenBySubGroupPriority block elements.
*/

#define IB_RN_GEN_BY_SUB_GROUP_PRIORITY_GEN_ARN_MASK		CL_HTON32(0x0000001)
#define IB_RN_GEN_BY_SUB_GROUP_PRIORITY_GEN_FRN_MASK		CL_HTON32(0x0000002)

/****s* IBA Base: Types/ib_rn_xmit_port_mask_t
* NAME
*	ib_rn_xmit_port_mask_t
*
* DESCRIPTION
*	Mellanox RNXmitPortMask block (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_rn_xmit_port_mask {
	uint8_t elements[IB_RN_XMIT_PORT_MASK_BLOCK_SIZE];
} PACK_SUFFIX ib_rn_xmit_port_mask_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	elements
* 		Array of 128 RNXmitPortMask block elements.
*/

#define IB_RN_XMIT_PORT_MASK_ENABLE_ARN_GEN			0x1
#define IB_RN_XMIT_PORT_MASK_ENABLE_FRN_GEN			0x2
#define IB_RN_XMIT_PORT_MASK_ENABLE_PASS_ON			0x4

#define BW_LAYOUT_RATE_LIMIT_MASK 0x000FFFFF
#define BW_LAYOUT_BW_ALLOC_MASK   0xFFF00000
#define BW_LAYOUT_BW_ALLOC_SHIFT 20

/****s* IBA Base: Types/ib_bw_layout_t
* NAME
*	ib_bw_layout_t
*
* DESCRIPTION
*	This structure is used to describe bandwidth layout as they will be
*	passed in QosConfig VS MADS.
*
*	Currently SM support rate limit only, guaranteed bandwidth support
*	shall be added later on
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_bw_layout {
	ib_net32_t bw_alloc_rate_limit;
} PACK_SUFFIX ib_bw_layout_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	bw_alloc_rate_limit
*		[0-19]	Rate Limit
*		[20-31]	Bandwidth allocation
*
*/

/****s* IBA Base: Types/qos_config_sl_t
* NAME
*	Types/qos_config_sl_t
*
* DESCRIPTION
*	IBA defined QoSConfigSL. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct qos_config_sl {
	 ib_bw_layout_t sl_bandwidth[IB_NUMBER_OF_SLS];
} PACK_SUFFIX qos_config_sl_t;
#include <complib/cl_packoff.h>

/*
* FIELDS
*
*	sl_bandwidth
*		Array of bandwidth layouts, one for each SL.
*/

/****f* IBA Base: Types/ib_bw_layout_set
* NAME
*	ib_bw_layout_set
*
* DESCRIPTION
*	Set values in ib_bw_layout_t struct
*
* SYNOPSIS
*/
static inline void OSM_API
ib_bw_layout_set_rate_limit(OUT ib_bw_layout_t *bw_layout,
			    IN uint32_t bw_alloc,
			    IN uint32_t rate_limit)
{
	uint32_t bw_alloc_val, rate_limit_val;

	rate_limit_val = rate_limit & BW_LAYOUT_RATE_LIMIT_MASK;

	bw_alloc_val = (bw_alloc << BW_LAYOUT_BW_ALLOC_SHIFT) &
		       BW_LAYOUT_BW_ALLOC_MASK;

	bw_layout->bw_alloc_rate_limit = cl_hton32(rate_limit_val | bw_alloc_val);
}

/*
* PARAMETERS
*	bw_layout
*		[out] Pointer to bandwidth layout structure to set
*
*	bw_allocation
*		[in] Value of bandwidth allocation to set
*
*	rate_limit
*		[in] Value of rate limit to set
*
* RETURN VALUE
*	N/A
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_bw_layout_get
* NAME
*	ib_bw_layout_get
*
* DESCRIPTION
*	Get values from ib_bw_layout_t struct
*
* SYNOPSIS
*/
static inline void OSM_API
ib_bw_layout_get_rate_limit(IN ib_bw_layout_t *bw_layout,
			    OUT uint32_t *bw_alloc,
			    OUT uint32_t *rate_limit)
{
	uint32_t payload = cl_ntoh32(bw_layout->bw_alloc_rate_limit);

	*rate_limit = payload & BW_LAYOUT_RATE_LIMIT_MASK;

	*bw_alloc = (payload & BW_LAYOUT_BW_ALLOC_MASK)
			>> BW_LAYOUT_BW_ALLOC_SHIFT;
}

/*
* PARAMETERS
*	bw_layout
*		[in] Pointer to bandwidth layout structure to set
*
*	bw_alloc
*		[out] Pointer in which to set value of bandwidth allocation
*
*	rate_limit
*		[out] Pointer in which to set value of rate limit
*
* RETURN VALUE
*	N/A
*
* NOTES
*
* SEE ALSO
*********/

#define IB_VL_CONFIG_DATA_TYPE_IB_BIT             0
#define IB_VL_CONFIG_DATA_TYPE_NVL_UC_REQ_BIT     1
#define IB_VL_CONFIG_DATA_TYPE_NVL_UC_RES_BIT     2
#define IB_VL_CONFIG_DATA_TYPE_NVL_MC_REQ_BIT     3
#define IB_VL_CONFIG_DATA_TYPE_NVL_MC_RES_BIT     4

#define IB_VL_CONFIG_DISABLE_HOQ_LIFE_SHIFT       0
#define IB_VL_CONFIG_DISABLE_HOQ_LIFE_MASK_HO     0x01

/****s* IBA Base: Types/ib_vl_config_block_t
* NAME
*	ib_vl_config_block_t
*
* DESCRIPTION
*	This structure is used to describe VL config block layout
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vl_config_block_t {
	uint8_t vl_buffer_weight;
	ib_net16_t data_type_bitmask;
	uint8_t disable_hoq_life;
} PACK_SUFFIX ib_vl_config_block_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	vl_buffer_weight
*		The relative part requested by the SM of the ingress
*		port buffer out of the full buffer size.
*		The size of current VL buffer will be calculated
*		according to its VLBufferWeight value relative to all
*		other VLs VLBufferWight values.
*
*	data_type_bitmask
*		When DataTypeBitmask[i] = 1, the corresponsing
*		data type is running on this VL.
*		Bit 0: IB
*		Bit 1: NVL UC requests
*		Bit 2: NVL UC responses
*		Bit 3: NVL MC requests
*		Bit 4: NVL MC responses
*		Bits 5-15: reserved.
*
*	disable_hoq_life
*		When set, disables the HLL feature on the VL this block
*		is applied to, the HLL value for this port, defined by
*		PortInfo.HoQLife.
*		reserved for HCA
*
*/


/****f* IBA Base: Types/ib_vl_config_block_get_disable_hoq_life
* NAME
*	ib_vl_config_block_get_disable_hoq_life
*
* DESCRIPTION
*       Returns whether HLL feature disabled
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_qos_config_vl_get_disable_hoq_life(IN ib_vl_config_block_t *p_vl_config)
{
	return IBTYPES_GET_ATTR8(p_vl_config->disable_hoq_life, IB_VL_CONFIG_DISABLE_HOQ_LIFE_MASK_HO,
				 IB_VL_CONFIG_DISABLE_HOQ_LIFE_SHIFT);
}

/****f* IBA Base: Types/ib_vl_config_block_set_disable_hoq_life
* NAME
*	ib_vl_config_block_set_disable_hoq_life
*
* DESCRIPTION
*       Set disable_hoq_life value. When set, disables the HLL feature on the VL this block
*       is applied to.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_qos_config_vl_set_disable_hoq_life(OUT ib_vl_config_block_t *p_vl_config, IN uint8_t disable_hoq_life)
{
	IBTYPES_SET_ATTR8(p_vl_config->disable_hoq_life, IB_VL_CONFIG_DISABLE_HOQ_LIFE_MASK_HO,
			  IB_VL_CONFIG_DISABLE_HOQ_LIFE_SHIFT, disable_hoq_life);
}
/*
* PARAMETERS
*	p_vl_config
*		[out] Pointer to a vl config block.
*
*	disable_hoq_life
*		[in] disable_hoq_life value to set.
*
* RETURN VALUE
*	N/A
*
* NOTES
*
* SEE ALSO
*********/

#define IB_QOS_CONFIG_VL_ATTR_MOD_PROFILE_EN_BIT    0x80000000
#define IB_QOS_CONFIG_VL_START_PROFILE              1
#define IB_QOS_CONFIG_VL_INVALID_PROFILE            0
#define IB_QOS_CONFIG_VL_DATA_TYPE_BITMASK_IB       0x0001

/****s* IBA Base: Types/qos_config_vl_t
* NAME
*	Types/qos_config_vl_t
*
* DESCRIPTION
*	IBA defined QoSConfigVL. (vendor specific MAD)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct qos_config_vl {
	 ib_vl_config_block_t vl_config_block[IB_DROP_VL];
	 ib_net32_t reserved;
} PACK_SUFFIX ib_qos_config_vl_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	vl_config_block
*		Array of configurations, one for each VL 0-14.
*/

#define IB_ENDP_MAX_NUMBER_PORTS	256
#define IB_PORT_MASK_BYTES_SIZE (IB_ENDP_MAX_NUMBER_PORTS/8)

/****d* IBA Base: Constants/IB_SPST_STATE_SIZE
* NAME
*	IB_SPST_STATE_SIZE
*
* DESCRIPTION
*	SPST (Switch Port State Table) mad includes the state of block
*	of the switch ports.
*	This constant defines what is the size in bits for each port state
*
* SOURCE
*/
#define IB_SPST_STATE_SIZE 4

/****d* IBA Base: Constants/IB_SPST_BLOCK_SIZE
 * Name
 *	IB_SPST_BLOCK_SIZE
 *
 * DESCRIPTION
 *	SPST (Switch Port State Table) mad describes the state of a block
 *	of the switch ports.
 *	This constant defines the number of ports in a port state mad block.
 *	Note: IB_SMP_DATA_SIZE is given in bytes (need to convert to bits)
 *
 * SYNOPSIS
 */
#define IB_SPST_BLOCK_SIZE	((IB_SMP_DATA_SIZE * 8)/IB_SPST_STATE_SIZE)

#define IB_SPST_NUM_BLOCKS	(IB_ENDP_MAX_NUMBER_PORTS/IB_SPST_BLOCK_SIZE)

/****d* OpenSM: Constants/OSM_SPST_PORT_STATE_MASK_ATTR_MOD_HO
 * Name
 *	OSM_SPST_PORT_STATE_MASK_ATTR_MOD_HO
 *
 * DESCRIPTION
 *	Bit 0 of SPST (Switch Port State Table) MAD attribute modifier is used
 *	for block number.
 *
 * SYNOPSIS
 */
#define OSM_SPST_PORT_STATE_MASK_ATTR_MOD_HO	0x1

/*
 * Indicate to the device that the sender supports extended port state reporting,
 * which includes state 5=active_defer.
 */
#define IB_SPST_ACT_DEFER_STATE_SUPPORTED_MASK_HO 0x00008000

#define IB_NUM_PORT_STATE_ELEMENTS_IN_BLOCK	64
/****s* IBA Base: Types/ib_switch_port_state_table_t
* NAME
*	ib_switch_port_state_table_t
*
* DESCRIPTION
*	IBA defined SwitchPortState table. (14.2.5.20)
*
* SYNOPSIS
*/

#include <complib/cl_packon.h>
typedef struct _ib_switch_port_state_table {
	uint8_t port_states[IB_NUM_PORT_STATE_ELEMENTS_IN_BLOCK];
} PACK_SUFFIX ib_switch_port_state_table_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*
*	ports_states
*		each entry contains the state of 2 switch external ports
*		the first 4 bits of each entry would be considered as even state
*		the last 4 bits of each entry would be considered as odd state
*
*/

#define IB_SPST_STATE_ODD_STATE_SHIFT		0
#define IB_SPST_STATE_EVEN_STATE_SHIFT		4
#define IB_SPST_STATE_ODD_STATE_MASK		0x0F
#define IB_SPST_STATE_EVEN_STATE_MASK		0xF0

#define IB_VS_DATA_SIZE 224
/****s* IBA Base: Types/ib_vs_mad_t
* NAME
*	ib_vs_mad_t
*
* DESCRIPTION
*	IBA defined Vendor MAD format. (16.5.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vs_mad_t {
	ib_mad_t header;
	ib_net64_t vend_key;
	uint8_t data[IB_VS_DATA_SIZE];
} PACK_SUFFIX ib_vs_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	header
*		Common MAD header.
*
*	vend_key
*		VS_Key of the vendor specific MAD.
*
*	data
*		data of the VS MAD.
*
* SEE ALSO
* ib_mad_t
*********/

/****f* IBA Base: Types/ib_vs_mad_get_data_ptr
* NAME
*	ib_vs_mad_get_data_ptr
*
* DESCRIPTION
*	Gets a pointer to the VS MAD's data area.
*
* SYNOPSIS
*/
static inline void * OSM_API
ib_vs_mad_get_data_ptr(const ib_vs_mad_t * const p_vs_mad)
{
	return ((void *)p_vs_mad->data);
}
/*
* PARAMETERS
*	p_vs_mad
*		Pointer to the VS MAD packet.
*
* RETURN VALUES
*	Pointer to VS MAD payload area.
*
* SEE ALSO
*	ib_mad_t
*********/

/* Vendor Specific ClassPortInfo capability mask */
#define IB_VS_CAPMASK_VENDKEY_SUPPORTED		CL_HTON16(0x0100)

/****s* IBA Base: Types/ib_vend_key_info
* NAME
*	ib_vend_key_info
*
* DESCRIPTION
*	IBA defined VendKeyInfo attribute
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_vend_key_info {
	ib_net64_t vend_key;
	ib_net16_t protect_bit;
	ib_net16_t lease_period;
	ib_net16_t violations;
} PACK_SUFFIX ib_vend_key_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	vend_key
*		8-byte VS Key.
*
*	protect_bit
*		Bit 15 is a VS Key protection bit,
*		other 15 bits are reserved.
*
*	lease_period
*		How long the VS Key protect bit is to remain non-zero.
*
*	violations
*		Number of received MADs that violated VS Key.
*
* SEE ALSO
*	ib_vs_mad_t
*********/

#define VS_TRAP_NUM_KEY_VIOLATION			(CL_HTON16(0000))
#define CC_TRAP_NUM_KEY_VIOLATION			(CL_HTON16(0000))
#define N2N_TRAP_NUM_KEY_VIOLATION				(CL_HTON16(0000))
#define N2N_TRAP_NUM_KEY_MISCONFIG				(CL_HTON16(0001))

#define IB_N2N_DATA_SIZE					224
#define IB_NUM_NEIGHBOR_REC_IN_BLOCK				14

#define N2N_NEIGHBORS_INFO_BLOCK_MASK				0xF
#define N2N_NEIGHBORS_BLOCK_NODE_TYPE_SHIFT			5

/****s* IBA Base: Types/ib_neighbor_record
* NAME
*	ib_neighbor_record
*
* DESCRIPTION
*	Information regarding the remote node connected to the port corresponds
*	to this entry of Neighbors Info MAD (N2N class).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct ib_neighbor_record {
	uint8_t node_type; // 3 MSB
	uint8_t reserved1;
	uint16_t lid;
	ib_net32_t reserved2;
	ib_net64_t n2n_key;
} PACK_SUFFIX ib_neighbor_record_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	node_type
*		Encoded node type as returned in the NodeInfo attribute of the
*		remote node.
*
*	lid
*		Lid of remote node.
*
*	n2n_key
*		Node to Node key of the remote node.
*
* SEE ALSO
*********/

/****s* IBA Base: Types/ib_neighbors_info
* NAME
*	ib_neighbors_info
*
* DESCRIPTION
*	Neighbors Info MAD (N2N class).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct ib_neighbors_info {
	ib_neighbor_record_t neighbor_record[IB_NUM_NEIGHBOR_REC_IN_BLOCK];
} PACK_SUFFIX ib_neighbors_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	neighbor_record
*		Structure holds information regarding the remote nodes of this
*		switch.	Each entry represents the node connected to corresponding
*		port of this node.
*
* SEE ALSO
*********/

#define IB_PFRN_CAPABILITY_POLICY_SUP			(CL_HTON32((uint32_t)1))
#define IB_PFRN_POLICY_TREE				0
#define IB_PFRN_POLICY_DFP				1

#define IB_PFRN_MAX_SL					0xF
#define IB_PFRN_MAX_MASK_CLEAR				0x7FF
#define IB_PFRN_MAX_MASK_FORCE_CLEAR			0x7FF

/****s* IBA Base: Types/ib_pfrn_config_t
* NAME
*	ib_pfrn_config_t
*
* DESCRIPTION
*	IBA defined Vendor MAD format (vendor specific MAD).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct ib_pfrn_config {
	ib_net32_t capability_bitmask;
	ib_net32_t reserved1;
	uint8_t sl;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t policy;
	ib_net16_t mask_force_clear_timeout;
	ib_net16_t mask_clear_timeout;
	ib_net32_t reserved[11];
} PACK_SUFFIX ib_pfrn_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	capability_bitmask
*		Bit 0: is policy field supported
*
* 	sl
* 		Bits 7-4: SL for pFRN communication
*
* 	policy
* 		pFRN policy (4 LSB). Supported values:
*		0: Device shall trigger pFRN packets for subgroup 0 only.
*		1: Device shall trigger pFRN packets for subgroup 0 and subgroup 1.
*
* 	mask_force_clear_timeout
* 		Bits 9-0: Maximal time (in seconds) since last mask clear, after
* 		which mask must cleared. During that period, mask bits may have
* 		been set.
*
* 	mask_clear_timeout
* 		Bits 9-0: Time since latest pFRN for a specific	sub group was
* 		received, after which the entire mask must be cleared
*
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_pfrn_config_mad_is_policy_supported
* NAME
*	ib_pfrn_config_mad_is_policy_supported
*
* DESCRIPTION
*	Returns whether policy bit of pFRNConfig MAD is supported, according to
*	pFRNConfig capability mask.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_pfrn_config_mad_is_policy_supported(const ib_pfrn_config_t * const p_pfrn_config)
{
	return p_pfrn_config->capability_bitmask & IB_PFRN_CAPABILITY_POLICY_SUP;
}
/*
* PARAMETERS
*	p_pfrn_config
*		Pointer to the pFRNConfig MAD packet.
*
* RETURN VALUES
*	True if policy bit of pFRNConfig MAD is supported, according to
*	pFRNConfig capability mask.
*
* SEE ALSO
*	ib_mad_t
*********/

/****s* IBA Base: Types/ib_n2n_key_info
* NAME
*	ib_n2n_key_info
*
* DESCRIPTION
*	IBA defined N2NKeyInfo attribute
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_n2n_key_info {
	ib_net64_t n2n_mgr_key;
	ib_net16_t protect_bit;
	ib_net16_t lease_period;
	ib_net16_t violations;
	ib_net16_t node_key_violations;
} PACK_SUFFIX ib_n2n_key_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	n2n_mgr_key
*		8-byte N2N Manager Key (key from SM to node).
*
*	protect_bit
*		Bit 15 is a N2N Key protection bit,
*		other 15 bits are reserved.
*
*	lease_period
*		How long the N2N Key protect bit is to remain non-zero.
*
*	violations
*		Number of received MADs that violated N2N Manager Key.
*
*	node_key_violations
*		Number of received MADs that violated N2N (between nodes) Key.
*
* SEE ALSO
*	ib_n2n_mad_t
*********/

/****s* IBA Base: Types/ib_n2n_mad_t
* NAME
*	ib_n2n_mad_t
*
* DESCRIPTION
*	IBA defined Vendor MAD format. (16.5.1)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_n2n_mad_t {
	ib_mad_t header;
	ib_net64_t n2n_mgr_key;
	uint8_t data[IB_N2N_DATA_SIZE];
} PACK_SUFFIX ib_n2n_mad_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	header
*		Common MAD header.
*
*	n2n_mgr_key
*		N2N_mgr_key of the N2N Class MAD for SM to node communication.
*
*	data
*		data of the N2N MAD.
*
* SEE ALSO
* ib_mad_t
*********/

/****f* IBA Base: Types/ib_n2n_mad_get_data_ptr
* NAME
*	ib_n2n_mad_get_data_ptr
*
* DESCRIPTION
*	Gets a pointer to the N2N MAD's data area.
*
* SYNOPSIS
*/
static inline void * OSM_API
ib_n2n_mad_get_data_ptr(const ib_n2n_mad_t * const p_n2n_mad)
{
	return ((void *)p_n2n_mad->data);
}
/*
* PARAMETERS
*	p_n2n_mad
*		Pointer to the N2N MAD packet.
*
* RETURN VALUES
*	Pointer to N2N MAD payload area.
*
* SEE ALSO
*	ib_mad_t
*********/

#define IB_N2N_CAPMASK_DUAL_KEY_SUPPORTED_BIT		9

/****f* IBA Base: Types/ib_n2n_cpi_is_cap_supported
* NAME
*	ib_n2n_cpi_is_cap_supported
*
* DESCRIPTION
*	Check whether a given capability is supported
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_n2n_cpi_is_cap_supported(ib_class_port_info_t const * const n2n_cpi,
			    const uint8_t cap_bit)
{
	uint16_t bit_in_mask = (uint16_t)1 << (cap_bit % 16);

	return ((cl_ntoh16(n2n_cpi->cap_mask) & bit_in_mask) != 0);
}

/****f* IBA Base: Types/ib_n2n_cpi_is_dual_key_supported
* NAME
*	ib_n2n_cpi_is_dual_key_supported
*
* DESCRIPTION
*	Check whether a dual key capability is supported
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_n2n_cpi_is_dual_key_supported(ib_class_port_info_t const * const n2n_cpi)
{
	return ib_n2n_cpi_is_cap_supported(n2n_cpi, IB_N2N_CAPMASK_DUAL_KEY_SUPPORTED_BIT);
}

#define	IB_SL2VL_PORTMASK_SIZE 128
#define	IB_SL2VL_PORT_MASK_BYTES_SIZE (IB_SL2VL_PORTMASK_SIZE/8)

/****s* IBA Base: Types/ib_sl2vl_mapping_table
* NAME
*	ib_sl2vl_mapping_table_t
*
* DESCRIPTION
*	IBA defined SLtoVLMappingTable Attribute. (14.2.6.8)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_sl2vl_mapping_table {
	uint8_t raw_vl_by_sl[IB_MAX_NUM_VLS / 2];
	uint8_t reserved[24];
	uint8_t input_port_mask[IB_SL2VL_PORT_MASK_BYTES_SIZE];
	uint8_t output_port_mask[IB_SL2VL_PORT_MASK_BYTES_SIZE];
} PACK_SUFFIX ib_sl2vl_mapping_table_t;
#include <complib/cl_packoff.h>

/****d* OpenSM: Constants/IB_HIERARCHY_INFO_ATTR_MOD_INDEX_MASK
 * Name
 *     IB_HIERARCHY_INFO_ATTR_MOD_INDEX_MASK
 *
 * DESCRIPTION
 *      AttributeModifier[15:8] defines the index of the hierarchy info block.
 *
 * SYNOPSIS
 */
#define IB_HIERARCHY_INFO_ATTR_MOD_INDEX_MASK		0x0000FF00
#define IB_HIERARCHY_INFO_ATTR_MOD_INDEX_SHIFT		8

/****d* OpenSM: Constants/IB_HIERARCHY_INFO_ATTR_MOD_PORT_MASK
 * Name
 *     IB_HIERARCHY_INFO_ATTR_MOD_PORT_MASK
 *
 * DESCRIPTION
 *      AttributeModifier[7:0] selects the port upon which the operation
 *      specified by the SMP is performed. For switches, the range of values is between 0 to N,
 *      where N is the number of switch ports. For CAs and routers, these bits are reserved
 *      and the operation is performed on the port that receives the SMP.
 *
 * SYNOPSIS
 */
#define IB_HIERARCHY_INFO_ATTR_MOD_PORT_MASK		0x000000FF
#define IB_HIERARCHY_INFO_LEVELS_NUM 			13

/****s* IBA Base: Types/ib_hierarchy_info
* NAME
*	ib_hierarchy_info_t
*
* DESCRIPTION
*	IBA defined HierarchyInfo. (Annex A15)
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_hierarchy_info {
	ib_net64_t template_guid;
	uint8_t max_supported_index;
	uint8_t max_active_index;
	uint8_t active_levels;
	uint8_t resv1;
	ib_net32_t levels[IB_HIERARCHY_INFO_LEVELS_NUM];
} PACK_SUFFIX ib_hierarchy_info_t;
#include <complib/cl_packoff.h>

#define IB_HIERARCHY_INFO_ACTIVE_LEVELS_MASK	0xF0

/****f* IBA Base: Types/ib_hierarchy_info_get_active_levels
* NAME
*	ib_hierarchy_info_get_active_levels
*
* DESCRIPTION
*	Returns the hierarchy info active levels.
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_hierarchy_info_get_active_levels(IN const ib_hierarchy_info_t * const p_hi)
{
	return ((uint8_t) ((p_hi->active_levels & IB_HIERARCHY_INFO_ACTIVE_LEVELS_MASK) >> 4));
}

/*
* PARAMETERS
*	p_hi
*		[in] Pointer to a HierarchyInfo attribute.
*
* RETURN VALUES
*	Hierarchy Info active levels.
*
* NOTES
*
* SEE ALSO
*********/


#define IB_CHASSIS_ID_SIZE 16

#include <complib/cl_packon.h>
typedef struct _ib_chassis_info {
	uint32_t reserved;
	uint8_t chassis_id[IB_CHASSIS_ID_SIZE];
} PACK_SUFFIX ib_chassis_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
*	chassis_id
*		Chassis ID, null terminated string.
*		chassis_id is the serial number of the chassis.
*
* SEE ALSO
*
*********/

#define IB_PROFILES_FEATURE_MASK_SHIFT				8
#define IB_PROFILE_CONFIG_NUM_PROFILES_PER_BLOCK		128
#define IB_PROFILE_CONFIG_ODD_PORT_MASK				0x0F
#define IB_PROFILE_CONFIG_EVEN_PORT_MASK			0xF0
#define IB_PROFILE_CONFIG_EVEN_PORT_MASK_SHIFT			4
#define IB_PROFILE_CONFIG_MAX_PROFILE				7
/*
 * Invalid profile according to currently supported features: Fast Recovery and NVL HBF.
 * Implies there are less than 15 profiles per port for a feature.
 */
#define IB_PROFILE_CONFIG_INVALID_PROFILE			0xF
#define IB_PROFILE_CONFIG_NUM_PROFILES				(IB_PROFILE_CONFIG_MAX_PROFILE + 1)
#define IB_PROFILE_CONFIG_NUM_BLOCKS				2 /* For maximal ports possible */

/****s* IBA Base: Types/ib_profiles_config_t
* NAME
*	ib_profiles_config_t
*
* DESCRIPTION
*	IBA defined ProfilesConfig attribute
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_profiles_config {
	/* Port profile is 4 bits */
	uint8_t port_profiles[IB_PROFILE_CONFIG_NUM_PROFILES_PER_BLOCK / 2];
} PACK_SUFFIX ib_profiles_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	port_profiles
*		Block of port profiles. Note each profile is 4bits.
*
* SEE ALSO
* ib_mad_t
*********/

/****d* OpenSM: Types/ib_profiles_feature_t
* NAME
*       ib_profiles_feature_t
*
* DESCRIPTION
*       Enumerates port profiles supported features
*       (Vendor Specific version 1.55.012 section 2.15)
*
* SYNOPSIS
*/
typedef enum _ib_profiles_feature {
	IB_PROFILES_FEATURE_FAST_RECOVERY,
	IB_PROFILES_FEATURE_QOS_VL_CONFIG,
	IB_PROFILES_FEATURE_DATA_TYPE_TO_VL,
	IB_PROFILES_FEATURE_NVL_HBF,
	IB_PROFILES_FEATURE_PORT_RECOVERY_POLICY,
	IB_PROFILES_FEATURE_MAX
} ib_profiles_feature_t;
/*********/

/****f* IBA Base: Types/ib_profiles_config_get_profile
* NAME
*	ib_profiles_config_get_profile
*
* DESCRIPTION
* 	Returns profile for given port offset from ib_profiles_config_t
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_profiles_config_get_profile(ib_profiles_config_t *profiles_cfg, uint8_t port)
{
	uint8_t index;

	/* MAD payload holds 128 port profiles, each profile is 4 bits */
	index = (port % IB_PROFILE_CONFIG_NUM_PROFILES_PER_BLOCK) / 2;
	if (port % 2)
		return profiles_cfg->port_profiles[index] & IB_PROFILE_CONFIG_ODD_PORT_MASK;

	return (profiles_cfg->port_profiles[index] & IB_PROFILE_CONFIG_EVEN_PORT_MASK) >>
		IB_PROFILE_CONFIG_EVEN_PORT_MASK_SHIFT;
}
/*
* PARAMETERS
*	profiles_cfg
*		Pointer to the ProfilesConfig MAD packet.
*
* 	port
* 		Port offset.
*
* RETURN VALUES
*	Profile assigned to specified port offset.
*
* SEE ALSO
*	ib_profiles_config_t
*********/

/****f* IBA Base: Types/ib_profiles_config_set_profile
* NAME
*	ib_profiles_config_set_profile
*
* DESCRIPTION
* 	Set profile for given port offset from ib_profiles_config_t
*
* SYNOPSIS
*/
static inline void OSM_API
ib_profiles_config_set_profile(ib_profiles_config_t *profiles_cfg, uint8_t port, uint8_t profile)
{
	uint8_t index;

	/* MAD payload holds 128 port profiles, each profile is 4 bits */
	CL_ASSERT(profile <= 0xF);

	index = (port % IB_PROFILE_CONFIG_NUM_PROFILES_PER_BLOCK) / 2;
	if (port % 2) {
		profiles_cfg->port_profiles[index] &= IB_PROFILE_CONFIG_EVEN_PORT_MASK;
		profiles_cfg->port_profiles[index] |= profile;
	} else {
		profiles_cfg->port_profiles[index] &= IB_PROFILE_CONFIG_ODD_PORT_MASK;
		profiles_cfg->port_profiles[index] |= profile << IB_PROFILE_CONFIG_EVEN_PORT_MASK_SHIFT;
	}
}
/*
* PARAMETERS
*	profiles_cfg
*		Pointer to the ProfilesConfig MAD packet.
*
* 	port
* 		Port offset.
*
*	profile
*		Profile to assign to specified port offset.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	ib_profiles_config_t
*********/

/****s* IBA Base: Types/ib_credit_watchdog_config_t
* NAME
*	ib_credit_watchdog_config_t
*
* DESCRIPTION
*	IBA defined CreditWatchdogConfig attribute
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_credit_watchdog_config {
	uint32_t reserved0[2];
	uint8_t enable_traps;
	uint8_t reserved1[2];
	uint8_t err_thr_act_en_thr;
	uint8_t reserved2[3];
	uint8_t error_thr;
	uint8_t reserved3[3];
	uint8_t warning_thr;
	uint8_t reserved4[3];
	uint8_t normal_thr;
	uint8_t reserved5[3];
	uint8_t time_window;
	uint8_t reserved6[3];
	uint8_t sampling_rate;
} PACK_SUFFIX ib_credit_watchdog_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	enable_traps
* 		NOTE! CURRENTLY FORCED TO 0 (DISABLE) BY SM
*
* 		bit 7: enable error trap
* 		bit 6: enable warning trap
* 		bit 5: enable normal trap
* 		bits 0-4: reserved
*
*		If bit is set, fast recovery trap will be sent by the device
*		when corresponds threshold is reached
*
* 	err_thr_act_en_thr
* 		bits 6-7: reserved
*
*		bits 4-5: error threshold action
*			Action required when exceeding error threshold.
*			0: Do nothing
*			1: Toggle the physical link (drop to polling)
*			2: Change logical state to Init.
*			3: Recovery
*
*		bits 2-3: reserved
*
*		bits 0-1: enable threshold. Supported values:
*			0: All disabled.
*			1: Only error threshold enabled.
*			2: Only warning and normal thresholds are enabled.
*			3: All enabled.
*
* 	error_thr
* 		error threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		When switch reaches this threshold, it drops the logical link.
*
* 	warning_thr
* 		Warning threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		Shall be smaller than error_thr when set.
*
* 	normal_thr
* 		Normal threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		Shall be smaller than warning_thr when set.
*
* 	time_window
*		The time frame which the events are grouped. Time frame to measure Normal_thr.
*		Units of sampling_rate.
*
* 	sampling_rate
* 		Time frame to measure warning and error thresholds.
* 		Units of 10msec, max value of 1sec.
*
* SEE ALSO
* 	ib_mad_t
*********/

#define IB_CREDIT_WATCHDOG_EN_ERR_TRAP			0x80
#define IB_CREDIT_WATCHDOG_EN_WARN_TRAP			0x40
#define IB_CREDIT_WATCHDOG_EN_NORM_TRAP			0x20
#define IB_CREDIT_WATCHDOG_ERR_THR_ACT			0x30
#define IB_CREDIT_WATCHDOG_ERR_THR_ACT_SHIFT		4
#define IB_CREDIT_WATCHDOG_EN_THR			0x03
#define IB_CREDIT_WATCHDOG_MAX_THRESHOLD		0x1F

typedef enum _ib_fast_recovery_threshold_type {
	IB_THR_TYPE_RESERVED,
	IB_THR_TYPE_ERROR,
	IB_THR_TYPE_WARNING,
	IB_THR_TYPE_NORMAL,
	IB_THR_TYPE_MAX,
} ib_fast_recovery_threshold_type_t;

/****f* IBA Base: Types/ib_credit_watchdog_config_set_enable_trap
* NAME
*	ib_credit_watchdog_config_set_enable_trap
*
* DESCRIPTION
* 	Set enable trap bit of CreditWatchdogConfig, according to threshold type
* 	and enabelement value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_enable_trap(ib_credit_watchdog_config_t * credit_watchdog_config,
					  ib_fast_recovery_threshold_type_t thr_type,
					  boolean_t enable_trap)
{
	uint8_t mask = 0;

	CL_ASSERT(thr_type && thr_type < IB_THR_TYPE_MAX);

	switch (thr_type) {
	case IB_THR_TYPE_ERROR:
		mask = IB_CREDIT_WATCHDOG_EN_ERR_TRAP;
		break;
	case IB_THR_TYPE_WARNING:
		mask = IB_CREDIT_WATCHDOG_EN_WARN_TRAP;
		break;
	case IB_THR_TYPE_NORMAL:
		mask = IB_CREDIT_WATCHDOG_EN_NORM_TRAP;
		break;
	default:
		return;
	}

	if (enable_trap)
		credit_watchdog_config->enable_traps |= mask;
	else
		credit_watchdog_config->enable_traps &= ~mask;
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
* 	thr_type
* 		Threshold type. Types supported: error, warning, normal.
*
* 	enable_trap
* 		Boolean indicate whether to enable trap for input threshold type
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****f* IBA Base: Types/ib_credit_watchdog_config_set_err_threshold_action
* NAME
*	ib_credit_watchdog_config_set_err_threshold_action
*
* DESCRIPTION
* 	Set error_threshold_action field of CreditWatchdogConfig, according to
* 	input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_err_threshold_action(ib_credit_watchdog_config_t * credit_watchdog_config,
						   uint8_t err_threshold_action)
{
	credit_watchdog_config->err_thr_act_en_thr &= ~IB_CREDIT_WATCHDOG_ERR_THR_ACT;
	credit_watchdog_config->err_thr_act_en_thr |=
	    err_threshold_action << IB_CREDIT_WATCHDOG_ERR_THR_ACT_SHIFT;
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****f* IBA Base: Types/ib_credit_watchdog_config_set_en_threshold
* NAME
*	ib_credit_watchdog_config_set_en_threshold
*
* DESCRIPTION
* 	Set enable_threshold field of CreditWatchdogConfig, according to input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_en_threshold(ib_credit_watchdog_config_t * credit_watchdog_config,
					   uint8_t en_threshold)
{
	CL_ASSERT(en_threshold <= IB_CREDIT_WATCHDOG_EN_THR);

	credit_watchdog_config->err_thr_act_en_thr &= ~IB_CREDIT_WATCHDOG_EN_THR;
	credit_watchdog_config->err_thr_act_en_thr |= en_threshold;
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****f* IBA Base: Types/ib_credit_watchdog_config_set_threshold
* NAME
*	ib_credit_watchdog_config_set_threshold
*
* DESCRIPTION
* 	Set threshold field of CreditWatchdogConfig, according to input
* 	threshold type and threshold value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_threshold(ib_credit_watchdog_config_t * credit_watchdog_config,
					ib_fast_recovery_threshold_type_t thr_type,
					uint8_t threshold)
{
	CL_ASSERT(threshold <= IB_CREDIT_WATCHDOG_MAX_THRESHOLD);
	CL_ASSERT(thr_type && thr_type < IB_THR_TYPE_MAX);

	switch (thr_type) {
	case IB_THR_TYPE_ERROR:
		credit_watchdog_config->error_thr = threshold;
		break;
	case IB_THR_TYPE_WARNING:
		credit_watchdog_config->warning_thr = threshold;
		break;
	case IB_THR_TYPE_NORMAL:
		credit_watchdog_config->normal_thr = threshold;
		break;
	default:
		break;
	}
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
* 	thr_type
* 		Threshold type. Types supported: error, warning, normal.
*
* 	threshold
* 		Threshold value (5bits)
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****f* IBA Base: Types/ib_credit_watchdog_config_set_time_window
* NAME
*	ib_credit_watchdog_config_set_time_window
*
* DESCRIPTION
* 	Set time window field of CreditWatchdogConfig, according to input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_time_window(ib_credit_watchdog_config_t * credit_watchdog_config,
					  uint8_t time_window)
{
	credit_watchdog_config->time_window = time_window;
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
*	time_window
*		Time window value to set
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****f* IBA Base: Types/ib_credit_watchdog_config_set_sampling_rate
* NAME
*	ib_credit_watchdog_config_set_sampling_rate
*
* DESCRIPTION
* 	Set sampling rate field of CreditWatchdogConfig, according to input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_credit_watchdog_config_set_sampling_rate(ib_credit_watchdog_config_t * credit_watchdog_config,
					    uint8_t sampling_rate)
{
	credit_watchdog_config->sampling_rate = sampling_rate;
}
/*
* PARAMETERS
*	credit_watchdog_config
*		Pointer to the CreditWatchdogConfig MAD packet.
*
*	sampling_rate
*		Sampling rate value to set
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_credit_watchdog_config_t
*********/

/****s* IBA Base: Types/ib_ber_config_t
* NAME
*	ib_ber_config_t
*
* DESCRIPTION
*	IBA defined BERConfig attribute
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_ber_config {
	uint32_t reserved0[2];
	uint8_t enable_traps;
	uint8_t reserved1[2];
	uint8_t err_thr_act_en_thr;
	uint8_t reserved2[2];
	ib_net16_t error_thr;
	uint8_t reserved3[2];
	ib_net16_t warning_thr;
	uint8_t reserved4[2];
	ib_net16_t normal_thr;
	ib_net32_t time_window;
	uint8_t reserved5[3];
	uint8_t sampling_rate;
} PACK_SUFFIX ib_ber_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	enable_traps
* 		NOTE! CURRENTLY FORCED TO 0 (DISABLE) BY SM
*
* 		bit 7: enable error trap
* 		bit 6: enable warning trap
* 		bit 5: enable normal trap
* 		bits 0-4: reserved
*
*		If bit is set, fast recovery trap will be sent by the device
*		when corresponds threshold is reached
*
* 	err_thr_act_en_thr
* 		bits 6-7: reserved
*
*		bits 4-5: error threshold action
*			Action required when exceeding error threshold.
*			0: Do nothing
*			1: Toggle the physical link (drop to polling)
*			2: Change logical state to Init.
*			3: Recovery
*
*		bits 2-3: reserved
*
*		bits 0-1: enable threshold. Supported values:
*			0: All disabled.
*			1: Only error threshold enabled.
*			2: Only warning and normal thresholds are enabled.
*			3: All enabled.
*
* 	error_thr
* 		Error threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		When switch reaches this threshold, it drops the logical link.
* 		When default_thr (on attribute modifier) is set, reflects the device
* 		default value.
*
* 	warning_thr
* 		Warning threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		Shall be smaller than error_thr when set.
* 		When default_thr (on attribute modifier) is set, reflects the device
* 		default value.
*
* 	normal_thr
* 		Normal threshold, refers to number of credit watchdog events that
* 		occurred in the specific time window.
* 		Shall be smaller than warning_thr when set.
* 		When default_thr (on attribute modifier) is set, reflects the device
* 		default value.
*
* 	time_window
*		The time frame which the events are grouped. READ ONLY - derives
*		from lowest threshold.
*		Units of sampling_rate.
*
* 	sampling_rate
* 		Time frame to measure warning and error thresholds.
* 		Units of 10msec, max value of 1sec.
*
* SEE ALSO
* 	ib_mad_t
*********/

typedef enum _ib_ber_type {
	IB_BER_TYPE_RAW,
	IB_BER_TYPE_EFFECTIVE,
	IB_BER_TYPE_SYMBOL,
	IB_BER_TYPE_MAX,
} ib_ber_type_t;

#define IB_BER_NUM_TYPES			IB_BER_TYPE_MAX

#define IB_BER_ERR_THR_ACT			0x30
#define IB_BER_ERR_THR_ACT_SHIFT		4
#define IB_BER_EN_THR				0x03
#define IB_BER_MAX_THRESHOLD_MANTISSA		0xF

#define IB_BER_ATTR_MOD_TYPE_SHIFT		8
#define IB_BER_ATTR_MOD_DEFAULT_THR_SHIFT	31
#define IB_BER_ATTR_MOD_TYPE_MASK		0x80000000
#define IB_BER_ATTR_MOD_DEFAULT_THR_MASK	0x00000700
#define IB_BER_ATTR_MOD_PROFILE_MASL		0x00000007

/****f* IBA Base: Types/ib_ber_config_set_err_threshold_action
* NAME
*	ib_ber_config_set_err_threshold_action
*
* DESCRIPTION
* 	Set error_threshold_action field of BERConfig, according to
* 	input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ber_config_set_err_threshold_action(ib_ber_config_t * ber_config,
				       uint8_t err_threshold_action)
{
	ber_config->err_thr_act_en_thr &= ~IB_BER_ERR_THR_ACT;
	ber_config->err_thr_act_en_thr |= err_threshold_action << IB_BER_ERR_THR_ACT_SHIFT;
}
/*
* PARAMETERS
*	ber_config
*		Pointer to the BERConfig MAD packet.
*
*	err_threshold_action
*		Error threshold action to set.
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_ber_config_t
*********/

#define IB_BER_THR_MASK_EXPONENT			0x00FF
#define IB_BER_THR_EXPONENT_MAX				0x11
#define IB_BER_THR_MASK_MANTISSA			0x0F00
#define IB_BER_THR_MANTISSA_MAX				9
#define IB_BER_THR_MANTISSA_SHIFT			8

/****f* IBA Base: Types/ib_ber_config_set_threshold
* NAME
*	ib_ber_config_set_threshold
*
* DESCRIPTION
* 	Set threshold field of BERConfig, according to input threshold
* 	threshold type (error / warning / normal)
*
* SYNOPSIS
*/
#include <stdio.h>
static inline void OSM_API
ib_ber_config_set_threshold(ib_ber_config_t * ber_config,
			    ib_fast_recovery_threshold_type_t thr_type,
			    uint16_t threshold)
{
	CL_ASSERT(thr_type && thr_type < IB_THR_TYPE_MAX);

	ib_net16_t net_threshold = cl_hton16(threshold);

	/* Which threshold type to set: error / warning / normal */
	switch (thr_type) {
	case IB_THR_TYPE_ERROR:
		ber_config->error_thr = net_threshold;
		break;
	case IB_THR_TYPE_WARNING:
		ber_config->warning_thr = net_threshold;
		break;
	case IB_THR_TYPE_NORMAL:
		ber_config->normal_thr = net_threshold;
		break;
	default:
		break;
	}
}
/*
* PARAMETERS
*	ber_config
*		Pointer to the BERConfig MAD packet.
*
* 	thr_type
* 		Threshold type. Types supported: error, warning, normal.
*
*	threshold
*		Threshold value of 12 bits:
*			bits 8-11: mantissa
*			bits 0-7: exponent
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_ber_config_t
*********/

/****f* IBA Base: Types/ib_ber_config_set_en_threshold
* NAME
*	ib_ber_config_set_en_threshold
*
* DESCRIPTION
* 	Set enable_threshold field of BERConfig, according to input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ber_config_set_en_threshold(ib_ber_config_t * ber_config,
			       uint8_t en_threshold)
{
	CL_ASSERT(en_threshold <= IB_BER_EN_THR);

	ber_config->err_thr_act_en_thr &= ~IB_BER_EN_THR;
	ber_config->err_thr_act_en_thr |= en_threshold;
}
/*
* PARAMETERS
*	ber_config
*		Pointer to the BERConfig MAD packet.
*
*	en_threshold
*		value to set.
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_ber_config_t
*********/

/****f* IBA Base: Types/ib_ber_config_set_sampling_rate
* NAME
*	ib_ber_config_set_sampling_rate
*
* DESCRIPTION
* 	Set sampling rate field of BERConfig, according to input value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_ber_config_set_sampling_rate(ib_ber_config_t * ber_config,
				uint8_t sampling_rate)
{
	ber_config->sampling_rate = sampling_rate;
}
/*
* PARAMETERS
*	ber_config
*		Pointer to the BERConfig MAD packet.
*
*	sampling_rate
*		Sampling rate value to set
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_ber_config_t
*********/

#define IB_PORT_RECOVERY_POLICY_START_PROFILE       1
#define IB_PORT_RECOVERY_POLICY_DISABLE_PROFILE     1
#define IB_PORT_RECOVERY_POLICY_DEFAULT_PROFILE     0

/****s* IBA Base: Types/ib_port_recovery_policy_config_t
* NAME
*	ib_port_recovery_policy_config_t
*
* DESCRIPTION
*	The port recovery policy allows to define how a port
*	should be recovered in case the link goes down.
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_port_recovery_policy_config {
	ib_net16_t recovery_type_capability;
	ib_net16_t recovery_type_en;
	uint8_t reserved0[3];
	uint8_t draining_timeout;
	uint8_t reserved1[2];
	ib_net16_t link_down_timeout;
} PACK_SUFFIX ib_port_recovery_policy_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	recovery_type_capability
* 		Indicate the device's supported recovery types.
*
* 		bit 0: Host logic re-lock
* 		bit 1: Host SerDes FEQ
* 		bit 2: Module TX disable
* 		bit 3: Module datapath full toggle
* 		Else, reserved.
*
* 	recovery_type_en
* 		Enable bitmap specifying per recovery type whether it is disabled or enabled.
*
* 		bit 0: Host logic re-lock
* 		bit 1: Host SerDes FEQ
* 		bit 2: Module TX disable
* 		bit 3: Module datapath full toggle
* 		Else, reserved.
*
* 	draining_timeout
* 		The time in msec from the start of the recovery process
* 		until port draining starts.
* 		Units of 10 msec, range 0-100, e.g.
* 		0: Disabled, start draining immediately.
* 		1: 10 msec
* 		2: 20 msec
* 		3: 30 msec
* 		...
* 		100: 1000 msec
* 		Must be equal or less then link_down_timeout.
*
* 	link_down_timeout
* 		The time in msec from the start of the recovery process
* 		until the port is brought down.
* 		Units of 10 msec, range 0-3000, e.g.
* 		0: The port will be brought down immediately.
* 		1: 10 msec
* 		2: 20 msec
* 		3: 30 msec
* 		...
* 		3000: 30 sec
* 		Value of 0xFFFFFF means all enabled recovery types will be used.
*
* SEE ALSO
* 	ib_mad_t
*********/

#define IB_ALID_INFO_ALIDS_PER_BLOCK			16
#define IB_ALID_INFO_ENTRY_ALID_MASK			CL_HTON32(0x000FFFFF)
#define IB_ALID_INFO_ENTRY_IS_USED_BIT			CL_HTON32(0x01000000)

#include <complib/cl_packon.h>
typedef struct _ib_alid_entry {
	ib_net32_t alid;
} PACK_SUFFIX ib_alid_entry_t;
#include <complib/cl_packoff.h>
/*
* PARAMETERS
*	alid
*		32 bits field to represent GPU Anycast LID entry of AnycastLIDInfo MAD.
*		bits 0-19: ALID.
*			   Currently SM supports up to 16bits LID.
*		bits 20-23: reserved.
*		bits 24-31: Properties.
*			bit 24: IsUsed.
*				Set by SM to indicate which ALID should be currently used.
*			bits 25-31: reserved.
*
* SEE ALSO
*	ib_alid_info_t
*********/

#include <complib/cl_packon.h>
typedef struct _ib_alid_info {
	ib_alid_entry_t alid_block[IB_ALID_INFO_ALIDS_PER_BLOCK];
} PACK_SUFFIX ib_alid_info_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_alid_info_get_alid_by_index
* NAME
*	ib_alid_info_get_alid_by_index
*
* DESCRIPTION
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_alid_info_get_alid_by_index(ib_alid_info_t * p_alid_info, uint8_t index)
{
	uint32_t alid = cl_ntoh32(p_alid_info->alid_block[index].alid & IB_ALID_INFO_ENTRY_ALID_MASK);
	/* ALID is currently 16bits */
	return (uint16_t)alid;
}
/*
* PARAMETERS
*	p_alid_info
*		Pointer ALIDInfo payload
*
*	index
*		Index of required ALID from ALIDInfo table
*
* RETURN VALUES
*	ALID of input index
*
* SEE ALSO
*	ib_alid_info_t
*********/

/****f* IBA Base: Types/ib_alid_info_is_used_alid_by_index
* NAME
*	ib_alid_info_is_used_alid_by_index
*
* DESCRIPTION
* 	Returns whether ALID of input index entry is marked as used
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_alid_info_is_used_alid_by_index(ib_alid_info_t * p_alid_info, uint8_t index)
{
	ib_net32_t is_used = p_alid_info->alid_block[index].alid & IB_ALID_INFO_ENTRY_IS_USED_BIT;

	return is_used ? TRUE : FALSE;
}
/*
* PARAMETERS
*	p_alid_info
*		Pointer ALIDInfo payload
*
*	index
*		Index of required ALID to check from ALIDInfo table
*
* RETURN VALUES
* 	TRUE if ALID of input index entry is marked as used. Otherwise, FALSE.
*
* SEE ALSO
*	ib_alid_info_t
*********/

#define IB_END_PORT_PLANE_FILTER_CONFIG_NUM_ENTRIES	4
#define IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_MASK	0x000FFFFF
#define IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_SHIFT	0

#include <complib/cl_packon.h>
typedef struct _ib_end_port_plane_filter_config {
	ib_net64_t reserved;
	ib_net32_t plane_filters[IB_END_PORT_PLANE_FILTER_CONFIG_NUM_ENTRIES]; /* 12b reserved, 20b lid. */
} PACK_SUFFIX ib_end_port_plane_filter_config_t;
#include <complib/cl_packoff.h>

static inline uint32_t OSM_API ib_end_port_plane_filter_config_get_lid_ho(
	IN ib_end_port_plane_filter_config_t * p_end_port_plane_filter_config, IN uint8_t entry)
{
	return IBTYPES_GET_ATTR32(p_end_port_plane_filter_config->plane_filters[entry],
				  IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_MASK,
				  IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_SHIFT);
}

static inline void OSM_API ib_end_port_plane_filter_config_set_lid_ho(
	IN ib_end_port_plane_filter_config_t * p_end_port_plane_filter_config, IN uint8_t entry,
	IN uint32_t lid_ho)
{
	IBTYPES_SET_ATTR32(p_end_port_plane_filter_config->plane_filters[entry],
			   IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_MASK,
			   IB_END_PORT_PLANE_FILTER_CONFIG_ENTRY_LID_SHIFT, lid_ho);
}

#define IB_RAIL_FILTER_CFG_NVLINK_VL_MASK		CL_HTON16(0x000C)
#define IB_RAIL_FILTER_CFG_MC_ENABLE_BIT		0x01
#define IB_RAIL_FILTER_CFG_UC_ENABLE_BIT		0x02
#define IB_RAIL_FILTER_CFG_INGRESS_MASK_LEN		128
#define IB_RAIL_FILTER_CFG_EGRESS_MASK_LEN		256
#define IB_RAIL_FILTER_CFG_PORT_MASK_BLOCK_SIZE		8
#define IB_RAIL_FILTER_CFG_INGRESS_MASK_BYTES_LEN	IB_RAIL_FILTER_CFG_INGRESS_MASK_LEN / \
							IB_RAIL_FILTER_CFG_PORT_MASK_BLOCK_SIZE
#define IB_RAIL_FILTER_CFG_EGRESS_MASK_BYTES_LEN	IB_RAIL_FILTER_CFG_EGRESS_MASK_LEN / \
							IB_RAIL_FILTER_CFG_PORT_MASK_BLOCK_SIZE

#define IB_RAIL_FILTER_ATTR_MOD_PORT_NUM_MASK		CL_NTOH32(0x00FF)
#define IB_RAIL_FILTER_ATTR_MOD_INGRESS_BLOCK_MASK	CL_NTOH32(0x0F00)
#define IB_RAIL_FILTER_ATTR_MOD_EGRESS_BLOCK_MASK	CL_NTOH32(0xF000)
#define IB_RAIL_FILTER_ATTR_MOD_INGRESS_BLOCK_SHIFT	16
#define IB_RAIL_FILTER_ATTR_MOD_EGRESS_BLOCK_SHIFT	24

#include <complib/cl_packon.h>
typedef struct _ib_rail_filter_config {
	uint8_t reserved;
	uint8_t mc_uc_enable;
	ib_net16_t vl_mask;
	uint8_t ingress_port_mask[IB_RAIL_FILTER_CFG_INGRESS_MASK_BYTES_LEN];
	uint8_t egress_port_mask[IB_RAIL_FILTER_CFG_EGRESS_MASK_BYTES_LEN];
} PACK_SUFFIX ib_rail_filter_config_t;
#include <complib/cl_packoff.h>

#define IB_NVL_HBF_CONFIG_FIELDS_ENABLE_ALL		0x00011FFF00011FFF
#define IB_NVL_HBF_CONFIG_HASH_TYPE_MASK		0xF0
#define IB_NVL_HBF_CONFIG_HASH_TYPE_SHIFT		4
#define IB_NVL_HBF_CONFIG_ATTR_MOD_PROFILE_EN_BIT	0x80000000

/****s* IBA Base: Types/ib_nvl_hbf_config_t
* NAME
*	ib_nvl_hbf_config_t
*
* DESCRIPTION
*	NVLink Hash Base Forwarding configuration (NVLHBFConfig) MAD
*	(Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_nvl_hbf_config {
	uint8_t hash_type;
	uint8_t reserved0[10];
	uint8_t packet_hash_bitmask;
	uint8_t reserved1[4];
	ib_net32_t seed;
	ib_net64_t fields_enable;
} PACK_SUFFIX ib_nvl_hbf_config_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	hash_type
* 		Hash type for NVLink HBF hash.
* 		Supported values:
* 			0: CRC
* 			1: Packed Hash - hash will be calculated according to packet_hash_bitmask
*
*	packet_hash_bitmask
*		Bitmask used when calculating the hash for NVLink HBF.
*		The bits that are set shall be consecutive.
*		Reserved when hash_type = 0 (CRC)
*
* 	seed
* 		A seed value for NVLink HBF hash, for both UC and MC.
* 		Reserved when hash_type = 1 (packed hash)
*
* 	fields_enable
* 		A bitmask with fields for NVLink HBF hash.
* 		Reserved when hash_type = 1 (packed hash)
*/

#define IB_LFT_SPLIT_LID_MASK				0x000FFFFF

#include <complib/cl_packon.h>
/* For all LFTSplit MAD fields, 20 MSB represent the LID and 12 LSB are reserved */
typedef struct _ib_linear_forwarding_table_split {
	ib_net32_t global_lid_range_start;
	ib_net32_t global_lid_range_cap;
	ib_net32_t global_lid_range_top;
	ib_net32_t alid_range_start;
	ib_net32_t alid_range_cap;
	ib_net32_t alid_range_top;
	ib_net32_t local_plane_lid_range_start;
	ib_net32_t local_plane_lid_range_cap;
	ib_net32_t local_plane_lid_range_top;
} PACK_SUFFIX ib_linear_forwarding_table_split_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_lft_split_set_global_lid_range
* NAME
*	ib_lft_split_set_global_lid_range
*
* DESCRIPTION
*
* SYNOPSIS
*/
static inline void OSM_API
ib_lft_split_set_global_lid_range(ib_linear_forwarding_table_split_t * p_lft_split,
				  uint32_t start,
				  uint32_t cap,
				  uint32_t top)
{
	CL_ASSERT(start <= IB_LID_UCAST_END_HO &&
		  cap <= IB_LID_UCAST_END_HO &&
		  top <= IB_LID_UCAST_END_HO);

	p_lft_split->global_lid_range_start = cl_hton32(start & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->global_lid_range_cap = cl_hton32(cap & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->global_lid_range_top = cl_hton32(top & IB_LFT_SPLIT_LID_MASK);
}
/*
* PARAMETERS
*	p_lft_split
*		Pointer to the LinearForwardingTableSplit MAD packet.
*
* 	start
* 		First LID of the global LIDs range
*
* 	cap
* 		Number of entries supported in the global LID range.
*
* 	top
* 		Top of the global LID range
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_linear_forwarding_table_split_t
*********/

/****f* IBA Base: Types/ib_lft_split_set_alid_range
* NAME
*	ib_lft_split_set_alid_range
*
* DESCRIPTION
*
* SYNOPSIS
*/
static inline void OSM_API
ib_lft_split_set_alid_range(ib_linear_forwarding_table_split_t * p_lft_split,
			    uint32_t start,
			    uint32_t cap,
			    uint32_t top)
{
	CL_ASSERT(start <= IB_LID_UCAST_END_HO &&
		  cap <= IB_LID_UCAST_END_HO &&
		  top <= IB_LID_UCAST_END_HO);

	p_lft_split->alid_range_start = cl_hton32(start & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->alid_range_cap = cl_hton32(cap & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->alid_range_top = cl_hton32(top & IB_LFT_SPLIT_LID_MASK);
}
/*
* PARAMETERS
*	p_lft_split
*		Pointer to the LinearForwardingTableSplit MAD packet.
*
* 	start
* 		First LID of the ALID range
*
* 	cap
* 		Number of entries supported in the ALID range.
*
* 	top
* 		Top of the ALID range
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_linear_forwarding_table_split_t
*********/

/****f* IBA Base: Types/ib_lft_split_set_local_plane_range
* NAME
*	ib_lft_split_set_local_plane_range
*
* DESCRIPTION
*
* SYNOPSIS
*/
static inline void OSM_API
ib_lft_split_set_local_plane_range(ib_linear_forwarding_table_split_t * p_lft_split,
				   uint32_t start,
				   uint32_t cap,
				   uint32_t top)
{
	CL_ASSERT(start <= IB_LID_UCAST_END_HO &&
		  cap <= IB_LID_UCAST_END_HO &&
		  top <= IB_LID_UCAST_END_HO);

	p_lft_split->local_plane_lid_range_start = cl_hton32(start & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->local_plane_lid_range_cap = cl_hton32(cap & IB_LFT_SPLIT_LID_MASK);
	p_lft_split->local_plane_lid_range_top = cl_hton32(top & IB_LFT_SPLIT_LID_MASK);
}
/*
* PARAMETERS
*	p_lft_split
*		Pointer to the LinearForwardingTableSplit MAD packet.
*
* 	start
* 		First LID of the local plane LID range
*
* 	cap
* 		Number of entries supported in the local plane LID range.
*
* 	top
* 		Top of the local plane LID range
*
* RETURN VALUES
*	None
*
* SEE ALSO
*	ib_linear_forwarding_table_split_t
*********/

#include <complib/cl_packon.h>
typedef struct _ib_fabric_manager_info {
	uint8_t reserved[7];
	uint8_t fm_state;
	ib_net64_t time_point_last_restart;
	ib_net64_t duration_since_last_restart;
} PACK_SUFFIX ib_fabric_manager_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	fm_state
* 		Specifies the current FM state.
* 		Supported values:
* 			0: Undefined
* 			1: Offline
* 			2: Standby
* 			3: Configured
* 			4: Timeout
* 			5: Error
*
*	time_point_last_restart
*		Number of seconds since epoch.
*
* 	duration_since_last_restart
* 		Number of seconds since last restart.
*/

/************/

/****s* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_t
*
* DESCRIPTION
*	IBA defined EnhQoSArbiterInfo. (14.2.6.21)
*
* SYNOPSIS
*/

#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_RL_SUPPORTED						(1 <<  1)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_VL_GROUP0_RL_SUPPORTED					(1 <<  2)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_RL_SUPPORTED_ON_ALL_VL_GROUPS				(1 <<  3)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_VL_GROUP_BW_SHARE_SUPPORTED				(1 <<  4)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_VL_BW_SHARE_SUPPORTED				(1 <<  5)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_VL_RL_SUPPORTED					(1 <<  6)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_SL_BW_SHARE_SUPPORTED				(1 <<  7)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_SL_RL_SUPPORTED					(1 <<  8)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_VL_GROUP0_MIN_BW_SUPPORTED				(1 <<  9)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_VL_MIN_BW_SUPPORTED_ON_ALL_VL_GROUPS			(1 << 10)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_PORT_VL_MIN_BW_SUPPORTED					(1 << 11)
#define IB_ENH_QOS_ARBINFO_CAPMASK_IS_VL_INDEPENDENT_RL_AND_MIN_BW_GRANULARITY_SUPPORTED	(1 << 12)

#define IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_MASK		0xf0
#define IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_SHIFT		4

#define IB_ENH_QOS_ARBITER_SL_CAP_MASK				0x0f
#define IB_ENH_QOS_ARBITER_SL_CAP_SHIFT				0

#define IB_ENH_QOS_RL_GRANULARITY_CAP_MASK			0xf0
#define IB_ENH_QOS_RL_GRANULARITY_CAP_SHIFT			4

#define IB_ENH_QOS_RL_GRANULARITY_ENABLED_MASK			0x0f
#define IB_ENH_QOS_RL_GRANULARITY_ENABLED_SHIFT			0

#define IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_MASK			0xf0
#define IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_SHIFT			4

#define IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_MASK		0x0f
#define IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_SHIFT		0

typedef enum {
	enq_qos_arbinfo_start		= 0,			// reserved - not used
	enh_qos_arbinfo_1_mbps		= 1,
	enh_qos_arbinfo_10_mbps		= 2,
	enh_qos_arbinfo_100_mbps	= 3,
	enh_qos_arbinfo_1_gbps		= 4,
	enh_qos_arbinfo_10_gbps		= 5,
	enh_qos_arbinfo_100_gbps	= 6,
	enh_qos_arbinfo_1_tbps		= 7,
	enq_qos_arbinfo_end		= 8,			// reserved - not used
} ib_enh_qos_ariter_rate_t;

#include <complib/cl_packon.h>
typedef struct _ib_enh_qos_arbiter_info {
	uint32_t cap_mask;
	uint8_t cap_info;		/* NumberOfVLGroupsCap, QoSArbiterSLCap */
	uint8_t reserved1;		/* Reserved */
	uint8_t rate_limit_info;	/* RateLimiterGranularityCap, RateLimiterGranularityEnabled */
	uint8_t min_bw_info;		/* MinBWGranularityCap, MinBWGranularityEnabled */
	uint8_t reserved2[56];
} PACK_SUFFIX ib_enh_qos_arbiter_info_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_capability_mask
*
* DESCRIPTION
*	Get Enhanced QoS Arbiter capability mask.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_enh_qos_arbiter_info_get_cap_mask(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return cl_hton32(p_info->cap_mask);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_num_vlgroups_cap
*
* DESCRIPTION
*	Get enhanced QoS number of VL Groups Cap.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_num_vlgroups_cap(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->cap_info, IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_MASK, IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_qos_arbiter_sl_cap
*
* DESCRIPTION
*	Get highest SL value supported.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_qos_arbiter_sl_cap(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->cap_info, IB_ENH_QOS_ARBITER_SL_CAP_MASK, IB_ENH_QOS_ARBITER_SL_CAP_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_rate_limiter_granularity_cap
*
* DESCRIPTION
*	Get rate limiter granularity cap.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_rate_limiter_granularity_cap(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->rate_limit_info, IB_ENH_QOS_RL_GRANULARITY_CAP_MASK, IB_ENH_QOS_RL_GRANULARITY_CAP_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_rate_limiter_granularity_enabled
*
* DESCRIPTION
*	Get rate limiter granularity enabled.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_rate_limiter_granularity_enabled(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->rate_limit_info, IB_ENH_QOS_RL_GRANULARITY_ENABLED_MASK, IB_ENH_QOS_RL_GRANULARITY_ENABLED_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_min_bw_granularity_cap
*
* DESCRIPTION
*	Get minimum bandwidth granularity cap.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_min_bw_granularity_cap(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->min_bw_info, IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_MASK, IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_get_min_bw_granularity_enabled
*
* DESCRIPTION
*	Get minimum bandwidth granularity enabled.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_qos_arbiter_info_get_min_bw_granularity_enabled(IN const ib_enh_qos_arbiter_info_t * const p_info)
{
	return IBTYPES_GET_ATTR8(p_info->min_bw_info, IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_MASK, IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_SHIFT);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_capability_mask
*
* DESCRIPTION
*	Set Enhanced QoS Arbiter capability mask.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_cap_mask(IN ib_enh_qos_arbiter_info_t * const p_info, IN uint32_t cap_mask)
{
	p_info->cap_mask = cl_hton32(cap_mask);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_num_vlgroups_cap
*
* DESCRIPTION
*	Set enhanced QoS number of VL Groups Cap.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_num_vlgroups_cap(IN ib_enh_qos_arbiter_info_t * const p_info, IN uint8_t cap)
{
	IBTYPES_SET_ATTR8(p_info->cap_info, IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_MASK, IB_ENH_QOS_ARBITER_NUM_VLGROUPS_CAP_SHIFT, cap);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_qos_arbiter_sl_cap
*
* DESCRIPTION
*	Set highest SL value supported.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_qos_arbiter_sl_cap(IN ib_enh_qos_arbiter_info_t * const p_info, IN uint8_t cap)
{
	IBTYPES_SET_ATTR8(p_info->cap_info, IB_ENH_QOS_ARBITER_SL_CAP_MASK, IB_ENH_QOS_ARBITER_SL_CAP_SHIFT, cap);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_rate_limiter_granularity_cap
*
* DESCRIPTION
*	Set rate limiter granularity cap.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_rate_limiter_granularity_cap(IN ib_enh_qos_arbiter_info_t * const p_info, uint8_t rate)
{
	IBTYPES_SET_ATTR8(p_info->rate_limit_info, IB_ENH_QOS_RL_GRANULARITY_CAP_MASK, IB_ENH_QOS_RL_GRANULARITY_CAP_SHIFT, rate);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_rate_limiter_granularity_enabled
*
* DESCRIPTION
*	Set rate limiter granularity enabled.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_rate_limiter_granularity_enabled(IN ib_enh_qos_arbiter_info_t * const p_info, uint8_t rate)
{
	IBTYPES_SET_ATTR8(p_info->rate_limit_info, IB_ENH_QOS_RL_GRANULARITY_ENABLED_MASK, IB_ENH_QOS_RL_GRANULARITY_ENABLED_SHIFT, rate);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_min_bw_granularity_cap
*
* DESCRIPTION
*	Set minimum bandwidth granularity cap.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_min_bw_granularity_cap(IN ib_enh_qos_arbiter_info_t * const p_info, uint8_t rate)
{
	IBTYPES_SET_ATTR8(p_info->min_bw_info, IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_MASK, IB_ENH_QOS_MIN_BW_GRANULARITY_CAP_SHIFT, rate);
}

/****f* IBA Base: Types/ib_enh_qos_arbiter_info_t
* NAME
*	ib_enh_qos_arbiter_info_set_min_bw_granularity_enabled
*
* DESCRIPTION
*	Set minimum bandwidth granularity enabled.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_qos_arbiter_info_set_min_bw_granularity_enabled(IN ib_enh_qos_arbiter_info_t * const p_info, uint8_t rate)
{
	IBTYPES_SET_ATTR8(p_info->min_bw_info, IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_MASK, IB_ENH_QOS_MIN_BW_GRANULARITY_ENABLED_SHIFT, rate);
}

/************/

/****s* IBA Base: Types/ib_enh_bw_per_element_t
* NAME
*	ib_enh_bw_per_element_t
*
* DESCRIPTION
*	IBA defined EnhPortVLArbiter. (14.2.6.22)
*	This struct is used in several enhanced
*	QOS MADs.
*
* SYNOPSIS
*/

#define IB_ENH_BW_PER_ELEMENT_SHARE_MASK			0xfff00000
#define IB_ENH_BW_PER_ELEMENT_SHARE_SHIFT			20

#define IB_ENH_BW_PER_ELEMENT_LIMIT_MASK			0x000fffff
#define IB_ENH_BW_PER_ELEMENT_LIMIT_SHIFT			0

#include <complib/cl_packon.h>
typedef struct _ib_enh_bw_per_element {
	uint32_t	share_limit;
} PACK_SUFFIX ib_enh_bw_per_element_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_enh_bw_per_element_t
* NAME
*	ib_enh_bw_per_element_get_share
*
* DESCRIPTION
*	Get bandwidth share.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_bw_per_element_get_share(IN const ib_enh_bw_per_element_t * const p_table, IN int index)
{
	return IBTYPES_GET_ATTR32(p_table[index].share_limit, IB_ENH_BW_PER_ELEMENT_SHARE_MASK, IB_ENH_BW_PER_ELEMENT_SHARE_SHIFT);
}

/****f* IBA Base: Types/ib_enh_bw_per_element_t
* NAME
*	ib_enh_bw_per_element_get_limit
*
* DESCRIPTION
*	Get rate limit.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_bw_per_element_get_limit(IN const ib_enh_bw_per_element_t * const p_table, IN int index)
{
	return IBTYPES_GET_ATTR32(p_table[index].share_limit, IB_ENH_BW_PER_ELEMENT_LIMIT_MASK, IB_ENH_BW_PER_ELEMENT_LIMIT_SHIFT);
}

/****f* IBA Base: Types/ib_enh_bw_per_element_t
* NAME
*	ib_enh_bw_per_element_set_share
*
* DESCRIPTION
*	Set bandwidth share.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_bw_per_element_set_share(IN ib_enh_bw_per_element_t * const p_table, IN int index, uint32_t share)
{
	IBTYPES_SET_ATTR32(p_table[index].share_limit, IB_ENH_BW_PER_ELEMENT_SHARE_MASK, IB_ENH_BW_PER_ELEMENT_SHARE_SHIFT, share);
}

/****f* IBA Base: Types/ib_enh_bw_per_element_t
* NAME
*	ib_enh_bw_per_element_set_limit
*
* DESCRIPTION
*	Set rate limit.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_bw_per_element_set_limit(IN ib_enh_bw_per_element_t * const p_table, IN int index, uint32_t limit)
{
	IBTYPES_SET_ATTR32(p_table[index].share_limit, IB_ENH_BW_PER_ELEMENT_LIMIT_MASK, IB_ENH_BW_PER_ELEMENT_LIMIT_SHIFT, limit);
}

/****s* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_t
*
* DESCRIPTION
*	IBA defined EnhPortVLArbiter. (14.2.6.22)
*
* SYNOPSIS
*/

#define IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_MASK		0xfffff000
#define IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_SHIFT		12

#include <complib/cl_packon.h>
typedef struct _ib_vl_groups_arbiter_config {
	uint8_t vl_group_mask;
	uint8_t reserved1[3];
	uint32_t rate_limiter;
	uint16_t vl_group_vl_mask[8];
	ib_enh_bw_per_element_t bw_per_vl_group[8];
	uint8_t reserved2[8];
} PACK_SUFFIX ib_vl_groups_arbiter_config_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_get_vl_group_mask
*
* DESCRIPTION
*	Get port VL group mask.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_vl_groups_arbiter_config_get_vl_group_mask(IN const ib_vl_groups_arbiter_config_t * const p_config)
{
	return cl_hton8(p_config->vl_group_mask);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_get_port_rate_limiter
*
* DESCRIPTION
*	Get port rate limiter.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_vl_groups_arbiter_config_get_port_rate_limiter(IN const ib_vl_groups_arbiter_config_t * const p_config)
{
	return IBTYPES_GET_ATTR32(p_config->rate_limiter, IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_MASK, IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_SHIFT);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_get_vl_group_vl_mask
*
* DESCRIPTION
*	Get a bit mask of VLs in the group.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_vl_groups_arbiter_config_get_vl_group_vl_mask(IN const ib_vl_groups_arbiter_config_t * const p_config, int vlg)
{
	return cl_hton16(p_config->vl_group_vl_mask[vlg]);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_get_bw_share
*
* DESCRIPTION
*	Get BW share for VL group.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_vl_groups_arbiter_config_get_bw_share(IN const ib_vl_groups_arbiter_config_t * const p_config, int vlg)
{
	return ib_enh_bw_per_element_get_share(p_config->bw_per_vl_group, vlg);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_get_rate_limit_or_min_bw
*
* DESCRIPTION
*	Get rate limit or minimum BW share for VL group.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_vl_groups_arbiter_config_get_rate_limit_or_min_bw(IN const ib_vl_groups_arbiter_config_t * const p_config, int vlg)
{
	return ib_enh_bw_per_element_get_limit(p_config->bw_per_vl_group, vlg);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_set_vl_group_mask
*
* DESCRIPTION
*	Set port VL group mask.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_groups_arbiter_config_set_vl_group_mask(IN ib_vl_groups_arbiter_config_t * const p_config, uint8_t mask)
{
	p_config->vl_group_mask = cl_hton8(mask);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_set_port_rate_limiter
*
* DESCRIPTION
*	Set port VL group rate limiter.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_groups_arbiter_config_set_port_rate_limiter(IN ib_vl_groups_arbiter_config_t * const p_config, uint32_t rate)
{
	IBTYPES_SET_ATTR32(p_config->rate_limiter, IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_MASK, IB_VL_GROUPS_ARBITER_CONFIG_PORT_RL_SHIFT, rate);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_set_vl_group_vl_mask
*
* DESCRIPTION
*	Set a bit mask of VLs in the group.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_groups_arbiter_config_set_vl_group_vl_mask(IN ib_vl_groups_arbiter_config_t * const p_config, int vlg, uint16_t mask)
{
	p_config->vl_group_vl_mask[vlg] = cl_hton16(mask);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_set_bw_share
*
* DESCRIPTION
*	Set BW share for VL group.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_groups_arbiter_config_set_bw_share(IN ib_vl_groups_arbiter_config_t * const p_config, int vlg, uint32_t bw)
{
	ib_enh_bw_per_element_set_share(p_config->bw_per_vl_group, vlg, bw);
}

/****f* IBA Base: Types/ib_vl_groups_arbiter_config_t
* NAME
*	ib_vl_groups_arbiter_config_set_rate_limit_or_min_bw
*
* DESCRIPTION
*	Set rate limit or minimum BW share for VL group.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_groups_arbiter_config_set_rate_limit_or_min_bw(IN ib_vl_groups_arbiter_config_t * const p_config, int vlg, uint32_t limit)
{
	ib_enh_bw_per_element_set_limit(p_config->bw_per_vl_group, vlg, limit);
}

/************/

/****s* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_t
*
* DESCRIPTION
*	IBA defined VLArbiterConfig. (14.2.6.22)
*
* SYNOPSIS
*/

#include <complib/cl_packon.h>
typedef struct _ib_vl_arbiter_config {
	uint16_t vlmask;
	uint16_t reserved;
	ib_enh_bw_per_element_t bw_per_vl[15];
} PACK_SUFFIX ib_vl_arbiter_config_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_get_vlmask
*
* DESCRIPTION
*	Get VL mask.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_vl_arbiter_config_get_vlmask(IN const ib_vl_arbiter_config_t * const p_config)
{
	return cl_hton16(p_config->vlmask);
}

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_get_bw_share
*
* DESCRIPTION
*	Get relative BW share for VL.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_vl_arbiter_config_get_bw_share(IN const ib_vl_arbiter_config_t * const p_config, int vl)
{
	return ib_enh_bw_per_element_get_share(p_config->bw_per_vl, vl);
}

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_get_rate_limit_or_min_bw
*
* DESCRIPTION
*	Get rate limit or minimum BW share.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_vl_arbiter_config_get_rate_limit_or_min_bw(IN const ib_vl_arbiter_config_t * const p_config, int vl)
{
	return ib_enh_bw_per_element_get_limit(p_config->bw_per_vl, vl);
}

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_set_vlmask
*
* DESCRIPTION
*	Get relative BW share for VL.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_arbiter_config_set_vlmask(IN ib_vl_arbiter_config_t * const p_config, uint16_t vlmask)
{
	p_config->vlmask = cl_hton16(vlmask);
}

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_set_bw_share
*
* DESCRIPTION
*	Set relative BW share for VL.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_arbiter_config_set_bw_share(IN ib_vl_arbiter_config_t * const p_config, int vl, uint32_t bw)
{
	ib_enh_bw_per_element_set_share(p_config->bw_per_vl, vl, bw);
}

/****f* IBA Base: Types/ib_vl_arbiter_config_t
* NAME
*	ib_vl_arbiter_config_set_rate_limit_or_min_bw
*
* DESCRIPTION
*	Set rate limit or minimum BW share.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_vl_arbiter_config_set_rate_limit_or_min_bw(IN ib_vl_arbiter_config_t * const p_config, int vl, uint32_t limit)
{
	ib_enh_bw_per_element_set_limit(p_config->bw_per_vl, vl, limit);
}

/************/

/****s* IBA Base: Types/ib_enh_port_vl_arbiter_t
* NAME
*	ib_enh_port_vl_arbiter_t
*
* DESCRIPTION
*	IBA defined EnhPortVLArbiter. (14.2.6.22)
*
* SYNOPSIS
*/

#define IB_VL_PORT_ARBITER_BW_TYPE_MASK		(0x80000000)
#define IB_VL_PORT_ARBITER_BW_TYPE_SHIFT	(31)

#define IB_VL_PORT_ARBITER_CONFIG_TYPE_MASK	(0x40000000)
#define IB_VL_PORT_ARBITER_CONFIG_TYPE_SHIFT	(30)

#define IB_VL_PORT_ARBITER_PORT_NUM_MASK	(0x000000ff)
#define IB_VL_PORT_ARBITER_PORT_NUM_SHIFT	(0)

#define IB_VL_PORT_ARBITER_BW_TYPE_RATE_LIMIT	(0)
#define IB_VL_PORT_ARBITER_BW_TYPE_MIN_BW	(1)

#define IB_VL_PORT_ARBITER_CONF_TYPE_GROUP_ARB	(0)
#define IB_VL_PORT_ARBITER_CONF_TYPE_PORT_ARB	(1)

/****f* IBA Base: Types/ib_enh_port_vl_arbiter_t
* NAME
*	ib_enh_port_vl_arbiter_get_bw_type
*
* DESCRIPTION
*	Get the limit type (BW share or rate limit).
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_port_vl_arbiter_get_bw_type(IN const uint32_t modifier)
{
	return IBTYPES_GET_ATTR32(modifier, IB_VL_PORT_ARBITER_BW_TYPE_MASK, IB_VL_PORT_ARBITER_BW_TYPE_SHIFT);
}

/****f* IBA Base: Types/ib_enh_port_vl_arbiter_t
* NAME
*	ib_enh_port_vl_arbiter_get_config_type
*
* DESCRIPTION
*	Get the config type (VL group or VL)
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_port_vl_arbiter_get_config_type(IN const uint32_t modifier)
{
	return IBTYPES_GET_ATTR32(modifier, IB_VL_PORT_ARBITER_CONFIG_TYPE_MASK, IB_VL_PORT_ARBITER_CONFIG_TYPE_SHIFT);
}

/****f* IBA Base: Types/ib_enh_port_vl_arbiter_t
* NAME
*	ib_enh_port_vl_arbiter_get_port_num
*
* DESCRIPTION
*	Get the port number associated with the request.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_enh_port_vl_arbiter_get_port_num(IN const uint32_t modifier)
{
	return IBTYPES_GET_ATTR32(modifier, IB_VL_PORT_ARBITER_PORT_NUM_MASK, IB_VL_PORT_ARBITER_PORT_NUM_SHIFT);
}

/****f* IBA Base: Types/ib_enh_port_vl_arbiter_t
* NAME
*	ib_enh_port_vl_arbiter_set_bw_type
*
* DESCRIPTION
*	Set the limit type (BW share or rate limit) in the MAD modifier.
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_enh_port_vl_arbiter_set_modifier(IN const uint8_t bw_type, IN uint8_t config_type, IN uint8_t port_num)
{
	uint32_t	modifier = 0;

	IBTYPES_SET_ATTR32(modifier, IB_VL_PORT_ARBITER_BW_TYPE_MASK, IB_VL_PORT_ARBITER_BW_TYPE_SHIFT, bw_type);
	IBTYPES_SET_ATTR32(modifier, IB_VL_PORT_ARBITER_CONFIG_TYPE_MASK, IB_VL_PORT_ARBITER_CONFIG_TYPE_SHIFT, config_type);
	IBTYPES_SET_ATTR32(modifier, IB_VL_PORT_ARBITER_PORT_NUM_MASK, IB_VL_PORT_ARBITER_PORT_NUM_SHIFT, port_num);
	return modifier;
}

/************/

/****s* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_enh_sl_arbiter_t
*
* DESCRIPTION
*	IBA defined EnhSLArbiter. (14.2.6.23)
*
* SYNOPSIS
*/

#define IB_ENH_SL_ARBITER_PORT_NUM_MASK		(0x000000ff)
#define IB_ENH_SL_ARBITER_PORT_NUM_SHIFT	(0)

#include <complib/cl_packon.h>
typedef struct _ib_enh_sl_arbiter {
	ib_enh_bw_per_element_t bw_per_sl[16];
} PACK_SUFFIX ib_enh_sl_arbiter_t;
#include <complib/cl_packoff.h>

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_sl_arbiter_get_port_num
*
* DESCRIPTION
*	Get the port number associated with the request.
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_sl_arbiter_get_port_num(IN const uint32_t attr_mod)
{
	return IBTYPES_GET_ATTR32(attr_mod, IB_ENH_SL_ARBITER_PORT_NUM_MASK, IB_ENH_SL_ARBITER_PORT_NUM_SHIFT);
}

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_enh_sl_arbiter_get_bw_share
*
* DESCRIPTION
*	Get relative BW share for VL.
*
* SYNOPSIS
*/
static inline uint16_t OSM_API
ib_enh_sl_arbiter_get_bw_share(IN const ib_enh_sl_arbiter_t * const p_config, int sl)
{
	return ib_enh_bw_per_element_get_share(p_config->bw_per_sl, sl);
}

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_enh_sl_arbiter_get_rate_limit_or_min_bw
*
* DESCRIPTION
*	Get rate limit or minimum BW share
*
* SYNOPSIS
*/
static inline uint32_t OSM_API
ib_enh_sl_arbiter_get_rate_limit(IN const ib_enh_sl_arbiter_t * const p_config, int sl)
{
	return ib_enh_bw_per_element_get_limit(p_config->bw_per_sl, sl);
}

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_sl_arbiter_get_port_num
*
* DESCRIPTION
*	Set the port number associated with the request.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_sl_arbiter_set_port_num(IN uint32_t attr_mod, IN uint8_t port_num)
{
	IBTYPES_SET_ATTR32(attr_mod, IB_ENH_SL_ARBITER_PORT_NUM_MASK, IB_ENH_SL_ARBITER_PORT_NUM_SHIFT, port_num);
}

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_enh_sl_arbiter_set_bw_share
*
* DESCRIPTION
*	Set relative BW share for VL.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_sl_arbiter_set_bw_share(IN ib_enh_sl_arbiter_t * const p_config, int sl, uint32_t bw)
{
	ib_enh_bw_per_element_set_share(p_config->bw_per_sl, sl, bw);
}

/****f* IBA Base: Types/ib_enh_sl_arbiter_t
* NAME
*	ib_enh_sl_arbiter_set_rate_limit_or_min_bw
*
* DESCRIPTION
*	Set rate limit or minimum BW share
*
* SYNOPSIS
*/
static inline void OSM_API
ib_enh_sl_arbiter_set_rate_limit(IN ib_enh_sl_arbiter_t * const p_config, int sl, uint32_t limit)
{
	ib_enh_bw_per_element_set_limit(p_config->bw_per_sl, sl, limit);
}

/************/

#include <complib/cl_packon.h>
typedef struct _ib_vport_enh_qos_arbiter_info_record {
	ib_net16_t lid;
	uint16_t reserved;
	ib_enh_qos_arbiter_info_t enh_qos_arbiter_info;
} PACK_SUFFIX ib_enh_qos_arbiter_info_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vl_groups_arbiter_config_record_t {
	ib_net16_t lid;
	uint8_t output_port_num;
	uint8_t rate_bw_rsvd;
	ib_vl_groups_arbiter_config_t vl_group_arbiter_config;
} PACK_SUFFIX ib_vl_groups_arbiter_config_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_vl_arbiter_config_record_t {
	ib_net16_t lid;
	uint8_t output_port_num;
	uint8_t rate_bw_rsvd;
	ib_vl_arbiter_config_t ib_vl_arbiter_config;
} ib_vl_arbiter_config_record_t;
#include <complib/cl_packoff.h>

#include <complib/cl_packon.h>
typedef struct _ib_enh_sl_arbiter_record_t {
	ib_net16_t lid;
	uint8_t output_port_num;
	uint8_t reserved;
	ib_enh_sl_arbiter_t ib_enh_sl_arbiter;
} ib_enh_sl_arbiter_record_t;
#include <complib/cl_packoff.h>

#define IB_MAX_NUM_OF_PLANES_XDR	4
#define IB_MIN_NUM_OF_PLANES_XDR	2

/****s* IBA Base: Types/ib_extended_switch_info_t
* NAME
*	ib_extended_switch_info_t
*
* DESCRIPTION
*	ExtendedSwitchInfo (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_extended_switch_info {
	uint8_t reserved[3];
	uint8_t turbo_path_cap_enable;		// 2b resv, 1b issu_approve, 1b issu_request, 1b turbo_path_cap, 3b turbo_path_en
	uint8_t advanced_turbo_path_cap;
	uint8_t reserved1;
	ib_net16_t req_delay;
	ib_net16_t set_trig_th;
	ib_net16_t rst_trig_th;
	ib_net16_t reserved2;
	ib_net16_t req_trig_window;
} PACK_SUFFIX ib_extended_switch_info_t;
#include <complib/cl_packoff.h>
/************/

#define IB_EXT_SW_INFO_TP_CAP_MASK			0x8
#define IB_EXT_SW_INFO_TP_CAP_SHIFT			3
#define IB_EXT_SW_INFO_TP_ENABLE_MASK			0x7
#define IB_EXT_SW_INFO_ADV_TP_REQ_DELAY_CAP_BIT		0x8
#define IB_EXT_SW_INFO_ADV_TP_SET_TRIG_TH_CAP_BIT	0x4
#define IB_EXT_SW_INFO_ADV_TP_RST_TRIG_TH_CAP_BIT	0x2
#define IB_EXT_SW_INFO_ADV_TP_REQ_TRIG_WINDOW_CAP_BIT	0x1
#define IB_EXT_SW_INFO_REQ_DELAY_MASK_HO		0x3FF
#define IB_EXT_SW_INFO_SET_TRIG_TH_MASK_HO		0x3FF
#define IB_EXT_SW_INFO_RST_TRIG_TH_MASK_HO		0x3FF
#define IB_EXT_SW_INFO_REQ_TRIG_WINDOW_MASK_HO		0x3FF
#define IB_EXT_SW_INFO_ISSU_REQUEST_MASK		0x10
#define IB_EXT_SW_INFO_ISSU_REQUEST_SHIFT		4
#define IB_EXT_SW_INFO_ISSU_APPROVE_MASK		0x20
#define IB_EXT_SW_INFO_ISSU_APPROVE_SHIFT		5

#define IB_EXT_SW_INFO_ISSU_STATE_MASK			0x30	// ISSU state is a combination of ISSU request and ISSU approve
#define IB_EXT_SW_INFO_ISSU_STATE_SHIFT			4

typedef enum {
	ISSU_IDLE       = 0,
	ISSU_REQUESTED  = 1,
	ISSU_ERROR      = 2,
	ISSU_APPROVED   = 3,
} ib_issu_state_t;

/****f* IBA Base: Types/ib_extended_switch_info_get_issu_state
* NAME
*	ib_extended_switch_info_get_issu_state
*
* DESCRIPTION
*	Returns the 2 bits <approve_issu, request_issu> as an int.
*
* SYNOPSIS
*/
static inline int OSM_API
ib_extended_switch_info_get_issu_state(IN ib_extended_switch_info_t *p_esi)
{
	return (int)IBTYPES_GET_ATTR8(p_esi->turbo_path_cap_enable, IB_EXT_SW_INFO_ISSU_STATE_MASK, IB_EXT_SW_INFO_ISSU_STATE_SHIFT);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The ISSU state.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_issu_state
* NAME
*	ib_extended_switch_info_set_issu_state
*
* DESCRIPTION
*	Set issu_state value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_issu_state(IN ib_extended_switch_info_t *p_esi, IN ib_issu_state_t state)
{
	IBTYPES_SET_ATTR8(p_esi->turbo_path_cap_enable, IB_EXT_SW_INFO_ISSU_STATE_MASK, IB_EXT_SW_INFO_ISSU_STATE_SHIFT, state);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*	state
*		[in] ISSU state (see ib_issu_state_t enum for values).
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_get_tp_cap
* NAME
*	ib_extended_switch_info_get_tp_cap
*
* DESCRIPTION
*	Returns Turbo Path capability value
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_extended_switch_info_get_tp_cap(ib_extended_switch_info_t *p_esi)
{
	return IBTYPES_GET_ATTR8(p_esi->turbo_path_cap_enable, IB_EXT_SW_INFO_TP_CAP_MASK,
				 IB_EXT_SW_INFO_TP_CAP_SHIFT);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The Turbo Path capability value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_is_req_trig_window_supported
* NAME
*	ib_extended_switch_info_is_req_trig_window_supported
*
* DESCRIPTION
*	Returns Turbo Path capability value
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_extended_switch_info_is_req_trig_window_supported(ib_extended_switch_info_t *p_esi)
{
	return p_esi->advanced_turbo_path_cap & IB_EXT_SW_INFO_ADV_TP_REQ_TRIG_WINDOW_CAP_BIT;
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	Return TRUE if req_trig_window field is supported. Otherwise, FALSE.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_is_rst_trig_th_supported
* NAME
*	ib_extended_switch_info_is_rst_trig_th_supported
*
* DESCRIPTION
*	Returns Turbo Path capability value
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_extended_switch_info_is_rst_trig_th_supported(ib_extended_switch_info_t *p_esi)
{
	return p_esi->advanced_turbo_path_cap & IB_EXT_SW_INFO_ADV_TP_RST_TRIG_TH_CAP_BIT;
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	Return TRUE if rst_trig_th field is supported. Otherwise, FALSE.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_is_set_trig_th_supported
* NAME
*	ib_extended_switch_info_is_set_trig_th_supported
*
* DESCRIPTION
*	Returns Turbo Path capability value
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_extended_switch_info_is_set_trig_th_supported(ib_extended_switch_info_t *p_esi)
{
	return p_esi->advanced_turbo_path_cap & IB_EXT_SW_INFO_ADV_TP_SET_TRIG_TH_CAP_BIT;
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	Return TRUE if set_trig_th field is supported. Otherwise, FALSE.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_is_req_delay_supported
* NAME
*	ib_extended_switch_info_is_req_delay_supported
*
* DESCRIPTION
*	Returns Turbo Path capability value
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_extended_switch_info_is_req_delay_supported(ib_extended_switch_info_t *p_esi)
{
	return p_esi->advanced_turbo_path_cap & IB_EXT_SW_INFO_ADV_TP_REQ_DELAY_CAP_BIT;
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	Return TRUE if req_delay field is supported. Otherwise, FALSE.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_tp_enable
* NAME
*	ib_extended_switch_info_set_tp_enable
*
* DESCRIPTION
*	Sets Turbo Path enable value
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_tp_enable(ib_extended_switch_info_t *p_esi, uint8_t tp_enable)
{
	IBTYPES_SET_ATTR8(p_esi->turbo_path_cap_enable, IB_EXT_SW_INFO_TP_ENABLE_MASK, 0, tp_enable);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The Turbo Path enable value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_req_delay
* NAME
*	ib_extended_switch_info_set_req_delay
*
* DESCRIPTION
*	Sets the request delay value.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_req_delay(ib_extended_switch_info_t *p_esi, uint16_t req_delay)
{
	IBTYPES_SET_ATTR16(p_esi->req_delay, IB_EXT_SW_INFO_REQ_DELAY_MASK_HO, 0,
			   req_delay);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The request delay value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_trig_thr
* NAME
*	ib_extended_switch_info_set_trig_thr
*
* DESCRIPTION
*	Sets the set trigger threshold value.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_trig_thr(ib_extended_switch_info_t *p_esi, uint16_t set_trig_thr)
{
	IBTYPES_SET_ATTR16(p_esi->set_trig_th, IB_EXT_SW_INFO_SET_TRIG_TH_MASK_HO, 0,
			   set_trig_thr);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The set trigger threshold value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_rst_trig_thr
* NAME
*	ib_extended_switch_info_set_rst_trig_thr
*
* DESCRIPTION
*	Sets the reset trigger threshold value.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_rst_trig_thr(ib_extended_switch_info_t *p_esi, uint16_t rst_trig_thr)
{
	IBTYPES_SET_ATTR16(p_esi->rst_trig_th, IB_EXT_SW_INFO_RST_TRIG_TH_MASK_HO, 0,
			   rst_trig_thr);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The reset trigger threshold value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_extended_switch_info_set_req_trig_window
* NAME
*	ib_extended_switch_info_set_req_trig_window
*
* DESCRIPTION
*	Return the request trigger window.
*
* SYNOPSIS
*/
static inline void OSM_API
ib_extended_switch_info_set_req_trig_window(ib_extended_switch_info_t *p_esi, uint16_t req_trig_window)
{
	IBTYPES_SET_ATTR16(p_esi->req_trig_window, IB_EXT_SW_INFO_REQ_TRIG_WINDOW_MASK_HO, 0,
			   req_trig_window);
}
/*
* PARAMETERS
*	p_esi
*		[in] Pointer to a ib_extended_switch_info_t attribute.
*
* RETURN VALUES
*	The request trigger window value.
*
* NOTES
*
* SEE ALSO
*********/

#define IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK			128
#define IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_SHIFT		0
#define IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_MASK		0x03
#define IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_SHIFT		2
#define IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_MASK		0x0C
#define IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_SHIFT		4
#define IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_MASK		0x30
#define IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_SHIFT		6
#define IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_MASK		0xC0
#define IB_CND_MAX_PORT_STATE					2
#define IB_CND_NUM_BLOCKS 					2
#define IB_CND_INVALID_BLOCK_NUM				IB_CND_NUM_BLOCKS + 1

/****s* IBA Base: Types/ib_contain_and_drain_info_t
* NAME
*	ib_contain_and_drain_info_t
*
* DESCRIPTION
*	ContainAndDrainInfo (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_contain_and_drain_info {
	/* Port block is 4 bits */
	uint8_t port_blocks[IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK / 2];
} PACK_SUFFIX ib_contain_and_drain_info_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	port_blocks
*		Blocks of ports contain and drain info. Note each port block is 4bits.
*
* SEE ALSO
*	ib_mad_t
*********/

/****d* OpenSM: Types/ib_cnd_info_t
* NAME
*       Types/ib_cnd_info_t
*
* DESCRIPTION
*       Enumerates for ContainAndDrainInfo port state values
*
* SYNOPSIS
*/
typedef enum _ib_cnd_info {
	IB_CND_INFO_NO_ACTION,
	IB_CND_INFO_CONTAIN_AND_DRAIN,
	IB_CND_INFO_LINK_DOWN,
	IB_CND_INFO_MAX,
} ib_cnd_info_t;
/***********/

/****f* IBA Base: Types/ib_contain_and_drain_info_get_ingress_port_state
* NAME
*	ib_contain_and_drain_info_get_ingress_port_state
*
* DESCRIPTION
* 	Returns ingress port state for a given port offset from ib_contain_and_drain_info_t
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_contain_and_drain_info_get_ingress_port_state(ib_contain_and_drain_info_t *cnd_info, uint8_t port_num)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port state block is 4 bits */
	index = (port_num % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;
	if (port_num % 2)
		return IBTYPES_GET_ATTR8(cnd_info->port_blocks[index],
					 IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_MASK,
					 IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_SHIFT);

	return IBTYPES_GET_ATTR8(cnd_info->port_blocks[index],
				 IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_MASK,
				 IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_SHIFT);
}
/*
* PARAMETERS
*	cnd_info
*		Pointer to the ContainAndDrainInfo MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port_num
* 		Port number.
*
* RETURN VALUES
*	Ingress port state assigned to specified port offset.
*
* SEE ALSO
*	ib_contain_and_drain_info_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_info_get_egress_port_state
* NAME
*	ib_contain_and_drain_info_get_egress_port_state
*
* DESCRIPTION
* 	Returns egress port state for a given port offset from ib_contain_and_drain_info_t
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_contain_and_drain_info_get_egress_port_state(ib_contain_and_drain_info_t *cnd_info, uint8_t port_num)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port state block is 4 bits */
	index = (port_num % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;
	if (port_num % 2)
		return IBTYPES_GET_ATTR8(cnd_info->port_blocks[index],
					 IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_MASK,
					 IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_SHIFT);

	return IBTYPES_GET_ATTR8(cnd_info->port_blocks[index],
				 IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_MASK,
				 IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_SHIFT);
}
/*
* PARAMETERS
*	cnd_info
*		Pointer to the ContainAndDrainInfo MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port_num
* 		Port number.
*
* RETURN VALUES
*	Egress port state assigned to specified port offset.
*
* SEE ALSO
*	ib_contain_and_drain_info_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_info_set_ingress_port_state
* NAME
*	ib_contain_and_drain_info_set_ingress_port_state
*
* DESCRIPTION
* 	Set ingress port state for given port offset from ib_contain_and_drain_info_t
*
* SYNOPSIS
*/
static inline void OSM_API
ib_contain_and_drain_info_set_ingress_port_state(ib_contain_and_drain_info_t *cnd_info,
						 uint8_t port,
						 uint8_t port_state)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port block is 4 bits */
	CL_ASSERT(port_state < IB_CND_INFO_MAX);

	index = (port % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;

	if (port % 2) {
		IBTYPES_SET_ATTR8(cnd_info->port_blocks[index],
				  IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_SHIFT,
				  port_state);
	} else {
		IBTYPES_SET_ATTR8(cnd_info->port_blocks[index],
				  IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_SHIFT,
				  port_state);
	}
}
/*
* PARAMETERS
*	cnd_info
*		Pointer to the ContainAndDrainInfo MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port
* 		Port number.
*
*	port_state
*		Ingress port state to assign to specified port offset.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	ib_contain_and_drain_info_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_info_set_egress_port_state
* NAME
*	ib_contain_and_drain_info_set_egress_port_state
*
* DESCRIPTION
* 	Set egress port state for given port offset from ib_contain_and_drain_info_t
*
* SYNOPSIS
*/
static inline void OSM_API
ib_contain_and_drain_info_set_egress_port_state(ib_contain_and_drain_info_t *cnd_info,
						uint8_t port,
						uint8_t port_state)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port block is 4 bits */
	CL_ASSERT(port_state < IB_CND_INFO_MAX);

	index = (port % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;

	if (port % 2) {
		IBTYPES_SET_ATTR8(cnd_info->port_blocks[index],
				  IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_SHIFT,
				  port_state);
	} else {
		IBTYPES_SET_ATTR8(cnd_info->port_blocks[index],
				  IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_SHIFT,
				  port_state);
	}
}
/*
* PARAMETERS
*	cnd_info
*		Pointer to the ContainAndDrainInfo MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port
* 		Port number.
*
*	port_state
*		Egress port state to assign to specified port offset.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	ib_contain_and_drain_info_t
*********/

/****s* IBA Base: Types/ib_contain_and_drain_port_state_t
* NAME
*	ib_contain_and_drain_port_state_t
*
* DESCRIPTION
*	ConrainAndDrainPortState (Vendor specific SM class attribute).
*
* SYNOPSIS
*/
#include <complib/cl_packon.h>
typedef struct _ib_contain_and_drain_port_state {
	/* Port block is 4 bits */
	uint8_t port_blocks[IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK / 2];
} PACK_SUFFIX ib_contain_and_drain_port_state_t;
#include <complib/cl_packoff.h>
/*
* FIELDS
* 	port_blocks
*		Blocks of ports contain and drain info. Note each port block is 4bits.
*
* SEE ALSO
* ib_mad_t
*********/

/****d* OpenSM: Types/ib_cnd_port_state_t
* NAME
*       Types/ib_cnd_port_state_t
*
* DESCRIPTION
*       Enumerates for ContainAndDrainPortState port state values
*
* SYNOPSIS
*/
typedef enum _ib_cnd_port_state {
	IB_CND_PORT_STATE_NO_CHANGE,
	IB_CND_PORT_STATE_SET,
	IB_CND_PORT_STATE_CLEAR,
	IB_CND_PORT_STATE_MAX,
} ib_cnd_port_state_t;
/***********/

/****f* IBA Base: Types/ib_contain_and_drain_port_state_get_ingress_port_state
* NAME
*	ib_contain_and_drain_port_state_get_ingress_port_state
*
* DESCRIPTION
* 	Returns ingress port state for a given port offset from ib_contain_and_drain_port_state_t
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_contain_and_drain_port_state_get_ingress_port_state(ib_contain_and_drain_port_state_t * cnd_port_state,
						       uint8_t port_num)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port state block is 4 bits */
	index = (port_num % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;
	if (port_num % 2)
		return IBTYPES_GET_ATTR8(cnd_port_state->port_blocks[index],
					 IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_MASK,
					 IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_SHIFT);

	return IBTYPES_GET_ATTR8(cnd_port_state->port_blocks[index],
				 IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_MASK,
				 IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_SHIFT);
}
/*
* PARAMETERS
*	cnd_port_state
*		Pointer to the ContainAndDrainPortState MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port_num
* 		Port number.
*
* RETURN VALUES
*	Ingress port state assigned to specified port offset.
*
* SEE ALSO
*	ib_contain_and_drain_port_state_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_port_state_get_egress_port_state
* NAME
*	ib_contain_and_drain_port_state_get_egress_port_state
*
* DESCRIPTION
* 	Returns egress port state for a given port offset from ib_contain_and_drain_port_state_t
*
* SYNOPSIS
*/
static inline uint8_t OSM_API
ib_contain_and_drain_port_state_get_egress_port_state(ib_contain_and_drain_port_state_t * cnd_port_state,
						      uint8_t port_num)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port state block is 4 bits */
	index = (port_num % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;
	if (port_num % 2)
		return IBTYPES_GET_ATTR8(cnd_port_state->port_blocks[index],
					 IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_MASK,
					 IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_SHIFT);

	return IBTYPES_GET_ATTR8(cnd_port_state->port_blocks[index],
				 IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_MASK,
				 IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_SHIFT);
}
/*
* PARAMETERS
*	cnd_port_state
*		Pointer to the ContainAndDrainPortState MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port_num
* 		Port number.
*
* RETURN VALUES
*	Egress port state assigned to specified port offset.
*
* SEE ALSO
*	ib_contain_and_drain_port_state_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_port_state_set_ingress_port_state
* NAME
*	ib_contain_and_drain_port_state_set_ingress_port_state
*
* DESCRIPTION
* 	Set ingress port state for given port offset from ib_contain_and_drain_port_state_t
*
* SYNOPSIS
*/
static inline void OSM_API
ib_contain_and_drain_port_state_set_ingress_port_state(ib_contain_and_drain_port_state_t * cnd_port_state,
						       uint8_t port,
						       uint8_t port_state)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port block is 4 bits */
	CL_ASSERT(port_state < IB_CND_PORT_STATE_MAX);

	index = (port % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;

	if (port % 2) {
		IBTYPES_SET_ATTR8(cnd_port_state->port_blocks[index],
				  IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_ODD_INGRESS_PORT_STATE_SHIFT,
				  port_state);
	} else {
		IBTYPES_SET_ATTR8(cnd_port_state->port_blocks[index],
				  IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_EVEN_INGRESS_PORT_STATE_SHIFT,
				  port_state);
	}
}
/*
* PARAMETERS
*	cnd_port_state
*		Pointer to the ContainAndDrainPortState MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port
* 		Port number.
*
*	port_state
*		Ingress port state to assign to specified port offset.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	ib_contain_and_drain_port_state_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_port_state_set_egress_port_state
* NAME
*	ib_contain_and_drain_port_state_set_egress_port_state
*
* DESCRIPTION
* 	Set egress port state for given port offset from ib_contain_and_drain_port_state_t
*
* SYNOPSIS
*/
static inline void OSM_API
ib_contain_and_drain_port_state_set_egress_port_state(ib_contain_and_drain_port_state_t * cnd_port_state,
						      uint8_t port,
						      uint8_t port_state)
{
	uint8_t index;

	/* MAD payload holds 128 port blocks, each port block is 4 bits */
	CL_ASSERT(port_state < IB_CND_PORT_STATE_MAX);

	index = (port % IB_CND_NUM_PORT_BLOCKS_PER_MAD_BLOCK) / 2;

	if (port % 2) {
		IBTYPES_SET_ATTR8(cnd_port_state->port_blocks[index],
				  IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_ODD_EGRESS_PORT_STATE_SHIFT,
				  port_state);
	} else {
		IBTYPES_SET_ATTR8(cnd_port_state->port_blocks[index],
				  IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_MASK,
				  IB_CND_PORT_BLOCK_EVEN_EGRESS_PORT_STATE_SHIFT,
				  port_state);
	}
}
/*
* PARAMETERS
*	cnd_port_state
*		Pointer to the ContainAndDrainPortState MAD packet that the requested port number
*		is within its port range (128 ports per packet).
*
* 	port
* 		Port number.
*
*	port_state
*		Egress port state to assign to specified port offset.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	ib_contain_and_drain_port_state_t
*********/

/****f* IBA Base: Types/ib_contain_and_drain_port_state_change_trap_is_block_changed
* NAME
*	ib_contain_and_drain_port_state_change_trap_is_block_changed
*
* DESCRIPTION
* 	Returns whether given block number is marked as changed on given notice of
* 	ContainAndDrainPortStateChangeTrap.
*
* SYNOPSIS
*/
static inline boolean_t OSM_API
ib_contain_and_drain_port_state_change_trap_is_block_changed(ib_mad_notice_attr_t * p_ntc,
							     uint8_t block_num)
{
	uint8_t byte_offset, bit_index;
	uint8_t *p_block_id_bitmask;

	CL_ASSERT((p_ntc->data_details.vendor_data_details.trap_num ==
		   SM_VS_TRAP_NUM_CND_PORT_STATE_CHANGE_TRAP));

	p_block_id_bitmask = p_ntc->data_details.vendor_data_details.v_data_details.ntc_1317.block_id_mask;

	byte_offset = block_num / CL_BITS_PER_BYTE;
	/*
	 * The bitmask is represented as an array of bytes,
	 * hence the bytes offsets are referred in reversed order.
	 */
	byte_offset = IB_CND_PORT_STATE_CHANGE_TRAP_ID_MASK_BYTES_LEN - byte_offset - 1;
	bit_index = block_num % CL_BITS_PER_BYTE;

	return p_block_id_bitmask[byte_offset] & ((uint8_t)1 << bit_index);
}
/*
* PARAMETERS
*	p_ntc
*		Pointer to the ContainAndDrainPortStateChangeTrap MAD packet.
*
* 	block_num
* 		Block number.
*
* RETURN VALUES
*	Returns TRUE when block number is marked as changed on input trap. Otherwise, FALSE.
*
*********/

END_C_DECLS
#else			/* __WIN__ */
#include <iba/ib_types_extended.h>
#endif			/* __WIN__ */
