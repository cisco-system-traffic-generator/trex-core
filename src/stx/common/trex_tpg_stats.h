/*
 Bes Dollma
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2021-2022 Cisco Systems, Inc.

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

#ifndef __TREX_TAGGED_PACKET_GROUPS_STATS_H__
#define __TREX_TAGGED_PACKET_GROUPS_STATS_H__

#include <iostream>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "stl/trex_stl_tpg.h"
#include "mbuf.h"

/*
    Tagged Flow stats are flow stats that take in consideration tagged packets
    such as dot1Q, QinQ, VxLAN.
    The user specifies the tags upon initialization and we calculate the statistics
    based on the tags.
    This file defines:
    1. Statistics we gather in the Rx Core for each tag.
    2. A class that provides an Api to handle TPG packets per port.
    3. A TPG Rx Context.

*/


/**************************************
 * CTPGTagCntr
 *************************************/
class CTPGTagCntr {
    /**
        Counters per Tag
    **/

public:

    /** Testing Classes. Friendship is not inherited. **/
    friend class TaggedPktGroupTest;
    friend class TPGTagCntrTest;
    friend class TPGRxStatsTest;

    /*
     * Use the ctor with care, if allocating a lot of counters it can become a bottleneck.
     */
    CTPGTagCntr();

    /**
     * Override operator ==, != in order to compare in between two tag counters so
     * we can be able to compress equal counters.
     **/
    friend bool operator==(const CTPGTagCntr& lhs, const CTPGTagCntr& rhs);
    friend bool operator!=(const CTPGTagCntr& lhs, const CTPGTagCntr& rhs);
    friend std::ostream& operator<<(std::ostream& os, const CTPGTagCntr& tag);

    /**
     * Updates counters when a packet is received with a new sequence number.
     *
     *
     * @param rcv_seq
     *    Sequence number extracted from the payload of packet.
     *
     * @param pkt_len
     *    Length of packet in bytes
     **/
    void update_cntrs(uint32_t rcv_seq, uint32_t pkt_len);

    /**
     * Dump stats as Json.
     *
     * @param stats
     *   Json to dump the stats at.
     **/
    void dump_json(Json::Value& stats);

private:

    /**
     * Set counters, used for testing.
     *
     * @param pkts
     *   Number of pkts received for this tag.
     *
     * @param bytes
     *   Number of bytes received for this tag.
     *
     * @param seq_err
     *   Number of errors (diff) in sequence.
     *
     * @param seq_err_too_big
     *   Number of times the packet was received with a bigger seq than expected.
     *
     * @param seq_err_too_small
     *   Number of times the packet was received with a small seq than expected.
     *
     * @param dup
     *   Number of duplicates packets received.
     *
     * @param ooo
     *   Number of out of order packets received.
     **/
    void set_cntrs(uint64_t pkts,
                   uint64_t bytes,
                   uint64_t seq_err,
                   uint64_t seq_err_too_big,
                   uint64_t seq_err_too_small,
                   uint64_t dup,
                   uint64_t ooo);

    /**
     * Update counters when we get a big sequence number.
     *
     * @param rcv_seq
     *    The received sequence number on the wire.
     **/
    void _update_cntr_seq_too_big(uint32_t rcv_seq);

    /**
     * Update counters when we get a small sequence number.
     *
     * @param rcv_seq
     *    The received sequence number on the wire.
     **/
    void _update_cntr_seq_too_small(uint32_t rcv_seq);


    uint64_t m_pkts;                    // Number of packets received
    uint64_t m_bytes;                   // Number of bytes received
    uint64_t m_seq_err;                 // How many packet seq num gaps we saw (packets lost or out of order)
    uint64_t m_seq_err_too_big;         // How many packet seq num greater than expected we had
    uint64_t m_seq_err_too_small;       // How many packet seq num smaller than expected we had
    uint64_t m_ooo;                     // Number of packets received Out of Order
    uint64_t m_dup;                     // Packets we got with same seq num
    uint32_t m_exp_seq;                 // Expected next sequence number
};

/**************************************
 * RxTPGPerPort
 *************************************/
class RxTPGPerPort {
    /*
        Tagged Packet Group Rx Core Handler per Port.
        Handles received packets and manages counters.
    */
public:
    RxTPGPerPort(uint8_t port_id, uint32_t num_pgids, PacketGroupTagMgr* tag_mgr, CTPGTagCntr* cntrs);
    ~RxTPGPerPort() {};

