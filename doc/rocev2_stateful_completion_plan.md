# RoCEv2 stateful completion implementation plan

Status: implementation and validation in progress; see the [implementation record](rocev2_stateful_implementation.md).
Planning date: 2026-10-21.
Branch: feature/rocev2-stateful-dataplane.
Base: PR #1224, 8bb9bef3b5400e7f858f4e0fa3c54781e15f1537.

This expands the [initial port plan](rocev2_stateful_plan.md). It does not
establish conformance or validate the existing draft. The accompanying
[coverage inventory](rocev2_stateful_coverage.md) is the starting requirements
ledger; it must become a clause-level matrix before a completeness claim.

## Objective and coverage claim

Implement complete requester/responder behavior for the selected RoCEv2
Reliable Connected (RC) profile: SEND/RECV and variants, Read/Write and
immediate-data semantics, baseline 64-bit atomics, ordering/recovery, memory and
QP lifecycle, and real connection-management wire exchanges. Use the same
native engine with physical DPDK PMDs and the AF_PACKET software carrier.
TRex-to-TRex remains the first deployment target. CCL collective scheduling and
DevX integration remain outside this work.

Adding the three headline features alone does not establish all RoCEv2
coverage. RC, UC, UD, management traffic and optional extensions must be
inventoried separately. Recognition of an opcode is not an implementation of
its state machine. The [verbs operation table][verbs] distinguishes transport
services and additional operations, and the [IBTA catalog][ibta] contains
versioned specifications.

The release claim will name the exact specification revision, transport
services and advertised capabilities. "100%" means every applicable mandatory
requirement and every advertised optional capability has passing evidence.
An unknown requirement stays an open gap. It cannot be relabeled optional or
not applicable to improve the percentage. A whole-protocol claim remains
blocked until the wider transport/extension inventory is closed.

## Observed starting point (before this implementation)

The branch contains uncommitted native codec/QP/transport/engine modules,
ASTF worker integration, owned RPCs, Python profiles and an isolated runner.
At the planning checkpoint these additions had not passed a build or runtime acceptance. This section records that historical starting point; current evidence is in the implementation record.

Concrete source gaps:

- WorkKind contains only WRITE and READ; parts of the completion ledger treat
  every non-Write operation as Read.
- post_receive adds credits, not receive descriptors or buffers. Write with
  immediate can consume a credit without producing a receive completion.
- The codec lacks RC SEND families, invalidate headers and atomic request/
  response headers. Existing UD/GSI envelope support can be reused for CM.
- Memory has no remote-atomic access, atomic primitive, protection-domain
  registry or complete local/remote key invalidation lifecycle.
- QP states are RESET/INIT/RTR/RTS/ERR. Applicable drain/error states and
  transitions need a transport-specific audit.
- The ASTF profile chooses one operation per QP. It cannot express a mixed
  ordered program, posted receive buffers or connection events.
- UDP/4791 dispatch maps destination QPNs to configured data QPs numbered at
  least 2. There is no separate QP1 CM listener or transaction state machine.
- The recent nonzero memory-ID assignment still has zero-based memory lookup
  call sites in the ASTF adapter. This is an existing baseline blocker.

## Architecture and invariants

Keep codec, QP/queues, memory registry, RC transport, workload scheduler,
connection manager and packet carrier separate. Python configures bounded
programs and reads results; it never processes individual packets.

Each QP and its SQ/RQ/CQ state has one dataplane owner. Introduce explicit
memory ownership: QPs sharing writable regions must execute through the same
memory owner, including overlapping Write, Read and atomic operations. The
initial implementation will assign QPs connected by shared-region references
to one worker and spread independent groups across workers. Configuration
must expose the resulting placement and reject an impossible resource request.
Do not preserve QPN-modulo sharding when it would violate shared-memory
atomicity. Cross-worker memory execution is a separately reviewed optimization.

All queues, replay records, descriptors, memory regions, CM transactions and
pending events have configured bounds. Reserve resources needed for a result
before irreversible side effects. Descriptor backpressure must not cause a
receive to be consumed twice or an atomic mutation to run twice. Preserve
accepted-prefix TX ownership and arm retry timers from actual PMD acceptance.

