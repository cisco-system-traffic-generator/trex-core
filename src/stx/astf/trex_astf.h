/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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
#ifndef __TREX_ASTF_H__
#define __TREX_ASTF_H__

#include <stdint.h>
#include <vector>
#include <map>

#include "common/trex_stx.h"
#include "trex_astf_defs.h"
#include "stt_cp.h"

class TrexAstfPort;
class CSyncBarrier;
class CRxAstfCore;

typedef std::unordered_map<uint8_t, TrexAstfPort*> astf_port_map_t;

const std::string DEFAULT_ASTF_PROFILE_ID = "_";

typedef enum trex_astf_hash {
    HASH_OK,
    HASH_ON_SAME_PROFILE,
    HASH_ON_OTHER_PROFILE,
} trex_astf_hash_e;

class AstfProfileState {
public:
    enum state_e {
        STATE_IDLE,
        STATE_LOADED,
        STATE_PARSE, // by DP core 0, not cancelable
        STATE_BUILD, // by DP cores, not cancelable
        STATE_TX,
        STATE_CLEANUP, // by DP cores, not cancelable
        AMOUNT_OF_STATES,
    };
};


/***********************************************************
 * TRex ASTF each profile in interactive
 ***********************************************************/
class TrexAstfPerProfile : public AstfProfileState {
public:
    TrexAstfPerProfile(uint32_t profile_index=0) {
        m_profile_index = profile_index;
        m_profile_state = STATE_IDLE;
        m_profile_buffer = "";
        m_profile_hash = "";
        m_profile_parsed = false;
        m_profile_stopping = false;

        m_active_cores = 0;
        m_duration = 0.0;
        m_factor = 1.0;
        m_error = "";

        m_stt_cp = new CSTTCp();
        m_stt_cp->Create(profile_index);
    }

    ~TrexAstfPerProfile() {
        m_stt_cp->Delete();
        delete m_stt_cp;
        m_stt_cp=0;
    }

    /**
     * CP sends a message to DP using profile_index(profile_id for DP),
     * which is converted from string type profile_id to integer.
     */
    int32_t         m_profile_index;

    state_e         m_profile_state;
    std::string     m_profile_buffer;
    std::string     m_profile_hash;
    bool            m_profile_parsed;
    bool            m_profile_stopping;

    int16_t         m_active_cores;
    double          m_duration;
    double          m_factor;
    std::string     m_error;
    CSTTCp*         m_stt_cp;
};


/***********************************************************
 * TRex ASTF dynamic multiple profiles in interactive
 ***********************************************************/
class TrexAstfProfile : public AstfProfileState {
public:
    typedef std::vector<state_e> states_t;

    TrexAstfProfile();
    ~TrexAstfProfile();

    /**
     * Handle dynamic multiple profiles
     */
    void add_profile(std::string profile_id);
    bool delete_profile(std::string profile_id);
    bool is_valid_profile(std::string profile_id);
    uint32_t get_num_profiles();

    TrexAstfPerProfile* get_profile_by_id(std::string profile_id);
    uint32_t            get_profile_index_by_id(std::string profile_id);
    state_e             get_profile_state_by_id(std::string profile_id);
    std::string         get_profile_id_by_index(uint32_t profile_index);
    CSTTCp*             get_sttcp_by_id(std::string profile_id);

    std::vector<std::string> get_profile_id_list();
    std::vector<state_e>     get_profile_state_list();
    std::vector<CSTTCp *>    get_sttcp_list();

    /**
     * Profile-related:
     * cmp - compare hash to loaded profile, true = matches (heavy profile will not be uploaded twice)
     * clear - clears profile JSON string (if any)
     * append - appends fragment to profile JSON string
     * loaded - move state from idle to loaded
     */
    trex_astf_hash_e profile_cmp_hash(std::string profile_id, const std::string &hash);
    void profile_clear(std::string profile_id);
    void profile_init(std::string profile_id);
    void profile_append(std::string profile_id, const std::string &fragment);
    void profile_set_loaded(std::string profile_id);

    /**
     * Handle profile status
     */
    void profile_change_state(std::string profile_id, state_e new_state);
    void profile_check_whitelist_states(std::string profile_id, const states_t &whitelist);
    bool is_profile_state_build(std::string profile_id);

    bool profile_needs_parsing(std::string profile_id);
    bool is_error(std::string profile_id) {
        return get_profile_by_id(profile_id)->m_error.size() > 0;
    }

protected:
    std::vector<std::string> m_states_names;
    bool is_another_profile_transmitting(std::string profile_id);
    bool is_another_profile_busy(std::string profile_id);
    virtual void del_cmd_in_wait_list(std::string profile_id) = 0;
    virtual void publish_astf_state(std::string profile_id) = 0;
    virtual void publish_astf_profile_clear(std::string profile_id) = 0;

private:
    uint32_t m_profile_last_index;
    std::unordered_map<std::string, TrexAstfPerProfile *> m_profile_list;
};


/***********************************************************
 * TRex adavanced stateful interactive object
 ***********************************************************/
class TrexAstf : public TrexAstfProfile, public TrexSTX {
public:
    enum cmd_wait_e {
        CMD_PARSE,
        CMD_BUILD,
        CMD_START,
        CMD_STOP,
        CMD_CLEANUP,
        AMOUNT_OF_CMDS,
    };

