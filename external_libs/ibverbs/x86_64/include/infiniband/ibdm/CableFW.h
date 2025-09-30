/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBDM_CABLE_FW_H
#define IBDM_CABLE_FW_H

#include <map>
#include <string>
#include <iostream>

struct cable_id_t
{
    std::string pn;
    std::string identifier;
    std::string vendor;
    std::string revision;

    bool operator< (const cable_id_t& other) const {
        if (this == &other)   // Short?cut self?comparison
            return false;

        // Compare first members
        //
        if (pn < other.pn)
            return true;
        else if (other.pn < pn)
            return false;

        // First members are equal, compare second members
        //
        if (identifier < other.identifier)
            return true;
        else if (other.identifier < identifier)
            return false;

        // Compare third members
        //
        if (vendor < other.vendor)
            return true;
        else if (other.vendor < vendor)
            return false;

        // Compare 4-th members
        //
        if (revision < other.revision)
            return true;
        else if (other.revision < revision)
            return false;

        // All members are equal, so return false
        return false;

    }
    bool IsValid() const { return !pn.empty() && !identifier.empty() &&
                                        !vendor.empty() && !revision.empty(); }
};

struct cable_fw_t
{
    uint8_t hi;
    uint8_t mid;
    uint16_t lo;

    cable_fw_t(): hi(0), mid(0), lo(0) {}
    bool operator< (const cable_fw_t& other) const {
        if (this == &other)   // Short?cut self?comparison
            return false;

        // Compare first members
        //
        if (hi < other.hi)
            return true;
        else if (other.hi < hi)
            return false;

        // First members are equal, compare second members
        //
        if (mid < other.mid)
            return true;
        else if (other.mid < mid)
            return false;

        // Second members are equal, compare third members
        //
        if (lo < other.lo)
            return true;
        else if (other.lo < lo)
            return false;

        // All members are equal, so return false
        return false;
    }

    bool operator== (const cable_fw_t& other) const {
        if (this == &other)   // Short?cut self?comparison
            return true;

        return (hi == other.hi && mid == other.mid && lo == other.lo);
    }

    bool operator!= (const cable_fw_t& other) const {
        return !(*this == other);
    }

    bool IsValid() const { return (hi || mid || lo); }
};

typedef std::map<cable_id_t, cable_fw_t> cable_id_to_fw_map;

#endif
