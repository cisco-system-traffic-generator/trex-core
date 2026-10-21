# RoCEv2 stateful coverage inventory

Requirements-family inventory for the [completion plan](rocev2_stateful_completion_plan.md).
This is not a test result or a normative list of every RoCEv2 requirement.
Every row must expand into requirements with pinned specification clauses,
capability applicability, implementation owner and positive/negative evidence.
Implementation evidence is tracked in the [implementation record](rocev2_stateful_implementation.md). Family status below is an engineering status, not normative verification.

## Status and completion rules

- Draft: source exists, but behavior has not passed the required acceptance.
- Partial: some supporting structure exists; endpoint semantics are incomplete.
- Missing: implementation is absent.
- Open: normative applicability or wider delivery scope needs classification.
- Verified: reserved for a requirement with reproducible passing evidence.

No row is currently Verified. Clause identifiers remain unfilled until the
authorized normative specification is obtained. Missing or unknown entries
must not be counted as passed or silently changed to not applicable.

Mandatory coverage is verified applicable mandatory requirements divided by
all applicable mandatory requirements for the named revision/profile.
Advertised optional capabilities have a separate denominator and must also
reach full verification. Publish raw counts and open classifications with
percentages; test count or opcode count is not protocol coverage.

## Required RC endpoint and CM work