Local generations protect local asynchronous objects; they are not carried in
standard data packets. QPN/connection-ID reuse therefore also needs the
protocol's stale-packet, replay-window and time-wait protections.

## Milestone 0: freeze requirements and stabilize the current draft

1. Obtain an authorized normative specification and record revision, annexes,
   errata and document checksum/reference. The current IBTA catalog identifies
   Release 2.1, but this plan has not inspected that full specification.
   Exact clause numbers and mandatory/optional classifications remain open
   until that review. Linux/rdma-core are implementation references, not a
   replacement for the normative text.
2. Expand every inventory row into clause-level positive, boundary and error
   assertions. Explicitly classify UC/UD, SRQ, memory windows, XRC, path
   migration, IPv6 packet shapes, ECN/CNP and newer extensions. No silent
   scope reductions. Later extensions such as MPE Flush/Atomic Write need
   explicit applicability decisions against the selected revision.
3. Fix all memory-ID versus QP-index lookups; audit source/target memory
   binding, completion validation, configure rollback, stop/restart and
   normal completion versus fatal QP error.
4. Resolve implicit Write-with-immediate defaults. A plain Write request must
   remain plain Write; variants must be explicit in the public program.
5. Build release/debug simulator and DPDK targets. Run existing Read/Write,
   Python API, stateless RoCEv2 and relevant ASTF regression coverage.
   Establish paired-process AF_PACKET acceptance before building on the draft.

Exit: known baseline defects fixed, existing behaviors demonstrated, exact
requirements ledger started, and no unsupported claim of current readiness.

## Milestone 1: queues, registered memory and ordered work

Primary files: trex_rocev2_native_{types,storage,qp,engine}.{h,cpp} under
src/stx/stl; trex_astf_rocev2.{h,cpp} under src/stx/astf.

- Replace binary Write/Read dispatch with explicit operation traits:
  ACK-completed, response-completed, receive-completed and local operations.
  Keep SQ and RQ accounting independent.
- Add bounded receive WQEs with caller work ID, local memory/SGE references,
  total capacity and ownership state. Implement batch posting/reposting and
  deterministic teardown of posted and partially consumed receives.
- Extend completion records with queue direction, opcode, status, byte length,
  QP/work identity and immediate/invalidation flags and values. Define which
  fields remain valid on errors. A CQ overrun must produce an explicit fatal
  outcome, never discard results and return PASS. These contracts follow the
  [receive][recv] and [completion][cq] interfaces.
- Add protection-domain/region lookup, local and remote keys, access flags,
  remote-atomic permission, key generations, SGEs and invalidation/rebind
  lifetime. Keep an adapter for the old single-buffer configuration.
- Add a bounded ordered workload representation, SQ fences, signalled/
  unsignalled retirement, and shared negotiated Read/Atomic requester and
  responder limits. A cumulative ACK cannot substitute for a Read/Atomic result.
- Account for all new resources in preflight storage estimates. No hot-path
  growth after configuration.

Exit: queue/memory/ordering contracts can represent every planned operation,
with failure/flush behavior and multi-QP shared-memory ownership proven.

## Milestone 2: complete the codec and operation legality tables

Primary files: trex_rocev2_native_codec.{h,cpp}; shared ICRC helpers.

Add RC SEND FIRST/MIDDLE/LAST/ONLY, terminal immediate and invalidate variants,
AtomicETH, Atomic ACK response data and invalidate fields. Preserve existing
Read/Write opcodes. Use a table of legal opcode/header/payload combinations
instead of treating unrecognized operations as Read or Write.

Define padding, byte order, reserved bits, payload limits and atomic alignment
from the pinned specification. Verify IPv4/IPv6, required UDP checksum and
ICRC behavior with independent packet vectors; include chained mbufs and
malformed/truncated frames. The [Linux wire definitions][opcodes] are a useful
cross-check, not the normative completeness list.

Exit: exact-byte vectors for every included request/response and rejection
vectors for illegal combinations; no transport state mutation on decode error.

## Milestone 3: SEND/RECV and immediate/invalidation semantics

Primary files: native transport, QP/engine adapters, workload scheduler.

