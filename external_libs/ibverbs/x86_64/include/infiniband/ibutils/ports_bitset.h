/*
 * Copyright (c) 2004-2018 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 /*
  * This file describes a bitset of 256 bits
  */



#ifndef PORTS_BITSET_H_
#define PORTS_BITSET_H_

#include <string>
#include <iostream>
#include <sstream>

#define BITSET_BLOCKS_NUM            4 //256 / 64
#define BITSET_BLOCK_SIZE_IN_BITS    (sizeof(u_int64_t)*8) //64 bits
#define PORTS_BITSET_PRINT_FORMAT    "(" U64H_FMT "):(" U64H_FMT "):(" U64H_FMT "):(" U64H_FMT ")"
#define MAX_VAL_64_BIT               0xffffffffffffffffULL

//============================================================================//
class PortsBitset {
private:
    uint64_t m_bitset[BITSET_BLOCKS_NUM];

public:
    ////////////////////
    //methods
    ////////////////////
    PortsBitset ()
    {
        memset(m_bitset, 0, sizeof(m_bitset));
    }

    PortsBitset (const PortsBitset & port_bitset)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = port_bitset.m_bitset[i];
    }

    PortsBitset&
    operator&=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] &= __rhs.m_bitset[i];
        return *this;
    }

    PortsBitset&
    operator|=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] |= __rhs.m_bitset[i];
        return *this;
    }

    PortsBitset&
    operator^=(const PortsBitset& __rhs)
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] ^= __rhs.m_bitset[i];
        return *this;
    }

    bool
    operator==(const PortsBitset& __rhs) const
    {
        return ((m_bitset[0]==__rhs.m_bitset[0]) &&
                (m_bitset[1]==__rhs.m_bitset[1]) &&
                (m_bitset[2]==__rhs.m_bitset[2]) &&
                (m_bitset[3]==__rhs.m_bitset[3]) );
    }

    bool
    operator!=(const PortsBitset& __rhs) const
    {
        return ((m_bitset[0]!=__rhs.m_bitset[0]) ||
                (m_bitset[1]!=__rhs.m_bitset[1]) ||
                (m_bitset[2]!=__rhs.m_bitset[2]) ||
                (m_bitset[3]!=__rhs.m_bitset[3]) );
    }

    /**
    *  @brief Sets every bit to true.
    */
    PortsBitset&
    set()
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = MAX_VAL_64_BIT;
        return *this;
    }

    /**
    *  @brief Sets a given bit.
    *  @param  position  The index of the bit.
    */
    PortsBitset&
    set(size_t __position)
    {
        m_bitset[__position/BITSET_BLOCK_SIZE_IN_BITS] |=
            (1ull << (__position%BITSET_BLOCK_SIZE_IN_BITS));
        return *this;
    }

    bool
    test(size_t __position) const
    {
        return (m_bitset[__position/BITSET_BLOCK_SIZE_IN_BITS] &
                (1ull << (__position%BITSET_BLOCK_SIZE_IN_BITS)));
    }

    /**
    *  @brief Sets every bit to false.
    */
    PortsBitset&
    reset()
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = 0;
        return *this;

    }

    /**
    *  @brief Reset a given bit.
    *  @param  position  The index of the bit.
    */
    PortsBitset&
    reset(size_t __position)
    {
        m_bitset[__position/BITSET_BLOCK_SIZE_IN_BITS] &=
            ~(1ull << (__position%BITSET_BLOCK_SIZE_IN_BITS));
        return *this;
    }

    /**
    *  @brief Toggles every bit to its opposite value.
    */
    PortsBitset&
    flip()
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            m_bitset[i] = ~m_bitset[i];
        return *this;
    }

    /**
    *  @brief Sets all bitset data.
    */
    PortsBitset&
    set(const uint64_t &b0, const uint64_t &b1, const uint64_t &b2, const uint64_t &b3)
    {
        m_bitset[0] = b0;
        m_bitset[1] = b1;
        m_bitset[2] = b2;
        m_bitset[3] = b3;
        return *this;
    }

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if at least one bit is set.
    */
    bool
    any() const
    {
        for (int i = 0 ; i < BITSET_BLOCKS_NUM ; i++)
            if (m_bitset[i] != 0)
                return true;
        return false;
    }

    bool
    operator<(const PortsBitset& __rhs) const
    {
        for (int i = BITSET_BLOCKS_NUM - 1 ; i >= 0 ; i--)
            if (m_bitset[i] != __rhs.m_bitset[i]) {
                return (m_bitset[i] < __rhs.m_bitset[i]);
            }
        return false;
    }

    std::ostream &
    to_ostream(std::ostream& __os) const
    {
        return __os <<"("<< hex << m_bitset[3] << "):"
                    <<"("<< hex << m_bitset[2] << "):"
                    <<"("<< hex << m_bitset[1] << "):"
                    <<"("<< hex << m_bitset[0] << ")";
    }

    PortsBitset
    operator~() const
    {
        PortsBitset __result(*this);
        return __result.flip();
    }

    std::string
    to_string() const
    {
        stringstream sstr;
        to_ostream(sstr);
        return sstr.str();
    }

    /**
    *  @brief Tests whether any of the bits are on.
    *  @return  True if none of the bits are set.
    */
    bool
    none() const
    { return !any(); }

    void
    get_data(uint64_t &b0, uint64_t &b1, uint64_t &b2, uint64_t &b3) const
    {
        b0 = m_bitset[0];
        b1 = m_bitset[1];
        b2 = m_bitset[2];
        b3 = m_bitset[3];
    }

    void
    get_data(u_int32_t port_bitset[8])
    {
        memcpy(port_bitset, m_bitset, sizeof(m_bitset));
    }

    /**
    *  @brief find max msb set
    *  @return  return max msb set index
    */
    size_t
    max_set_bit_pos() const
    {
        size_t cnt = 0;

        for (int i = BITSET_BLOCKS_NUM - 1; i >= 0 ; i--) {
            uint64_t bits = m_bitset[i];

            //find the first that is not empty
            if (bits == 0)
                continue;
            while (bits >>= 1)
                cnt++;
            cnt += i * BITSET_BLOCK_SIZE_IN_BITS;
            break;
        }
        return cnt;
    }

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

//the 'compare' predicat for the STD containers (ex: map)
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


#endif          /* PORTS_BITSET_H_ */
