/*
 * Copyright (c) 2020-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2024-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef IBDIAG_SMDB_H
#define IBDIAG_SMDB_H

#include <stdlib.h>
#include <infiniband/ibdm/Fabric.h>
#include "ibdiag_types.h"
#include <ibis/csv_parser.h>

using namespace std;

struct smdb_switch_Info {
    u_int8_t    rank;        //This structure may be extended in the future
};

typedef map < u_int64_t , struct smdb_switch_Info >  map_smdb_guid_2_switch_info;

class SMDBSMRecord;
class SMDBSwitchRecord;

class IBDiagSMDB {
private:
    CsvParser m_csv_parser;

    map_smdb_guid_2_switch_info guid_2_switch_info;
    IBRoutingEngine routing_engine;
    bool is_smdb_parsed;

public:
    int ParseSMDB(const string &csv_file);
    int Apply(IBFabric &discovered_fabric);
    int ParseSMSection(const SMDBSMRecord &smdbSMRecord);
    int ParseSwitchSection(const SMDBSwitchRecord &smdbSwitchRecord);
    inline bool IsSMDBParsed() { return is_smdb_parsed; }
    inline IBRoutingEngine GetRoutingEngine() { return routing_engine; }

    IBDiagSMDB() :
        routing_engine(IB_ROUTING_UNKNOWN), is_smdb_parsed(false) { }
};

/*******************************
 *       SECTION RECORDS       *
 *******************************/

class SMDBSMRecord {
public:
    string              routing_engine;

    static int Init(vector < ParseFieldInfo <class SMDBSMRecord> > &parse_section_info);

    bool SetRoutingEngine(const char *field_str);
};

class SMDBSwitchRecord {
public:
    u_int64_t             node_guid;
    u_int8_t              rank;

    SMDBSwitchRecord() : node_guid(0), rank(0) {}

    static int Init(vector < ParseFieldInfo <class SMDBSwitchRecord> > &parse_section_info);

    bool SetNodeGUID(const char *field_str);
    bool SetRank(const char *field_str);
};

#endif   /* IBDIAG_SMDB_H */
