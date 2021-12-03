/*
 Bes Dollma
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2021-2021 Cisco Systems, Inc.

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

#ifndef __TREX_TAGGED_PACKET_GROUPS_H__
#define __TREX_TAGGED_PACKET_GROUPS_H__


#include <unordered_map>
#include "trex_stl_fs.h"
#include "trex_exception.h"
#include "trex_global.h"

/*
    Tagged Flow stats are flow stats that take in consideration tagged packets
    such as dot1Q, QinQ, VxLAN.
    The user specifies the tags upon initialization and we calculate the statistics
    based on the tags.
    - This file defines a PacketGroupTagMgr object used in Control Plane to map the Tags to the TagId.
    - It defines a Tagged Packet Group Stream Manager used in Control Plane to investigate the user 
      defined streams.
    - It defines a Tagged Packet Group Data Plane Manager to keep hold of the sequence numbers for each
      tpgid.
*/

#define TPG_PAYLOAD_MAGIC 0xE50B5CC1 // Consider the endianess

/**************************************
 * Tagged Packet Group Payload Header
 *************************************/
struct tpg_payload_header {
    uint32_t magic;         // Magic to differentiate the packets
    uint32_t tpgid;         // Tagged Packet Group Identifier
    uint32_t seq;           // Sequence Number
    uint32_t reserved;      // Reserved for future use
};

/**
 * Is a uint16 a valid vlan? Valid Vlans are [1-4094].
 * @param vlan
    Vlan we are meant to check its validity.
 * @return bool
    True iff vlan is a valid Vlan value.
**/
inline bool valid_vlan(uint16_t vlan) {
    return (vlan == 0 or vlan >= (1 << 12) - 1 ) ? false : true;
}

/**************************************
 * Tag Definitions
 *************************************/
class BasePacketGroupTag {
    // Abstract Base Class representing a Packet Group Tag.
public:

    virtual ~BasePacketGroupTag() = 0; // Make the class pure abstract so no one can instantiate.

    /**
     * get_tag returns the tag identifier.
    **/
    inline uint16_t get_tag() {return m_tag;}

    /**
     * set_tag sets the tag identifier.
    **/
    inline void set_tag(uint16_t tag) {m_tag = tag;}

private:
    uint16_t m_tag;
};


class Dot1QTag: public BasePacketGroupTag {

public:

    Dot1QTag(uint16_t vlan, uint16_t tag) {
        if (!valid_vlan(vlan)) {
            throw TrexException("Vlan must be 1-4094.");
        }
        m_vlan = vlan;
        set_tag(tag);
    }

    /**
     * Get the Vlan of a Dot1Q.
     *
     * @return uint16_t
     *   Vlan
     */
    inline uint16_t get_vlan() { return m_vlan; }

private:
    uint16_t m_vlan;
};


class QinQTag: public BasePacketGroupTag {

public:
    QinQTag(uint16_t inner_vlan, uint16_t outter_vlan, uint16_t tag) {
        if (!valid_vlan(inner_vlan) || !valid_vlan(outter_vlan)) {
            throw TrexException("Vlan must be 1-4094.");
        }
        m_inner_vlan = inner_vlan;
        m_outter_vlan = outter_vlan;
        set_tag(tag);
    }

    /**
     * Get the inner Vlan of a QinqQ.
     *
     * @return uint16_t
     *   Inner Vlan
     */
    inline uint16_t get_inner_vlan() { return m_inner_vlan; }

    /**
     * Get the outter Vlan of a QinqQ.
     *
     * @return uint16_t
     *   Outter Vlan
     */
    inline uint16_t get_outter_vlan() { return m_outter_vlan; }

private:
    uint16_t m_inner_vlan;
    uint16_t m_outter_vlan;

};


/**************************************
 * Packet Group Tag Manager
 *************************************/
class PacketGroupTagMgr {

public:
    PacketGroupTagMgr() : m_num_tags(0) {};

    PacketGroupTagMgr(const PacketGroupTagMgr* tag_mgr);

    ~PacketGroupTagMgr();

    PacketGroupTagMgr(const PacketGroupTagMgr&) = delete;

    PacketGroupTagMgr& operator=(const PacketGroupTagMgr&) = delete;

    /**
     * Check if Dot1Q Vlan exists as a valid tag.
     *
     * @param vlan
     *      Vlan to check if it is defined as a tag.
     *
     * @return bool
     *      True if the Vlan exists as a tag, False otherwise.
     **/
    inline bool dot1q_tag_exists(uint16_t vlan) {
        return m_dot1q_map.find(vlan) != m_dot1q_map.end();
    }

