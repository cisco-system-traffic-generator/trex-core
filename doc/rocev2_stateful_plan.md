# Native RoCEv2 endpoint implementation plan

Base: upstream PR #1224, `8bb9bef3b5400e7f858f4e0fa3c54781e15f1537`.
Development branch: `feature/rocev2-stateful-dataplane`.

The required expansion beyond the initial Read/Write port is specified in
[the stateful completion plan](rocev2_stateful_completion_plan.md), with a
[coverage inventory](rocev2_stateful_coverage.md). Those documents govern the
remaining SEND/RECV, atomics, connection-management, and conformance work.

Native transport extraction source:
reference checkout `trex-rocev2/trex-core`, HEAD
`9d7a5ae73bac2479af15e45ca0e242f8eaf0a40f` (`9d7a5ae73`).
The extracted `trex_rocev2_native_{types,storage,codec,qp,transport,engine}`
modules and codec/QP/transport/engine regression tests retain their Cisco
copyright and Apache 2.0 notices. The port excludes CCL session management,
collective scheduling, bootstrapping, and DevX integrations. Local adaptations
separate wire constants/opcodes from the CCL workload header, preserve unlimited
retry value 7, add non-consuming burst TX inspection, ignore sequence NAKs for
non-live PSNs, and make disabled CNP handling leave transport pacing unchanged.

## Intended behavior

Two TRex instances emulate reliable-connected RoCEv2 endpoints using native C++
packet processing. The first implementation supports segmented RDMA Write and
Read, actual bounded memory backing, payload verification, ACK/NAK handling,
retry exhaustion, and operation completions. Python configures workloads and
retrieves statistics; it does not process individual packets. CCL orchestration
and DevX integrations are excluded.

Use ASTF's DPDK worker and packet-I/O infrastructure. Each QP belongs to one
worker; incoming frames are dispatched by destination QPN and endpoint identity.
Keep resource ownership, generations, bounded queues, and stop/reconfigure
behavior explicit. Initially a configured endpoint session reserves its ASTF
context, preventing simultaneous TCP/UDP or STL workloads from consuming the
same packets. Retain existing stateless RoCEv2 APIs unchanged.

The development branch has expanded the initial Read/Write port with SEND/RECV,
immediate/invalidate variants, baseline atomics, wire connection management and
independent virtual-device profiles. The
[implementation record](rocev2_stateful_implementation.md) describes current
behavior and test results; the
[virtual-device record](rocev2_virtual_devices_plan.md) specifies host identity
reuse and the configurable 32-device/16-QP defaults. Independent
interoperability, wider protocol capabilities and measured physical performance
have separate acceptance gates. Physical DPDK and AF_PACKET software operation
use the same transport engine.

## Implementation status

The development branch now contains the native transport/codec/engine, the
ASTF worker adapter, owned RPC commands, Python endpoint APIs, an isolated
wire-level runner, and the usage/limitation notes in `doc/trex_astf.asciidoc`.
Simulator and DPDK builds, release/debug unit suites, Python functional tests
and isolated actual-server AF_PACKET acceptance have passed. This includes
default-scale admission and 1024 validated operations across two port
identities. Exact results and remaining conformance, interoperability and
physical-NIC qualification are recorded in the implementation document.

## Implementation sequence

1. Verify PR head and create the development branch without changing the PR.
2. Extract the CCL-free codec, QP, reliable transport, bounded storage, and
   memory-backed endpoint engine from the reference checkout. Preserve source
   notices and record the reference revision. Audit dependencies and retry
   behavior; port focused protocol and memory correctness tests.
3. Add an ASTF worker adapter with bounded RX/TX bursts, destination-QPN
   ownership, cross-worker forwarding, timer progress, and lifecycle cleanup.
   Preserve TX backpressure and avoid a central worker owning every QP.
4. Add owned native RPCs and Python methods for configuring endpoints, starting
   and stopping finite workloads, snapshots, and completion/payload validation.
   Validate requests before activation, roll back failed multi-worker setup,
   and reject conflicting traffic operations.
5. Add runnable paired-endpoint examples and a standalone software acceptance
   runner. Cover segmented Write/readback, loss recovery, corrupt frames,
   unreachable peers, bounded resource failures, and clean shutdown.
6. Build simulator and DPDK targets; run release/debug C++ tests, relevant Python
   functional tests, isolated endpoint acceptance, existing packet-generation
   comparisons, and `git diff --check`. Record commands, results, and remaining
   hardware qualification separately.

## Acceptance and performance

Operation success requires protocol completion and validated data, not matching
TX/RX packet counts. Report retransmissions and unique completed payload bytes
separately from wire traffic. Missing completions, fatal protocol errors,
verification failures, and deadline expiry must fail validation.

Software carrier tests prove transport behavior only. AF_PACKET/veth tests prove
real Ethernet frame exchange through the software PMD. Physical NIC tests must
record NIC/link speed, CPU/core count, QP count, MTU, message size, ACK policy,
ICRC and payload-validation settings before making throughput claims. A line-rate
goal is not a measured result, and one QP does not automatically scale across
workers.

## Delivery

Keep changes reviewable and uncommitted on the development branch unless a
commit is requested. Do not push or open a PR as part of implementation. Preserve
the reference checkout and unrelated worktrees. Add implementation provenance,
usage, validation results, and known limitations to contributor documentation.
