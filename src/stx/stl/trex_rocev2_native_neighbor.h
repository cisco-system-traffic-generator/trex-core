/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#ifndef TREX_ROCEV2_NATIVE_NEIGHBOR_H
#define TREX_ROCEV2_NATIVE_NEIGHBOR_H
#include "trex_rocev2_native_cm.h"

namespace TrexRoceV2 { namespace Native {
struct NativeNeighborConfig {
    NativeNeighborConfig();
    size_t max_interfaces,max_entries,max_events,max_tx_frames;
    uint64_t retry_timeout_ns,cache_lifetime_ns;
    uint8_t max_retries;
};
struct NativeNeighborInterface {
    NativeNeighborInterface();
    uint64_t interface_id;
    uint16_t port;
    NativeEndpoint endpoint;       // local address/MAC/VLAN; dst ignored
    uint8_t prefix_length;
    bool has_gateway;
    std::array<uint8_t,16> gateway;
};
enum class NeighborEventType { RESOLVED, TIMEOUT, CANCELLED };
struct NativeNeighborEvent {
    NativeNeighborEvent();
    uint64_t operation_id;
    NeighborEventType type;
    uint16_t port;
    NativeEndpoint endpoint;       // original peer IP and resolved next-hop MAC
};
/* Explicit address ownership. Cache keys include physical port, VLAN, family,
 * local source and next-hop address. Static MACs bypass active resolution.
 * Register/unregister lifetime belongs to the reserved endpoint session. */
class NativeNeighborManager {
public:
    NativeNeighborManager();
    bool configure(const NativeNeighborConfig &config,Status *status=NULL);
    bool register_interface(const NativeNeighborInterface &interface,Status *status=NULL);
    bool unregister_interface(uint64_t interface_id,Status *status=NULL);
    bool resolve(uint64_t operation_id,uint16_t port,const NativeEndpoint &path,
                 uint64_t now_ns,Status *status=NULL);
    bool cancel(uint64_t operation_id,uint64_t now_ns,Status *status=NULL);
    bool receive(uint16_t port,ByteSpan frame,uint64_t now_ns,Status *status=NULL);
    void poll(uint64_t now_ns);
    bool pop_event(NativeNeighborEvent &event);
    bool peek_tx(CmTxFrame &frame) const;
    bool complete_tx(uint64_t token,bool accepted,uint64_t now_ns,Status *status=NULL);
private:
    struct Entry {
        Entry();
        uint64_t operation_id,interface_id,deadline,expires;
        NativeEndpoint path;
        std::array<uint8_t,16> next_hop;
        uint16_t port;
        uint8_t retries;
        bool used,pending,awaiting_tx;
    };
    const NativeNeighborInterface *interface(uint16_t port,const NativeEndpoint &path) const;
    bool queue_probe(Entry &entry,Status *status);
    bool queue_reply(uint16_t port,const uint8_t *data,size_t length,Status *status);
    void finish(Entry &entry,NeighborEventType type,uint64_t now_ns,const uint8_t *mac=NULL);
    NativeNeighborConfig m_config;
    bool m_configured;
    uint64_t m_next_token;
    std::vector<NativeNeighborInterface> m_interfaces;
    std::vector<Entry> m_entries;
    std::vector<NativeNeighborEvent> m_events;
    std::vector<CmTxFrame> m_tx;
};
} }
#endif