    /**
     * Check if QinQ Vlans exist as a valid tag.
     *
     * @param inner_vlan
     *      Inner Vlan in QinQ
     *
     * @param outter_vlan
     *      Outter Vlan in QinQ
     *
     * @return bool
     *      True if the QinQ exists as a tag, False otherwise.
     **/
    inline bool qinq_tag_exists(uint16_t inner_vlan, uint16_t outter_vlan) {
        return m_qinq_map.find(get_qinq_key(inner_vlan, outter_vlan)) != m_qinq_map.end();
    }

    /**
     * Get the number of Tags in the Tag Manager.
     *
     * @return uint16_t
     *    Number of Tags in Tag Manager.
     **/
    inline uint16_t get_num_tags() {
        return m_num_tags;
    }

    /**
     * Get the Tag for some Dot1Q Vlan.
     * In case the Dot1Q Vlan isn't defined in the tags definition, it throws a *TrexException*.
     * You can verify it exists, using **dot1q_tag_exists**.
     *
     * @param vlan
     *     Dot1Q Vlan whose Tag we want to get
     *
     * @return
     *     Tag for that Dot1Q Vlan.
     **/
    uint16_t get_dot1q_tag(uint16_t vlan);

    /**
     * Get the Tag for some QinQ instance.
     * In case the QinQ isn't defined in the tags definition, it throws a *TrexException*.
     * You can verify it exists, using **qinq_tag_exists**.
     *
     * @param inner_vlan
     *      QinQ Inner Vlan
     *
     * @param outter_vlan
     *      QinQ Outter Vlan
     *
     * @return
     *     Tag for the QinQ.
     **/
    uint16_t get_qinq_tag(uint16_t inner_vlan, uint16_t outter_vlan);

    /**
     * Add a new tag definition for a Dot1Q Vlan.
     * In case this Dot1Q Vlan is already defined, it throws a *TrexException*.
     *
     * @param vlan
     *      Dot1Q Vlan that we are tagging
     *
     * @param tag
     *      Tag identifier
     *
     * @return bool
     *   True if adding the Dot1Q succeeded.
     **/
    bool add_dot1q_tag(uint16_t vlan, uint16_t tag);

    /**
     * Add a new tag definition for a QinQ.
     * In case this QinQ is already defined, it throws a *TrexException*.
     *
     * @param inner_vlan
     *      QinQ inner_vlan
     *
     * @param outter_vlan
     *      QinQ outter_vlan
     *
     * @param tag
     *      Tag identifier
     *
     * @return bool
     *   True if adding the QinQ succeeded.
     **/
    bool add_qinq_tag(uint16_t inner_vlan, uint16_t outter_vlan, uint16_t tag);

private:

    inline uint32_t get_qinq_key(uint16_t inner_vlan, uint16_t outter_vlan) {
        return inner_vlan << 16 | outter_vlan;
    }

    std::unordered_map<uint16_t, Dot1QTag*>   m_dot1q_map;    // Vlan to Dot1QTag
    std::unordered_map<uint32_t, QinQTag*>    m_qinq_map;     // Inner, Outter Vlan to QinQTag
    uint16_t                                  m_num_tags;     // Number of Tags
};

/**
 * TPGState represents the state machine for the Tagged Packet Group Feature.
 * 1. By default the feature is disabled and the State Machine is set to DISABLED.
 * 2. When a user enables TPG (via RPC), we move to ENABLED_CP. In this state we allocate
 *    the PacketGroupTagMgr in CP and send an async message to start allocating to Rx (which can take a lot of time).
 * 3. When the allocation finishes we move to ENABLED_CP_RX.
 * 4. After number 3 finishes, the RPC enables TPG in DP. Now we move to ENABLED.
 * 5. The user can disable TPG via RPC. We disable TPG in DP, send a async message to Rx to deallocate
      and move to DISABLED_DP.
 * 6. When the deallocation finishes we move to DISABLED_DP_RX. At this point the TPGCpCtx can be destroyed.

    The following diagram describes the state machine:

     -------------            -------------       -------------
    |             | USER RPC |             |     |             |
    |   DISABLED  | -------> |  ENABLED_CP | --> |   ENABLED   |
    |             |          |             |     |    CP_RX    |
     -------------            -------------       ------------- 
                                                         |
                                          AUTOMATIC RPC  |
                                                         ↓
     ------------          ------------              -----------
    |            |        |            |  USER RPC  |           |
    |  DISABLED  | <----- |  DISABLED  |  <-------  |  ENABLED  |
    |   DP_RX    |        |     DP     |            |           |
     ------------          ------------              -----------

**/
enum TPGState {
    DISABLED,       // Tagged Packet Group is disabled. Default state.
    ENABLED_CP,     // Tagged Packet Group is enabled at CP. Message sent to Rx but it hasn't finished allocating.
    ENABLED_CP_RX,  // Tagged Packet Group is enabled at CP and Rx.
    ENABLED,        // Tagged Packet Group is enabled at CP, Rx and DP.
    DISABLED_DP,    // Tagged Packet Group is disabled at DP. Message sent to Rx but it hasn't finished deallocating.
    DISABLED_DP_RX, // Tagged Packet Group is disabled at DP and Rx.
};