    /**
     * Handle a packet received on Rx. If the packet contains a TPG header, parse it,
     * find the appropriate tag, and count the stats.
     *
     * @param m
     *   Mbuf of the packet received
     **/
    void handle_pkt(const rte_mbuf_t* m);


    /**
     * Provided a tpgid and a tag, gets the counter to that tag. In case of invalid params, it will return nullptr.
     *
     * @param tpgid
     *   Tagged Packet Group Identifier
     *
     * @param tag
     *   Tag
     *
     * @return CTPGTagCntr*
     *   Pointer to the tag counters.
     **/
    CTPGTagCntr* get_tag_cntr(uint32_t tpgid, uint16_t tag);

    /**
     * Provided a tpgid get the unknown tag counters for it. In case of invalid params, it will return nullptr.
     *
     * @param tpgid
     *   Tagged Packet Group Identifier
     *
     *
     * @return CTPGTagCntr*
     *   Pointer to the unknown tag counters
     **/
    CTPGTagCntr* get_unknown_tag_cntr(uint32_t tpgid);

    /**
     * Updates counters when a packet is received with a new sequence number.
     *
     * @param tpgid
     *    Packet Group Identifier extracted from the payload of the packet.
     *
     * @param rcv_seq
     *    Sequence number extracted from the payload of packet.
     *
     * @param pkt_len
     *    Length of packet in bytes
     *
     * @param tag_id
     *    Tag extracted from the Tag Manager and the Dot1Q or QinQ tag in the packet.
     *
     * @param tag_exists
     *    Boolean indicating if this tag exists in the Tag Manager.
     **/
    void update_cntrs(uint32_t tpgid, uint32_t rcv_seq, uint32_t pkt_len, uint16_t tag_id, bool tag_exists);

    /** 
     * Get Tagged Packet Group Statistics of a specific tpgid from [min_tag, max_tag) dumped as a Json.
     *
     * @param stats
     *   Json on which we dump the stats
     *
     * @param tpgid
     *   Tagged Packet Group Identifier whose stats we want to collect.
     *
     * @param min_tag
     *   Min Tag to dump in the stats. Inclusive.
     *
     * @param max_tag
     *   Max Tag to dump in the stats. Not inclusive.
     *
     * @param unknown_tag
     *   Add the stats for unknown tags in the Json too.
     **/
    void get_tpg_stats(Json::Value& stats, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag);

private:

    uint8_t             m_port_id;          // Port Id
    uint32_t            m_num_tpgids;       // Number of Tagged Packet Groups
    uint16_t            m_num_tags;         // Number of Tags
    PacketGroupTagMgr*  m_tag_mgr;          // Pointer to cloned Rx Tagged Manager.
    CTPGTagCntr*        m_cntrs;            // Counters per Group

};

/**************************************
 * TPGRxCtx
 *************************************/

/**
 * TPGRxState represents the state machine for the Tagged Packet Group Feature in Rx.
 * NOTE: Since the state is accessed both from Rx and the thread that allocates, the state needs to be guarded.
 * 1. By default the feature is disabled and the State Machine is set to DISABLED.
 * 2. When we call @enable_tpg_ctx in Rx, we start a thread that allocates memory. This thread will set the state to AWAITING_ENABLE
 *    after it has finished allocating. Rx will check the state periodically until it moves to AWAITING_ENABLE.
 * 3. The thread has finished allocating, now the Rx run the post allocation code and set the status to enabled.
 * 4. When @disable_tpg_ctx is called in Rx, we start a thread that starts deallocating. This thread will set the state to AWAITING_DISABLE
 *    after it has finished deallocating. Rx will check the state periodically until it moves to AWAITING_DISABLE.
 * 5. The thread has finished deallocating, Rx will destroy the object.

    The following diagram describes the state machine:

     -------------                           -------------
    |             | allocation finishes     |             |
    |   DISABLED  | ----------------------> |  AWAITING   |
    |             |                         |   ENABLE    |
     -------------                           ------------- 
                                                    |
                               Periodic Rx Checks   |
                                                    ↓
     ------------                             -----------
    |            |  deallocation finishes    |           |
    |  AWAITING  | <------------------------ |  ENABLED  |
    |   DISABLE  |                           |           |
     ------------                             -----------

**/

