/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#ifndef TREX_ROCEV2_NATIVE_CM_H
#define TREX_ROCEV2_NATIVE_CM_H

#include "trex_rocev2_native_cm_codec.h"
#include <vector>

namespace TrexRoceV2 { namespace Native {

enum class CmState { FREE, REQ_SENT, REQUEST_RECEIVED, REP_SENT, REP_RECEIVED, ESTABLISHED,
                     DREQ_SENT, TIME_WAIT, REJECTED, FAILED };
enum class CmEventType { CONNECT_REQUEST, CONNECT_RESPONSE, ESTABLISHED, DISCONNECTED, REJECTED,
                         TIMEOUT, CANCELLED };

struct NativeCmConfig {
    NativeCmConfig();
    size_t max_connections, max_listeners, max_events, max_tx_frames;
    uint64_t retry_timeout_ns, pending_accept_timeout_ns, timewait_ns, handshake_timeout_ns;
    uint8_t max_retries;
    uint32_t connection_id_base, connection_id_limit;
};
struct CmListenRequest {
    CmListenRequest();
    uint64_t listener_id, service_id;
    uint16_t port;
    NativeEndpoint endpoint; // local IP/MAC/VLAN identify listener
    size_t backlog;
    bool ip_cma;
};
struct CmConnectRequest {
    CmConnectRequest();
    uint64_t operation_id, service_id;
    uint16_t port, source_port;
    NativeEndpoint endpoint; // egress next-hop MAC independent of destination IP
    CmParameters parameters;
    bool ip_cma;
    std::array<uint8_t,224> private_data;
    size_t private_data_length;
};
struct CmConnectionInfo {
    CmConnectionInfo();
    uint32_t connection_id, remote_id;
    uint64_t operation_id, listener_id, service_id;
    uint16_t port;
    bool active;
    CmState state;
    NativeEndpoint endpoint;
    CmParameters local, remote;
    uint32_t send_psn,receive_psn;
    uint8_t retry_count,rnr_retry_count;
    uint8_t negotiated_mtu, requester_depth, responder_depth;
};
struct CmEvent {
    CmEvent();
    CmEventType type;
    CmConnectionInfo connection;
    uint16_t reason;
    std::array<uint8_t,224> private_data;
    size_t private_data_length;
};
struct CmTxFrame {
    CmTxFrame();
    uint64_t token;
    uint32_t connection_id;
    uint16_t port;
    std::array<uint8_t,512> data;
    size_t length;
};
struct CmStatistics {
    CmStatistics();
    uint64_t received, transmitted, retries, malformed, unknown, duplicates;
    uint64_t established, disconnected, rejected, timeouts, resource_exhaustion;
};

/* Single dataplane owner. No live QP mutation: CONNECT_REQUEST asks the owner
 * to provision QP/RQ before accept; ESTABLISHED supplies the peer parameters.
 * poll/receive never block. TX retry timers start only after PMD acceptance.
 * Bounded event storage reserves three lifecycle events for each live slot. */
class NativeCmManager {
public:
    NativeCmManager();
    bool configure(const NativeCmConfig &config, Status *status = NULL);
    bool listen(const CmListenRequest &request, Status *status = NULL);
    bool unlisten(uint64_t listener_id, Status *status = NULL);
    bool connect(const CmConnectRequest &request, uint64_t now_ns,
                 uint32_t &connection_id, Status *status = NULL);
    bool accept(uint32_t id, const CmParameters &local, ByteSpan private_data,
                uint64_t now_ns, Status *status = NULL);
    /* Active side provisions peer-derived QP/RQ parameters after
     * CONNECT_RESPONSE, then confirms to release RTU onto the wire. */
    bool confirm(uint32_t id,uint64_t now_ns,Status *status = NULL);
    bool reject(uint32_t id, uint16_t reason, ByteSpan private_data,
                uint64_t now_ns, Status *status = NULL);
    bool disconnect(uint32_t id, uint64_t now_ns, Status *status = NULL);
    bool cancel(uint32_t id, uint64_t now_ns, Status *status = NULL);
    bool receive(uint16_t port, ByteSpan frame, uint64_t now_ns, Status *status = NULL);
    void poll(uint64_t now_ns);
    bool peek_tx(CmTxFrame &frame) const;
    bool complete_tx(uint64_t token, bool accepted, uint64_t now_ns, Status *status = NULL);
    bool pop_event(CmEvent &event);
    bool connection(uint32_t id, CmConnectionInfo &info) const;
    const CmStatistics &statistics() const;
    size_t live_connections() const;
private:
    struct Slot {
        Slot();
        CmConnectionInfo info;
        uint64_t transaction_id, deadline, timewait_deadline, overall_deadline, close_transaction_id;
        uint32_t retries;
        uint8_t event_budget;
        bool awaiting_tx, retry_message, emitted_terminal, established_event;
        CmMessage saved;
        CmMessage last_reply;
        CmMessage request;
    };
    Slot *find(uint32_t id);
    const Slot *find(uint32_t id) const;
    Slot *allocate(uint64_t now_ns);
    bool queue(Slot &slot, const CmMessage &message, bool arm_retry, Status *status);
    bool reject_request(uint16_t port,const NativeEndpoint &path,const CmMessage &request,
                        uint16_t reason,Status *status);
    bool event(Slot &slot, CmEventType type, const CmMessage *message = NULL, uint16_t reason = 0);
    void terminal(Slot &slot, CmEventType type, uint64_t now_ns, uint16_t reason = 0,
                  const CmMessage *message = NULL);
    void negotiate(Slot &slot);
    CmMessage response(const Slot &slot, CmMessageType type) const;
    uint64_t response_deadline(const Slot &slot,uint64_t now_ns,uint8_t exponent) const;
    bool matching_peer(const Slot &slot,uint16_t port,const NativeEndpoint &incoming) const;
    NativeCmConfig m_config;
    bool m_configured;
    uint32_t m_next_id, m_ud_psn;
    uint64_t m_next_transaction, m_next_token;
    std::vector<Slot> m_slots;
    std::vector<CmListenRequest> m_listeners;
    std::vector<CmEvent> m_events;
    std::vector<CmTxFrame> m_tx;
    CmStatistics m_statistics;
};
const char *cm_state_to_string(CmState state);
const char *cm_event_to_string(CmEventType type);

} }
#endif
