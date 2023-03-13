#ifndef STT_CP_H
#define STT_CP_H

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <assert.h>
#include "44bsd/tcp_var.h"
#include "44bsd/flow_table.h"
#include "utl_counter.h"
#include "utl_dbl_human.h"
#include "dyn_sts.h"



/* TCP Stateful CP object for counters and such */


typedef uint8_t tcp_dir_t;
typedef std::map<std::string, std::pair<CGSimpleBase*, CGSimpleBase*>> dyn_sts_range_map_t;
typedef std::map<std::string, CDynStsCpGroup*> dyn_sts_map_t;
typedef std::vector<CDynStsCpGroup*> dyn_sts_group_vec_t;

class CSTTCp;

class CSTTCpPerTGIDPerDir {
public:
    bool Create(uint32_t time_msec, bool is_dynamic=false);
    void Delete();
    void update_counters(bool is_sum, uint16_t tg_id=0);
    void accumulate_counters(CSTTCpPerTGIDPerDir* lpstt_sts);
    void calculate_ft_counters(CSTTCpPerTGIDPerDir* lpstt_sts);
    void calculate_avr_counters();
    void accumulate_dyn_counters(CSTTCpPerTGIDPerDir* lpstt_sts);
    void clear_counters();
    void create_clm_counters();
    void clear_sum_counters();
    void dump_dyn_stats_desc(Json::Value&);
    bool add_dyn_stats(const meta_data_t* meta_data, cp_dyn_sts_group_args_t* dyn_sts_group_args);
    bool delete_dyn_sts(std::string sts_group_name);
    void clear_dps_dyn_counters();

private:
    void clear_aggregated_counters();

public:
    CTcpStats           m_tcp;
    CUdpStats           m_udp;
    CAppStats           m_app;
    CSttFlowTableStats  m_ft;
    dyn_sts_group_vec_t m_dyn_sts;
    dyn_sts_range_map_t m_dyn_sts_range_map;
    dyn_sts_map_t       m_dyn_sts_map;

    /* externation counters */
    uint64_t            m_active_flows;
    uint64_t            m_est_flows;

    CBwMeasure          m_tx_bw_l7;
    double              m_tx_bw_l7_r;
    CBwMeasure          m_tx_bw_l7_total;
    double              m_tx_bw_l7_total_r;
    CBwMeasure          m_rx_bw_l7;
    double              m_rx_bw_l7_r;
    CPPSMeasure         m_tx_pps;
    double              m_tx_pps_r;
    CPPSMeasure         m_rx_pps;
    double              m_rx_pps_r;

    double              m_avg_size;
    double              m_tx_ratio;

    double              m_traffic_duration;

    /* externation counters --end */

    CGTblClmCounters    m_clm; /* utility for dump */
    uint32_t            m_clm_static_size;

    std::vector<CTcpPerThreadCtx*>  m_tcp_ctx; /* vectors contexts*/

    std::vector<CPerProfileCtx*>  m_profile_ctx; /* profile context */
};

class CSTTCp {

public:
    void Create(uint32_t stt_id=0, uint16_t num_of_tg_ids=1, bool first_time=true);
    void Delete(bool last_time=true);
    void Add(tcp_dir_t dir, CTcpPerThreadCtx* ctx);
    void AddProfileCtx(tcp_dir_t dir, CPerProfileCtx* pctx);
    void Init(bool first_time=true, bool is_dynamic=false);
    void Update();
    void Accumulate(bool clear, bool calculate, CSTTCp* lpstt);
    void DumpTable();
    bool dump_json(std::string &json);
    void clear_counters(bool epoch_increment=true);
    void Resize(uint16_t new_num_of_tg_ids);
    void DumpTGNames(Json::Value &result);
    void UpdateTGNames(const std::vector<std::string>& tg_names);
    void DumpTGDynStatsDesc(Json::Value &result);
    void DumpTGStats(Json::Value &result, const std::vector<uint16_t>& tg_ids);
    void UpdateTGStats(const std::vector<uint16_t>& tg_ids);
    void clear_profile_ctx();
    void update_profile_ctx();
    bool need_profile_ctx_update() { return !m_profile_ctx_updated; }
    bool Add_dyn_stats(const meta_data_t* meta_data, cp_dyn_sts_group_args_t dyn_sts_group_args[TCP_CS_NUM]);
    bool Delete_dyn_sts(std::string group_sts_name);
    void clear_dps_dyn_counters();

public:
    uint64_t                            m_epoch;
    std::vector<CSTTCpPerTGIDPerDir*>   m_sts_per_tg_id[TCP_CS_NUM];
    std::vector<CTblGCounters*>         m_dtbl_per_tg_id;
    CSTTCpPerTGIDPerDir                 m_sts[TCP_CS_NUM]; // This isn't really per TGID, it's the sum over all TGIDs per client/server
    CTblGCounters                       m_dtbl;
    bool                                m_init;
    bool                                m_update;
    uint16_t                            m_num_of_tg_ids;
    std::vector<std::string>            m_tg_names;

private:
    uint32_t m_stt_id;
    bool     m_profile_ctx_updated;
};


#endif