- Implement post_send and requester segmentation/retries. RECV is a local
  posted-buffer operation, not a new packet opcode.
- Match a message to one RQ WQE at the specification-defined admission point;
  place segmented payload across SGEs and emit exactly one receive completion.
  Retain assembly/ownership across legal retransmissions.
- Implement no-buffer RNR, delayed repost, encoded RNR timers/retry limits,
  including unlimited-retry semantics. Handle undersized/invalid receive
  buffers with the prescribed local/remote errors.
- SEND with immediate returns metadata in its receive completion. SEND with
  invalidate applies the permitted key invalidation exactly once at the
  required point relative to completion.
- Plain Write consumes no RQ WQE. Write with immediate places payload through
  RETH into remote memory while consuming the required receive resource and
  producing a receive-with-immediate completion.
- Audit mixed SEND/Write ordering, duplicate first/last packets, lost ACKs and
  PSN wrap. Do not promise transactional rollback of partially transferred
  data where the specification does not provide it.

Exit: payload, immediate/invalidation metadata, WQE consumption and completions
are correct under loss/retry and exhaustion, not merely packet-count matched.

## Milestone 4: 64-bit atomics with exactly-once execution

Primary files: codec, native memory registry, transport, QP completion ledger.

- Add compare-and-swap and fetch-and-add with explicit remote-atomic access,
  alignment, bounds, key/PD and local result-buffer checks.
- Return the original value using the specified wire representation. Implement
  match/mismatch CAS and modular arithmetic behavior for fetch-and-add.
- Add an indivisible memory operation through the region owner. Avoid casting
  arbitrary byte-vector storage to an unaligned native atomic pointer.
- Reserve a bounded responder replay entry and response resources before
  mutation. Commit the operation/result and replay identity together. A lost
  response followed by a repeated request returns the original saved result
  without another mutation.
- Identify replay entries by QP instance, request PSN and operation identity;
  detect conflicting duplicates and stale packets without aliasing a new
  operation after wrap or reuse.
- Retain/retire replay entries using the protocol's outstanding-operation,
  PSN and retry rules. Exhaustion must backpressure or fail according to the
  defined contract; eviction must not make a stale retry execute as new work.
- Apply fences and negotiated Read/Atomic resource limits to mixed workloads.
  Cover multiple QPs targeting the same word and overlapping memory accesses.

The [RXE responder][rxe] supplies independent behavior to compare, especially
duplicate-request handling. Use it as an oracle with license-aware review,
not as code to copy automatically into this Apache-licensed implementation.

Exit: mutation count, returned old values and final memory are correct after
request/response loss, duplicate delivery, replay pressure, wrap and reconnect.

## Milestone 5: recovery, lifecycle and error completion closure

Audit all included operations together, not in isolated verb-only tests.
Cover applicable QP transitions and drain/error states; do not add a
transport-specific state to RC without verifying applicability. Define
normal drain, explicit cancellation and fatal flush as separate outcomes.

Exercise cumulative/duplicate/future ACKs, all applicable NAK syndromes,
out-of-order segments, retry exhaustion, RNR, outstanding-resource limits and
result-before-completion ordering. Every accepted work request must reach
one terminal outcome, including posted receives on QP error/teardown.

Validate stale keys, QPN reuse, generation changes and old packets after
stop/reconfigure. Release queued mbufs, timers, replay records and references
only after the owning worker acknowledges cleanup. Repeat start/stop and
partial multi-worker setup failure without resource growth.

Exit: the transport/lifecycle matrix passes across mixed operations and all
failure paths; completion uniqueness and memory isolation remain intact.

## Milestone 6: wire-compatible RoCE connection management

Add separate native CM codec/state-machine and neighbor modules, proposed as
trex_rocev2_native_cm_codec.{h,cpp}, trex_rocev2_native_cm.{h,cpp} and
trex_rocev2_native_neighbor.{h,cpp}, plus ASTF dispatch and CP/DP messages.

### Carrier and ownership