    enum state_latency_e {
        STATE_L_IDLE,
        STATE_L_WORK,
        AMOUNT_OF_L_STATES,
    };

    /** 
     * create ASTF object 
     */
    TrexAstf(const TrexSTXCfg &cfg);
    virtual ~TrexAstf();

    /**
     * ASTF control plane
     * 
     */
    void launch_control_plane();

    /**
     * shutdown ASTF
     * 
     */
    void shutdown();

    void all_dp_cores_finished(uint32_t profile_index);
    void dp_core_finished(int thread_id, uint32_t profile_index);

    /**
     * async data sent for ASTF
     * 
     */
    void publish_async_data();

    /**
     * create a ASTF DP core
     * 
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);

    /**
     * fetch astf port object by port ID
     *
     */
    TrexAstfPort *get_port_by_id(uint8_t port_id);

    /**
     * returns the port list as astf port objects
     */
    const astf_port_map_t &get_port_map() const {
        return *(astf_port_map_t *)&m_ports;
    }

    /**
     * Acquire context (and all ports)
     */
    void acquire_context(const std::string &user, bool force);

    /**
     * Release context (and all ports)
     */
    void release_context();

    /**
     * Topology-related:
     * cmp - compare hash to loaded topo, true = matches (heavy topo will not be uploaded twice)
     * clear - clears topo JSON string (if any)
     * append - appends fragment to topo JSON string
     * loaded - mark that topo JSON string was fully loaded
     */
    bool topo_cmp_hash(const std::string &hash);
    void topo_clear();
    void topo_append(const std::string &fragment);
    void topo_set_loaded();
    void topo_get(Json::Value &obj);

    /**
     * Stats Related
     */
    bool is_state_build();

    /**
     * Start transmit
     */
    void start_transmit(std::string profile_id, const start_params_t &args);

    /**
     * Stop transmit
     */
    void stop_transmit(std::string profile_id);

    /**
     * Update traffic rate
     */
    void update_rate(std::string profile_id, double mult);

    TrexOwner& get_owner() {
        return m_owner;
    }

    /**
     * Start transmit latency streams only 
     */
    void start_transmit_latency(const lat_start_params_t &args);

    /**
     * Stop transmit latency streams only 
     */
    bool stop_transmit_latency();


    /**
     * Get latency stats
     */
    void get_latency_stats(Json::Value & obj);

    /**
     * update latency rate command
     */
    void update_latency_rate(double mult);

    CSyncBarrier* get_barrier() {
        return m_sync_b;
    }

    void dp_core_error(int thread_id, uint32_t profile_index, const std::string &err);

    state_e get_state() {
        return m_state;
    }

    uint64_t get_epoch() {
        return m_epoch;
    }

    void inc_epoch();

    CRxAstfCore * get_rx(){
        assert(m_rx);
        return((CRxAstfCore *)m_rx);
    }

    bool topo_needs_parsing();

    /**
     * update_astf_state          : update state for all profiles
     * get_profiles_status        : get JSON value list for all profiles id
     * publish_astf_state         : publish state change event for profile
     * publish_astf_profile_clear : publish clear event for profile
     */
    void update_astf_state();
    void get_profiles_status(Json::Value &result);
    void publish_astf_state(std::string profile_id);
    void publish_astf_profile_clear(std::string profile_id);

    /**
     * run_cmd_in_wait_list : run the first command in m_cmd_wait_list
     * add_cmd_in_wait_list : add command in m_cmd_wait_list
     * del_cmd_in_wait_list : delete command in m_cmd_wait_list
     * get_cmd_in_wait_list : get command in m_cmd_wait_list
     */
    void run_cmd_in_wait_list();
    void add_cmd_in_wait_list(std::string profile_id, cmd_wait_e cmd);
    void del_cmd_in_wait_list(std::string profile_id);
    cmd_wait_e get_cmd_in_wait_list(std::string profile_id);

protected:
    // message to DP involved:
    void parse(std::string profile_id);
    void build(std::string profile_id);
    void transmit(std::string profile_id, double duration);
    void cleanup(std::string profile_id);
    bool is_trans_state();

    void change_state(state_e new_state);
    void set_barrier(double timeout_sec);
    void send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg, bool clone = false);
    void send_message_to_all_dp(TrexCpToDpMsgBase *msg);

    void check_whitelist_states(const states_t &whitelist);
    //void check_blacklist_states(const states_t &blacklist);

    void ports_report_state(state_e state);

    state_latency_e m_l_state;
    uint32_t        m_latency_pps;
    bool            m_lat_with_traffic;

    TrexOwner       m_owner;
    state_e         m_state;
    CSyncBarrier   *m_sync_b;
    CFlowGenList   *m_fl;
    CParserOption  *m_opts;
    std::string     m_topo_buffer;
    std::string     m_topo_hash;
    bool            m_topo_parsed;
    uint64_t        m_epoch;

private:
    /* Waiting list to serialize mass command execution. <profile_id, cmd> */
    std::vector<pair<std::string, cmd_wait_e>> m_cmd_wait_list;
};

static inline TrexAstf * get_astf_object() {
    return (TrexAstf *)get_stx();
}


#endif /* __TREX_STL_H__ */