/**************************************
 * Tagged Packet Group Control Plane Context
 *************************************/
class TPGCpCtx {
public:

    TPGCpCtx(const std::vector<uint8_t>& acquired_ports,
             const std::vector<uint8_t>& rx_ports,
             const std::set<uint8_t>& cores,
             const uint32_t num_tpgids,
             const std::string& username);
    ~TPGCpCtx();
    TPGCpCtx(const TPGCpCtx&) = delete;
    TPGCpCtx& operator=(const TPGCpCtx&) = delete;

    /**
     * Get the vector of ports on which this TPG context resides (Tx + Rx)
     *
     * @return vector<uint8_t>
     *   Vector of ports on which this Tagged Packet Grouping context resides.
     **/
    const std::vector<uint8_t>& get_acquired_ports() { return m_acquired_ports; }

    /**
     * Get the vector of ports on which this TPG context collects (Rx)
     *
     * @return vector<uint8_t>
     *   Vector of ports on which this Tagged Packet Grouping context collects.
     **/
    const std::vector<uint8_t>& get_rx_ports() { return m_rx_ports; }

    /**
     * Get the set of cores on which this TPG context resides.
     *
     * @return set<uint8_t>
     *   Set of cores on which this Tagged Packet Grouping context resides.
     **/
    const std::set<uint8_t>& get_cores() { return m_cores; }

    /**
     * Get the number of Tagged Packet Group Identifiers.
     *
     * @return uint32_t
     *   Number of Tagged Packet Group Identifiers
     */
    const uint32_t get_num_tpgids() { return m_num_tpgids; }

    /**
     * Get the username that defines this TPG context.
     *
     * @return string
     *   Username that defines this context.
     */
    const std::string get_username() { return m_username; }

    /**
     * Get the Packet Group Tag Manager
     *
     * @return
     *   Packet Group Tag Manager
     */
    PacketGroupTagMgr* get_tag_mgr() { return m_tag_mgr; }

    /**
     * Get the state of Tagged Packet Group Context.
     *
     * @return TPGState
     *   State of Tagged Packet Group Context
     */
    const TPGState get_tpg_state() { return m_tpg_state; }

    /**
     * Set the state of Tagged Packet Group Context.
     *
     * @param tpg_state
     *   State of Tagged Packet Group Context
     */
    void set_tpg_state(TPGState tpg_state) { m_tpg_state = tpg_state; }

    /**
     * Check if port set to collect TPG stats.
     *
     * @param port_id
     *    Port identifier
     *
     * @return bool
     *   True iff port is defined as a port that collects TPG stats.
     **/
    bool is_port_collecting(uint8_t port_id) {
        return std::find(m_rx_ports.begin(), m_rx_ports.end(), port_id) != m_rx_ports.end();
    }

private:
    const std::vector<uint8_t>    m_acquired_ports;   // Ports on which this TPG context resides (Tx + Rx)
    const std::vector<uint8_t>    m_rx_ports;         // Ports on which this TPG context collects
    const std::set<uint8_t>       m_cores;            // Cores on which this TPG context resides
    const uint32_t                m_num_tpgids;       // Number of Tagged Packet Group Identifiers
    const std::string             m_username;         // Username that defines this TPG context.
    PacketGroupTagMgr*            m_tag_mgr;          // Tag Manager
    TPGState                      m_tpg_state;        // State Machine for Tagged Packet Grouping
};

/**************************************
 * Tagged Packet Group Stream Manager
 *************************************/
class TPGStreamMgr {
public:

    /** 
     * Tagged Packet Group Stream Manager.
     *
     * NOTE: The streams are identified by the stream_id.
     * stream_id is unique per port (supports multi profile)
     * However, we need to differentiate the stream ids per port.
     **/

    enum TPGStreamState {
        TPGStreamAdded,
        TPGStreamStarted,
        TPGStreamStopped,
    };

    /**
     * Get the TPGStreamMgr instance. This class is a manager, and as such
     * it should be a singleton. In case this is called for the first time,
     * it will create a new instance.
     *
     * @return TPGStreamMgr*
     *   A pointer to the Tagged Packet Group Stream Manager instance.
     **/
    static TPGStreamMgr* instance() {
         if (!m_instance) {
             m_instance = new TPGStreamMgr;
         }
        return m_instance;
    }

