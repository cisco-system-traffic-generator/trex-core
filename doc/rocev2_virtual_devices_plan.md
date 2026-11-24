# RoCEv2 virtual devices and configurable scale

Design decision: extend the existing emulated-host identity model with an RDMA
device capability. A new `rocev2_subinterface` object is unnecessary. This
document records the implemented schema 3 device profiles and their accepted
defaults. Existing explicit flat-QP profiles remain available as schema 2.

## Defaults and configuration

- Default to **32 emulated devices per configured physical port**, with
  **16 QPs per device**: 512 local QPs per physical port.
- A dual-stack device counts once. Its IPv4 and IPv6 paths share the device's
  QP count; the defaults do not mean 32 devices for each address family.
- These are profile-generation defaults, rather than a certified active-device
  maximum or a requirement to allocate every possible device for small profiles.
  Existing explicit flat-QP profiles retain their requested QP count.
- `ASTFRoceV2Scale` exposes `devices_per_port` and `qps_per_device`, with environment
  fallbacks `TREX_ROCEV2_DEVICES_PER_PORT` and `TREX_ROCEV2_QPS_PER_DEVICE`.
  An explicitly supplied setting takes precedence over its environment value;
  an absent environment value uses 32 or 16 respectively. Invalid selected
  values must fail, including zero, negative, fractional and nonnumeric values.
- Resolve environment defaults once in the client/profile builder, serialize
  the effective settings, and validate the full request again on the server.
  Workers must not independently read environment variables. A requested scale
  must never be silently truncated to fit a budget.
- Device/QP counts are separate from registered-memory, native-storage and
  queue budgets. Admission uses the actual physical-port and worker topology.
  `max_native_storage_bytes` defaults to 512 MiB and accepts explicit values up
  to 16 GiB; the total session remains bounded to 4096 QPs and at most 256 MiB
  registered-memory payload. No environment value is read by the server.

The adapter defaults to 512 MiB estimated native storage per session.
Legacy registered-memory payload defaults to 64 MiB and permits
an explicit limit up to 256 MiB. These budgets are shared by all local ports.
At the default scale on two local ports, 4-KiB messages with SQ/RQ depths of 16
need 68 MiB registered memory. Device profiles use a 128-MiB registered-memory
default, while preserving the existing flat-profile default.
QPs per device and SQ/RQ depths are independent settings;
the existing QP defaults remain SQ depth 1 and no automatic receive postings.
The memory budget alone cannot guarantee admission: larger messages, more
ports or more owning workers may exceed another budget.

## Object and ownership model

```text
Physical port
  L2 namespace / VLAN context
    Emulated host identity: MAC, IPv4/IPv6, prefixes, gateways
      RoCEv2 device: device_id, node GUID, GID table
        Protection domains, registered memory, completion queues
          Reliable-connected QPs and work programs
```

`ASTFRoceV2Host` reuses the host identity concepts expressed by
`EMUNamespaceKey`, `EMUClientKey` and `EMUClientObj`, independently of EMU runtime
plugins. `ASTFRoceV2Host.from_emu_client(client, namespace)` copies an existing
EMU client's static identity into a native profile and rejects unsupported
address/VLAN shapes, including QinQ. `ASTFRoceV2Device` adds a unique device ID,
node GUID and GID table. Each QP selects one local GID/IP family. Both host
families receive ARP/ND ownership even when one family has no active QP.

The DevX IPv4/IPv6 host helpers are a separate integration surface. Their
address configuration and neighbor services can feed an ownership adapter, but
the corresponding DevX host RPCs are not present in this upstream checkout.
They do not supply RDMA QP, memory or completion state by themselves.