| ID | Behavior/requirement family | Current | Milestone | Minimum acceptance evidence |
| --- | --- | --- | --- | --- |
| BASE-01 | Existing Read/Write build and memory mapping | Implemented; baseline tests pass | M0 | Release/debug builds; nonzero region-ID lookup; paired Write/readback |
| WIRE-01 | IPv4/IPv6 UDP/4791 lengths, padding, ICRC/checksums | Implemented; vectors/software carrier tests | M0/M2 | Independent vectors, invalid checksums, chained mbufs, truncation |
| WIRE-02 | Opcode/header/segment legality and reserved fields | Implemented; codec tests | M2 | Every included opcode and illegal transition/header combination |
| WIRE-03 | Port/VLAN/P_Key/QP/peer identity validation | Implemented; negative tests partial | M2/M5 | Correct peer succeeds; mismatches fail without memory effects |
| QUEUE-01 | Independent bounded SQ/RQ and work identities | Implemented; bounded ledger tests | M1 | Admission limits, ordered consumption, cancellation and generation tests |
| QUEUE-02 | Posted receive buffers and SGEs | Implemented; SGE tests | M1/M3 | Multi-SGE, exact/undersized buffers, key/access errors, reuse lifetime |
| QUEUE-03 | Receive completion metadata and error validity | Implemented; CQE tests | M1/M3 | wr_id, opcode, length, immediate/invalidation fields, exactly one CQE |
| QUEUE-04 | CQ capacity, fatal overflow and no lost outcomes | Implemented; pressure tests partial | M1/M5 | CQ pressure/overflow, explicit error state, deterministic flush |
| MEM-01 | PD/MR registry, lkey/rkey, permissions and bounds | Implemented; same-owner registry tests | M1 | Cross-PD/key failures, overflow, stale keys, memory isolation |
| MEM-02 | Key invalidation/rebind and in-flight lifetime | Implemented; drained key lifecycle tests | M1/M3/M5 | Ordered invalidate, access after revoke, duplicate invalidate |
| MEM-03 | Shared-memory ownership across QPs/workers | Implemented per-port owner; wider scope open | M1/M4 | Shared-region placement, overlapping operations, serialization proof |
| SEND-01 | Segmented SEND and local posted RECV | Implemented; native/software PMD tests | M2/M3 | Zero/one/many packets; exact MTU/MTU+1; exact payload and completion |
| SEND-02 | SEND with immediate | Implemented; native/software PMD tests | M2/M3 | Terminal immediate values, network representation, duplicate-last |
| SEND-03 | SEND with invalidate | Implemented; native/software PMD tests | M2/M3 | Correct key semantics, no repeated revoke/CQE on retries |
| SEND-04 | RQ starvation and replenishment/RNR | Implemented; native fault tests | M3/M5 | Delayed receive, timer encodings, retry exhaustion/unlimited setting |
| RW-01 | Plain Write without receive consumption | Implemented; plain opcode/software PMD tests | M0/M3/M5 | Exact placement; unchanged RQ; no implicit immediate variant |
| RW-02 | Write with immediate and receive completion | Implemented; native and software PMD tests | M3 | RETH placement, one required WQE/CQE, RNR/duplicate recovery |
| RW-03 | Segmented Read and exact result completion | Implemented; replay regressions and software PMD tests | M0/M5 | Exact data; loss/replay; read limits; ACK alone cannot complete Read |
| ATM-01 | 64-bit compare-and-swap | Implemented; native tests | M2/M4 | Match/mismatch, original value, alignment, key/access/bounds |
| ATM-02 | 64-bit fetch-and-add | Implemented; native tests | M2/M4 | Original value, final word, wrap behavior, no mutation on error |
| ATM-03 | Atomic ACK/requester result placement | Implemented; codec/registry tests | M2/M4 | Byte-order vectors, local result errors, duplicate result handling |
| ATM-04 | Exactly-once mutation and bounded replay | Implemented; native replay tests | M4/M5 | Drop response; retry request; one mutation and identical cached result |
| ATM-05 | Shared Read/Atomic initiator/responder limits | Implemented; asymmetric and replay regressions | M1/M4 | Mixed outstanding limits, cache exhaustion, no unsafe eviction |
| ORD-01 | Per-QP mixed-operation ordering and fences | Implemented; native tests partial | M1/M5 | Send/Write/Read/Atomic ordering, fenced/unfenced allowed outcomes |
| ORD-02 | Signalled/unsignalled completion retirement | Implemented; retirement tests | M1/M5 | Ordered retirement and errors without invented CQEs |
| REC-01 | PSN wrap, ACK/NAK and retransmit correctness | Implemented; mixed fault tests partial | M5 | Boundary/future/stale ACKs, all applicable NAKs, mixed retries |
| LIFE-01 | Applicable QP transitions and drain states | Implemented through SQD; normative audit open | M0/M5 | Clause-based transition matrix; normal drain versus error/cancel |
| LIFE-02 | SQ/RQ flush, teardown, stale wire traffic/reuse | Implemented; lifecycle tests partial | M5 | One terminal outcome per accepted WQE; no resource growth; reuse guards |
| CM-01 | GSI/QP1 dispatch and CM MAD envelope/validation | Implemented; independent field vectors | M2/M6 | Independent CM packet vectors, Q_Key/class/length validation |
| CM-02 | Active/passive listen, REQ/REP/RTU, accept/reject | Implemented; module and software PMD tests | M6 | Negotiated QP transitions; prepost before accept/REP; workload starts after establishment; independent CM oracle |
| CM-03 | MRA, retries, duplicates, transaction resources | Implemented; bounded fault tests | M6 | Pending-accept timeout, delays/loss/repeat requests, bounded backlog and half-open cleanup |
| CM-04 | Disconnect, simultaneous close, time-wait | Implemented; module and software PMD tests | M6 | Lost DREQ/DREP, reconnect, stale transactions, deterministic events |
| CM-05 | IP/CMA layout, service IDs and private-data limits | Implemented; codec/schema tests | M6 | Standard-compatible layouts; opaque app data; no implicit MR exchange |
| NET-01 | Explicit next-hop, ARP/IPv6 ND resolution and virtual-host replies | Module tests pass; dynamic adapter qualification open | M6 | Direct/routed paths; ARP replies/NS-to-NA; port/VLAN ownership and removal; bounded unresolved timeout |
| API-01 | Bounded mixed workloads and receive repost | Implemented; schema/RPC tests | M1/M7 | Versioned schema, malformed configs, legacy Read/Write translation |
| API-02 | Async CM APIs, cancellation and capability query | Implemented; schema, RPC and software PMD tests | M6/M7 | Ownership, operation IDs/events, no RPC lifetime blocking |
| OBS-01 | Per-op, receive, atomic/replay, CM stats/verdicts | Implemented; native and software PMD tests | M7 | Validate payload/results/CQEs/errors; counters alone cannot PASS |
| OBS-02 | Request/response/CM capture and worker snapshots | Implemented; software PMD capture tests | M7/M8 | Packet identity/bytes, accepted TX prefix, worker-safe snapshots |
| DP-01 | Same semantics over AF_PACKET and physical DPDK | Software PMD tests pass; physical hardware pending | M8 | Same corpus, independent processes, RSS/NUMA/backpressure |

