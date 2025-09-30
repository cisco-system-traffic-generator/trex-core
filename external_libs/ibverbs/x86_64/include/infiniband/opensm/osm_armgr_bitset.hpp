/*                  - Mellanox Confidential and Proprietary -
 *
 *  Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *  Copyright (C) 2010-2011, Mellanox Technologies Ltd.  ALL RIGHTS RESERVED.
 *
 *  Except as specifically permitted herein, no portion of the information,
 *  including but not limited to object code and source code, may be reproduced,
 *  modified, distributed, republished or otherwise exploited in any form or by
 *  any means for any purpose without the prior written permission of Mellanox
 *  Technologies Ltd. Use of software subject to the terms and conditions
 *  detailed in the file "COPYING".
 *
 */

#pragma once

#include <string>
#include <iostream>
#include <sstream>

#define MAX_SUPPORTED_PORTS 255
#define BITSET_BLOCKS_NUM   4

//============================================================================//
class PortsBitset {
public:
    ////////////////////
    //methods
    ////////////////////

    PortsBitset (const PortsBitset & port_bitset)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = port_bitset.m_bitset[i];
    }

    PortsBitset& operator&=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i]&=__rhs.m_bitset[i];
        return *this;
    }
    
    PortsBitset& operator|=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i]|=__rhs.m_bitset[i];
        return *this;
    }
    
    PortsBitset& operator^=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i]^=__rhs.m_bitset[i];
        return *this;
    }

    bool operator==(const PortsBitset& __rhs) const
    {
        return ((m_bitset[0]==__rhs.m_bitset[0]) &&
                (m_bitset[1]==__rhs.m_bitset[1]) &&
                (m_bitset[2]==__rhs.m_bitset[2]) &&
                (m_bitset[3]==__rhs.m_bitset[3]) );
    }

    bool operator!=(const PortsBitset& __rhs) const
    {
        return ((m_bitset[0]!=__rhs.m_bitset[0]) ||
                (m_bitset[1]!=__rhs.m_bitset[1]) ||
                (m_bitset[2]!=__rhs.m_bitset[2]) ||
                (m_bitset[3]!=__rhs.m_bitset[3]) );
    }
    
    /**
    *  @brief Sets every bit to true.
    */
    PortsBitset& set()
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = 0xFFFFFFFFFFFFFFFFULL;
        return *this;
    }
    
    /**
    *  @brief Sets a given bit.
    *  @param  position  The index of the bit.
    */
    PortsBitset& set(size_t __position)
    {
        m_bitset[__position/64] |= (1ull << (__position%64));
        return *this;
    }

    bool test(size_t __position)
    {
        return (m_bitset[__position/64] & (1ull << (__position%64)));
    }

    /**
    *  @brief Sets every bit to false.
    */
    PortsBitset& reset()
    {

        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = 0;

        return *this;

    }

    /**
    *  @brief Reset a given bit.
    *  @param  position  The index of the bit.
    */
    PortsBitset& reset(size_t __position)
    {
        m_bitset[__position/64] &= ~(1ull << (__position%64));
        return *this;
    }

    /**
    *  @brief Toggles every bit to its opposite value.
    */
    PortsBitset& flip()
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = ~m_bitset[i];
        return *this;
    }

    /**
    *  @brief Sets all bitset data.
    */
    PortsBitset& set(uint64_t &b0, uint64_t &b1, uint64_t &b2, uint64_t &b3)
    {
        m_bitset[0] = b0;
        m_bitset[1] = b1;
        m_bitset[2] = b2;
        m_bitset[3] = b3;
        return *this;
    }

    /**
    *  @brief Sets all bitset data.
    */
    PortsBitset& set(osm_ar_subgroup_t &ar_subgroup)
    {
        /*
	 * this->b0 contains mask for ports 0-63
	 * ar_subgroup->mask[31] contains mask for ports 0-7
	 */
        m_bitset[3] = cl_ntoh64(ar_subgroup.mask64[0]);
        m_bitset[2] = cl_ntoh64(ar_subgroup.mask64[1]);
        m_bitset[1] = cl_ntoh64(ar_subgroup.mask64[2]);
        m_bitset[0] = cl_ntoh64(ar_subgroup.mask64[3]);
	return *this;
    }

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if at least one bit is set.
    */
    bool any() const
    { 
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            if (m_bitset[i] != 0)
                return true; 
        return false;
    }

    bool operator<(const PortsBitset& __rhs) const
    { 
        for (int i = BITSET_BLOCKS_NUM-1 ; i >= 0 ; i--)
            if (m_bitset[i] != __rhs.m_bitset[i]) {
                return (m_bitset[i] < __rhs.m_bitset[i]);
            }
        return false;
    }

    std::ostream& to_ostream(std::ostream& __os) const
    {
        return __os <<"("<< hex << m_bitset[3] << "):"
                    <<"("<< hex << m_bitset[2] << "):"
                    <<"("<< hex << m_bitset[1] << "):"
                    <<"("<< hex << m_bitset[0] << ")";
    }

    PortsBitset operator~() const
    {
        PortsBitset __result(*this);
        return __result.flip();
    }

    std::string to_string() const
    {
        stringstream sstr;
        to_ostream(sstr);
        return sstr.str();
    }

    PortsBitset ()
    {
        memset(m_bitset, 0, sizeof(m_bitset));
    }

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if none of the bits are set.
    */
    bool none() const
    {
        return !any();
    }

    void get_data(uint64_t &b0, uint64_t &b1, uint64_t &b2, uint64_t &b3)
    {
        b0 = m_bitset[0];
        b1 = m_bitset[1];
        b2 = m_bitset[2];
        b3 = m_bitset[3];
    }

    void get_data(u_int32_t port_bitset[8])
    {
        memcpy(port_bitset, m_bitset, sizeof(m_bitset));
    }

    void get_data(osm_ar_subgroup_t &ar_subgroup)
    {
        /*
	 * this->b0 contains mask for ports 0-63
	 * ar_subgroup->mask[31] contains mask for ports 0-7
	 */
        ar_subgroup.mask64[0] = cl_hton64(m_bitset[3]);
        ar_subgroup.mask64[1] = cl_hton64(m_bitset[2]);
        ar_subgroup.mask64[2] = cl_hton64(m_bitset[1]);
        ar_subgroup.mask64[3] = cl_hton64(m_bitset[0]);
    }

    void get_data_union(osm_ar_subgroup_t &ar_subgroup)
    {
        ar_subgroup.mask64[0] |= cl_hton64(m_bitset[3]);
        ar_subgroup.mask64[1] |= cl_hton64(m_bitset[2]);
        ar_subgroup.mask64[2] |= cl_hton64(m_bitset[1]);
        ar_subgroup.mask64[3] |= cl_hton64(m_bitset[0]);
    }

