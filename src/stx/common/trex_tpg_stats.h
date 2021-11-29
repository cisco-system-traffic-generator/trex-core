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

#ifndef __TREX_TAGGED_PACKET_GROUPS_STATS_H__
#define __TREX_TAGGED_PACKET_GROUPS_STATS_H__

#include <iostream>
#include <vector>
#include <unordered_map>
#include "stl/trex_stl_tpg.h"
#include "mbuf.h"

/*
    Tagged Flow stats are flow stats that take in consideration tagged packets
    such as dot1Q, QinQ, VxLAN.
    The user specifies the tags upon initialization and we calculate the statistics
    based on the tags.
    This file defines:
    1. Statistics we gather in the Rx Core for each tag, packet group and port.
    2. Handler for a TPG packet.

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

    CTPGTagCntr() { init(); }

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
     * Initialize a CTPGTagCntr.
     **/
    void init();

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
 * CTPGGroupCntr
 *************************************/
class CTPGGroupCntr {

public:
    /**
        Counters per Packet Group (multiple tags).
        We define copy constructor and operator= since this class dynamically allocates memory.
        Yet, this can be too much data and we don't want to offer the possibility to copy it.
    **/
    CTPGGroupCntr(uint32_t tpgid, uint16_t num_tags);         // Ctor
    CTPGGroupCntr(const CTPGGroupCntr&) = delete;            // Copy Ctor
    ~CTPGGroupCntr();                                        // Desctructor
    CTPGGroupCntr& operator=(const CTPGGroupCntr&) = delete; // Operator=

    /**
     * Get object that collects stats for a specific tag for this Group.
     *
     * @param tag_id
     *    Tag whose stats we are interested on
     *
     * @return CTPGTagCntr
     *   A pointer to the object that holds the stats
     **/
    CTPGTagCntr* get_tag_cntr(uint16_t tag_id);

    /**
     * Get object that collects stats for all unknown tags.
     *
     * @return CTPGTagCntr
     *   A pointer to the object that holds the stats
     **/
    CTPGTagCntr* get_unknown_tag_cntr();

    /**
     * Get Tagged Packet Group Statistics from [min_tag, max_tag) dumped as a Json.
     *
     * @param stats
     *   Json on which we dump the stats
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
    void get_tpg_stats(Json::Value& stats, uint16_t min_tag, uint16_t max_tag, bool unknown_tag);

private:

    uint32_t        m_tpgid;                // Tagged Packet Group Identifier
    uint16_t        m_num_tags;             // Number of tags
    CTPGTagCntr*    m_unknown_tag_cntrs;    // Counters for unknown tags
    CTPGTagCntr*    m_tag_counters;         // Counter per tag
};

/**************************************
 * CTPGPortCntr
 *************************************/
class CTPGPortCntr{
public:
    /**
        Counters per Port (multiple packet groups).
        We define copy constructor and operator= since this class dynamically allocates memory.
        Yet, this can be too much data and we don't want to offer the possibility to copy it,
        hence we delete them.
    **/
    CTPGPortCntr(uint8_t port_id, uint32_t num_pgids, uint16_t num_tags);   // Ctor
    CTPGPortCntr(const CTPGPortCntr&) = delete;                             // Copy Ctor
    ~CTPGPortCntr();                                                        // Destructor
    CTPGPortCntr& operator=(const CTPGPortCntr&) = delete;                  // Operator=
    CTPGGroupCntr* get_pkt_group_cntr(uint32_t tpgid);

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
     * @param tag
     *    Tag extracted from the Tag Manager and the Dot1Q or QinQ tag in the packet.
     **/
    void update_cntrs(uint32_t tpgid, uint32_t rcv_seq, uint32_t pkt_len, uint16_t tag, bool tag_exists);

private:

    uint8_t         m_port_id;          // Port Id
    uint32_t        m_num_tpgids;       // Number of Tagged Packet Groups
    uint16_t        m_num_tags;         // Number of Tags
    CTPGGroupCntr** m_group_counters;   // Counters per Group
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
    RxTPGPerPort(uint8_t port_id, uint32_t num_pgids, PacketGroupTagMgr* tag_mgr);
    RxTPGPerPort(const RxTPGPerPort&) = delete;
    ~RxTPGPerPort();
    RxTPGPerPort& operator=(const RxTPGPerPort&) = delete;

    /**
     * Handle a packet received on Rx. If the packet contains a TPG header, parse it,
     * find the appropriate tag, and count the stats.
     *
     * @param m
     *   Mbuf of the packet received
     **/
    void handle_pkt(const rte_mbuf_t* m);

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

    PacketGroupTagMgr*  m_tag_mgr;      // Packet Group Tag Manager
    CTPGPortCntr*       m_port_cntr;    // Counters per port
};


#endif /* __TREX_TAGGED_PACKET_GROUPS_STATS_H__ */