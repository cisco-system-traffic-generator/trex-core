/*
 * Copyright (c) 2004-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef KEY_MANAGER_H_
#define KEY_MANAGER_H_

#include <vector>
#include "ibis_types.h"

using namespace std;

enum IBISKeyType {
    IBIS_VS_KEY,
    IBIS_CC_KEY,
    IBIS_CLASS_C_KEY,
    IBIS_PM_KEY,
    IBIS_AM_KEY,
    IBIS_NUM_OF_KEY_TYPES
};

class KeyManager {
private:
    struct IBISKey {
        uint64_t key;
        bool is_set;

        IBISKey():
            key(IBIS_IB_DEFAULT_KEY), is_set(false) { };
    };

    vector < vector < IBISKey > > lid_2_key_vec_vec; // lid_2_key_vec_vec[key_type][lid]
    vector < u_int64_t > default_keys; // default_keys[key_type]
public:
    KeyManager();

    inline static bool IsVlaidKeyType(IBISKeyType key_type) {
        if (key_type < IBIS_NUM_OF_KEY_TYPES)
            return true;
        else
            return false;
    }
    static const char* GetTypeName(IBISKeyType type);
    u_int64_t GetKey(u_int16_t lid, IBISKeyType key_type) const;
    void SetKey(u_int16_t lid, IBISKeyType ket_type, u_int64_t key);
    void UnSetKey(u_int16_t lid, IBISKeyType ket_type);

    inline u_int64_t GetDefaultKey(IBISKeyType key_type) const {
        if (KeyManager::IsVlaidKeyType(key_type))
            return this->default_keys[key_type];
        else
            return IBIS_IB_DEFAULT_KEY;
    };

    inline void SetDefaultKey(u_int64_t default_key, IBISKeyType key_type) {
        if (KeyManager::IsVlaidKeyType(key_type))
            this->default_keys[key_type] = default_key;
    };
};

#endif	/* KEY_MANAGER_H_ */