private:

    uint64_t m_bitset[BITSET_BLOCKS_NUM];

};

/**
*  @brief  Global bitwise operations on bitsets.
*  @param  x  A bitset.
*  @param  y  A bitset of the same size as @a x.
*  @return  A new bitset.
*
*  These should be self-explanatory.
*/
inline PortsBitset
operator&(const PortsBitset& __x, const PortsBitset& __y)
{
    PortsBitset __result(__x);
    __result &= __y;
    return __result;
}

inline PortsBitset
operator|(const PortsBitset& __x, const PortsBitset& __y)
{
    PortsBitset __result(__x);
    __result |= __y;
    return __result;
}

inline PortsBitset
operator^(const PortsBitset& __x, const PortsBitset& __y)
{
    PortsBitset __result(__x);
    __result ^= __y;
    return __result;
}

inline std::ostream &
operator<<(std::ostream& __os,
const PortsBitset& __x)
{
    return __x.to_ostream(__os);
}

struct PortsBitsetLstr
{
  bool operator()(const PortsBitset *bs1, const PortsBitset *bs2) const
  {
    return *bs1 < *bs2;
  }

  bool operator()(const PortsBitset &bs1, const PortsBitset &bs2) const
  {
    return bs1 < bs2;
  }

};


//Primary Secondary

class PSPortsBitset {
public:
    ////////////////////
    //methods
    ////////////////////

    PSPortsBitset (const PSPortsBitset & ps_port_bitset)
    {
        m_primary = ps_port_bitset.m_primary;
        m_secondary = ps_port_bitset.m_secondary;
    }
      /**
       *  @brief  Operations on bitsets.
       *  @param  rhs  A same-sized bitset.
       *
       *  These should be self-explanatory.
       */
    /*
    PSPortsBitset&
    operator&=(const PSPortsBitset& __rhs)
    {
        m_bitset[0]&=__rhs.m_bitset[0];
        return *this;
    }

    PSPortsBitset&
    operator|=(const PSPortsBitset& __rhs)
    {
        m_bitset[0]|=__rhs.m_bitset[0];
        return *this;
    }

    PSPortsBitset&
    operator^=(const PSPortsBitset& __rhs)
    {
        m_bitset[0]^=__rhs.m_bitset[0];
        return *this;
    }
    */

