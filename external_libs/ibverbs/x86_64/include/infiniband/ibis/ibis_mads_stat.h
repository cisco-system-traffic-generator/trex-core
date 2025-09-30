/*
 * Copyright (c) 2020-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBIS_STAT_H_
#define IBIS_STAT_H_

#include <time.h>
#include <stdint.h>
#include <iostream>
#include <iomanip>

#include <map>
#include <vector>

#define IBISStat_COLUMN_WIDTH       12
#define IBISStat_COLUMN_NUM          4
#define IBISStat_HISTOGRAM_SIZE     40
#define IBISStat_TABLE_WIDTH        ( IBISStat_COLUMN_WIDTH + 1 ) * IBISStat_COLUMN_NUM
#define IBISStat_LINE               output_prefix << std::setw(IBISStat_TABLE_WIDTH) \
                                        << std::setfill('-') <<  "" << std::setfill(' ') << std::endl

class IbisMadsStat
{
    private:
        typedef struct {
            uint8_t        base_version;
            uint8_t        mgmt_class;
            uint8_t        class_version;
            uint8_t        method;
            uint16_t       status;
            uint16_t       class_specific;
            uint64_t       tid;
            uint16_t       attr_id;
            uint16_t       resv;
            uint32_t       attr_mod;
        } umad_hdr_t;

    public:
        typedef struct mad_key_fields {
            uint8_t     mgmt_class;
            uint16_t    attr_id;
            uint8_t     method;
        } mad_key_fields_t;

    public:
        typedef struct key {
            union {
                uint32_t        hash;
                struct {
                    uint16_t    attr_id;
                    uint8_t     method;
                    uint8_t     mgmt_class;
                } fields;
            };

            key(const umad_hdr_t & hdr) {
                fields.mgmt_class   = hdr.mgmt_class;
                fields.method       = hdr.method;
                fields.attr_id      = (uint16_t)(((hdr.attr_id & 0xFF) << 8) |
                                                 ((hdr.attr_id >> 8) & 0xFF));
            }

            inline bool operator < (const key & val) const {
                return hash < val.hash;
            }

            inline bool operator > (const key & val) const {
                return hash > val.hash;
            }

            inline bool operator==(const key & val) const {
                return hash == val.hash;
            }

            inline bool operator!=(const key & val) const {
                return !(*this == val);
            }

        } key_t;

        typedef struct {
            uint64_t    second;
            uint64_t    count;
        } time_record_t;

    public:
        typedef std::map<key_t, uint64_t>                        mads_map_t;
        typedef std::vector< std::pair<__time_t, uint64_t> >     time_histogram_t;

    public:
        typedef struct mads_record {
            struct timespec                start_time;
            struct timespec                end_time;

            std::string                    name;
            mads_map_t                     table;

            time_histogram_t               histogram;
            time_histogram_t::iterator     histogram_entry;

            mads_record(const std::string & name) : name(name)
            {
                clock_gettime(CLOCK_REALTIME, &start_time);

                end_time.tv_sec    = 0;
                end_time.tv_nsec   = 0;
            }

        } mads_record_t;

    public:
        class histogram_base {
            public:
                uint64_t    m_max_value;
                uint64_t    m_min_value;
                uint64_t    m_total;

                __time_t    m_max_time;
                __time_t    m_min_time;
                __time_t    m_end_time;

                bool        m_is_valid;

            private:
                histogram_base();
                histogram_base(const histogram_base &);

            public:
                histogram_base(const mads_record_t * record);

        };

    public:
        typedef std::vector<mads_record_t*> mads_db_t;

    private:
        typedef struct {
            mads_map_t::iterator    first;
            mads_map_t::iterator    second;
            mads_map_t::iterator    thrid;
        } mads_cache_t;


    private:
        mads_db_t           mads_db;
        mads_record_t *     mads_record;
        mads_cache_t        mads_cache;
        std::string         output_prefix;
        bool                is_histogram_enabled;
        bool                is_histogram_full_output;

    public:
        IbisMadsStat()
            : mads_record(NULL), is_histogram_enabled(true), is_histogram_full_output(false)
        {
        }

        ~IbisMadsStat();

    public:
        const mads_db_t & data() const { return mads_db; }

    public:
        void set_output_prefix(const std::string & prefix) {
            output_prefix = prefix;
        }

        void set_histogram_full_output(bool histogram_full_output) {
            is_histogram_full_output = histogram_full_output;
        }

    public:
        void add(uint8_t * raw);
        void start(const std::string & name);
        void stop();
        void clear();
        void resume();

    private:
        void aggregate(mads_record_t & record);

    public:
        std::ostream & output_all_mads_tables(std::ostream & stream, bool skip_empty);
        std::ostream & output_all_time_histograms(std::ostream & stream, bool skip_empty);
        std::ostream & output_all_records(std::ostream & stream, bool skip_empty);
        std::ostream & output_all_records_csv(std::ostream & stream);
        std::ostream & output_all_perf_records_csv(std::ostream & stream);

        std::ostream & output_mads_table_summary(std::ostream & stream);
        std::ostream & output_mads_table(std::ostream & stream, const mads_record_t * record);

        std::ostream & output_time_histogram_summary(std::ostream & stream);
        std::ostream & output_time_histogram(std::ostream & stream, const mads_record_t * record);

        std::ostream & output_summary(std::ostream & stream);
        std::ostream & output_record(std::ostream & stream, const mads_record_t * record);

};

#endif /* IBIS_STAT_H_ */
