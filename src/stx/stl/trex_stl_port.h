/*
 Itay Marom
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
#ifndef __TREX_STL_PORT_H__
#define __TREX_STL_PORT_H__

#include "common/basic_utils.h"

#include <json/json.h> 

/* common files */
#include "trex_port.h"
#include "trex_rx_defs.h"
#include "trex_exception.h"
#include "trex_capture.h"

/* STL files */
#include "trex_stl_stream.h"


class TrexCpToDpMsgBase;
class TrexCpToRxMsgBase;
class TrexStreamsGraphObj;
class TrexPortMultiplier;
class TrexPktBuffer;


class AsyncStopEvent;
class AsyncProfileStopEvent;

class TrexProfileTable;



/**
 * port multiplier object
 *
 */
class TrexPortMultiplier {
public:


    /**
     * defines the type of multipler passed to start
     */
    enum mul_type_e {
        MUL_FACTOR,
        MUL_BPS,
        MUL_BPSL1,
        MUL_PPS,
        MUL_PERCENTAGE
    };

    /**
     * multiplier can be absolute value
     * increment value or subtract value
     */
    enum mul_op_e {
        OP_ABS,
        OP_ADD,
        OP_SUB
    };


    TrexPortMultiplier(mul_type_e type, mul_op_e op, double value) {
        m_type   = type;
        m_op     = op;
        m_value  = value;
    }

    TrexPortMultiplier(const std::string &type_str, const std::string &op_str, double value);


public:
    static const std::initializer_list<std::string> g_types;
    static const std::initializer_list<std::string> g_ops;

    mul_type_e             m_type;
    mul_op_e               m_op;
    double                 m_value;
};


/**
 * describes a stateless profile
 *
 * @author imarom (31-Aug-15)
 */
class TrexStatelessProfile: public TrexPort {
    
    friend TrexDpPortEvents;
    friend TrexDpPortEvent;
    friend AsyncProfileStopEvent;

public:

    TrexStatelessProfile(uint8_t port_id, std::string profile_id); 
    ~TrexStatelessProfile();


    /*  implemented for Json RPC related Profile */
    /* provides storage for the profile json*/
    void store_profile_json(const Json::Value &profile_json);

    /* access the profile json */
    const Json::Value & get_profile_json();

    /* original template provided by requester */
    Json::Value m_profile_json;    


    /**
     * validate the state of the port before start
     * it will return a stream graph
     * containing information about the streams
     * configured on this port
     *
     * on error it throws TrexException
     */
    const TrexStreamsGraphObj *validate(void);

    /**
     * start traffic
     * throws TrexException in case of an error
     */
    void start_traffic(const TrexPortMultiplier &mul, double duration, bool force = false, uint64_t core_mask = UINT64_MAX, double start_at_ts = 0);

    /**
     * stop traffic
     * throws TrexException in case of an error
     */
    void stop_traffic(void);

    /**
     * remove all RX filters 
     * valid only when port is stopped 
     * 
     * @author imarom (28-Mar-16)
     */
    void remove_rx_filters(void);

    /**
     * pause traffic
     * throws TrexException in case of an error
     */
    void pause_traffic(void);
    void pause_streams(stream_ids_t &stream_ids);

    /**
     * resume traffic
     * throws TrexException in case of an error
     */
    void resume_traffic(void);
    void resume_streams(stream_ids_t &stream_ids);

    /**
     * update current traffic on port
     *
     */
    void update_traffic(const TrexPortMultiplier &mul, bool force);
    void update_streams(const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams);


    /**
     * sets service mode
     * 
     * @author imarom (1/22/2017)
     * 
     * @param enabled 
     */
    
    bool is_service_mode_on() const {
        return false;
    }

    bool is_service_filtered_mode_on() const {
        return false;
    }

    bool is_running_flow_stats();
    bool has_flow_stats();

    /**
     * the the max stream id currently assigned
     *
     */
    int get_max_stream_id() const;

    /**
     * delegators
     *
     */

    void add_stream(TrexStream *stream);
    void remove_stream(TrexStream *stream);
    void remove_and_delete_all_streams();

    TrexStream * get_stream_by_id(uint32_t stream_id) {
        return m_stream_table.get_stream_by_id(stream_id);
    }

    int get_stream_count() {
        return m_stream_table.size();
    }

    void get_id_list(std::vector<uint32_t> &id_list) {
        m_stream_table.get_id_list(id_list);
    }