    bool
    operator==(const PSPortsBitset& __rhs) const
    {
        return ((m_primary==__rhs.m_primary) &&
                (m_secondary==__rhs.m_secondary));
    }

    bool
    operator!=(const PSPortsBitset& __rhs) const
    {
        return ((m_primary!=__rhs.m_primary) ||
                (m_secondary!=__rhs.m_secondary));
    }

    /**
    *  @brief Sets every bit to true.
    */
    PSPortsBitset&
    set()
    {
        m_primary.set();
        m_secondary.set();
        return *this;
    }

    /**
    *  @brief Sets a given bit.
    *  @param  position  The index of the bit.
    *  @param  val  Either true or false, defaults to true.
    */
    PSPortsBitset&
    set_pri(size_t __position)
    {
        m_primary.set(__position);
        return *this;
    }

    PSPortsBitset&
    set_sec(size_t __position)
    {
        m_secondary.set(__position);
        return *this;
    }

    PSPortsBitset&
    set_pri(PortsBitset & port_bitset)
    {
        m_primary = port_bitset;
        return *this;
    }

    PSPortsBitset&
    set_sec(PortsBitset & port_bitset)
    {
        m_secondary = port_bitset;
        return *this;
    }

    PSPortsBitset&
    add_pri(PortsBitset & port_bitset)
    {
        m_primary |= port_bitset;
        return *this;
    }

    PSPortsBitset&
    add_sec(PortsBitset & port_bitset)
    {
        m_secondary |= port_bitset;
        return *this;
    }

    bool
    test_pri(size_t __position)
    {
        return m_primary.test(__position);
    }

    bool
    test_sec(size_t __position)
    {
        return m_secondary.test(__position);
    }

    /**
    *  @brief Sets every bit to false.
    */
    PSPortsBitset&
    reset()
    {
        m_primary.reset();
        m_secondary.reset();
        return *this;
    }

    /**
    *  @brief Reset a given bit.
    *  @param  position  The index of the bit.
    *  @param  val  Either true or false, defaults to true.
    */
    PSPortsBitset&
    reset_pri(size_t __position)
    {
        m_primary.reset(__position);
        return *this;
    }

    PSPortsBitset&
    reset_sec(size_t __position)
    {
        m_secondary.reset(__position);
        return *this;
    }

    /**
    *  @brief Toggles every bit to its opposite value.
    */
    PSPortsBitset&
    flip()
    {
        m_primary.flip();
        m_secondary.flip();
        return *this;
    }

    PSPortsBitset&
    flip_pri()
    {
        m_primary.flip();
        return *this;
    }

    PSPortsBitset&
    flip_sec()
    {
        m_secondary.flip();
        return *this;
    }

    /**
    *  @brief Sets all bitset data.
    */
    /*
    PSPortsBitset&
    set(uint64_t &b0, uint64_t &, uint64_t &, uint64_t &)
    {
        m_bitset[0] = b0;
        return *this;
    }
    */

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if at least one bit is set.
    */
    bool
    any() const
        { return (m_primary.any() || m_secondary.any()); }

    bool
    any_pri() const
        { return m_primary.any(); }

    bool
    any_sec() const
    { return m_secondary.any(); }

    bool
    operator<(const PSPortsBitset& __rhs) const
    {
        if (m_primary != __rhs.m_primary) {
            return (m_primary< __rhs.m_primary);
        }
        return (m_secondary< __rhs.m_secondary);
    }

    std::ostream &
    to_ostream(std::ostream& __os) const
    {
        return __os <<"pri:" << m_primary.to_string()
                    << " sec:" << m_secondary.to_string();
    }

    std::string
    to_string() const
    {
        stringstream sstr;
        to_ostream(sstr);
        return sstr.str();
    }

    PSPortsBitset ()
    {
        reset();
    }

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if none of the bits are set.
    */
    bool
    none() const
    { return !any(); }

    bool
    none_pri() const
    { return !any_pri(); }

    bool
    none_sec() const
    { return !any_sec(); }

    PortsBitset & get_primary() {return m_primary;}
    PortsBitset & get_secondary() {return m_secondary;}



private:

    PortsBitset m_primary;
    PortsBitset m_secondary;


};

inline std::ostream &
operator<<(std::ostream& __os,
const PSPortsBitset& __x)
{
    return __x.to_ostream(__os);
}

struct PSPortsBitsetLstr
{
  bool operator()(const PSPortsBitset *bs1, const PSPortsBitset *bs2) const
  {
    return *bs1 < *bs2;
  }

  bool operator()(const PSPortsBitset &bs1, const PSPortsBitset &bs2) const
  {
    return bs1 < bs2;
  }

};