The existing EMU service handles host protocols in a separate process, whereas
native RoCEv2 packet execution runs in ASTF C++ dataplane workers. Reusing host
configuration does not imply routing RoCE data packets through EMU. Provide
exactly one ARP/ND/address owner per host identity. The converter does not start
EMU responders. The reserved native context and service-mode guard exclude
competing TRex service operation. External Linux/EMU address owners are not
automatically discovered; the caller must keep those responders inactive.
Simultaneous EMU operation needs a separately validated ownership integration.
This copies static host identity and supplies native ARP/ND; it does not add
general ICMP echo, DHCP, RA or other
EMU runtime plugins.

Linux subinterfaces, namespaces, macvlan and RXE are optional backend
mechanisms. Native/DPDK virtual devices do not require one Linux interface,
kernel RXE device or NIC VF per emulated device.

## Implemented native integration

Incoming packets select their physical port, VLAN, destination MAC/IP and QPN.
`device_id` is not on the wire. Ambiguous bindings and duplicate owned IPs are
rejected. Device-local PD/MR identities are translated before native registry
access, while wire keys and virtual addresses stay unchanged. CM listener
routes include the destination endpoint and service ID. One device owns one
worker, retaining shared-memory atomicity; independent devices distribute
across workers with bounded redirection when RSS selects another worker.

The native engine indexes QP IDs, qualified wire QPNs, memory IDs and PD/key
lookups. Closing a QP only purges queued packets for its qualified identity.
Rebind, failed attach and teardown update these indexes transactionally.
The workload adapter still polls configured QP lists; high active scale and
physical multiworker throughput require performance qualification.

Statistics include QP, device and physical-port completion/error summaries,
configured node GUID/host/GID identity, effective scale and global admission.
`data_tx_packets` counts RC frames accepted by the PMD; `data_tx_packets_queued`
counts transport-produced RC frames, including frames not yet accepted.
`data_rx_packets` counts codec-valid routed RC input before peer/P_Key checks;
`endpoint_mismatch_packets` and `malformed_rc_packets` expose transport rejects.
Ordinary physical-port counters include CM, neighbors and other received frames.
Validation requires protocol completions and payload outcomes rather than
matching packet counters alone. Device/port error sums describe QP errors;
unattributable worker/control errors also affect the top-level verdict.

Device `capture_filter(direction='both')` supplies BPF for the existing capture
API. NIC receive filters or promiscuous mode must accept all device MACs.
Changing device membership requires stopping and reconfiguring the session;
the API does not expose unrestricted live device/QP/MR mutation.

## Usage

```python
from trex.astf.api import ASTFRoceV2Scale

# Explicit settings override environment, which overrides 32/16 defaults.
profile = ASTFRoceV2Scale().paired_profile(ports=(0, 1), ip_version='dual')
client.configure_rocev2(profile)
client.start_rocev2()
client.wait_on_rocev2(timeout=30)
print(client.get_rocev2_stats()['devices'])
client.stop_rocev2()
```

The reciprocal factory uses deterministic demonstration addresses and distinct
UDP source-port entropy per QP. Configure DUT forwarding for these addresses,
or construct `ASTFRoceV2Host`, `ASTFRoceV2Device` and topology-specific QPs.
The example at `trex/examples/astf/rocev2_devices.py` accepts JSON `--settings`,
explicit count/budget overrides, IPv4/IPv6/dual selection and PCAP capture.
`--dry-run` emits the profile without connecting or taking port ownership.

Regression coverage resides in `functional_tests/rocev2_endpoint_test.py`,
`src/gtest/trex_rocev2_native_gtest.cpp` and
`rocev2_endpoint/run_astf.py`. It covers environment precedence, default-scale
construction, dual-stack counting, host/GID constraints, QPN/key reuse,
ambiguous identities, rollback, stale-result clearing and capture isolation.
The isolated AF_PACKET driver restricts ASTF to one worker per port pair;
these software tests do not qualify physical multiworker operation.

The 32-device by 16-QP default was selected for the intended 12-CPU/32-GB
baseline. Local software acceptance validates that scale on two port identities
with one dataplane worker, including 1024 completed operations and verified
payloads. Minimum-host, multiworker and physical line-rate performance remain
separate qualification work. See the implementation record for exact results.
