#!/usr/bin/env python3
"""Exercise the actual ASTF RPC/worker path with DPDK AF_PACKET ports.

The server and client run in a fresh user/network namespace containing one
veth pair. This runner never acquires lab ports. It uses an already-built
TRex binary; it does not build, install or publish anything. Evidence includes
worker/memory verdicts, exact PMD counters and directional packet captures.
This is functional dataplane acceptance, not a line-rate or interoperability test.

Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0.
"""

import argparse
from collections import Counter
import ipaddress
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import zlib


ROOT = Path(__file__).resolve().parents[4]
STATIC_SCENARIOS = ('read-write', 'write-immediate', 'send-immediate', 'send-invalidate', 'atomics')
DEVICE_SCENARIOS = ('devices-read-write', 'devices-cm', 'devices-dual-stack',
                    'devices-scale-configure', 'devices-scale-traffic', 'devices-host-neighbors')
SCENARIOS = STATIC_SCENARIOS + ('cm-read-write', 'cm-manual-accept', 'cm-manual-reject') + DEVICE_SCENARIOS
INTERFACES = ('rvastf0', 'rvastf1')
MACS = ('02:00:00:00:00:0a', '02:00:00:00:00:14')


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def namespace_supported():
    if not shutil.which('unshare') or not shutil.which('ip'):
        return False
    return subprocess.run(['unshare', '--user', '--map-root-user', '--net',
                           'ip', 'link', 'add', 'rvasprobe0', 'type', 'veth',
                           'peer', 'name', 'rvasprobe1'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0


def setup_veth():
    subprocess.run(['ip', 'link', 'add', INTERFACES[0], 'type', 'veth',
                    'peer', 'name', INTERFACES[1]], check=True)
    for interface, mac in zip(INTERFACES, MACS):
        # Avoid kernel-generated IPv6 link-local traffic affecting PMD counts;
        # IPv6 test frames are still delivered as raw Ethernet by the PMD.
        subprocess.run(['ip', 'link', 'set', 'dev', interface,
                        'addrgenmode', 'none'], check=True)
        subprocess.run(['ip', 'link', 'set', 'dev', interface,
                        'address', mac, 'mtu', '1500', 'up'], check=True)
    subprocess.run(['ip', 'link', 'set', 'lo', 'up'], check=True)


def platform_config(directory):
    cores = sorted(os.sched_getaffinity(0))
    check(len(cores) >= 3, 'ASTF acceptance requires three available logical CPUs')
    node_paths = sorted(Path('/sys/devices/system/cpu/cpu%s' % cores[0]).glob('node[0-9]*'))
    socket = int(node_paths[0].name[4:]) if node_paths else 0
    same_node = [core for core in cores if
                 Path('/sys/devices/system/cpu/cpu%s/node%s' % (core, socket)).exists()]
    if len(same_node) >= 3:
        cores = same_node
    config = directory / 'trex_cfg.yaml'
    lines = ['- version: 2', '  port_limit: 2',
             '  interfaces:',
             '    - "--vdev=net_af_packet0,iface=rvastf0,blocksz=4096,framesz=2048,framecnt=8"',
             '    - "--vdev=net_af_packet1,iface=rvastf1,blocksz=4096,framesz=2048,framecnt=8"',
             '  port_bandwidth_gb: 1', '  limit_memory: 256',
             '  zmq_rpc_port: 14501', '  zmq_pub_port: 14500',
             '  enable_zmq_pub: true', '  port_info:',
             '    - src_mac: "%s"' % MACS[0], '      dest_mac: "%s"' % MACS[1],
             '    - src_mac: "%s"' % MACS[1], '      dest_mac: "%s"' % MACS[0],
             '  platform:', '    master_thread_id: %s' % cores[0],
             '    latency_thread_id: %s' % cores[1], '    dual_if:',
             '      - socket: %s' % socket, '        threads: [%s]' % cores[2],
             '  memory:']
    for kind in ('mbuf', 'traffic_mbuf'):
        for size in ('64', '128', '256', '512', '1024', '2048', '4096', '9k'):
            lines.append('    %s_%s: 4096' % (kind, size))
    lines += ['    dp_flows: 4096', '    global_flows: 4096']
    config.write_text('\n'.join(lines) + '\n')
    return config


def api():
    sys.path.insert(0, str(ROOT / 'scripts/automation/trex_control_plane/interactive'))
    from trex.astf.api import (ASTFClient, ASTFRoceV2Connection, ASTFRoceV2Profile,
                               ASTFRoceV2QP, ASTFRoceV2Work)
    return ASTFClient, ASTFRoceV2Connection, ASTFRoceV2Profile, ASTFRoceV2QP, ASTFRoceV2Work


def connect(server, timeout):
    ASTFClient = api()[0]
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError('TRex exited before its RPC endpoint became ready')
        client = ASTFClient(server='127.0.0.1', sync_port=14501, async_port=14500,
                            sync_timeout=1, async_timeout=1, verbose_level='none')
        try:
            client.connect()
            client.acquire(force=False)
            return client
        except Exception as exc:
            last = exc
            try:
                client.disconnect()
            except Exception:
                pass
            if 'Version mismatch' in str(exc):
                raise RuntimeError('Use the freshly built ASTF endpoint server: %s' % exc)
            time.sleep(0.2)
    raise RuntimeError('TRex RPC readiness timed out: %s' % last)


def make_profile(scenario, ipv6, generation):
    _, Connection, Profile, QP, Work = api()
    ips = ('2001:db8::a', '2001:db8::14') if ipv6 else ('192.0.2.10', '192.0.2.20')
    size = 16 if scenario == 'atomics' else 773
    common = dict(message_size=size, mtu=256, dscp=26, outstanding=1, operation='none',
                  timeout_exp=20, retry_count=3, memory_size=2 * size)
    left = dict(common, port=0, local_qpn=2, remote_qpn=3, src_mac=MACS[0], dst_mac=MACS[1],
                src_ip=ips[0], dst_ip=ips[1], seed=11, peer_seed=22,
                local_rkey=1, remote_rkey=2)
    right = dict(common, port=1, local_qpn=3, remote_qpn=2, src_mac=MACS[1], dst_mac=MACS[0],
                 src_ip=ips[1], dst_ip=ips[0], seed=22, peer_seed=11,
                 local_rkey=2, remote_rkey=1)
    expected = dict(left_completed=1, right_received=0, left_opcodes=[], right_opcodes=[])
    if scenario in ('read-write', 'cm-read-write', 'cm-manual-accept', 'cm-manual-reject'):
        left['program'] = [Work('write'), Work('read', expected_seed=22)]
        right['expect_writes'] = True
        expected.update(left_completed=2, left_opcodes=[6, 7, 7, 8, 12],
                        right_opcodes=[17, 13, 14, 14, 15])
        if scenario.startswith('cm-'):
            service = 0x01061234
            left['connection'] = Connection('active', service, source_port=50000,
                                             destination_port=4660, private_data=b'request\x00\x80')
            right['connection'] = Connection('passive', service, destination_port=4660,
                                              auto_accept=scenario == 'cm-read-write')
            expected['cm_attributes'] = [0x10, 0x13, 0x14, 0x15, 0x15, 0x16, 0x16]
            if scenario == 'cm-manual-accept':
                expected['cm_attributes'].append(0x11)  # pending-request MRA
            elif scenario == 'cm-manual-reject':
                right['expect_writes'] = False
                expected.update(rejected=True, left_completed=0, left_opcodes=[], right_opcodes=[],
                                cm_attributes=[0x10, 0x11, 0x12])
    elif scenario == 'send-immediate':
        left['program'] = [Work('send_imm', immediate_data=0xaabbccdd)]
        right.update(receive_depth=1, receive_repost=True, expected_receives=1)
        expected.update(right_received=1, left_opcodes=[0, 1, 1, 3], right_opcodes=[17],
                        last_immediate=0xaabbccdd)
    elif scenario == 'write-immediate':
        left['program'] = [Work('write_imm', immediate_data=0x12345678)]
        right.update(receive_depth=1, receive_repost=True, expected_receives=1, expect_writes=True)
        expected.update(right_received=1, left_opcodes=[6, 7, 7, 9], right_opcodes=[17],
                        last_immediate=0x12345678)
    elif scenario == 'send-invalidate':
        left['program'] = [Work('send_inv', invalidate_rkey=2)]
        right.update(receive_depth=1, receive_repost=True, expected_receives=1)
        expected.update(right_received=1, left_opcodes=[0, 1, 1, 0x16], right_opcodes=[17],
                        last_invalidated_rkey=2)
    elif scenario == 'atomics':
        left['program'] = [Work('compare_swap', compare=5, swap_add=9, expected_original=5),
                           Work('fetch_add', swap_add=3, expected_original=9)]
        right.update(initial_u64=5, expected_u64=12)
        expected.update(left_completed=2, left_opcodes=[0x13, 0x14], right_opcodes=[0x12, 0x12])
    return Profile([QP(**left), QP(**right)], generation=generation, duration=10,
                   max_memory_bytes=1024 * 1024), expected


def internet_checksum(binary):
    if len(binary) & 1:
        binary += b'\x00'
    total = sum(struct.unpack('!%sH' % (len(binary) // 2), binary))
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff


def canonical_packet(binary):
    """Trim Ethernet minimum-frame padding; preserve the actual protocol bytes."""
    wire = bytes(binary)
    check(len(wire) >= 34, 'capture contains a truncated Ethernet/IP packet')
    ipv6 = wire[14] >> 4 == 6
    l3 = 40 if ipv6 else (wire[14] & 15) * 4
    length = 14 + 40 + struct.unpack('!H', wire[18:20])[0] if ipv6 else \
             14 + struct.unpack('!H', wire[16:18])[0]
    check(length <= len(wire), 'capture truncates the IP payload')
    wire = wire[:length]
    udp = 14 + l3
    check(struct.unpack('!H', wire[udp + 2:udp + 4])[0] == 4791, 'unexpected captured UDP port')
    udp_length = len(wire) - udp
    check(struct.unpack('!H', wire[udp + 4:udp + 6])[0] == udp_length,
          'captured UDP length disagrees with IP length')
    if ipv6:
        pseudo = wire[22:54] + struct.pack('!I', udp_length) + b'\x00\x00\x00\x11'
        check(wire[udp + 6:udp + 8] != b'\x00\x00', 'IPv6 RoCEv2 UDP checksum is missing')
    else:
        check(internet_checksum(wire[14:udp]) == 0, 'captured IPv4 header checksum is invalid')
        pseudo = wire[26:34] + b'\x00\x11' + struct.pack('!H', udp_length)
    if ipv6 or wire[udp + 6:udp + 8] != b'\x00\x00':
        check(internet_checksum(pseudo + wire[udp:]) == 0, 'captured UDP checksum is invalid')
    bth = udp + 8
    invariant = bytearray(wire[14:-4])
    if ipv6:
        invariant[0:4] = b'\x6f\xff\xff\xff'
        invariant[7] = 0xff
    else:
        for index in (1, 8, 10, 11):
            invariant[index] = 0xff
    invariant[l3 + 6:l3 + 8] = b'\xff\xff'
    invariant[l3 + 8 + 4] = 0xff
    expected_icrc = struct.pack('<I', zlib.crc32(b'\xff' * 8 + invariant) & 0xffffffff)
    check(wire[-4:] == expected_icrc, 'captured packet has incorrect invariant CRC')
    dscp = ((wire[14] & 15) << 2 | wire[15] >> 6) if ipv6 else wire[15] >> 2
    check(dscp == 26, 'captured RoCEv2 packet has the wrong DSCP')
    return wire, wire[bth], bth


def validate_capture(packets, expected, directory, name):
    from scapy.layers.l2 import Ether
    from scapy.utils import wrpcap
    groups = {(port, origin): [] for port in (0, 1) for origin in ('TX', 'RX')}
    opcodes = {0: [], 1: []}
    cm_messages = []
    for packet in packets:
        key = (packet['port'], packet['origin'])
        check(key in groups, 'capture contains an unexpected port or direction')
        wire, opcode, bth = canonical_packet(packet['binary'])
        groups[key].append(wire)
        if packet['origin'] == 'TX':
            if opcode == 0x64:
                # QP1 UD DETH is eight bytes; MAD attribute ID identifies CM.
                mad = bth + 12 + 8
                cm_messages.append(struct.unpack('!H', wire[mad + 16:mad + 18])[0])
            else:
                opcodes[packet['port']].append(opcode)
    for port, label in ((0, 'left_opcodes'), (1, 'right_opcodes')):
        check(opcodes[port] == expected[label],
              'port %s TX opcode sequence %r != %r' % (port, opcodes[port], expected[label]))
        check(Counter(groups[(port, 'TX')]) == Counter(groups[(1 - port, 'RX')]),
              'port %s accepted TX packets do not exactly match peer RX' % port)
    if name.startswith('cm-'):
        check(Counter(cm_messages) == Counter(expected['cm_attributes']),
              'CM capture handshake/disconnect attributes disagree: %r' % cm_messages)
    else:
        check(not cm_messages, 'manual QP scenario unexpectedly generated CM packets')
    for (port, origin), frames in groups.items():
        wrpcap(str(directory / ('%s-port%s-%s.pcap' % (name, port, origin.lower()))),
               [Ether(frame) for frame in frames])
    return dict(counts={'%s_%s' % key: len(value) for key, value in groups.items()},
                tx_opcodes=opcodes, cm_attributes=cm_messages)


def record_cm_events(response, events):
    seen = {(entry['worker_id'], entry['sequence']) for entry in events}
    batch = []
    for worker in response['workers']:
        for entry in worker.get('events', []):
            identity = (worker['thread_id'], entry['sequence'])
            if identity not in seen:
                event = dict(entry, worker_id=worker['thread_id'])
                events.append(event)
                batch.append(event)
                seen.add(identity)
    return batch


def wait_cm_request(client, events):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        response = client.get_rocev2_events()
        batch = record_cm_events(response, events)
        requests = [entry for entry in batch if entry['type'] == 'connect_request']
        if requests:
            check(len(requests) == 1 and requests[0]['port'] == 1 and requests[0]['remote_qpn'] == 2,
                  'manual CM request reached an unexpected QP/port')
            check(requests[0]['private_data'].startswith(b'request\x00\x80'.hex()),
                  'manual CM event did not expose application private data')
            return requests[0]['connection_id']
        check(not response.get('errors', 0), 'CM worker failed before a request event arrived')
        time.sleep(0.01)
    raise RuntimeError('Timed out waiting for a passive CM request event')


def graceful_disconnect(client, expected_qps=2):
    client.disconnect_rocev2()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        response = client.get_rocev2_stats()
        qps = [qp for worker in response['workers'] for qp in worker['qps']]
        if len(qps) == expected_qps and all(qp['connection_state'] == 'disconnected' for qp in qps):
            check(not response.get('errors', 0), 'graceful CM disconnect reported errors')
            return
        time.sleep(0.01)
    raise RuntimeError('Timed out waiting for both CM connections to disconnect')


def wait_rejected(client):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        response = client.get_rocev2_stats()
        qps = [qp for worker in response['workers'] for qp in worker['qps']]
        # Rejected communication IDs remain in TIME_WAIT to reject delayed
        # duplicates safely; rejection is the event/outcome, not that timer state.
        if (len(qps) == 2 and all(qp['connection_state'] in ('time_wait', 'rejected') and
                                  qp['connection_failures'] == 1 and not qp['ever_established']
                                  for qp in qps) and
                sum(worker['cm']['rejected'] for worker in response['workers']) == 2):
            check(all(qp['posted'] == qp['completed'] == qp['failed'] == 0 for qp in qps),
                  'CM rejection submitted data or invented failed SQ work')
            return
        time.sleep(0.01)
    raise RuntimeError('Timed out waiting for both CM connections to report rejection')


def run_scenario(client, scenario, ipv6, generation, directory):
    profile, expected = make_profile(scenario, ipv6, generation)
    label = '%s-ipv%s' % (scenario, 6 if ipv6 else 4)
    client.clear_stats(ports=[0, 1], clear_traffic=False)
    client.configure_rocev2(profile)
    capture = client.start_capture(tx_ports=[0, 1], rx_ports=[0, 1], limit=4096,
                                    bpf_filter='udp dst port 4791')
    packets = []
    cm_events = []
    failure = None
    try:
        client.start_rocev2()
        if scenario in ('cm-manual-accept', 'cm-manual-reject'):
            connection_id = wait_cm_request(client, cm_events)
            if scenario == 'cm-manual-accept':
                client.accept_rocev2(connection_id, private_data=b'accepted\x00\xff')
            else:
                client.reject_rocev2(connection_id, private_data=b'rejected\x00\xff')
        if expected.get('rejected'):
            wait_rejected(client)
            verdict = client.validate_rocev2()
            check(not verdict.get('passed'), 'rejected CM connection falsely validated as successful traffic')
        else:
            verdict = client.wait_on_rocev2(timeout=15)
        if scenario.startswith('cm-') and not expected.get('rejected'):
            connected = client.get_rocev2_stats()
            check(all(qp['connection_state'] == 'established'
                      for worker in connected['workers'] for qp in worker['qps']),
                  'managed workload completed without both CM QPs established')
            graceful_disconnect(client)
            verdict = client.validate_rocev2()
        if scenario.startswith('cm-'):
            record_cm_events(client.get_rocev2_events(), cm_events)
        if expected.get('rejected'):
            rejections = [event for event in cm_events if event['type'] == 'rejected']
            check(len(rejections) == 2 and all(event['reason'] == 28 for event in rejections),
                  'CM did not report consumer-defined rejection on both peers')
        stats = client.get_rocev2_stats()
        ports = client.get_stats(ports=[0, 1], skip_zero=False)
        client.stop_capture(capture['id'], output=packets)
        capture = None
        if not expected.get('rejected'):
            check(verdict.get('passed'), 'native memory/completion validation failed')
        workers = stats['workers']
        entries = [entry for worker in workers for entry in worker['qps']]
        check(len(entries) == 2, 'two-QP ownership contract was not honored')
        qps = {entry['port']: entry for entry in entries}
        check(set(qps) == {0, 1}, 'worker assigned QPs to the wrong ports')
        check(qps[0]['local_qpn'] == 2 and qps[1]['local_qpn'] == 3,
              'worker assigned an unexpected QP identity')
        check(qps[0]['completed'] == expected['left_completed'], 'wrong requester completion count')
        check(qps[1]['received'] == expected['right_received'], 'wrong receive completion count')
        check(qps[0]['retransmissions'] == qps[1]['retransmissions'] == 0,
              'clean isolated link unexpectedly needed transport retries')
        for field in ('redirect_drops', 'ingress_drops', 'malformed_packets', 'allocation_failures'):
            check(all(worker[field] == 0 for worker in workers), 'worker reported %s' % field)
        if 'last_immediate' in expected:
            check(qps[1]['has_immediate'] and qps[1]['last_immediate'] == expected['last_immediate'],
                  'receive completion lost its immediate data')
        if 'last_invalidated_rkey' in expected:
            check(qps[1]['has_invalidate'] and
                  qps[1]['last_invalidated_rkey'] == expected['last_invalidated_rkey'],
                  'receive completion lost invalidation metadata')
        evidence = validate_capture(packets, expected, directory, label)
        tx = sum(evidence['counts']['%s_TX' % port] for port in (0, 1))
        rx = sum(evidence['counts']['%s_RX' % port] for port in (0, 1))
        check(sum(worker['tx_packets'] for worker in workers) == tx and
              sum(worker['rx_packets'] for worker in workers) == rx,
              'worker wire counters disagree with directional captures')
        for port in (0, 1):
            check(ports[port]['opackets'] == evidence['counts']['%s_TX' % port],
                  'PMD port %s TX count disagrees with captures' % port)
            check(ports[port]['ipackets'] == evidence['counts']['%s_RX' % port],
                  'PMD port %s RX count disagrees with captures' % port)
        result = dict(scenario=scenario, ip_version=6 if ipv6 else 4, passed=True,
                      expected_outcome='rejected' if expected.get('rejected') else 'successful_traffic',
                      profile=profile.to_json(), validation=verdict, stats=stats,
                      ports={str(port): ports[port] for port in (0, 1)}, capture=evidence,
                      cm_events=cm_events)
        print('PASS %s: %s accepted TX and %s received frames' % (label, tx, rx), flush=True)
        return result
    except Exception as exc:
        failure = dict(scenario=scenario, ip_version=6 if ipv6 else 4,
                       error=str(exc), profile=profile.to_json())
        try:
            failure['stats'] = client.get_rocev2_stats()
        except Exception as stats_error:
            failure['stats_error'] = str(stats_error)
        raise RuntimeError('%s: %s' % (label, exc)) from exc
    finally:
        if capture is not None:
            try:
                client.stop_capture(capture['id'], output=packets)
            except Exception as capture_error:
                if failure is not None:
                    failure['capture_error'] = str(capture_error)
        # Preserve exact captured bytes even when a malformed packet or a
        # failed memory/completion assertion prevented successful PCAP checks.
        raw_capture = [dict(port=entry['port'], origin=entry['origin'],
                            wire_hex=bytes(entry['binary']).hex()) for entry in packets]
        (directory / ('%s-capture.json' % label)).write_text(json.dumps(raw_capture, indent=2) + '\n')
        try:
            client.stop_rocev2()
        except Exception as stop_error:
            if failure is None:
                raise
            failure['stop_error'] = str(stop_error)
        if failure is not None:
            (directory / ('%s-failure.json' % label)).write_text(json.dumps(failure, indent=2) + '\n')


def device_api():
    api()
    from trex.astf.api import ASTFRoceV2Device, ASTFRoceV2Host, ASTFRoceV2Scale
    return ASTFRoceV2Device, ASTFRoceV2Host, ASTFRoceV2Scale


def make_device_profile(scenario, ipv6, generation):
    _, Connection, Profile, QP, Work = api()
    Device, Host, _ = device_api()
    dual = scenario == 'devices-dual-stack'
    devices = []
    for port in (0, 1):
        for index in range(2):
            mac = '02:52:00:00:%02x:%02x' % (port, index + 1)
            remote_mac = '02:52:00:00:%02x:%02x' % (1 - port, index + 1)
            ipv4 = '192.0.2.%s' % (10 + 20 * port + index)
            ipv6_address = '2001:db8::%x' % (10 + 20 * port + index)
            host = Host(port, mac, ipv4=ipv4 if dual or not ipv6 else None,
                        ipv6=ipv6_address if dual or ipv6 else None)
            qps = []
            for number in range(2):
                use_ipv6 = number == 1 if dual else ipv6
                source = ipv6_address if use_ipv6 else ipv4
                destination = ('2001:db8::%x' if use_ipv6 else '192.0.2.%s') % \
                              (10 + 20 * (1 - port) + index)
                options = dict(port=port, local_qpn=number + 2, remote_qpn=number + 2,
                               src_mac=mac, dst_mac=remote_mac, src_ip=source, dst_ip=destination,
                               message_size=773, memory_size=1546, mtu=256, dscp=26,
                               operation='none', outstanding=1, timeout_exp=20,
                               seed=11 + index if port == 0 else 22 + index,
                               peer_seed=22 + index if port == 0 else 11 + index,
                               local_rkey=1, remote_rkey=1, lkey=1,
                               memory_id=number + 1, protection_domain=number + 1)
                if port == 0:
                    options['program'] = [Work('write'), Work('read', expected_seed=22 + index)]
                else:
                    options['expect_writes'] = True
                if scenario == 'devices-cm':
                    destination_port = 4660 + number
                    options['connection'] = Connection(
                        'active' if port == 0 else 'passive', (0x0106 << 16) | destination_port,
                        source_port=50000 + number if port == 0 else 0,
                        destination_port=destination_port)
                qps.append(QP(**options))
            devices.append(Device(port * 100 + index + 1, host, qps))
    return Profile(devices=devices, generation=generation, duration=10), devices


def check_raw_device_admission(client, profile):
    """Exercise server validation rather than relying only on Python checks."""
    failures = []
    for label in ('storage-budget', 'host-mismatch', 'unknown-device'):
        config = json.loads(json.dumps(profile.to_json()))
        if label == 'storage-budget':
            config['max_native_storage_bytes'] = 1
        elif label == 'host-mismatch':
            config['qps'][0]['src_mac'] = '02:ff:ff:ff:ff:ff'
        else:
            config['qps'][0]['device_id'] = 0xffffffff
        try:
            client._rocev2_command('configure', config=config)
        except Exception as exc:
            failures.append(dict(case=label, error=str(exc)))
            stats = client.get_rocev2_stats()
            check(not stats['configured'],
                  'failed device admission retained its context reservation')
            check(not stats['passed'] and not any(worker['qps'] for worker in stats['workers']),
                  'failed device admission exposed an earlier session\'s successful results')
        else:
            client.stop_rocev2()
            raise RuntimeError('server accepted invalid device configuration: %s' % label)
    return failures


def validate_device_capture(packets, devices, managed, directory, label):
    from scapy.layers.l2 import Ether
    from scapy.utils import wrpcap
    groups = {(port, origin): [] for port in (0, 1) for origin in ('TX', 'RX')}
    data = {}
    cm = []
    data_tx_count = 0
    for packet in packets:
        wire, opcode, bth = canonical_packet(packet['binary'])
        groups[(packet['port'], packet['origin'])].append(wire)
        if packet['origin'] != 'TX':
            continue
        if opcode == 0x64:
            mad = bth + 20
            cm.append(struct.unpack('!H', wire[mad + 16:mad + 18])[0])
        else:
            data_tx_count += 1
            qpn = int.from_bytes(wire[bth + 5:bth + 8], 'big')
            data.setdefault((packet['port'], wire[6:12].hex(), qpn), []).append(opcode)
    for port in (0, 1):
        check(Counter(groups[(port, 'TX')]) == Counter(groups[(1 - port, 'RX')]),
              'multi-device TX differs from peer RX on port %s' % port)
    for device in devices:
        metadata = device.to_json()
        host = metadata['host']
        port = metadata['port']
        source_mac = host['mac'].replace(':', '').lower()
        expected = [6, 7, 7, 8, 12] if port == 0 else [17, 13, 14, 14, 15]
        for qpn in (2, 3):
            check(data.pop((port, source_mac, qpn), []) == expected,
                  'wrong per-device/QP wire sequence for device %s QPN %s' % (metadata['device_id'], qpn))
    check(not data, 'capture contains data for an unconfigured device/QP')
    expected_cm = Counter([0x10, 0x13, 0x14, 0x15, 0x15, 0x16, 0x16] * 4) if managed else Counter()
    check(Counter(cm) == expected_cm, 'multi-device CM handshakes/disconnects disagree')
    for (port, origin), frames in groups.items():
        wrpcap(str(directory / ('%s-port%s-%s.pcap' % (label, port, origin.lower()))),
               [Ether(frame) for frame in frames])
    return dict(counts={'%s_%s' % key: len(value) for key, value in groups.items()},
                cm_attributes=cm, data_tx_packets=data_tx_count)


def run_device_scenario(client, scenario, ipv6, generation, directory):
    profile, devices = make_device_profile(scenario, ipv6, generation)
    dual = scenario == 'devices-dual-stack'
    label = '%s-%s' % (scenario, 'dual' if dual else 'ipv%s' % (6 if ipv6 else 4))
    admission_failures = check_raw_device_admission(client, profile) if scenario == 'devices-read-write' else []
    client.set_port_attr(ports=[0, 1], promiscuous=True)
    client.clear_stats(ports=[0, 1], clear_traffic=False)
    configuration = client.configure_rocev2(profile)
    captures = []
    packets, selected_packets = [], []
    failure = None
    try:
        captures.append((client.start_capture(tx_ports=[0, 1], rx_ports=[0, 1], limit=16384,
                                              bpf_filter='udp dst port 4791'), packets))
        captures.append((client.start_capture(tx_ports=[0], rx_ports=[0], limit=4096,
                                              bpf_filter=devices[0].capture_filter()), selected_packets))
        client.start_rocev2()
        verdict = client.wait_on_rocev2(timeout=15)
        check(verdict.get('passed'), 'multi-device memory/completion validation failed')
        if scenario == 'devices-cm':
            graceful_disconnect(client, expected_qps=8)
            verdict = client.validate_rocev2()
            check(verdict.get('passed'), 'multi-device validation failed after CM disconnect')
        stats = client.get_rocev2_stats()
        port_stats = client.get_stats(ports=[0, 1], skip_zero=False)
        while captures:
            capture, output = captures.pop()
            client.stop_capture(capture['id'], output=output)
        qps = [qp for worker in stats['workers'] for qp in worker['qps']]
        check(len(qps) == 8, 'multi-device profile attached the wrong number of QPs')
        check(len(stats['devices']) == 4 and len(stats['ports']) == 2,
              'missing device or port statistics')
        for device in stats['devices']:
            check(device['qp_count'] == 2 and device['passed'] and device['errors'] == 0,
                  'device statistics disagree with QP validation')
            check(device['completed'] == (4 if device['port'] == 0 else 0),
                  'device statistics mixed another device\'s completions')
            declared = next(entry.to_json() for entry in devices
                            if entry.to_json()['device_id'] == device['device_id'])
            check(all(device[field] == declared[field] for field in ('host', 'gids', 'node_guid')),
                  'device statistics lost its configured identity/GID table')
        for port in stats['ports']:
            check(port['device_count'] == 2 and port['qp_count'] == 4 and port['passed'],
                  'port statistics disagree with device/QP ownership')
        for qp in qps:
            check(qp['passed'] and qp['errors'] == 0 and qp['retransmissions'] == 0,
                  'isolated multi-device QP failed or required retransmission')
        for worker in stats['workers']:
            for field in ('redirect_drops', 'ingress_drops', 'malformed_packets', 'allocation_failures'):
                check(worker[field] == 0, 'multi-device worker reported %s' % field)
        capture_evidence = validate_device_capture(packets, devices, scenario == 'devices-cm', directory, label)
        check(sum(device['data_tx_packets'] for device in stats['devices']) ==
              capture_evidence['data_tx_packets'], 'device TX acceptance counters disagree with RC capture')
        check(sum(device['data_rx_packets'] for device in stats['devices']) ==
              capture_evidence['data_tx_packets'], 'device valid RC RX counters disagree with capture')
        selected_mac = bytes.fromhex(devices[0].to_json()['host']['mac'].replace(':', ''))
        expected_selected = Counter((entry['origin'], canonical_packet(entry['binary'])[0])
                                    for entry in packets if entry['port'] == 0 and
                                    (bytes(entry['binary'])[0:6] == selected_mac or
                                     bytes(entry['binary'])[6:12] == selected_mac))
        observed_selected = Counter((entry['origin'], canonical_packet(entry['binary'])[0])
                                    for entry in selected_packets)
        check(expected_selected and observed_selected == expected_selected,
              'device capture filter omitted its packets or captured another device')
        from scapy.layers.l2 import Ether
        from scapy.utils import wrpcap
        wrpcap(str(directory / ('%s-selected-device.pcap' % label)),
               [Ether(canonical_packet(entry['binary'])[0]) for entry in selected_packets])
        for port in (0, 1):
            check(port_stats[port]['opackets'] == capture_evidence['counts']['%s_TX' % port] and
                  port_stats[port]['ipackets'] == capture_evidence['counts']['%s_RX' % port],
                  'multi-device PMD counters disagree with directional captures')
        result = dict(scenario=scenario, ip_version='dual' if dual else 6 if ipv6 else 4,
                      passed=True, configuration=configuration, profile=profile.to_json(),
                      validation=verdict, stats=stats, capture=capture_evidence,
                      selected_capture_packets=len(selected_packets), admission_rejections=admission_failures)
        print('PASS %s: four devices/eight QPs, device-local QPN/key reuse and capture isolation' % label, flush=True)
        return result
    except Exception as exc:
        failure = dict(scenario=scenario, error=str(exc), profile=profile.to_json())
        try:
            failure['stats'] = client.get_rocev2_stats()
        except Exception as stats_error:
            failure['stats_error'] = str(stats_error)
        raise RuntimeError('%s: %s' % (label, exc)) from exc
    finally:
        for capture, output in captures:
            try:
                client.stop_capture(capture['id'], output=output)
            except Exception:
                pass
        (directory / ('%s-capture.json' % label)).write_text(json.dumps([
            dict(port=entry['port'], origin=entry['origin'], wire_hex=bytes(entry['binary']).hex())
            for entry in packets], indent=2) + '\n')
        try:
            client.stop_rocev2()
        except Exception:
            if failure is None:
                raise
        if failure is not None:
            (directory / ('%s-failure.json' % label)).write_text(json.dumps(failure, indent=2) + '\n')


def run_scale_configuration(client, generation, traffic=False):
    _, _, Scale = device_api()
    profile = Scale(devices_per_port=32, qps_per_device=16).paired_profile(
        generation=generation, duration=30, timeout_exp=20)
    client.set_port_attr(ports=[0, 1], promiscuous=True)
    configuration = client.configure_rocev2(profile)
    try:
        stats = client.get_rocev2_stats()
        check(len(stats['devices']) == 64 and sum(len(w['qps']) for w in stats['workers']) == 1024,
              'default scale was truncated or expanded incorrectly')
        check(len(stats['ports']) == 2 and
              all(port['device_count'] == 32 and port['qp_count'] == 512 for port in stats['ports']),
              'default per-port scale is incorrect')
        admission = stats['admission']
        check(admission['estimated_storage_bytes'] <= admission['max_native_storage_bytes'],
              'server admitted storage above the configured budget')
        check(admission['registered_memory_bytes'] <= admission['max_memory_bytes'],
              'server admitted memory above the configured budget')
        validation = None
        if traffic:
            client.start_rocev2()
            validation = client.wait_on_rocev2(timeout=60)
            check(validation.get('passed'), 'default scale failed memory/completion validation')
            stats = client.get_rocev2_stats()
            check(all(device['completed'] == 16 and device['passed'] and device['errors'] == 0
                      for device in stats['devices']), 'default-scale device validation disagrees')
        scenario = 'devices-scale-traffic' if traffic else 'devices-scale-configure'
        print('PASS %s: 32 devices x 16 QPs on each of two ports' % scenario, flush=True)
        return dict(scenario=scenario, passed=True, traffic_started=traffic, validation=validation,
                    configuration=configuration, admission=admission,
                    device_count=64, qp_count=1024, devices=stats['devices'], ports=stats['ports'])
    finally:
        client.stop_rocev2()


def run_host_neighbors(client, generation, directory):
    """Probe both host addresses while its only QP uses IPv4."""
    _, _, Scale = device_api()
    from scapy.layers.l2 import ARP, Ether
    from scapy.layers.inet6 import (IPv6, ICMPv6ND_NS, ICMPv6ND_NA,
                                    ICMPv6NDOptSrcLLAddr, ICMPv6NDOptDstLLAddr)
    from scapy.utils import wrpcap
    profile = Scale(devices_per_port=1, qps_per_device=1).paired_profile(
        ip_version='dual', generation=generation, operation='none', expect_writes=False)
    metadata = profile.to_json()
    host = metadata['devices'][0]['host']
    check(all(':' not in qp['src_ip'] for qp in metadata['qps']),
          'unused-IPv6-GID test unexpectedly configured an IPv6 QP')
    peer_mac, peer4, peer6 = '02:fe:00:00:00:01', '10.0.255.254', 'fe80::feed'
    low = int(ipaddress.IPv6Address(host['ipv6'])) & 0xffffff
    multicast = str(ipaddress.IPv6Address(int(ipaddress.IPv6Address('ff02::1:ff00:0')) | low))
    multicast_mac = '33:33:ff:%02x:%02x:%02x' % (low >> 16, (low >> 8) & 255, low & 255)
    arp = Ether(src=peer_mac, dst='ff:ff:ff:ff:ff:ff') / ARP(
        op=1, hwsrc=peer_mac, psrc=peer4, hwdst='00:00:00:00:00:00', pdst=host['ipv4'])
    ns = Ether(src=peer_mac, dst=multicast_mac) / IPv6(src=peer6, dst=multicast, hlim=255) / \
         ICMPv6ND_NS(tgt=host['ipv6']) / ICMPv6NDOptSrcLLAddr(lladdr=peer_mac)
    capture, packets = None, []
    client.set_port_attr(ports=[0, 1], promiscuous=True)
    client.configure_rocev2(profile)
    try:
        capture = client.start_capture(tx_ports=[0, 1], rx_ports=[0, 1], limit=256,
                                       bpf_filter='arp or icmp6')
        with socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3)) as wire_socket:
            wire_socket.bind((INTERFACES[1], 0))
            wire_socket.settimeout(0.1)
            client.start_rocev2()
            wire_socket.send(bytes(arp))
            wire_socket.send(bytes(ns))
            responses = {}
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and len(responses) < 2:
                try:
                    wire = wire_socket.recv(65535)
                except socket.timeout:
                    continue
                packet = Ether(wire)
                if packet.src != host['mac'] or packet.dst != peer_mac:
                    continue
                if ARP in packet and packet[ARP].op == 2:
                    check(packet[ARP].psrc == host['ipv4'] and packet[ARP].pdst == peer4 and
                          packet[ARP].hwsrc == host['mac'], 'native ARP reply has incorrect host identity')
                    responses['arp'] = wire
                elif ICMPv6ND_NA in packet:
                    check(packet[IPv6].src == host['ipv6'] and packet[IPv6].dst == peer6 and
                          packet[IPv6].hlim == 255 and packet[ICMPv6ND_NA].tgt == host['ipv6'] and
                          packet[ICMPv6ND_NA].S == 1 and ICMPv6NDOptDstLLAddr in packet and
                          packet[ICMPv6NDOptDstLLAddr].lladdr == host['mac'],
                          'unused IPv6 GID produced an incorrect neighbor advertisement')
                    icmp = wire[54:54 + packet[IPv6].plen]
                    pseudo = wire[22:54] + struct.pack('!I', len(icmp)) + b'\x00\x00\x00\x3a'
                    check(internet_checksum(pseudo + icmp) == 0, 'native neighbor reply checksum is invalid')
                    responses['nd'] = wire
            check(set(responses) == {'arp', 'nd'}, 'native host did not answer ARP and unused-GID IPv6 NS')
        client.stop_capture(capture['id'], output=packets)
        capture = None
        wrpcap(str(directory / 'devices-host-neighbors.pcap'), [Ether(packet['binary']) for packet in packets])
        stats = client.get_rocev2_stats()
        check(all(qp['posted'] == qp['completed'] == 0 for worker in stats['workers'] for qp in worker['qps']),
              'address probing unexpectedly submitted RDMA work')
        print('PASS devices-host-neighbors: ARP and unused IPv6 GID reply with no RDMA work', flush=True)
        return dict(scenario='devices-host-neighbors', passed=True, rdma_work=False,
                    responses={kind: wire.hex() for kind, wire in responses.items()}, stats=stats,
                    captured_packets=len(packets))
    finally:
        if capture is not None:
            client.stop_capture(capture['id'], output=packets)
        client.stop_rocev2()


def run_namespace(args):
    check(args.parent_netns and os.readlink('/proc/self/ns/net') != args.parent_netns,
          'refusing to create interfaces outside a fresh network namespace')
    setup_veth()
    directory = args.output_dir.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    config = platform_config(directory)
    # Unix telemetry socket paths are limited to 108 bytes. Keep the PMD
    # runtime prefix short even when evidence lives in a deeply nested checkout.
    runtime = tempfile.TemporaryDirectory(prefix='tr-as-')
    command = [str(args.binary.resolve()), '-i', '--astf', '--software', '--cfg', str(config),
               '--no-ofed-check', '--no-scapy-server', '--prefix', 'roce-astf-%s' % os.getpid(), '-c', '1']
    environment = dict(os.environ, RUNTIME_DIRECTORY=runtime.name)
    environment['LD_LIBRARY_PATH'] = '/usr/lib64:' + str(ROOT / 'scripts/so/x86_64') + \
        (':' + environment['LD_LIBRARY_PATH'] if environment.get('LD_LIBRARY_PATH') else '')
    evidence = dict(scope='ASTF RPC and DPDK AF_PACKET worker acceptance',
                    hardware=False, performance_test=False, command=command,
                    binary=str(args.binary.resolve()), results=[], passed=False)
    client = None
    with (directory / 'server.log').open('w') as output:
        server = subprocess.Popen(command, cwd=str(ROOT / 'scripts'), stdout=output,
                                  stderr=subprocess.STDOUT, env=environment)
        try:
            client = connect(server, args.startup_timeout)
            capabilities = client.get_rocev2_capabilities()
            check(capabilities['transport'] == 'RC' and capabilities['max_receive_sge'] == 16,
                  'server advertises an unexpected endpoint transport/SGE limit')
            check(set(('send', 'send_imm', 'send_inv', 'write', 'write_imm', 'read',
                       'compare_swap', 'fetch_add')).issubset(capabilities['operations']),
                  'server lacks the requested baseline endpoint verbs')
            evidence['capabilities'] = capabilities
            generation = 1
            versions = (4, 6) if args.ip_version == 'both' else (int(args.ip_version),)
            for scenario in args.scenario or STATIC_SCENARIOS:
                for version in versions:
                    if scenario == 'devices-host-neighbors':
                        if version != versions[0]:
                            continue
                        result = run_host_neighbors(client, generation, directory)
                    elif scenario in ('devices-scale-configure', 'devices-scale-traffic'):
                        if version != versions[0]:
                            continue
                        result = run_scale_configuration(client, generation, scenario == 'devices-scale-traffic')
                    elif scenario in DEVICE_SCENARIOS:
                        if scenario == 'devices-dual-stack' and version != versions[0]:
                            continue
                        result = run_device_scenario(client, scenario, version == 6, generation, directory)
                    else:
                        result = run_scenario(client, scenario, version == 6, generation, directory)
                    evidence['results'].append(result)
                    generation += 1
            evidence['passed'] = True
        except Exception as exc:
            evidence['error'] = str(exc)
            traceback.print_exc()
        finally:
            if client is not None:
                try:
                    client.disconnect()
                except Exception:
                    pass
            if server.poll() is None:
                server.send_signal(signal.SIGINT)
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
            evidence['server_exit_code'] = server.returncode
            (directory / 'report.json').write_text(json.dumps(evidence, indent=2) + '\n')
    if not evidence['passed']:
        print('See %s for the server log and report.' % directory, file=sys.stderr)
    runtime.cleanup()
    return 0 if evidence['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'scripts/_t-rex-64')
    parser.add_argument('--output-dir', type=Path, default=ROOT / '.dev/rocev2-completion/astf')
    parser.add_argument('--scenario', choices=SCENARIOS, action='append')
    parser.add_argument('--all', action='store_true', help='include automatic/manual CM and intentional rejection')
    parser.add_argument('--ip-version', choices=('4', '6', 'both'), default='both')
    parser.add_argument('--startup-timeout', type=float, default=30)
    parser.add_argument('--in-namespace', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--parent-netns', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.all:
        if args.scenario:
            parser.error('--all and --scenario cannot be combined')
        args.scenario = list(SCENARIOS)
    check(args.binary.is_file(), 'TRex binary does not exist: %s' % args.binary)
    check(args.startup_timeout > 0, '--startup-timeout must be positive')
    if args.in_namespace:
        return run_namespace(args)
    check(namespace_supported(), 'DPDK AF_PACKET acceptance needs unprivileged user/network namespaces')
    command = ['unshare', '--user', '--map-root-user', '--net', sys.executable,
               str(Path(__file__).resolve()), '--in-namespace',
               '--parent-netns', os.readlink('/proc/self/ns/net'),
               '--binary', str(args.binary.resolve()), '--output-dir', str(args.output_dir.resolve()),
               '--ip-version', args.ip_version, '--startup-timeout', str(args.startup_timeout)]
    for scenario in args.scenario or []:
        command += ['--scenario', scenario]
    return subprocess.run(command).returncode


if __name__ == '__main__':
    sys.exit(main())