enum class TPGRxState {
    DISABLED,           // TPGRxCtx created but memory is not allocated yet.
    AWAITING_ENABLE,    // TPGRxCtx created and memory has just been allocated.
    ENABLED,            // TPGRxCtx enabled and working.
    AWAITING_DISABLE,   // TPGRxCtx still exists but memory has been deallocated.
};

class TPGRxCtx {
public:
    TPGRxCtx(const std::vector<uint8_t>& rx_ports,
             const uint32_t num_tpgids,
             const std::string& username,
             PacketGroupTagMgr* tag_mgr);
    ~TPGRxCtx();
    TPGRxCtx(const TPGRxCtx&) = delete;
    TPGRxCtx& operator=(const TPGRxCtx&) = delete;

    /**
     * Get the username that defines this TPG context.
     *
     * @return string
     *   Username that defines this context.
     */
    const std::string get_username() { return m_username; }

    /**
     * Get the vector of ports on which this TPG context collects (Rx)
     *
     * @return vector<uint8_t>
     *   Vector of ports on which this Tagged Packet Grouping context collects.
     **/
    const std::vector<uint8_t>& get_rx_ports() { return m_rx_ports; }

    /**
     * Get the number of Tagged Packet Group Identifiers.
     *
     * @return uint32_t
     *   Number of Tagged Packet Group Identifiers
     */
    const uint32_t get_num_tpgids() { return m_num_tpgids; }

    /**
     * Get the Packet Group Tag Manager
     *
     * @return
     *   Packet Group Tag Manager
     */
    PacketGroupTagMgr* get_tag_mgr() { return m_tag_mgr; }

    /**
     * Allocates the counters in a chunk using *calloc*.
     * This way we zero the counters when writing them and distribute the workload.
     * Calls internal _allocate in a separate thread.
     **/
    void allocate();

    /**
     * Deallocates the counters.
     * Calls internal _deallocate in a separate thread.
     **/
    void deallocate();

    /**
     * Destroy the thread that allocated memory.
     * Called by Rx when it knows that the thread has finished.
     **/
    void destroy_thread();

    /**
     * Get a pointer to the counters of this port. If port is invalid return nullptr
     *
     * @param port_id
     *   Port Identifier whose counters we are interested on.
     *
     * @return CTPGTagCntr*
     *   Pointer to the first tag counters for this port.
     **/
    CTPGTagCntr* get_port_cntr(uint8_t port_id);

    /**
     * Set new state for TPG Rx Context multi-thread safe.
     * NOTE: Get and Set are not atomic.
     *
     * @param state
     *   New State to set to.
     **/
    void set_state(TPGRxState state);

    /**
     * Get the state of the TPG Rx Context multi-thread safe.
     * NOTE: Get and Set are not atomic.
     *
     * @return TPGRxState
     *   State of the TPG Rx Context
     */
    TPGRxState get_state();

private:

    /**
     * Does the actual allocation and sets the state to AWAITING_ENABLE.
     * Should be run in another thread.
     **/
    void _allocate();

    /**
     * Frees the counters and sets the state to AWAITING_DISABLE.
     * Should be run in another thread.
     **/
    void _deallocate();

    /**
     * Spawns a new thread to run the function provided as a parameter.
     *
     * @param name
     *   Name of the new thread
     *
     * @param func
     *   Func to run, either _allocate or _deallocate.
     **/
    void spawn_thread(const std::string& name, std::function<void(TPGRxCtx*)> func);

    const std::vector<uint8_t>    m_rx_ports;         // Ports on which this TPG context collects
    const uint32_t                m_num_tpgids;       // Number of Tagged Packet Group Identifiers
    const std::string             m_username;         // Username that defines this TPG context.
    PacketGroupTagMgr*            m_tag_mgr;          // Tag Manager
    CTPGTagCntr*                  m_cntrs;            // Counters
    volatile TPGRxState           m_state;            // State
    std::mutex                    m_state_mutex;      // Mutex to protect the change of the state
    std::thread*                  m_thread;           // Thread that allocates and deallocates.
};


#endif /* __TREX_TAGGED_PACKET_GROUPS_STATS_H__ */