# Native RoCEv2 connection management

The packet-native connection manager implements the primary-path Reliable
Connected (RC) CM exchange for emulated QPs. It sends management datagrams
through the same packet carrier as the dataplane. It does not delegate QP
management to a host RDMA device or use an out-of-band TCP handshake.

This document describes implemented behavior and its present limits. It is
not an IBTA conformance statement. The normative specification, clause-level
requirements, independent RXE interoperability, physical NIC qualification
and complete transport/extension inventory remain release gates in
[the completion plan](rocev2_stateful_completion_plan.md).

## Ownership and native interface

`NativeCmManager` has one dataplane owner. The manager contains bounded
connection slots, listeners, events and output frames. The control plane
configures it and issues listen, connect, accept, reject, disconnect and cancel
commands; `receive()` and `poll()` perform packet and timer progress.

It emits these asynchronous events:

| Event | Required owner action |
| --- | --- |
| `CONNECT_REQUEST` | Reserve local QP, memory and receive resources; accept with local QPN/PSN and resource limits or reject. |
| `CONNECT_RESPONSE` | Active side installs REP-derived peer parameters and prepares receive resources, then calls `confirm()` to release RTU. |
| `ESTABLISHED` | Install the negotiated peer path/QPN/PSN and limits in the QP owner; enable the application workload. |
| `DISCONNECTED` | Drain/flush the data QP through its owner and release application resources. |
| `REJECTED`, `TIMEOUT`, `CANCELLED` | Complete the asynchronous operation with a failure/cancellation outcome and reclaim owned resources. |

The manager never changes a live data QP owned by another worker. The
`CmConnectionInfo` carried with events includes the operation/listener IDs,
local and remote CM IDs, physical port, IP family, source and destination IP,
local/next-hop MAC, VLAN, local/remote QPN/PSN, MTU and Read/Atomic limits.
An accepted passive request must have receive resources prepared before REP.
Application memory addresses/rkeys are not part of base CM negotiation.

The CM wire Starting PSN selects the receiver's SQ start and the sender's
expected RQ PSN. Events therefore provide normalized `send_psn` and
`receive_psn`: local SQ starts at the peer's wire PSN, and local RQ expects
the locally proposed wire PSN. For REQ PSN 101 and REP PSN 202, the active
endpoint uses SQ 202 / RQ 101 and the passive endpoint uses SQ 101 / RQ 202.
At the first passive `CONNECT_REQUEST`, only `send_psn` is known; the owner
chooses `receive_psn` through the PSN it supplies in its REP parameters.
`retry_count` is the REQ value; `rnr_retry_count` is the value received in
REQ (passive) or REP (active). These normalized values must be installed in
the QP before allowing application work.

The TX API is `peek_tx()` followed by `complete_tx(token, accepted, now_ns)`.
A rejected PMD submission leaves the frame queued. Retransmission timers arm
only after acceptance. Connection IDs can be assigned a disjoint inclusive
namespace using `connection_id_base` and `connection_id_limit`; they are not
reused within one configured owner lifetime. This allows QP1 replies to be
steered to the owner by the high ID bits. REQs still require listener routing
by physical port, service and owned address.

Three event records are reserved per live connection before accepting work.
Undrained events reduce admission capacity. Each connection has one bounded
pending output; duplicate requests cannot grow an unbounded retransmit list.
Handshake and pending-accept timers cap MRA extensions and unsent requests.
REQ Remote and Local CM Response Timeout fields are modeled independently:
the active REQ timer uses its advertised Remote timeout; the passive REP
timer uses the received Local timeout. The configured `retry_timeout_ns`
adds the carrier round-trip allowance to that encoded peer processing time.
The overall handshake deadline caps all such timers, including hostile
maximum exponents. Passive REP retransmission counts also respect the REQ's
Max CM Retries field. The overall deadline is a local cancellation policy,
not evidence that a slower peer violates the protocol.
Time-wait retains terminal reply state, expires pending output and prevents
live ID aliasing. A new configuration/session must preserve the outer
session's stale-packet and QPN reuse rules; local generations alone do not
appear in wire CM or RC packets.

## Management wire envelope

