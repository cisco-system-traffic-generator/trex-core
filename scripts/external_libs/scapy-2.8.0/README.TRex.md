# TRex Scapy integration

This directory vendors Scapy **2.8.0** for the TRex client, simulator, packet
builder, Scapy RPC service, and release client package. Scapy-dependent Python
entry points require **Python 3.7 or newer**. Python 3.12 is used for the local
upgrade validation; this change does not migrate TRex's other Python tooling.

## Upstream source and license

- Release: <https://pypi.org/project/scapy/2.8.0/>
- Source: <https://github.com/secdev/scapy/releases/tag/v2.8.0>
- Distribution: `scapy-2.8.0.tar.gz` from the PyPI release.
- SHA-256: `0eacd1f940d115611c1b8a4696f315b4b80caf2951e546893a3d0cd7f6bfd6fd`.
- Python requirement from release metadata: `>=3.7,<4`.

The `scapy/` runtime tree and upstream `LICENSE`, `PKG-INFO`, `README.md`,
`pyproject.toml`, `setup.py`, and `setup.cfg` are retained. Python and metadata
line endings are normalized to LF, and the generated `setup.cfg` whitespace is
normalized. The upstream documentation, test tree, and generated egg-info tree
are not part of this runtime vendor directory. Scapy's GPL-2.0-only license and
individual file notices remain in place.

## Local compatibility changes

The previous TRex Scapy 2.4.3 tree was compared against its upstream release.
The Scapy 2.7 port in the reference RoCEv2 checkout was inspected at
`a93e9539d99cad582fc58a343b3a70d1b2cb03d6`; its cache fix was inspected at
`d3d4ed016b7c9edd3e5ea79884c14ba43a6b9c05`. Changes here are applied to the 2.8
source rather than copying the 2.7 implementation wholesale.

### Packet and field metadata

- `fields.py` retains `_offset`, `get_size_bytes()`, and `get_size_bits()`,
  including field containers. Bit fields retain a zero byte size and their
  actual bit size. Legacy six-character binary MAC strings remain supported.
- `packet.py` retains `_offset`, `_length`, `offset_fields`, the offset dump
  helpers, and the optional `CPacketRes` build result. Offsets survive packet
  construction, dissection, copying, cloning, and cache reuse. Per-packet field
  offsets restore metadata after another packet uses the shared field objects.
  Final IPv4/TCP option padding is included in payload offsets.
- Payload build calls retain the upstream no-argument interface, including
  subclasses that override `do_build()`.

### Deterministic templates and legacy helpers

- `base_classes.py` avoids DNS lookup for packet-template addresses.
- `layers/l2.py`, `layers/inet.py`, and `layers/inet6.py` retain the fixed TRex
  Ethernet, IPv4, and IPv6 defaults without address/neighbor/interface lookup.
- `data.py` keeps service ports numeric instead of reading host service names.
- `utils.py` retains `str2bytes`, `str2int`, `int2str`, and `str2ip`. Legacy binary
  strings use one byte per character rather than UTF-8 encoding.
- Upstream route, platform, ASN.1, and volatile-value implementations are kept.
  The old global route disabling, Python 2 compatibility changes, optional
  graphics workaround, and old version-identification substitution are not
  copied into the modern source. Template lookup behavior is tested directly.

### Protocol extensions

- Existing `contrib/ipfix.py`, its Cisco/IETF CSV dictionaries, CAPWAP modules
  and encrypted vendor payload, ANQP, and `layers/iapp.py` are retained.
- ANQP length fields and IAPP imports use symbols available in modern Scapy.
  `fields.py` retains `UTF8LenField` and `VarLenIntField`; `layers/netflow.py`
  retains the `Raw` export used by the IPFIX extension.
- `layers/inet6.py` retains the `ICMPv6MLReportV2`, `MLDv2Addr`, and
  `mldv2_group_types` names used by TRex's IPv6 service. Modern upstream MLDv2
  dissectors and bindings are retained.
- The legacy `modules/ethertypes.py` and `modules/winpcapy.py` import locations
  remain available.
- The separate stateless RoCEv2 integration adds `RETH` and RC write/read
  bindings in `contrib/roce.py`, based on reference commit
  `69d567910dcaf8a5f9fdc68475ad3f7257a6f837`. BTH and the upstream Scapy ICRC
  implementation are retained.
- CAPWAP's existing raw fallback remains available when the optional private
  key for encrypted vendor-specific payloads is absent. No private key is
  included in this directory.

### Optional data cache

`data.py` treats cache stat/read/removal/write failures as nonfatal and loads
the source data when caching is unavailable, including a daemon retaining an
inaccessible parent cache directory. Scapy 2.8's `_CacheUnpickler` and its class
allowlist are retained. A failed or forbidden unpickle falls back to source
data; the cache fix does not reintroduce unrestricted `pickle.load()`.

## TRex adapters outside this directory

The client import paths and release client manifest select this directory.
The wireless loader resolves both the checkout and standalone client package,
and honors its documented `TREX_EXT_PATH` override.
TRex PCAP consumers use the iterator and public `write_packet()` interfaces;
capture timestamps and truncated-packet wire lengths are preserved. Scapy RPC
protocol lookup handles classes not exported in the service's global namespace,
and template responses remain JSON-serializable base64 strings.

Profile export keeps an exact Ethernet byte literal if reconstructing Scapy's
dissected fields would change the bytes. This covers arbitrary traffic payloads
that newer dissectors recognize as a protocol without changing profile or
packet-comparison expectations.

## Validation

The integration regression tests can run independently of the legacy Nose runner:

```bash
PYTHONPATH=scripts/automation/trex_control_plane/interactive \
python3.12 -m unittest discover \
  -s scripts/automation/regression/functional_tests \
  -p scapy_upgrade_test.py
```

They cover offsets and field sizes, copying and cache reuse, deterministic wire
bytes, binary conversion helpers, custom protocols and MLDv2, PCAP profile and
capture output, profile export, inaccessible/corrupt caches, and the restricted
unpickler. Also run the complete functional suite and Scapy service tests before
submitting a change. The bundled Nose 1.3.4 runner has pre-existing dependencies
on removed Python APIs; local Python 3.12 validation uses a bootstrap for those
APIs without changing Nose or the project's runtime policy.

Release validation should stage/extract the client package using the release
dependency manifest and check that fresh STL/ASTF imports resolve Scapy from the
extracted 2.8 directory, including its custom modules and data files.
