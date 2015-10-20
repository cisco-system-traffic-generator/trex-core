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
#ifndef __TREX_STATELESS_PORT_H__
#define __TREX_STATELESS_PORT_H__

#include <trex_stream.h>

/**
 * bandwidth measurement class
 * 
 */
class BWMeasure {
public:
    BWMeasure();
    void reset(void);
    double add(uint64_t size);

private:
    double calc_MBsec(uint32_t dtime_msec,
                      uint64_t dbytes);

public:
   bool      m_start;
   uint32_t  m_last_time_msec;
   uint64_t  m_last_bytes;
   double    m_last_result;
};

/**
 * TRex stateless port stats
 * 
 * @author imarom (24-Sep-15)
 */
class TrexPortStats {

public:
    TrexPortStats() {
        m_stats = {0};

        m_bw_tx_bps.reset();
        m_bw_rx_bps.reset();

        m_bw_tx_pps.reset();
        m_bw_rx_pps.reset();
    }

public:

    BWMeasure m_bw_tx_bps;
    BWMeasure m_bw_rx_bps;

    BWMeasure m_bw_tx_pps;
    BWMeasure m_bw_rx_pps;

    struct {

        double m_tx_bps;
        double m_rx_bps;
       
        double m_tx_pps;
        double m_rx_pps;
       
        uint64_t m_total_tx_pkts;
        uint64_t m_total_rx_pkts;
       
        uint64_t m_total_tx_bytes;
        uint64_t m_total_rx_bytes;
       
        uint64_t m_tx_rx_errors;
    } m_stats;
};

/**
 * describes a stateless port
 * 
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort {
public:

    /**
     * port state
     */
    enum port_state_e {
        PORT_STATE_DOWN,
        PORT_STATE_UP_IDLE,
        PORT_STATE_TRANSMITTING
    };

    /**
     * describess different error codes for port operations
     */
    enum rc_e {
        RC_OK,
        RC_ERR_BAD_STATE_FOR_OP,
        RC_ERR_NO_STREAMS,
        RC_ERR_FAILED_TO_COMPILE_STREAMS
    };

    TrexStatelessPort(uint8_t port_id);

    /**
     * start traffic
     * 
     */
    rc_e start_traffic(void);

    /**
     * stop traffic
     * 
     */
    void stop_traffic(void);

    /**
     * access the stream table
     * 
     */
    TrexStreamTable *get_stream_table();

    /**
     * get the port state
     * 
     */
    port_state_e get_state() {
        return m_port_state;
    }

    /**
     * port state as string
     * 
     */
    std::string get_state_as_string();

    /**
     * fill up properties of the port
     * 
     * @author imarom (16-Sep-15)
     * 
     * @param driver 
     * @param speed 
     */
    void get_properties(std::string &driver, std::string &speed);

    /**
    * query for ownership
    * 
    */
    const std::string &get_owner() {
        return m_owner;
    }

    /**
     * owner handler 
     * for the connection 
     * 
     */
    const std::string &get_owner_handler() {
        return m_owner_handler;
    }

    bool is_free_to_aquire() {
        return (m_owner == "none");
    }

    /**
    * take ownership of the server array 
    * this is static 
    * ownership is total 
    * 
    */
    void set_owner(const std::string &owner) {
        m_owner = owner;
        m_owner_handler = generate_handler();
    }

    void clear_owner() {
        m_owner = "none";
        m_owner_handler = "";
    }

    bool verify_owner_handler(const std::string &handler) {

        return ( (m_owner != "none") && (m_owner_handler == handler) );

    }

    /**
     * update the values of the stats
     * 
     */
    void update_stats();

    const TrexPortStats & get_stats();

    /**
     * encode stats as JSON
     */
    void encode_stats(Json::Value &port);

private:

    std::string generate_handler();

    TrexStreamTable  m_stream_table;
    uint8_t          m_port_id;
    port_state_e     m_port_state;
    std::string      m_owner;
    std::string      m_owner_handler;
    TrexPortStats    m_stats;
};

#endif /* __TREX_STATELESS_PORT_H__ */