    void get_object_list(std::vector<TrexStream *> &object_list) {
        m_stream_table.get_object_list(object_list);
    }

    
    /**
     * returns the traffic multiplier currently being used by the DP
     *
     */
    double get_multiplier() {
        return (m_factor);
    }


    /**
     * get the port effective rate (on a started / paused port)
     *
     * @author imarom (07-Jan-16)
     *
     */
    void get_port_effective_rate(double &pps,
                                 double &bps_L1,
                                 double &bps_L2,
                                 double &percentage);
    std::string m_profile_id;


    /**
     * profile_id  for DP
     */
    uint32_t m_dp_profile_id;

    /* verify state for profile */
    bool verify_profile_state(int state, const char *cmd_name, bool should_throw = true) const;

private:

    /**
     * when a port stops, perform various actions
     *
     */
    void common_profile_stop_actions(bool async);
    
    /**
     * calculate effective M per core
     *
     */
    double calculate_effective_factor(const TrexPortMultiplier &mul, bool force, const TrexStreamsGraphObj &graph_obj);
    double calculate_effective_factor_internal(const TrexPortMultiplier &mul, const TrexStreamsGraphObj &graph_obj);


    /**
     * generates a graph of streams graph
     *
     */
    void generate_streams_graph();

    /**
     * dispose of it
     *
     * @author imarom (26-Nov-15)
     */
    void delete_streams_graph();


    TrexStreamTable    m_stream_table;

    bool               m_last_all_streams_continues;
    double             m_last_duration;
    double             m_factor;
    uint32_t           m_non_explicit_dst_macs_count;
    uint32_t           m_flow_stats_count;

    /* holds a graph of streams rate*/
    const TrexStreamsGraphObj  *m_graph_obj;

    bool  m_is_service_mode_on;
    
    static const uint32_t MAX_STREAMS = 20000;

};



/**
 *  TrexProfileTable class 
 *  
 */
class TrexProfileTable {
public:

    TrexProfileTable();
    ~TrexProfileTable();

    /**
     * add a profile 
     * if a previous one exists, the old one  will be deleted 
     */
    void add_profile(TrexStatelessProfile *mprofile);

 
    /**
     * fetch a stream if exists 
     * o.w NULL 
     *  
     */
    TrexStatelessProfile * get_profile_by_id(string profile_id);


    /** 
     * populate a list with all the profile IDs
     * 
     * @author imarom (06-Sep-15)
     * 
     * @param profile_list 
     */
    void get_profile_id_list(std::vector<string> &profile_id_list);

    /** 
     * populate a list with all the stream objects
     * 
     */
    void get_profile_object_list(std::vector<TrexStatelessProfile *> &profile_object_list);

    /** 
     * get the table size
     * 
     */
    int size();

    std::unordered_map<string, TrexStatelessProfile *>::iterator begin() {return m_profile_table.begin();}
    std::unordered_map<string, TrexStatelessProfile *>::iterator end() {return m_profile_table.end();}

private:
    /**
     * holds all the profile in a hash table by profile id
     * 
     */
    std::unordered_map<string, TrexStatelessProfile *> m_profile_table;
};     




