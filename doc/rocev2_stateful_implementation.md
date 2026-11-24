# Stateful RoCEv2 implementation record

Implementation date: 2026-10-21. Branch: `feature/rocev2-stateful-dataplane`.
Base: PR #1224, `8bb9bef3b5400e7f858f4e0fa3c54781e15f1537`.

This record accompanies the [completion plan](rocev2_stateful_completion_plan.md)
and [requirements inventory](rocev2_stateful_coverage.md). It records the
implementation and validation snapshot prepared for contribution. The reference
RoCEv2 checkout is unchanged.

## Implemented behavior

- RC SEND, segmented receive placement, immediate data, SEND invalidation,
  plain Write, Write with immediate, Read, 64-bit compare-and-swap and fetch-add.
- Bounded SQ/RQ/CQ state, work IDs and receive SGEs; ordered completion,
  Read/Atomic limits and fences; error flushing and SQ drain/resume.
- Registered-memory lookup by protection domain and key, access/range checks,
  shared ownership, remote invalidation, drained removal and fresh-key rebind.
- Retained atomic original values and bounded replay to avoid repeated mutation.
  Read/Atomic response replay uses requester depth and a bounded responder ring;
  ordinary SQ ACKs do not acknowledge response-cache entries.
- Actual QP1/GSI CM MAD exchanges, asynchronous events and manual or automatic
  accept/reject, retries/MRA, disconnect and bounded transaction lifetime.
  QPs and receive queues are installed before REP/RTU can enable peer traffic.
- Owned virtual addresses, static next-hop MACs or bounded ARP/IPv6 ND resolution,
  including replies to probes for emulated local addresses.
- Versioned Python profiles with mixed work programs, receive descriptors,
  memory assertions, CM policy/private data, owned RPCs, capability query,
  completion/memory validation, protocol statistics and ordinary TRex captures.

The native engine runs in the ASTF dataplane worker. Python configures the
session and reads results; it does not process individual packets. AF_PACKET
acceptance uses the actual DPDK PMD inside an isolated network namespace.

## Ownership and current limits

Schema 3 adds host identities, independent devices and configurable scale,
defaulting to 32 devices with 16 QPs per physical port. The
[virtual-device record](rocev2_virtual_devices_plan.md) describes the Python
API, environment overrides, ownership and resource admission. Existing
explicit flat-QP profiles retain schema 2 behavior and their original defaults.

The native MR namespace is local to a physical-port engine. Device profiles
translate device-local PD/MR IDs into disjoint internal identities and assign
all QPs of one device to one worker. Different devices can reuse wire keys,
QPNs and virtual addresses. Legacy QPs in the same protection domain on a port
share one worker; omitted protection domains default to their memory-region ID.
A shared region ID requires identical configuration and one port. Memory is
never shared concurrently across independent workers.

The profile currently reserves one endpoint QP per configured CM listener and
allows one listener per endpoint/service ID. Distinct devices can reuse the
same service ID. Reconnection requires a fresh session;
the lower CM module supports transaction slots and time-wait, but this adapter
does not replace an already attached QP with a new connection in place.

The public profile receive descriptors use the QP's configured MR. The native
C++ engine supports SGEs and result placement across registered regions in the
same owner/protection domain. Drained MR rebind/removal and SQ drain/resume are
native engine APIs; they are not exposed as unrestricted live Python mutations.

CM events are a bounded, nonconsuming per-worker history with sequence numbers.
`private_data` excludes the IP-CMA header on an incoming IP-CMA request;
`wire_private_data` preserves the entire fixed wire field, including padding.
`stop_rocev2` releases local resources. Use `disconnect_rocev2` and observe the
disconnect events before stopping when wire DREQ/DREP teardown is required.

## Validation

Reproducible local artifacts are under `.dev/rocev2-completion/`; this ignored
directory is not part of the contribution. Builds use the retained local Waf
Python environment. Python functional tests use Python 3.12 and the local Nose
compatibility bootstrap; the repository test framework was not migrated.

The pre-extension release and debug simulator baselines each passed all 460
tests. The independent-process Read/Write runner passed all five scenarios on
both UDP and AF_PACKET: clean transfer, packet loss, corruption, malformed
packet and blackholed peer. These establish a baseline and do not qualify the
later extensions by themselves. The historical N2 tests and standalone native
wire runner explicitly select the diagnostic synthetic-response-ACK mode.
The production native default, the new verbs tests and actual ASTF acceptance
use standard response replay without synthetic response ACKs.

Results before the virtual-device extension:

| Check | Result | Local evidence |
| --- | --- | --- |
| Release/debug simulator build | Passed | `integrated-build-sim.log` |
| All DPDK server variants | Passed | `integrated-build-dpdk.log` |
| Full release C++ suite | 526/526 passed | `integrated-unit.xml` |
| Full debug C++ suite | 526/526 passed | `integrated-unit-debug.xml` |
| Native standalone corpus | 104/104 passed | `native-all-unit.log` |
| New standard-mode native verbs | 21/21 passed, Valgrind clean | `verbs-unit.log`, `verbs-valgrind.log` |
| Codec extension | 14/14 passed, Valgrind clean | focused codec test artifacts |
| CM/neighbor module | 31/31 passed | focused CM test artifacts |
| Python schema/RPC contract | 36/36 passed, zero skips | `endpoint-python.log` |
| Complete Python functional suite | 200/200 passed | `functional-all.log` |
| Python 3.11 smoke | ASTF schema, STL imports and packet compilation passed | local smoke invocation |
| Actual ASTF/DPDK AF_PACKET | 16/16 expected outcomes | `astf-final/report.json` |

The 16 actual-server cases cover IPv4 and IPv6 Read/Write, Write with immediate,
SEND with immediate/invalidate, CAS/FAA, automatic/manual CM acceptance and
manual rejection. Fourteen are positive traffic cases and two require rejected
connections to fail traffic validation, with no invented SQ work failures.
CM acceptance includes DREQ/DREP teardown. All 134 accepted TX frames match
134 peer RX frames, with independent checksum/ICRC checks and 64 directional
PCAPs. These are correctness checks, not performance measurements.

The integration runner is
`scripts/automation/regression/rocev2_endpoint/run_astf.py`. It starts a real
ASTF server with two isolated DPDK AF_PACKET ports, sends commands through the
public Python API, verifies completion and memory outcomes, independently
checks wire headers/checksums/ICRC, compares accepted TX with peer RX, and saves
directional PCAPs and JSON evidence. No shared lab ports are used.

## Virtual-device validation

The subsequent schema 3 device extension was validated on 2026-10-21. Its local
evidence is under `.dev/rocev2-devices/` and its usage/ownership contract is in
[the virtual-device record](rocev2_virtual_devices_plan.md).

| Check | Result | Local evidence |
| --- | --- | --- |
| Release/debug simulator and all DPDK variants | Passed | `build-sim-final.log`, `build-dpdk-final.log` |
| Full release C++ suite | 537/537 passed | `unit-release.xml` |
| Full debug C++ suite | 537/537 passed | `unit-debug.xml` |
| Complete Python functional suite | 212/212 passed | `functional-all.log` |
| Focused Python endpoint contract | 48/48 passed | `python-endpoints.log` |
| Native engine standalone | 33/33 passed, including 11 device cases | `trex-rocev2-engine-devices-all.log` |
| New native device cases under Valgrind | 11/11 passed, no errors/leaks | `trex-rocev2-engine-devices-valgrind.log` |
| Actual ASTF/DPDK AF_PACKET | 24/24 expected outcomes, 90 PCAPs | `astf-final/report.json` |
| Python 3.11 smoke | Device profiles, STL imports and packet compilation passed | `python311-smoke.log` |
| CLI and capture filters | Default/env/settings dry runs and offline libpcap isolation passed | `python-review.json`, `capture-filter-offline.json` |

The actual-server additions cover IPv4/IPv6 multi-device Write/Read, reuse of
QPN/PD/MR/key values, reuse of CM services, dual-stack device counting,
per-device statistics and selected-device capture. Raw RPC tests reject unknown
devices, mismatched host identities and insufficient storage, verify rollback,
and ensure a failed new configuration cannot expose previous-session success.
An address-only case verifies ARP and the unused IPv6 GID's neighbor responder
while no RDMA work is submitted.

The default scale is admitted and exercised on two local physical-port
identities: 32 devices and 16 QPs each, 64 devices/1024 QPs total. With 4-KiB
messages, SQ depth 1 and no automatic receive postings, all 1024 operations
completed and validated with zero retransmissions. The native storage estimate
was 225,023,888 bytes and registered memory was 8,388,608 bytes. This uses one
DPDK worker because this AF_PACKET driver restricts ASTF to one worker per port
pair; it does not qualify physical-NIC throughput, other Linux distributions,
or the minimum 12-CPU/32-GB host's maximum active capacity.

## Conformance and performance qualification

The advertised implementation is an RC baseline, not an established claim of
complete RoCEv2 conformance. The normative IBTA revision, Volume 1/RoCE annexes
and applicable errata have not been supplied. No requirements-family row is
marked normatively Verified and no protocol coverage percentage is asserted.
Public Linux/rdma-core interfaces and source provide independent engineering
references; they do not replace the normative specification.

UC, user UD/multicast, SRQ, XRC, extended atomics, alternate path migration,
SIDR and ECE are not implemented. The adapter disables congestion-control
algorithms and CNP generation/reaction; parsing a CNP is not a congestion-control
implementation. CCL collective scheduling and DevX integration are excluded.
See the wider-scope rows in the requirements inventory before broadening any
claim. [CM protocol details and provenance](rocev2_cm_protocol.md) document the
implemented wire profile and explicit rejection behavior.

Independent SoftRoCE/RXE or RNIC interoperability, physical-NIC RSS/NUMA and
backpressure qualification, and throughput/line-rate measurements remain
separate work. Software carrier correctness is not evidence of line rate.