QP1 traffic uses UDP destination 4791, UD SEND ONLY (`0x64`), destination and
source QPN 1, DETH Q_Key `0x80010000`, and the configured full-member P_Key
`0xffff`. Native packet decoding verifies the ordinary IPv4/IPv6 checksums,
UDP checksum rules and RoCE ICRC before CM processing.

The payload is exactly one 256-byte CM MAD. Its 24-byte common header uses
base version 1, management class 7, class version 2 and SEND method 3. The
transaction ID is 64 bits. Status, class-specific/reserved and attribute
modifier fields are zero for the advertised profile. Multibyte fields are
network byte order; message field bits follow the IB CM field layout.

| Attribute | ID | Body private-data offset | Maximum private bytes |
| --- | --- | --- | --- |
| REQ | `0x0010` | 140 | 92 |
| MRA | `0x0011` | 10 | 222 |
| REJ | `0x0012` | 84 | 148 |
| REP | `0x0013` | 36 | 196 |
| RTU | `0x0014` | 8 | 224 |
| DREQ | `0x0015` | 12 | 220 |
| DREP | `0x0016` | 8 | 224 |

Offsets in this table start after the MAD header. REQ carries the service ID,
communication ID, QPN, starting PSN, primary GIDs, P_Key, MTU, resource limits,
retry/RNR values and path timing/class parameters. REP supplies the peer
communication ID, QPN, starting PSN, Read/Atomic limits and responder timing.
RTU gates establishment; DREQ carries the target data QPN and DREP confirms
disconnect. MRA applies to REQ or REP and extends the transaction deadline
within the configured total handshake bound. REJ includes its reason,
additional rejected information and bounded private data.

Private-data fields have fixed wire lengths and no general-purpose encoded
application length. Decoding reports their full message-specific field size;
applications must define their own length/padding contract. Oversize data is
rejected before output is queued.

## IP CMA addressing and neighbor ownership

`ip_service_id(port, port_space)` forms the standard service identifier as
`(port_space << 16) | port`; the default TCP port space is `0x0106`.
IPv4-mapped GIDs and native IPv6 GIDs are encoded separately from application
private data. When IP CMA is enabled, REQ private data begins with the 36-byte
version-0 CMA header: IP version in the high nibble, source port and two
16-byte source/destination address slots. IPv4 addresses occupy the last
four bytes of each slot with twelve zero padding bytes. This leaves at most
56 application bytes in a REQ. Received GIDs, CMA family/addresses and the
IP packet path must agree.

`NativeNeighborManager` explicitly registers local address ownership by
physical port, VLAN and source IP/MAC. Each interface has a prefix and an
optional gateway. Off-link peers resolve the gateway rather than the peer;
the peer IP is retained in the resulting `NativeEndpoint`. IPv6 link-local
gateways are permitted. Nonzero configured destination MACs select static
resolution, preserving deterministic lab setups.

Dynamic resolution sends ARP requests or IPv6 solicited-node NS packets,
retries after actual TX acceptance, and reports resolved/timeout/cancelled
events. The cache key includes local interface ownership and next-hop IP;
interface ownership includes physical port, VLAN, IP family and source
identity. Only pending requests are learned from replies. ARP replies must
target the registered local MAC/IP; NA must be solicited unicast, contain a
valid target link-layer option and match the pending target. IPv6 packets
must have hop limit 255 and a valid ICMPv6 checksum; malformed option lengths
and inconsistent Ethernet/options are rejected.
Cancelling a pending operation removes only its queued probe/retry state and
emits its reserved cancellation event; other operations sharing the local
interface continue. An already completed/nonexistent operation returns
`NOT_FOUND` rather than producing a second terminal event.

Registered addresses answer ARP and NS. DAD NS requests receive the
appropriate unsolicited all-nodes NA; registration itself does not perform
DAD or advertise a full host network stack. Unregistering cancels outstanding
resolution operations, evicts their entries and discards their unsent output.

## Capability and rejection boundaries

