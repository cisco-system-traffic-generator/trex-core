/*--
* SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
* SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
--*/

#ifndef _IBDIAG_KEY_UPDATER
#define _IBDIAG_KEY_UPDATER

#include <map>

#include "infiniband/ibdiag/ibdiag_types.h"
#include <ibis/key_mngr.h>

class IBDiag;
class KeyUpdater
{
public:
    KeyUpdater(IBDiag* p_ibdiag);
    ~KeyUpdater();

    int SetKey(IBISKeyType type, const string& path);
    int UpdateSetKeysIfNeeded();
    void UpdateKeysPerPort(const std::set<IBISKeyType>& include_list);
    bool IsKeyAvailable(IBISKeyType type);

    void detach() { p_ibdiag = nullptr; }


private:
    struct KeyEntry {
        KeyEntry() : last_modified(0) {};
            string path;
            uint64_t last_modified;
            map_guid_2_key_t key_map;
    };

    void SetKeyPerPort(const map_guid_2_key_t &guid_2_key, IBISKeyType key_type,
                       const string &key_type_str, const set <IBNodeType> &filter);
    
    void SetAMKeyPerPort(const map_guid_2_key_t &guid_2_key);
    
    int ParseGuid2Key(KeyEntry& key_entry,
                      string &key_type_str, bool should_print_error,
                      const string &default_file_name, bool init);

    IBDiag* p_ibdiag;
    std::map<IBISKeyType, KeyEntry> key_maps;
};

#endif