/**
 * describes a stateless port
 *
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort : public TrexPort {
    
    friend TrexDpPortEvents;
    friend TrexDpPortEvent;
    friend AsyncStopEvent;

public:

    TrexStatelessPort(uint8_t port_id);

    ~TrexStatelessPort();


    /**
     * validate the state of the port before start
     * it will return a stream graph
     * containing information about the streams
     * configured on this port
     *
     * on error it throws TrexException
     */
    const TrexStreamsGraphObj *validate(string profile_id);

    /**
     * start traffic
     * throws TrexException in case of an error
     */
    void start_traffic(string profile_id, const TrexPortMultiplier &mul, double duration, bool force = false, uint64_t core_mask = UINT64_MAX, double start_at_ts = 0);


    /**
     * stop traffic
     * throws TrexException in case of an error
     */
    void stop_traffic(string profile_idi = "_");
    void stop_traffic_pcap();
    /**
     * remove all RX filters 
     * valid only when port is stopped 
     * 
     * @author imarom (28-Mar-16)
     */
    void remove_rx_filters(string profile_id);

    /**
     * pause traffic
     * throws TrexException in case of an error
     */
    void pause_traffic(string profile_id);
    void pause_streams(string profile_id, stream_ids_t &stream_ids);

    /**
     * resume traffic
     * throws TrexException in case of an error
     */
    void resume_traffic(string profile_id);
    void resume_streams(string profile_id, stream_ids_t &stream_ids);

    /**
     * update current traffic on port
     *
     */
    void update_traffic(string profile_id, const TrexPortMultiplier &mul, bool force);
    void update_streams(string profile_id, const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams);

    /**
     * push a PCAP file onto the port
     * 
     */
    void push_remote(const std::string &pcap_filename,
                     double            ipg_usec,
                     double            min_ipg_sec,
                     double            speedup,
                     uint32_t          count,
                     double            duration,
                     bool              is_dual);

    

    /**
     * sets service mode
     * 
     * @author imarom (1/22/2017)
     * 
     * @param enabled 
     */
    void set_service_mode(bool enabled); // wraps for backwards compatibility
    void set_service_mode(bool enabled, bool filtered, uint8_t mask);

    bool is_service_mode_on() const {
        return m_is_service_mode_on;
    }

    bool is_service_filtered_mode_on() const {
        return m_is_service_filtered_mode_on;
    }

    bool is_running_flow_stats();
    bool has_flow_stats(string profile_id);

    /**
     * the the max stream id currently assigned
     *
     */
    int get_max_stream_id(string profile_id);
    int get_max_stream_id();


    /**
     * delegators
     *
     */

    void add_stream(string profile_id, TrexStream *stream);
    void remove_stream(string profile_id, TrexStream *stream);
    void remove_and_delete_all_streams(string profile_id = "_");

    TrexStream * get_stream_by_id(string profile_id, uint32_t stream_id) {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        return mprofile->get_stream_by_id(stream_id);
    }

    int get_stream_count(string profile_id = "_") {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        return mprofile->get_stream_count();
    }

    void get_id_list(string profile_id, std::vector<uint32_t> &id_list) {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        mprofile->get_id_list(id_list);
    }

    void get_object_list(string profile_id, std::vector<TrexStream *> &object_list) {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        mprofile->get_object_list(object_list);
    }

   
    TrexStatelessProfile * get_profile_by_id(string profile_id) {
        return m_profile_table.get_profile_by_id(profile_id);
    }

    int get_profile_count() {
        return m_profile_table.size();
    }

    void get_profile_id_list(std::vector<string> &profile_id_list) {
        m_profile_table.get_profile_id_list(profile_id_list);
    }

    void get_profile_object_list(std::vector<TrexStatelessProfile *> &profile_object_list) {
        m_profile_table.get_profile_object_list(profile_object_list);
    }

    /**
     * returns the traffic multiplier currently being used by the DP
     *
     */
    double get_multiplier(string profile_id) {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        return mprofile->get_multiplier();
    }

    /**
     * get the port effective rate (on a started / paused port)
     *
     * @author imarom (07-Jan-16)
     *
     */
    void get_port_effective_rate(double &pps,
                                 double &bps_L1,
                                 double &bps_L2,
                                 double &percentage);

    /**
     * get profile state
     */
    TrexPort::port_state_e get_profile_state(string profile_id);

    void get_state_as_string(string profile_id, Json::Value &result);

    void get_profiles_status(Json::Value &result);

    /**
     * profile state as string
     */
    std::string get_profile_state_as_string(string profile_id);

    /**
     * update port state with default profile("_") state
     */
    void update_port_state();

    void update_and_publish_port_state(bool async = false);

    string default_profile = "_";

    void valide_profile(string profile_id);

    void get_profile_by_dp_id(uint32_t profile_id);

    TrexDpPortEvents & get_dp_events(uint32_t profile_id) {

        std::vector <TrexStatelessProfile *> profiles;
        get_profile_object_list(profiles);

        for (auto &mprofile : profiles) {
            if ((mprofile->m_dp_profile_id) == profile_id) {
                return mprofile->m_dp_events;
            }
        }
        return m_dp_events;
    }     


private:

    
    /**
     * when a port stops, perform various actions
     *
     */
    void common_port_stop_actions(bool async);


    /**
     * generates a graph of streams graph
     *
     */
    void generate_streams_graph();

    /**
     * dispose of it
     *
     * @author imarom (26-Nov-15)
     */
    void delete_streams_graph();

    bool  m_is_service_mode_on;
    bool  m_is_service_filtered_mode_on;

    TrexProfileTable    m_profile_table;

    uint32_t m_dp_profile_id_inc = 0;
};


#endif /* __TREX_STL_PORT_H__ */