Reuse/verify the existing UD BTH/DETH/GSI envelope, then implement CM MAD
validation and message bodies. Dispatch QP1 management traffic before data-QP
lookup to one bounded CM owner per port/listener context. CM events configure
or release data QPs through their owning workers; the CM actor must not mutate
live QP state from a second thread.

Replace configure-time forced RTR/RTS transitions for managed QPs with
connection-driven creation and transitions. Post passive receive WQEs before
accept/REP; define pending-accept timeout and applicable MRA handling. Start
workload transmission only after connection establishment. Keep CM state
distinct from QP state, and keep data-packet processing outside the serialized
CM path.

Use genuine CM wire exchanges over the packet carrier. A private TCP/RPC
handshake is not RDMA-CM. Calling librdmacm alone cannot manage these emulated
QP objects on a DPDK-owned interface; use rdma-core/RXE as independent peers
for qualification. The [CM interface][cm] and [Linux CM fields/states][cmstates]
describe separate connection-management behavior.

### State machine and addressing

Implement active and passive open, listen/backlog, accept/reject, REQ/REP/RTU,
applicable MRA handling, DREQ/DREP, timeout/retry, duplicate transactions,
simultaneous close and time-wait. Inventory SIDR, alternate-path messages and
ECE against the selected transport/capabilities; implement required behavior
or explicit supported/unsupported negotiation.

Validate transaction/communication IDs, peer identity, service IDs, P_Key/
Q_Key, field lengths and negotiated QPN/PSN/MTU/Read-Atomic/retry parameters.
Bound half-open connections, events and retransmission caches.

Keep endpoint IP identity separate from next-hop MAC. Provide explicit static
next-hop configuration and bounded ARP/IPv6 ND resolution with VLAN/route
selection and failures reported as events. Configure local prefixes/routes,
optional gateways and IPv4/IPv6 GIDs; emit ARP replies and IPv6 NS-to-NA replies
for emulated local addresses with explicit registration/removal lifetime.
Key caches by port, VLAN, family and next hop. Integrate
host-service ownership with the reserved session rather than enabling a
competing service-mode client. Preserve static deterministic mode.

Base CM does not automatically exchange application MR addresses/rkeys. Keep
that as explicit application metadata. Support the standard IP/CMA private
data layout where required for an rdma-core peer, within message-specific
limits. Exchange larger TRex workload descriptors over a defined application
protocol after establishment; never pretend private metadata is standard CM.

### Native API

Add asynchronous listen/connect/accept/reject/disconnect/event APIs with
operation IDs and bounded event polling. Connection retries may outlive one
RPC, so do not hold the CP in the existing synchronous worker-wait loop for
the full connection lifetime. Managed connections configure local intents
and obtain remote QPN/PSN/path parameters through CM rather than requiring
them up front. Preserve manual/static setup, context ownership and cancellation.

Exit: establishment and teardown work between independent TRex processes in
both roles, under lost/duplicate CM messages and resource exhaustion, with
valid captures and an independent rdma-core/RXE comparison.

## Milestone 7: workload APIs, statistics and captures

Extend Python trex_astf_rocev2.py and trex_astf_client.py, native RPCs and the
ASTF worker adapter. Proposed schema version 2 includes regions, queue limits,
SGEs, receive programs, ordered mixed operations and optional CM configuration.
Translate the existing one-operation Read/Write profile into the same internal
program to preserve compatibility. Add a capability query and reject
unsupported requests before changing live state.

Support finite/continuous workloads, bounded receive replenishment, fences,
atomic operands/result expectations and memory assertions. Configure reusable
programs/batches; do not create per-packet Python RPC traffic.
Gate application work on connection establishment and availability of any
required application memory descriptors.

Publish per-op posted/completed/failed/byte/result counters; RQ depth/consumed/
reposted counts; receive CQEs and immediate/invalidation metadata; RNR and
retry events; atomic executions versus replays; CM lifecycle/failure counters;
CQ errors; and per-op completion latency. Snapshot through worker-owned
messages. Capture request/response and CM frames through existing capture APIs.

Exit: an API user can create each workload, establish a connection, inspect
captures and validate data/results/errors without relying on matching TX/RX.

## Milestone 8: conformance and performance acceptance

Run the same semantic corpus through successive carriers/oracles:

1. Deterministic fake-clock unit/engine tests, exact-byte vectors and
   reproducible loss/duplication/reordering/corruption faults.
2. Two independent TRex processes over AF_PACKET/veth, covering actual RX/TX,
   RSS redirection, partial PMD acceptance and repeated lifecycle operations.
3. Independent RXE/rdma-core comparisons for data verbs and CM in both roles.
   Use CM/SEND test tools appropriate to the feature; rping also requires its
   application memory-descriptor protocol, not just CM establishment.
4. Physical DPDK PMDs with multiple ports, QPs and workers, NUMA placement,
   descriptor pressure, IPv4/IPv6 and applicable VLAN/routed cases.
5. Separate performance runs recording hardware/link/core/QP/MTU/message/
   operation mix, payload verification and ICRC settings. Report completed
   payload goodput, wire rate, PPS, retransmissions, CPU and completion latency.
   Do not weaken protocol checks to obtain a line-rate claim.

Critical scenarios include zero/exact-MTU/MTU+1/long messages; RQ and CQ
exhaustion; delayed receives; immediate/invalidation; CAS match/mismatch;
fetch-add overflow; atomic-response loss; shared-word concurrent QPs; mixed
Write/Atomic/Read fences; PSN wrap; stale reconnect traffic; and lost
REQ/REP/RTU/DREQ/DREP. Unknown/malformed requests must not corrupt memory.

Keep existing STL and ASTF traffic, stateless RoCEv2, packet golden comparisons,
Python compatibility and isolated client packaging in the regression gate.
Use release/debug and appropriate sanitizers; record source/diff identity,
tool versions, logs and capture hashes for reproducibility.

Release requires all mandatory/advertised-capability matrix rows verified,
no unexplained skips, independent protocol review, complete documentation and
honest published limitations. No numerical whole-protocol percentage is
published while requirement classification or wider-transport scope is open.

## Work order and review boundaries

M0 precedes extensions. M1 defines shared contracts; M2 codec work and CM
design can then proceed in parallel. M3 and M4 share QP/memory/replay code and
need coordinated ownership. M5 validates their combined semantics. M6 uses
the stable QP lifecycle. M7 evolves with each feature; M8 closes evidence.

Use reviewable local changes/commits for foundation, SEND/RECV, atomics,
recovery/lifecycle, CM, and API/qualification. Keep shared-file edits serial
or in isolated worktrees. Preserve existing dirty work and the reference
checkout. Commit, push and PR publication remain separate user actions.

## Primary references

The normative specification must be acquired and pinned in M0. The following
links support planning; reference tags are fixed for reproducibility rather
than asserted to be the latest versions. Review each file's license before
reusing any implementation.

- [IBTA specification access and revision catalog][ibta]
- [rdma-core operation/flag table][verbs]
- [rdma-core receive descriptors][recv]
- [rdma-core completion contract][cq]
- [Linux wire opcode definitions][opcodes]
- [Linux RXE responder behavior][rxe]
- [rdma-core CM interface][cm]
- [Linux CM lifecycle definitions][cmstates]
- [Linux CM message definitions][cmfields]

[ibta]: https://infinibandta.org/ibta-specification/
[verbs]: https://github.com/linux-rdma/rdma-core/blob/v54.0/libibverbs/man/ibv_post_send.3
[recv]: https://github.com/linux-rdma/rdma-core/blob/v54.0/libibverbs/man/ibv_post_recv.3
[cq]: https://github.com/linux-rdma/rdma-core/blob/v54.0/libibverbs/man/ibv_poll_cq.3
[opcodes]: https://github.com/torvalds/linux/blob/v6.12/include/rdma/ib_pack.h
[rxe]: https://github.com/torvalds/linux/blob/v6.12/drivers/infiniband/sw/rxe/rxe_resp.c
[cm]: https://github.com/linux-rdma/rdma-core/blob/v54.0/librdmacm/man/rdma_cm.7
[cmstates]: https://github.com/torvalds/linux/blob/v6.12/include/rdma/ib_cm.h
[cmfields]: https://github.com/torvalds/linux/blob/v6.12/include/rdma/ibta_vol1_c12.h