## Wider protocol/capability scope to resolve explicitly

These entries are not automatically excluded. M0 must classify each against
the selected specification revision, intended claim and advertised features.
Anything required by the chosen scope becomes an implementation work package
with its own state machine and acceptance rows. Optional unsupported features
must be declared and rejected correctly; that does not constitute their support.

| ID | Area | Current observation | Required scope decision/evidence |
| --- | --- | --- | --- |
| SCOPE-01 | User-facing UC transport | No UC endpoint | Inventory applicable SEND/Write, loss and lifecycle semantics; include if claiming whole transport coverage |
| SCOPE-02 | User-facing UD transport and multicast | GSI envelope recognition only | Separate user UD/receive/addressing/SIDR behavior from internal CM QP1 use |
| SCOPE-03 | Shared receive queues | No SRQ | Specify capability; sharing, limit events, teardown and receive ownership if supported |
| SCOPE-04 | Memory windows, bind/local invalidate | Single-window abstraction only | Inventory types/capabilities and required ordered key lifecycle |
| SCOPE-05 | XRC and other transport extensions | No endpoint implementation | Determine applicability to pinned profile; never imply coverage from RC |
| SCOPE-06 | Extended/masked atomics, Atomic Write, Flush/MPE | No implementation | Inventory revision-specific/optional capabilities and add if within advertised scope |
| SCOPE-07 | Alternate path/migration and associated CM messages | No implementation | Determine RoCE applicability, negotiated behavior and explicit unsupported outcomes |
| SCOPE-08 | ECN/CNP and congestion capability | Codec/counters; adapter disables CNP reaction | Audit normative notification behavior; test enabled/disabled policy; distinguish CCL from congestion control |
| SCOPE-09 | Named congestion-control algorithms | Not enabled in adapter | Publish actual algorithm/capability; no implication from a CNP parser |
| SCOPE-10 | IPv6 extensions, VLAN variants, fragmentation/path MTU | Some shapes rejected | Verify required acceptance versus required rejection; document bounds |
| SCOPE-11 | CM SIDR/ECE and optional management behaviors | Missing | Resolve applicability/negotiation and add tests for support or mandated rejection |
| SCOPE-12 | Event/CQ notification and broader verbs ABI | Native RPC only | Distinguish required emulated behavior from a promised libibverbs/librdmacm ABI, which is not currently provided |

## Evidence required for Verified status

Each expanded requirement records:

1. Stable requirement ID, normative revision/clause and applicability reason.
2. Implementation symbol/path and exact tested commit or diff identity.
3. Positive/boundary/negative/fault test IDs and outcomes; all unexplained
   skips remain gaps.
4. Carrier: deterministic engine, AF_PACKET, independent RXE/rdma-core oracle,
   physical DPDK; include tool versions and hardware where applicable.
5. Relevant packet capture/log artifact paths and checksums, payload/result
   assertions, completion counts and resource/cleanup evidence.
6. Reviewer and any explicit interoperability or performance limitations.

Performance evidence is separate from protocol coverage. A passing semantic
matrix does not certify line rate, and a high packet rate does not prove
correct verbs, connection management or exactly-once atomics.