    /**
     * Delete the TPGStreamMgr instance.
    **/
    static void cleanup() {
        if (m_instance) {
            delete m_instance;
            m_instance = nullptr;
        }
    }

    ~TPGStreamMgr() { delete m_parser; };
    TPGStreamMgr(const TPGStreamMgr& other) = delete;           // Singeltons can't be copied.
    TPGStreamMgr& operator=(TPGStreamMgr& other) = delete;      // Singletons can't be assigned.

    /**
     * Add a stream. This is safe even if called on a non TPG stream.
     * Once a stream is added, the tpgid that the stream holds is considered active.
     *
     * @param stream
     *   Stream to add
     **/
    void add_stream(TrexStream* stream);

    /**
     * Delete a stream. This is safe even if called on a non TPG stream.
     * Once a stream is deleted, the tpgid that the stream is holding is considered inactive.
     * If the stream is not stopped, this will stop it.
     *
     * @param stream
     *   Stream to remove.
     **/
    void del_stream(TrexStream* stream);
    
    /**
     * Start a stream. This is safe even if called on a non TPG Stream.
     * If the stream is non added yet, this will add it.
     *
     * @param stream
     *   Stream to start.
     **/
    void start_stream(TrexStream* stream);

    /**
     * Stop a stream.  This is safe even if called on a non TPG Stream.
     *
     * @param stream
     *   Stream to stop.
     **/
    void stop_stream(TrexStream* stream);

    /**
     * Reset a stream.
     *
     * @param stream
     *   Stream to reset.
     **/
    void reset_stream(TrexStream* stream);

    /**
     * Copy stream state. This is provided for completness only.
     * TPG streams don't hold any state instilled in the stream, hence no need for this.
     *
     * @param from
     *   Stream to copy state from;
     * @param to
     *   Stream to copy state to.
     */
    void copy_state(TrexStream* from, TrexStream* to);

private:

    /**
     * Validates that the stream can be compiled and parsed.
     *
     * @param stream
     *   Stream to validate
     **/
    void compile_stream(const TrexStream* stream);

    /**
     * Indicate if the stream already exists in the manager?
     *
     * @param stream
     *   Stream to check if it exists.
     *
     * @return bool
     *   True iff stream already exists
     **/
    bool stream_exists(const TrexStream* stream);

    /**
     * Make a key that identifies each stream uniquely.
     * Since each stream is uniquely identified by stream_id in its own port,
     * it is enough to account for both.
     **/
    uint64_t stream_key(const TrexStream* stream) {
        uint64_t port_id = stream->m_port_id;
        return port_id << 32 | stream->m_stream_id;
    }

    /**
     * Make a key that identifies each tpgid uniquely per port.
     * We don't want to limit the user to unique tpgid in the system,
     * rather unique tpgid per port.
     **/
    uint64_t tpgid_key(uint8_t port_id, uint32_t tpgid) {
        uint64_t port_id_ext = port_id;
        return port_id_ext << 32 | tpgid;
    }

    TPGStreamMgr();  // Make the constructor private, so none except for us can create instances.

    static TPGStreamMgr*                            m_instance;      // Instance object
    CFlowStatParser*                                m_parser;        // Parser
    std::set<uint64_t>                              m_active_tpgids; // Active Tagged Packet Group Identifiers Per Port
    std::unordered_map<uint64_t, TPGStreamState>    m_stream_states; // State of the streams
    bool                                            m_software_mode; // Are we running in software mode
};


/**************************************
 * Tagged Packet Group Data Plane Manager
 *************************************/
class TPGDpMgrPerSide {
    /**
    Tagged Packet Group Data Plane Manager Per Core and Per Side.
    **/

public:

    TPGDpMgrPerSide(uint32_t num_tpgids);
    ~TPGDpMgrPerSide();
    TPGDpMgrPerSide(const TPGDpMgrPerSide&) = delete;
    TPGDpMgrPerSide& operator=(const TPGDpMgrPerSide&) = delete;

    /**
     * Get the next sequence number for a TPGID.
     *
     * @param tpgid
     *   Tagged Packet Group Identifier
     *
     * @return uint32_t
     *   Sequence for that Packet Group Identifier
     */
    uint32_t get_seq(uint32_t tpgid);

    /**
     * Increment the sequence for a TPGID.
     *
     * @param tpgid
     *   Tagged Packet Group Identifier.
     **/
    void inc_seq(uint32_t tpgid);

private:

    uint32_t*     m_seq_array;       // Sequence number for each tpgid per each port.
    uint32_t      m_num_tpgids;      // Number of Tagged Packet Group Identifiers.
};

#endif /* __TREX_TAGGED_PACKET_GROUPS_H__ */