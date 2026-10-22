"""Configuration for native, memory-backed RC endpoint emulation.

Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0.
"""

import copy
import ipaddress
import math
import os
import re
import zlib

from ..common.trex_exceptions import TRexError


def _integer(value, name, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise TRexError('%s must be an integer in [%s, %s]' % (name, minimum, maximum))
    return value


def _private_data_hex(value, maximum):
    """Normalize bounded opaque CM bytes for the JSON RPC boundary."""
    if isinstance(value, bytes):
        if len(value) > maximum:
            raise TRexError('CM private_data exceeds %s bytes' % maximum)
        return value.hex()
    if isinstance(value, str):
        if len(value) > maximum * 2 or not re.fullmatch(r'(?:[0-9a-fA-F]{2})*', value):
            raise TRexError('CM private_data must be at most %s bytes of hexadecimal data' % maximum)
        return value.lower()
    raise TRexError('CM private_data must be bytes or a hexadecimal string')


class ASTFRoceV2Work(object):
    """One ordered SQ operation; offsets are relative to registered memory.

    A missing length uses the QP message size (eight bytes for atomics).
    Missing local/remote offsets use the legacy source/result buffer layout.
    An expected atomic original value is checked from the completion itself,
    independently of later operations reusing the result buffer.
    """

    OPERATIONS = ('write', 'write_imm', 'read', 'send', 'send_imm', 'send_inv',
                  'compare_swap', 'fetch_add')

    def __init__(self, operation, length=None, local_offset=None, remote_offset=None,
                 repeat=1, immediate_data=0, invalidate_rkey=0, compare=0,
                 swap_add=0, expected_original=None, fenced=False, expected_seed=None):
        if operation not in self.OPERATIONS:
            raise TRexError('Unsupported RoCEv2 work operation: %r' % operation)
        _integer(repeat, 'repeat', 1, 2**32 - 1)
        values = dict(operation=operation, repeat=repeat, immediate_data=immediate_data,
                      invalidate_rkey=invalidate_rkey, compare=compare,
                      swap_add=swap_add, fenced=fenced)
        for name, value, limit in (
                ('length', length, 16 * 1024 * 1024),
                ('local_offset', local_offset, 2**64 - 1),
                ('remote_offset', remote_offset, 2**64 - 1),
                ('expected_original', expected_original, 2**64 - 1),
                ('expected_seed', expected_seed, 2**32 - 1)):
            if value is not None:
                _integer(value, name, 0, limit)
                values[name] = value
        for name in ('immediate_data', 'invalidate_rkey'):
            _integer(values[name], name, 0, 2**32 - 1)
        for name in ('compare', 'swap_add'):
            _integer(values[name], name, 0, 2**64 - 1)
        if not isinstance(fenced, bool):
            raise TRexError('fenced must be a bool')
        if operation in ('compare_swap', 'fetch_add') and length not in (None, 8):
            raise TRexError('Atomic operations transfer exactly eight bytes')
        if operation in ('compare_swap', 'fetch_add'):
            values['length'] = 8
        if operation == 'send_inv' and not invalidate_rkey:
            raise TRexError('send_inv requires a nonzero invalidate_rkey')
        self._values = values

    def to_json(self):
        return copy.deepcopy(self._values)


class ASTFRoceV2Receive(object):
    """A posted receive descriptor with local address/length/lkey SGEs."""

    def __init__(self, wr_id, sges):
        _integer(wr_id, 'wr_id', 0, 2**64 - 1)
        if not isinstance(sges, (list, tuple)) or not 1 <= len(sges) <= 16:
            raise TRexError('A receive requires between 1 and 16 SGEs')
        entries = []
        for entry in sges:
            if not isinstance(entry, dict) or set(entry) != {'address', 'length', 'lkey'}:
                raise TRexError('Receive SGEs require address, length and lkey')
            entry = dict(entry)
            _integer(entry['address'], 'SGE address', 0, 2**64 - 1)
            _integer(entry['length'], 'SGE length', 0, 16 * 1024 * 1024)
            _integer(entry['lkey'], 'SGE lkey', 1, 2**32 - 1)
            if entry['address'] + entry['length'] > 2**64 - 1:
                raise TRexError('Receive SGE address range overflows')
            entries.append(entry)
        self._values = dict(wr_id=wr_id, sges=entries)

    def to_json(self):
        return copy.deepcopy(self._values)


class ASTFRoceV2Connection(object):
    """CM connection policy for one pre-provisioned endpoint QP.

    Active endpoints send a CM request; passive endpoints listen for the
    matching service. ``ip_cma`` defaults to whether either port is set; an
    active IP-CMA connection requires a source port. Private data accepts raw
    bytes or an even-length hexadecimal string, serialized as lowercase hex.
    Request limits are 92 raw or 56 IP-CMA bytes; passive reply data allows 196
    bytes. ``auto_accept=False`` exposes passive requests through the client's
    get_rocev2_events/accept_rocev2/reject_rocev2 APIs. Omitted QP connection
    policies retain manual setup; QPs and receives exist before establishment.
    """

    def __init__(self, mode, service_id, source_port=0, destination_port=0, backlog=1,
                 auto_accept=True, ip_cma=None, private_data=b''):
        if mode not in ('active', 'passive'):
            raise TRexError('CM connection mode must be active or passive')
        _integer(service_id, 'CM service_id', 1, 2**64 - 1)
        _integer(source_port, 'CM source_port', 0, 65535)
        _integer(destination_port, 'CM destination_port', 0, 65535)
        _integer(backlog, 'CM backlog', 1, 4096)
        if destination_port and service_id != ((0x0106 << 16) | destination_port):
            raise TRexError('IP CM destination_port must match service_id 0x0106:<port>')
        if not isinstance(auto_accept, bool):
            raise TRexError('CM auto_accept must be a bool')
        if ip_cma is None:
            ip_cma = bool(source_port or destination_port)
        elif not isinstance(ip_cma, bool):
            raise TRexError('CM ip_cma must be a bool or None')
        if mode == 'active' and ip_cma and not source_port:
            raise TRexError('An active IP-CMA connection requires a nonzero source_port')
        maximum = (56 if ip_cma else 92) if mode == 'active' else 196
        self._values = dict(mode=mode, service_id=service_id,
                            source_port=source_port, destination_port=destination_port,
                            backlog=backlog, auto_accept=auto_accept, ip_cma=ip_cma,
                            private_data=_private_data_hex(private_data, maximum))

    def to_json(self):
        return copy.deepcopy(self._values)


class ASTFRoceV2QP(object):
    """One QP and its finite or duration-limited workload.

    Source data occupies the first ``message_size`` bytes of registered memory.
    Write destinations and local Read results use the next ``message_size`` bytes.
    ``seed`` and ``peer_seed`` select deterministic payload patterns. Both peers
    must agree on message size, keys, addresses, and initial PSNs.
    """

    _defaults = dict(local_psn=0, remote_psn=0, local_address=0x100000,
                     remote_address=0x100000, local_rkey=1, remote_rkey=1,
                     message_size=4096, seed=1, peer_seed=1, operation='write',
                     iterations=1, outstanding=1, expect_writes=False, mtu=1024,
                     dscp=26, udp_sport=49152, timeout_exp=12, retry_count=3,
                     lkey=1, receive_depth=0,
                     receive_repost=False, expected_receives=0,
                     immediate_data=0, invalidate_rkey=0, atomic_compare=0,
                     atomic_swap_add=0)
    _required = ('port', 'local_qpn', 'remote_qpn', 'src_mac', 'dst_mac', 'src_ip', 'dst_ip')

    def __init__(self, port, local_qpn, remote_qpn, src_mac, dst_mac, src_ip, dst_ip, **options):
        values = dict(zip(self._required, (port, local_qpn, remote_qpn, src_mac,
                                          dst_mac, src_ip, dst_ip)))
        unknown = set(options) - set(self._defaults) - {
            'memory_size', 'vlan', 'memory_id', 'receives', 'program',
            'initial_u64', 'initial_u64_offset', 'expected_u64', 'expected_original',
            'connection', 'protection_domain', 'prefix_length', 'gateway', 'max_rd_atomic', 'max_dest_rd_atomic',
            'device_id', 'local_gid_index'}
        if unknown:
            raise TRexError('Unknown RoCEv2 QP options: %s' % ', '.join(sorted(unknown)))
        values.update(self._defaults)
        values.update(options)
        values.setdefault('memory_size', 2 * values['message_size'])
        self._values = self._validate(values)

    @staticmethod
    def _validate(values):
        bounds = dict(port=(0, 255), local_qpn=(2, 0xffffff), remote_qpn=(2, 0xffffff),
                      local_psn=(0, 0xffffff), remote_psn=(0, 0xffffff),
                      local_address=(0, 2**64 - 1), remote_address=(0, 2**64 - 1),
                      local_rkey=(1, 2**32 - 1), remote_rkey=(1, 2**32 - 1),
                      lkey=(1, 2**32 - 1), protection_domain=(1, 2**32 - 1),
                      max_rd_atomic=(0, 16), max_dest_rd_atomic=(0, 16),
                      message_size=(1, 16 * 1024 * 1024), memory_size=(2, 64 * 1024 * 1024),
                      seed=(0, 2**32 - 1), peer_seed=(0, 2**32 - 1),
                      iterations=(0, 2**32 - 1), outstanding=(1, 1024),
                      dscp=(0, 63), udp_sport=(1, 65535), timeout_exp=(0, 31),
                      retry_count=(0, 7), receive_depth=(0, 1024),
                      expected_receives=(0, 2**32 - 1),
                      immediate_data=(0, 2**32 - 1), invalidate_rkey=(0, 2**32 - 1),
                      atomic_compare=(0, 2**64 - 1), atomic_swap_add=(0, 2**64 - 1))
        bounds.update(device_id=(1, 2**32 - 1), local_gid_index=(0, 65535))
        for name, (minimum, maximum) in bounds.items():
            if name in values:
                _integer(values[name], name, minimum, maximum)
        if values['memory_size'] < 2 * values['message_size']:
            raise TRexError('memory_size must hold the source and destination message buffers')
        for name in ('local_address', 'remote_address'):
            if values[name] + values['memory_size'] > 2**64 - 1:
                raise TRexError('%s plus memory_size overflows the address space' % name)
        if values['operation'] not in ASTFRoceV2Work.OPERATIONS + ('none',):
            raise TRexError('Unsupported RoCEv2 QP operation')
        if 'program' not in values and values['operation'] in ('compare_swap', 'fetch_add'):
            if (values['remote_address'] + values['message_size']) % 8:
                raise TRexError('Remote atomic address must be eight-byte aligned')
            if values['message_size'] + 8 > values['memory_size']:
                raise TRexError('Atomic result requires eight bytes within the memory window')
        for name in ('expect_writes', 'receive_repost'):
            if not isinstance(values[name], bool):
                raise TRexError('%s must be a bool' % name)
        for name in ('initial_u64', 'expected_u64', 'expected_original'):
            if name in values:
                _integer(values[name], name, 0, 2**64 - 1)
        if 'initial_u64_offset' in values:
            _integer(values['initial_u64_offset'], 'initial_u64_offset', 0,
                     values['memory_size'] - 8)
        if any(name in values for name in ('initial_u64', 'expected_u64')):
            if values['memory_size'] < 8 or values.get('initial_u64_offset', values['message_size']) + 8 > values['memory_size']:
                raise TRexError('64-bit initial/expected value is outside local memory')
        if 'memory_id' in values:
            _integer(values['memory_id'], 'memory_id', 1, 2**32 - 1)
        if 'program' in values:
            if not isinstance(values['program'], (list, tuple)) or not 1 <= len(values['program']) <= 4096:
                raise TRexError('program must contain between 1 and 4096 work entries')
            program = []
            for work in values['program']:
                if isinstance(work, dict):
                    work = ASTFRoceV2Work(**work)
                if not isinstance(work, ASTFRoceV2Work):
                    raise TRexError('program entries must be RoCEv2 work requests')
                step = work.to_json()
                atomic = step['operation'] in ('compare_swap', 'fetch_add')
                length = step.get('length', 8 if atomic else values['message_size'])
                local_offset = step.get('local_offset', 0 if step['operation'].startswith(('send', 'write')) else values['message_size'])
                remote_offset = step.get('remote_offset', values['message_size'] if step['operation'].startswith('write') or atomic else 0)
                if local_offset + length > values['memory_size'] or remote_offset + length > values['memory_size']:
                    raise TRexError('Work request exceeds the configured memory window')
                if atomic and (values['remote_address'] + remote_offset) % 8:
                    raise TRexError('Remote atomic address must be eight-byte aligned')
                step.update(length=length, local_offset=local_offset, remote_offset=remote_offset)
                program.append(step)
            values['program'] = program
        if 'receives' in values:
            if not isinstance(values['receives'], (list, tuple)) or len(values['receives']) > 1024:
                raise TRexError('receives must be a list of at most 1024 descriptors')
            receives = []
            for receive in values['receives']:
                if isinstance(receive, dict):
                    receive = ASTFRoceV2Receive(**receive)
                if not isinstance(receive, ASTFRoceV2Receive):
                    raise TRexError('receives entries must be receive descriptors')
                descriptor = receive.to_json()
                for sge in descriptor['sges']:
                    offset = sge['address'] - values['local_address']
                    if (sge['lkey'] != values['lkey'] or offset < 0 or
                            offset > values['memory_size'] or
                            sge['length'] > values['memory_size'] - offset):
                        raise TRexError('Receive SGE must use the QP local memory window and lkey')
                receives.append(descriptor)
            ids = [entry['wr_id'] for entry in receives]
            if len(ids) != len(set(ids)):
                raise TRexError('Posted receive wr_id values must be unique')
            if set(ids).intersection(range(1, values['receive_depth'] + 1)):
                raise TRexError('Explicit receive wr_id conflicts with an automatic receive')
            if len(receives) + values['receive_depth'] > 1024:
                raise TRexError('Total explicit and automatic receive depth exceeds 1024')
            values['receives'] = receives
        if values['receive_depth'] * values['message_size'] + values['message_size'] > values['memory_size']:
            raise TRexError('receive_depth requires disjoint target buffers inside memory_size')
        if values['mtu'] not in (256, 512, 1024, 2048, 4096):
            raise TRexError('mtu must be an RDMA path MTU: 256, 512, 1024, 2048, or 4096')
        for name in ('src_mac', 'dst_mac'):
            if not isinstance(values[name], str) or not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', values[name]):
                raise TRexError('%s must be a colon-separated Ethernet address' % name)
        try:
            src = ipaddress.ip_address(values['src_ip'])
            dst = ipaddress.ip_address(values['dst_ip'])
        except (TypeError, ValueError) as exc:
            raise TRexError('Invalid RoCEv2 endpoint IP: %s' % exc)
        if src.version != dst.version:
            raise TRexError('RoCEv2 endpoint addresses must use the same IP version')
        values['src_ip'], values['dst_ip'] = str(src), str(dst)
        if 'prefix_length' in values:
            _integer(values['prefix_length'], 'prefix_length', 0, src.max_prefixlen)
        if 'gateway' in values:
            try:
                gateway = ipaddress.ip_address(values['gateway'])
            except (TypeError, ValueError) as exc:
                raise TRexError('Invalid RoCEv2 gateway: %s' % exc)
            if gateway.version != src.version:
                raise TRexError('gateway must match the endpoint IP family')
            values['gateway'] = str(gateway)
        if values['dst_mac'] == '00:00:00:00:00:00' and 'connection' not in values:
            raise TRexError('Dynamic neighbor resolution requires a managed CM connection')
        if 'vlan' in values:
            _integer(values['vlan'], 'vlan', 0, 4095)
        if 'connection' in values:
            connection = values['connection']
            if isinstance(connection, dict):
                try:
                    connection = ASTFRoceV2Connection(**connection)
                except TypeError as exc:
                    raise TRexError('Invalid CM connection fields: %s' % exc)
            if not isinstance(connection, ASTFRoceV2Connection):
                raise TRexError('connection must be an ASTFRoceV2Connection or dictionary')
            values['connection'] = connection.to_json()
        return values

    def to_json(self):
        return copy.deepcopy(self._values)


def _address(value, family, name):
    if not isinstance(value, str) or '%' in value:
        raise TRexError('%s must be an IPv%s address without a scope identifier' % (name, family))
    try:
        address = ipaddress.ip_address(value)
    except ValueError as exc:
        raise TRexError('Invalid %s: %s' % (name, exc))
    if address.version != family or address.is_unspecified or address.is_multicast:
        raise TRexError('%s must be a unicast IPv%s address' % (name, family))
    return str(address)


class ASTFRoceV2Host(object):
    """Static Ethernet/VLAN and IPv4/IPv6 identity owned by a native device.

    This copies address configuration; it does not start an EMU client, Linux
    interface, or a second ARP/ND responder.
    """

    def __init__(self, port, mac, ipv4=None, ipv6=None, ipv4_prefix=None,
                 ipv4_gateway=None, ipv6_prefix=None, ipv6_gateway=None, vlan=None):
        _integer(port, 'host port', 0, 255)
        if not isinstance(mac, str) or not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', mac):
            raise TRexError('Host mac must be a colon-separated Ethernet address')
        mac = mac.lower()
        if int(mac[:2], 16) & 1 or mac == '00:00:00:00:00:00':
            raise TRexError('Host mac must be a nonzero unicast Ethernet address')
        values = dict(port=port, mac=mac)
        for family, address, prefix, gateway in (
                (4, ipv4, ipv4_prefix, ipv4_gateway),
                (6, ipv6, ipv6_prefix, ipv6_gateway)):
            name = 'ipv%s' % family
            if address is None:
                if prefix is not None or gateway is not None:
                    raise TRexError('%s prefix/gateway requires its host address' % name)
                continue
            values[name] = _address(address, family, name)
            if prefix is not None:
                values[name + '_prefix'] = _integer(prefix, name + '_prefix', 0, 32 if family == 4 else 128)
            if gateway is not None:
                values[name + '_gateway'] = _address(gateway, family, name + '_gateway')
        if ipv4 is None and ipv6 is None:
            raise TRexError('A RoCEv2 host requires IPv4, IPv6, or both')
        if vlan is not None:
            values['vlan'] = _integer(vlan, 'host vlan', 0, 4094)
        self._values = values

    def to_json(self):
        return copy.deepcopy(self._values)

    @classmethod
    def from_emu_client(cls, client, namespace=None, port=None, **options):
        """Copy static addresses from EMUClientObj/JSON and EMUNamespaceKey.

        Only an untagged or one-802.1Q namespace is representable. Runtime EMU
        plugins are not enabled or copied; unsupported address/namespace fields
        fail explicitly instead of losing VLAN or address information.
        """
        if hasattr(client, 'to_json'):
            client = client.to_json()
        if not isinstance(client, dict) or set(client) - {'mac', 'ipv4', 'ipv4_dg', 'ipv6', 'dg_ipv6', 'plugs'}:
            raise TRexError('Unsupported EMU client shape for a static RoCEv2 host')
        if 'mac' not in client or ('plugs' in client and not isinstance(client['plugs'], dict)):
            raise TRexError('EMU client requires mac and an optional plugin dictionary')
        values = dict(options)
        if namespace is not None:
            if hasattr(namespace, 'conv_to_dict'):
                namespace = namespace.conv_to_dict()
            if isinstance(namespace, dict) and set(namespace) == {'tun'}:
                namespace = namespace['tun']
            if not isinstance(namespace, dict) or set(namespace) - {'vport', 'tci', 'tpid'}:
                raise TRexError('Unsupported EMU namespace shape')
            ns_port = namespace.get('vport')
            _integer(ns_port, 'EMU vport', 0, 255)
            if port is not None and port != ns_port:
                raise TRexError('Explicit port conflicts with the EMU namespace')
            port = ns_port
            tci, tpid = namespace.get('tci', [0, 0]), namespace.get('tpid', [0, 0])
            tci = [tci] if isinstance(tci, int) else tci
            tpid = [tpid] if isinstance(tpid, int) else tpid
            if (not isinstance(tci, (list, tuple)) or not 1 <= len(tci) <= 2 or
                    not isinstance(tpid, (list, tuple)) or not 1 <= len(tpid) <= 2):
                raise TRexError('Unsupported EMU VLAN shape')
            for tag in tci:
                _integer(tag, 'EMU VLAN TCI', 0, 4094)
            for protocol in tpid:
                _integer(protocol, 'EMU VLAN TPID', 0, 65535)
            if any(tci[1:]) or any(tpid[1:]) or tpid[0] not in (0, 0x8100):
                raise TRexError('QinQ and non-802.1Q EMU namespaces are unsupported')
            if not tci[0] and tpid[0]:
                raise TRexError('A priority-only EMU VLAN cannot be represented')
            if tci[0]:
                if 'vlan' in values and values['vlan'] != tci[0]:
                    raise TRexError('Explicit VLAN conflicts with the EMU namespace')
                values['vlan'] = tci[0]
            elif 'vlan' in values:
                raise TRexError('Explicit VLAN conflicts with an untagged EMU namespace')
        if port is None:
            raise TRexError('EMU conversion requires a namespace or explicit port')
        mac = client['mac']
        if isinstance(mac, (list, tuple)):
            if len(mac) != 6:
                raise TRexError('EMU mac must contain six bytes')
            mac = ':'.join('%02x' % _integer(byte, 'EMU mac byte', 0, 255) for byte in mac)
        for field, output, family in (('ipv4', 'ipv4', 4), ('ipv4_dg', 'ipv4_gateway', 4),
                                      ('ipv6', 'ipv6', 6), ('dg_ipv6', 'ipv6_gateway', 6)):
            if field not in client:
                continue
            address = client[field]
            if isinstance(address, (list, tuple)):
                if len(address) != (4 if family == 4 else 16):
                    raise TRexError('EMU %s has an invalid byte count' % field)
                address = str(ipaddress.ip_address(bytes(_integer(byte, 'EMU IP byte', 0, 255) for byte in address)))
            if not isinstance(address, str):
                raise TRexError('EMU %s must be a string or address bytes' % field)
            try:
                unspecified = ipaddress.ip_address(address).is_unspecified
            except ValueError as exc:
                raise TRexError('Invalid EMU address: %s' % exc)
            if not unspecified:
                if output in values and values[output] != address:
                    raise TRexError('Explicit address conflicts with the EMU client')
                values[output] = address
        return cls(port=port, mac=mac, **values)


class ASTFRoceV2Device(object):
    """One native device identity, GID table, and device-local QP namespace."""

    def __init__(self, device_id, host, qps=None, node_guid=None, gids=None):
        _integer(device_id, 'device_id', 1, 2**32 - 1)
        if isinstance(host, dict):
            host = ASTFRoceV2Host(**host)
        if not isinstance(host, ASTFRoceV2Host):
            raise TRexError('device host must be an ASTFRoceV2Host or constructor dictionary')
        identity = host.to_json()
        if node_guid is None:
            node_guid = 0x0200000000000000 | device_id
        _integer(node_guid, 'node_guid', 1, 2**64 - 1)
        if gids is None:
            gids = []
            for family in (4, 6):
                ip = identity.get('ipv%s' % family)
                if ip is not None:
                    gid = str(ipaddress.IPv6Address('::ffff:' + ip)) if family == 4 else ip
                    gids.append(dict(index=len(gids), ip=ip, gid=gid))
        if not isinstance(gids, (list, tuple)) or not 1 <= len(gids) <= 256:
            raise TRexError('A device GID table requires between 1 and 256 entries')
        entries, indices, addresses = [], set(), set()
        for entry in gids:
            if not isinstance(entry, dict) or set(entry) != {'index', 'ip', 'gid'}:
                raise TRexError('GID entries require index, ip and gid')
            index = _integer(entry['index'], 'GID index', 0, 65535)
            try:
                ip = ipaddress.ip_address(entry['ip'])
                gid = ipaddress.IPv6Address(entry['gid'])
            except (TypeError, ValueError) as exc:
                raise TRexError('Invalid GID entry: %s' % exc)
            expected = ipaddress.IPv6Address('::ffff:' + str(ip)) if ip.version == 4 else ip
            if (str(ip) != identity.get('ipv%s' % ip.version) or gid != expected or
                    index in indices or str(ip) in addresses):
                raise TRexError('GID indices/IPs must be unique and match the device host')
            indices.add(index); addresses.add(str(ip))
            entries.append(dict(index=index, ip=str(ip), gid=str(gid)))
        if addresses != {identity[key] for key in ('ipv4', 'ipv6') if key in identity}:
            raise TRexError('The GID table must cover every device host address')
        if qps is None:
            qps = []
        if not isinstance(qps, (list, tuple)) or len(qps) > 4096:
            raise TRexError('Device qps must be a list of at most 4096 QPs')
        self._host, self._gids = host, entries
        self._values = dict(device_id=device_id, port=identity.pop('port'), host=identity,
                            node_guid=node_guid, gids=entries)
        self._qps = [self._normalize_qp(qp) for qp in qps]

    def _normalize_qp(self, qp):
        if isinstance(qp, ASTFRoceV2QP):
            values = qp.to_json()
        elif isinstance(qp, dict):
            values = copy.deepcopy(qp)
        else:
            raise TRexError('Device QPs must be ASTFRoceV2QP objects or constructor dictionaries')
        host = self._host.to_json()
        if values.get('device_id', self._values['device_id']) != self._values['device_id']:
            raise TRexError('QP device_id conflicts with its containing device')
        if 'local_gid_index' in values:
            gid = next((entry for entry in self._gids if entry['index'] == values['local_gid_index']), None)
        elif 'src_ip' in values:
            try:
                source = str(ipaddress.ip_address(values['src_ip']))
            except (TypeError, ValueError) as exc:
                raise TRexError('Invalid device QP source address: %s' % exc)
            gid = next((entry for entry in self._gids if entry['ip'] == source), None)
        else:
            gid = self._gids[0]
        if gid is None:
            raise TRexError('QP source address/local_gid_index is absent from its device GID table')
        values.setdefault('port', host['port'])
        values.setdefault('src_mac', host['mac'])
        values.setdefault('src_ip', gid['ip'])
        if 'vlan' in host:
            values.setdefault('vlan', host['vlan'])
        family = ipaddress.ip_address(gid['ip']).version
        if 'ipv%s_prefix' % family in host:
            values.setdefault('prefix_length', host['ipv%s_prefix' % family])
        if 'ipv%s_gateway' % family in host:
            values.setdefault('gateway', host['ipv%s_gateway' % family])
        values.update(device_id=self._values['device_id'], local_gid_index=gid['index'])
        normalized = ASTFRoceV2QP(**values).to_json()
        if (normalized['port'] != host['port'] or normalized['src_mac'].lower() != host['mac'] or
                normalized['src_ip'] != gid['ip'] or normalized.get('vlan') != host.get('vlan')):
            raise TRexError('QP port/MAC/IP/VLAN must match its device host and selected GID')
        prefix = host.get('ipv%s_prefix' % family, 32 if family == 4 else 128)
        if normalized.get('prefix_length', prefix) != prefix or normalized.get('gateway') != host.get('ipv%s_gateway' % family):
            raise TRexError('QP prefix/gateway conflicts with its device host configuration')
        normalized['src_mac'] = host['mac']
        return normalized

    def to_json(self):
        result = copy.deepcopy(self._values)
        result['qps'] = copy.deepcopy(self._qps)
        return result

    def capture_filter(self, direction='both'):
        """BPF for existing start_capture(bpf_filter=...) APIs."""
        if direction not in ('both', 'rx', 'tx'):
            raise TRexError('Capture direction must be both, rx or tx')
        host = self._host.to_json()
        terms = []
        for packet_direction in (('src', 'dst') if direction == 'both' else
                                 ('src',) if direction == 'tx' else ('dst',)):
            ips = ' or '.join('%s host %s' % (packet_direction, entry['ip']) for entry in self._gids)
            terms.append('(ether %s %s and (%s))' % (packet_direction, host['mac'], ips))
        result = '(' + ' or '.join(terms) + ')'
        return '(vlan %s and %s)' % (host['vlan'], result) if 'vlan' in host else result


class ASTFRoceV2Scale(object):
    """Explicit scale overrides win over TREX_ROCEV2_* environment defaults.

    Device/QP counts do not change per-QP outstanding or receive depths. Values
    are resolved once and serialized; workers never consult client environment.
    """
    def __init__(self, devices_per_port=None, qps_per_device=None, environ=None):
        environ = os.environ if environ is None else environ
        self._values = {}
        for name, explicit, default in (('devices_per_port', devices_per_port, 32),
                                         ('qps_per_device', qps_per_device, 16)):
            value = explicit
            if value is None:
                variable = 'TREX_ROCEV2_' + name.upper()
                raw = environ.get(variable)
                if variable not in environ:
                    value = default
                elif isinstance(raw, str) and re.fullmatch(r'[0-9]+', raw):
                    value = int(raw)
                else:
                    raise TRexError('TREX_ROCEV2_%s must contain a decimal integer' % name.upper())
            self._values[name] = _integer(value, name, 1, 4096)
        if self._values['devices_per_port'] * self._values['qps_per_device'] > 4096:
            raise TRexError('Requested devices/QPs exceed the session 4096-QP bound')

    def to_json(self):
        return dict(self._values)

    def paired_profile(self, ports=(0, 1), ip_version='ipv4', message_size=4096,
                       outstanding=1, receive_depth=0, generation=1, duration=10.0,
                       max_memory_bytes=None, max_native_storage_bytes=512 * 1024 * 1024,
                       ingress_queue_size=1024, vlan=None, **qp_options):
        """Build reciprocal devices on physical-port pairs, without trimming.

        Each device receives the requested total QP count; dual stack alternates
        those QPs across its IPv4/IPv6 GIDs. QPN/MR/PD/key values may repeat in
        different devices. Every QP owns private memory and its own PD.
        """
        if (not isinstance(ports, (list, tuple)) or not ports or len(ports) % 2 or
                len(ports) != len(set(ports))):
            raise TRexError('ports must contain unique physical-port pairs')
        for port in ports:
            _integer(port, 'scale port', 0, 255)
        if ip_version not in ('ipv4', 'ipv6', 'dual'):
            raise TRexError('ip_version must be ipv4, ipv6 or dual')
        count, qps_per_device = self._values['devices_per_port'], self._values['qps_per_device']
        if len(ports) * count * qps_per_device > 4096:
            raise TRexError('Requested scale exceeds the total 4096-QP session limit')
        _integer(message_size, 'message_size', 1, 16 * 1024 * 1024)
        _integer(outstanding, 'outstanding', 1, 1024)
        _integer(receive_depth, 'receive_depth', 0, 1024)
        reserved = set(qp_options).intersection({'port', 'src_mac', 'src_ip', 'dst_mac', 'dst_ip',
                                                'local_qpn', 'remote_qpn', 'device_id',
                                                'local_gid_index', 'memory_id', 'protection_domain',
                                                'message_size', 'outstanding', 'receive_depth', 'connection'})
        if reserved:
            raise TRexError('Scale identity/workload fields cannot be overridden: %s' % ', '.join(sorted(reserved)))
        hosts = {}
        for port in ports:
            for number in range(count):
                mac = 0x020000000000 | (port << 24) | (number + 1)
                values = dict(port=port, mac=':'.join('%02x' % ((mac >> shift) & 255)
                                                     for shift in range(40, -1, -8)))
                if vlan is not None:
                    values['vlan'] = vlan
                if ip_version in ('ipv4', 'dual'):
                    values.update(ipv4=str(ipaddress.IPv4Address(0x0a000001 + (port << 16) + number)), ipv4_prefix=16)
                if ip_version in ('ipv6', 'dual'):
                    values.update(ipv6=str(ipaddress.IPv6Address(int(ipaddress.IPv6Address('2001:db8::1')) +
                                                                (port << 64) + number)), ipv6_prefix=64)
                hosts[(port, number)] = ASTFRoceV2Host(**values)
        devices = []
        for port_index, port in enumerate(ports):
            peer_port = ports[port_index ^ 1]
            for number in range(count):
                host, peer = hosts[(port, number)], hosts[(peer_port, number)].to_json()
                identity = host.to_json()
                device_id = port * 4096 + number + 1
                qps = []
                for index in range(qps_per_device):
                    family = 6 if ip_version == 'ipv6' or (ip_version == 'dual' and index % 2) else 4
                    values = dict(qp_options)
                    values.setdefault('memory_size', max(2, receive_depth + 1) * message_size)
                    values.setdefault('peer_seed', values.get('seed', 1))
                    if 'program' in values and isinstance(values['program'], (list, tuple)):
                        operations = [(work.to_json() if isinstance(work, ASTFRoceV2Work) else work).get('operation')
                                      for work in values['program'] if isinstance(work, (ASTFRoceV2Work, dict))]
                    else:
                        operations = [values.get('operation', 'write')]
                    values.setdefault('expect_writes', any(isinstance(operation, str) and operation.startswith('write')
                                                           for operation in operations))
                    entropy = zlib.crc32(('%s:%s:%s' % (port, device_id, index)).encode('ascii'))
                    values.setdefault('udp_sport', 49152 + entropy % 16384)
                    values.update(port=port, local_qpn=index + 2, remote_qpn=index + 2,
                                  src_mac=identity['mac'], dst_mac=peer['mac'],
                                  src_ip=identity['ipv%s' % family], dst_ip=peer['ipv%s' % family],
                                  memory_id=index + 1, protection_domain=index + 1,
                                  message_size=message_size, outstanding=outstanding,
                                  receive_depth=receive_depth)
                    qps.append(values)
                devices.append(ASTFRoceV2Device(device_id, host, qps=qps))
        return ASTFRoceV2Profile(devices=devices, scale=self, generation=generation, duration=duration,
                                max_memory_bytes=max_memory_bytes,
                                max_native_storage_bytes=max_native_storage_bytes,
                                ingress_queue_size=ingress_queue_size)


class ASTFRoceV2Profile(object):
    """A bounded endpoint session reserved exclusively in an ASTF context.

    Explicit flat QPs retain schema 2. Device profiles use schema 3 and scoped
    QPN/PD/MR identities; native admission remains authoritative for storage.
    """

    def __init__(self, qps=None, generation=1, duration=10.0, max_memory_bytes=None,
                 ingress_queue_size=1024, devices=None, scale=None,
                 max_native_storage_bytes=512 * 1024 * 1024):
        device_mode = devices is not None
        if max_memory_bytes is None:
            max_memory_bytes = (128 if device_mode else 64) * 1024 * 1024
        _integer(generation, 'generation', 1, 2**64 - 1)
        _integer(max_memory_bytes, 'max_memory_bytes', 1, 256 * 1024 * 1024)
        _integer(max_native_storage_bytes, 'max_native_storage_bytes', 1, 16 * 1024**3)
        _integer(ingress_queue_size, 'ingress_queue_size', 1, 65536)
        if isinstance(duration, bool) or not isinstance(duration, (int, float)) or not math.isfinite(duration) or not 0 < duration <= 3600:
            raise TRexError('duration must be finite and in (0, 3600] seconds')
        metadata, device_by_id = [], {}
        if device_mode:
            if not isinstance(devices, (list, tuple)) or not 1 <= len(devices) <= 4096:
                raise TRexError('devices must contain between 1 and 4096 devices')
            embedded, hosts, guids = [], set(), set()
            for device in devices:
                if isinstance(device, dict):
                    values = copy.deepcopy(device)
                    if 'port' in values:
                        host = dict(values['host'])
                        port = values.pop('port')
                        if 'port' in host and host['port'] != port:
                            raise TRexError('Device port conflicts with its host port')
                        host.setdefault('port', port)
                        values['host'] = host
                    device = ASTFRoceV2Device(**values)
                if not isinstance(device, ASTFRoceV2Device):
                    raise TRexError('devices entries must be RoCEv2 devices or constructor dictionaries')
                entry = device.to_json()
                device_id = entry['device_id']
                if device_id in device_by_id or entry['node_guid'] in guids:
                    raise TRexError('Device IDs and node GUIDs must be globally unique')
                device_by_id[device_id] = device
                guids.add(entry['node_guid'])
                for gid in entry['gids']:
                    owned = (entry['port'], entry['host'].get('vlan'), gid['ip'])
                    if owned in hosts:
                        raise TRexError('Multiple devices cannot own the same port/VLAN/IP address')
                    hosts.add(owned)
                embedded.extend(entry.pop('qps'))
                metadata.append(entry)
            if qps is None:
                qps = embedded
            elif embedded:
                raise TRexError('Use embedded device QPs or flattened qps, not both')
        elif scale is not None:
            raise TRexError('Scale metadata requires a device profile')
        if not isinstance(qps, (list, tuple)) or not 1 <= len(qps) <= 4096:
            raise TRexError('qps must contain between 1 and 4096 queue pairs')
        entries = []
        identities = set()
        for qp in qps:
            if device_mode:
                values = qp.to_json() if isinstance(qp, ASTFRoceV2QP) else qp
                if not isinstance(values, dict) or values.get('device_id') not in device_by_id:
                    raise TRexError('Every device-profile QP requires a known device_id')
                entry = device_by_id[values['device_id']]._normalize_qp(values)
                identity = (entry['device_id'], entry['local_qpn'])
            elif isinstance(qp, dict):
                qp = ASTFRoceV2QP(**qp)
            if not device_mode:
                if not isinstance(qp, ASTFRoceV2QP):
                    raise TRexError('qps entries must be ASTFRoceV2QP objects or dictionaries')
                entry = qp.to_json()
                if 'device_id' in entry or 'local_gid_index' in entry:
                    raise TRexError('Device/GID tags require a device profile')
                identity = (entry['port'], entry['local_qpn'])
            if identity in identities:
                raise TRexError('Duplicate scoped local_qpn: %r' % (identity,))
            identities.add(identity)
            entries.append(entry)
        if device_mode and {entry['device_id'] for entry in entries} != set(device_by_id):
            raise TRexError('Every declared device requires at least one QP')
        regions = {}
        for index, entry in enumerate(entries):
            memory_id = entry.get('memory_id', index + 1)
            scope = (entry['device_id'], memory_id) if device_mode else memory_id
            region = tuple(entry.get(name) for name in ('port', 'local_address', 'memory_size',
                'local_rkey', 'lkey', 'seed', 'initial_u64', 'initial_u64_offset')) + (entry.get('protection_domain', memory_id),)
            if scope in regions and regions[scope] != region:
                raise TRexError('Shared memory_id requires identical local region configuration')
            regions[scope] = region
        if sum(region[2] for region in regions.values()) > max_memory_bytes:
            raise TRexError('QP memory exceeds the session max_memory_bytes budget')
        self._values = dict(schema_version=3 if device_mode else 2, generation=generation,
                            duration=float(duration), qps=entries, max_memory_bytes=max_memory_bytes,
                            max_native_storage_bytes=max_native_storage_bytes,
                            ingress_queue_size=ingress_queue_size)
        if device_mode:
            self._values['devices'] = metadata
        if scale is not None:
            if isinstance(scale, dict):
                scale = ASTFRoceV2Scale(environ={}, **scale)
            if not isinstance(scale, ASTFRoceV2Scale):
                raise TRexError('scale must be ASTFRoceV2Scale or its constructor dictionary')
            self._values['scale'] = scale.to_json()

    def to_json(self):
        return copy.deepcopy(self._values)
