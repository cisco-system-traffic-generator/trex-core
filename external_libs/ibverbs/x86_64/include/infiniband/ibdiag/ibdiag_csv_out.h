/*
 * Copyright (c) 2017-2021 Mellanox Technologies LTD. All rights reserved.
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

#ifndef IBDIAG_CSV_OUT_H_
#define IBDIAG_CSV_OUT_H_

#include <sys/time.h>
#include <sys/resource.h>

#include <infiniband/ibdm/Fabric.h>
#include "ibdiag_types.h"

#define SECTION_INDEX_TABLE "INDEX_TABLE"


class CSVOut {

    ofstream sout;
    bool current_section_disabled;
    list_index_line index_table;
    size_t cur_CSV_line;
    size_t idx_tbl_comment_pos;
    index_line_t cur_idx;
	string filename;


	// Performance
    std::stringstream       m_perf_info;
    struct timespec         m_began_tm;
    struct rusage           m_began_usage;


    string CommentString(size_t offset, size_t line)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "offset: %11lu, line: %11lu", offset, line);
        return string(buffer);
    }

public:

    CSVOut() : current_section_disabled(false), cur_CSV_line(1), idx_tbl_comment_pos(0)
    {
        m_perf_info << "Name,Status,Real,User,Sys\n";
        memset(&m_began_tm, 0, sizeof(m_began_tm));        
        memset(&m_began_usage, 0, sizeof(m_began_usage));
    }

    ~CSVOut()
    {
        Close();
    }

protected:
    void Init();
    void DumpIndexTableCSV();
    void DumpPerfTableCSV();
    void SetCommentPos();


public:
    const string &GetFileName() const {
        return filename;
    }

    int Open(const char *file_name, string &err_message, bool is_custom = false, const char * header_line_prefix = NULL);
    void Close();

    int DumpStart(const char *name);
    void DumpEnd(const char *name);

    void WriteBuf(const string &buf);

};


#endif          /* not defined IBDIAG_CSV_OUT_H_ */