| Capability | Current behavior |
| --- | --- |
| Active/passive RC primary-path REQ/REP/RTU | Implemented; peer identity/transaction IDs/resource bounds checked. |
| MRA for REQ/REP | Implemented; cannot extend the overall handshake indefinitely. |
| Consumer reject, unknown service, backlog/resource exhaustion | REJ with reason 28, 8 or 3, respectively; an application can supply its reject reason. Pre-allocation REJ uses local communication ID zero. |
| DREQ/DREP, simultaneous disconnect | Implemented with terminal events, replay and bounded time-wait. |
| Lost REQ/REP/RTU/DREQ/DREP and duplicates | Bounded request retransmission, saved response replay and unique lifecycle events. |
| RC Read/Atomic requester/responder depths | Negotiated and exposed; caller must apply them to the owning QP. |
| Asymmetric CM response timeouts, PSNs and RNR/retry counts | Distinct wire fields decoded; normalized owner-facing PSNs/retry values and per-direction timer use are implemented. |
| Static MAC, IPv4 ARP, IPv6 NS/NA, one VLAN | Implemented with explicit interface/route lifetime and bounds. |
| UC/XRC/extended CM transport | REQ decoder returns unsupported. No transport state is created. |
| Alternate path, SAP/LAP/APR and path migration | Alternate path/capability fields rejected; LAP/APR attributes unsupported. |
| SIDR | Unsupported attribute; no UD data service is advertised by this RC manager. |
| ECE/vendor option negotiation | Nonzero attribute modifiers rejected; informational vendor ID fields are tolerated. |
| SRQ, RDC/EEC contexts, end-to-end flow control | Unsupported capability fields rejected. |
| Nonzero primary flow label or service level | Rejected until the data carrier can preserve/apply these negotiated fields. |
| Nonzero RC Q_Key or invalid partition/GSI envelope | Rejected before state mutation. |
| Unsupported MAD class/version/method/status | Rejected before state mutation. No assertion that every unsupported packet receives an on-wire REJ. |
| QinQ, IPv6 extension-header ND, IP fragmentation | Outside this carrier profile; not silently treated as supported. |
| Gateway discovery, router advertisements, DNS, full DAD implementation | Not implemented; configure addresses/prefix/gateway explicitly. |
| Cross-process RXE/RNIC interoperability | Not yet qualified. Unit wire vectors alone do not establish this. |

REQ MTU describes the path selected by the requester. The passive owner must
be able to support it; REP has no separate MTU field. Read/Atomic depths in
REP must fit the request. Software tests do not establish correct limits or
throughput on a physical RNIC/PMD.

## Provenance and verification

All added implementation is original Apache-2.0 code. The following pinned
Linux v6.12 interfaces were consulted for numeric wire offsets, field sizes,
state/message names, service-ID construction and CMA address layout. Their
implementation bodies were not copied:

- [CM wire field declarations](https://github.com/torvalds/linux/blob/v6.12/include/rdma/ibta_vol1_c12.h)
- [CM message IDs, sizes and event interface](https://github.com/torvalds/linux/blob/v6.12/include/rdma/ib_cm.h)
- [MAD common header](https://github.com/torvalds/linux/blob/v6.12/include/rdma/ib_mad.h)
- [IP CMA header and service-ID construction](https://github.com/torvalds/linux/blob/v6.12/drivers/infiniband/core/cma.c)
- [CM field/state use as an independent cross-check](https://github.com/torvalds/linux/blob/v6.12/drivers/infiniband/core/cm.c)

`src/gtest/trex_rocev2_cm_gtest.cpp` provides deterministic tests for exact REQ
field positions, every included MAD's bounds/round trip, CMA padding/service
IDs, unsupported transport/ECE, IPv4 and IPv6/VLAN active/passive setup,
lost REQ/REP/RTU, duplicate DREQ/DREP, simultaneous close, pending-accept
timeout, active-QP preparation/RTU acceptance gating, confirmation timeout,
consumer/unknown-service rejection and reject private data, peer/port mismatch, event
pressure, communication-ID exhaustion, bounded repeated-MRA behavior, PMD
backpressure, ARP/NS/NA and cache use, gateway selection, neighbor expiry,
cancellation and port ownership.
Additional timing tests verify different Local/Remote Response Timeout
exponents, passive REQ retry limits, maximum-exponent deadline caps, and
PSN/retry direction with distinct REQ and REP values.

These focused tests establish the stated local behaviors. Independent packet
captures and RXE peers must still verify negotiation and recovery against
the pinned normative requirements; hardware/software end-to-end runs must
verify the ASTF ownership and packet-carrier integration.
