"""Stateless RoCEv2 packet, VM, and counter-validation regressions."""

import contextlib
from collections import Counter
import copy
import io
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace
import zlib

from trex.stl.api import (
    Ether, IP, IPv6, UDP, STLFlowLatencyStats, STLFlowStats, STLPktBuilder,
    STLProfile, STLScVmRaw, STLStream, STLTXSingleBurst, STLVM,
    STLVmFixRoceV2Icrc, rocev2_stats_expectations, validate_rocev2_stats,
    STLClient,
)
from trex.common.trex_types import RC_OK, RC_ERR, PortProfileID
from trex.stl.trex_stl_port import STLPort
from trex.stl.trex_stl_stats import CPgIdStats
from trex.stl.trex_stl_sim import STLSim
from scapy.contrib.roce import AETH, BTH, RETH
from scapy.layers.inet import in4_chksum
from scapy.layers.inet6 import in6_chksum
from scapy.utils import RawPcapReader, checksum


SCRIPTS = Path(__file__).resolve().parents[3]
PROFILE = SCRIPTS / 'stl/rocev2_gpu.py'


def load_streams(*tunables, direction=0):
    return STLProfile.load_py(str(PROFILE), direction=direction,
                              tunables=list(tunables)).get_streams()


def invariant_crc(wire):
    """Calculate directly over wire bytes, independently of Scapy's builder."""
    packet = Ether(wire)
    l2 = packet[IP if IP in packet else IPv6]._offset
    data = bytearray(wire[l2:-4])
    l3 = 20 if IP in packet else 40
    if IP in packet:
        for offset in (1, 8, 10, 11):
            data[offset] = 0xff
    else:
        data[0:4] = b'\x6f\xff\xff\xff'
        data[7] = 0xff
    data[l3 + 6:l3 + 8] = b'\xff\xff'
    data[l3 + 8 + 4] = 0xff
    return struct.pack('<I', zlib.crc32(b'\xff' * 8 + data) & 0xffffffff)


class RoCEv2Test(unittest.TestCase):
    def test_write_packet_fields_offsets_and_icrc(self):
        streams = load_streams('--message-size', '17', '--qp-base', '0x123', '--psn', '0xfffffe')
        packet = Ether(streams[0].get_pkt())
        self.assertEqual(packet[UDP].dport, 4791)
        self.assertEqual(packet[BTH].opcode, 0x0a)
        self.assertEqual(packet[BTH].dqpn, 0x123)
        self.assertEqual(packet[BTH].psn, 0xfffffe)
        self.assertEqual(packet[BTH].padcount, 3)
        self.assertEqual(packet[RETH].dma_len, 17)
        self.assertEqual(packet[RETH].rkey, 0x10101010)
        self.assertEqual(packet[BTH]._offset, 42)
        self.assertEqual(streams[0].get_pkt()[-4:], invariant_crc(streams[0].get_pkt()))

    def test_ipv6_vlan_headers_and_icrc(self):
        stream = load_streams('--ip-version', '6', '--vlan', '7', '--message-size', '31')[0]
        packet = Ether(stream.get_pkt())
        self.assertEqual(packet[IPv6].tc, 105)
        self.assertEqual(packet[BTH]._offset, 66)
        self.assertEqual(stream.to_json()['vm']['instructions'][-1], {
            'type': 'fix_checksum_rocev2_icrc', 'l2_len': 18, 'l3_len': 40, 'bth_offset': 66})
        self.assertEqual(stream.get_pkt()[-4:], invariant_crc(stream.get_pkt()))

    def test_write_segments_cover_message_and_respect_frame_mtu(self):
        streams = load_streams('--message-size', '4001', '--mtu', '512', '--psn', '0xffffff')
        packets = [Ether(stream.get_pkt()) for stream in streams]
        self.assertGreater(len(packets), 2)
        self.assertEqual(packets[0][BTH].opcode, 6)
        self.assertEqual(packets[-1][BTH].opcode, 8)
        self.assertTrue(all(packet[BTH].opcode == 7 for packet in packets[1:-1]))
        size = 0
        for index, (stream, packet) in enumerate(zip(streams, packets)):
            self.assertLessEqual(stream.get_pkt_len(), 512)
            self.assertEqual(packet[BTH].psn, (0xffffff + index) & 0xffffff)
            self.assertEqual(stream.get_pkt()[-4:], invariant_crc(stream.get_pkt()))
            ext = 16 if index == 0 else 0
            size += len(bytes(packet[UDP].payload)) - 12 - ext - 4 - packet[BTH].padcount
        self.assertEqual(size, 4001)

    def test_read_request_and_segmented_response(self):
        request = Ether(load_streams('--operation', 'read-request', '--message-size', '9001')[0].get_pkt())
        self.assertEqual(request[BTH].opcode, 12)
        self.assertEqual(request[RETH].dma_len, 9001)
        streams = load_streams('--operation', 'read-response', '--message-size', '4001', '--mtu', '512')
        packets = [Ether(stream.get_pkt()) for stream in streams]
        self.assertEqual(packets[0][BTH].opcode, 13)
        self.assertEqual(packets[-1][BTH].opcode, 15)
        self.assertTrue(AETH in packets[0] and AETH in packets[-1])
        self.assertTrue(all(packet[BTH].opcode == 14 for packet in packets[1:-1]))
        self.assertTrue(all(stream.get_pkt()[-4:] == invariant_crc(stream.get_pkt()) for stream in streams))

    def test_qp_distribution_gid_and_direction(self):
        streams = load_streams('--qps', '2', '--message-size', '16', '--src-ip-step', '1',
                               '--dst-gid', '::ffff:48.0.0.1', direction=1)
        packets = [Ether(stream.get_pkt()) for stream in streams]
        self.assertEqual([packet[BTH].dqpn for packet in packets], [0x100, 0x101])
        self.assertEqual([packet[IP].dst for packet in packets], ['16.0.0.1', '16.0.0.2'])
        self.assertEqual(packets[0][IP].src, '48.0.0.1')
        self.assertEqual(packets[0].src, '00:00:00:00:02:00')
        self.assertEqual([packet[UDP].sport for packet in packets], [49152, 49153])
        self.assertEqual(packets[1][BTH].psn, 1024)

    def test_chain_links_and_per_qp_flow_stats(self):
        streams = load_streams('--qps', '2', '--stream-mode', 'chain', '--total-pkts', '3',
                               '--message-size', '2000', '--mtu', '512', '--flow-stats', '--pg-id-base', '50')
        self.assertEqual(sorted({stream.get_pg_id() for stream in streams}), [50, 51])
        self.assertEqual([stream.fields['self_start'] for stream in streams], [True] + [False] * (len(streams) - 1))
        self.assertEqual([stream.next for stream in streams[:-1]], [stream.name for stream in streams[1:]])
        self.assertIsNone(streams[-1].next)
        expected = rocev2_stats_expectations(streams)
        self.assertEqual(sum(entry['tx_pkts'] for entry in expected.values()), 3 * len(streams))

    def test_reject_invalid_profile_parameters(self):
        for option, value in [('--vlan', '4095'), ('--qps', '0'), ('--qp-base', '0x1000000'),
                              ('--psn', '-1'), ('--dscp', '64'), ('--ecn', '4'), ('--ttl', '256'),
                              ('--virtual-address', '-1'), ('--rkey', '0x100000000'),
                              ('--mtu', '60'), ('--message-size', '0'), ('--pps', 'nan'),
                              ('--total-pkts', '0'), ('--payload-pattern', '0x')]:
            with self.assertRaises(Exception, msg=option):
                load_streams(option, value)
        with self.assertRaises(Exception):
            load_streams('--ip-version', '6', '--src-gid', '::ffff:16.0.0.1', '--src-ip', '2001:db8::1')

    def test_reject_headers_that_do_not_fit_mtu(self):
        with self.assertRaises(Exception):
            load_streams('--ip-version', '6', '--operation', 'read-request', '--mtu', '64')

    def test_vm_json_and_generated_profile_round_trip(self):
        stream = load_streams('--message-size', '64', '--flow-stats')[0]
        copy = STLStream.from_json(json.loads(json.dumps(stream.to_json())))
        self.assertEqual(copy.get_pkt(), stream.get_pkt())
        self.assertEqual(copy.to_json()['vm'], stream.to_json()['vm'])
        with tempfile.TemporaryDirectory() as directory:
            profile = Path(directory) / 'generated_rocev2_roundtrip.py'
            profile.write_text(STLProfile([stream]).dump_to_code())
            restored = STLProfile.load_py(str(profile)).get_streams()[0]
        self.assertEqual(restored.get_pkt(), stream.get_pkt())
        self.assertEqual(restored.to_json()['vm'], stream.to_json()['vm'])

    def test_latency_payload_stamping_is_rejected(self):
        stream = load_streams('--message-size', '64')[0]
        with self.assertRaises(Exception):
            STLStream(packet=stream.scapy_pkt_builder, flow_stats=STLFlowLatencyStats(pg_id=0))

    def test_vm_offset_validation_is_not_an_assertion(self):
        packet = Ether(load_streams('--message-size', '64')[0].get_pkt())
        for offsets in ((-1, 34, 42), (14, 34, 43), (14, 14, 42), (14, 34, 65536)):
            with self.assertRaises(Exception):
                STLPktBuilder(pkt=packet, vm=STLScVmRaw([STLVmFixRoceV2Icrc(*offsets)]))

    def _simulate(self, streams):
        with tempfile.TemporaryDirectory() as directory:
            capture = str(Path(directory) / 'rocev2.pcap')
            previous = os.getcwd()
            try:
                os.chdir(str(SCRIPTS))
                with contextlib.redirect_stdout(io.StringIO()):
                    STLSim(str(SCRIPTS)).run(streams, outfile=capture, pkt_limit=100,
                                             mult='1', silent=True, tunables=[])
            finally:
                os.chdir(previous)
            with RawPcapReader(capture) as reader:
                return [wire for wire, _ in reader]

    def test_simulated_ipv4_and_ipv6_flow_stats_have_valid_icrc(self):
        for version in ('4', '6'):
            streams = load_streams('--ip-version', version, '--qps', '2', '--message-size', '2000',
                                   '--mtu', '512', '--flow-stats', '--stream-mode', 'burst', '--total-pkts', '2')
            packets = self._simulate(streams)
            self.assertEqual(len(packets), 2 * len(streams))
            for wire in packets:
                self.assertEqual(wire[-4:], invariant_crc(wire))
                self.assertLessEqual(len(wire) + 4, 512)
                packet = Ether(wire)
                udp_check = in4_chksum(17, packet[IP], bytes(packet[UDP])) if version == '4' else \
                    in6_chksum(17, packet[UDP], bytes(packet[UDP]))
                self.assertEqual(udp_check, 0)

    def test_simulated_vm_writes_update_crc_after_header_mutation(self):
        packet = Ether(load_streams('--message-size', '256')[0].get_pkt())
        packet[UDP].chksum = 0  # Valid IPv4 UDP without a transport checksum.
        vm = STLVM()
        vm.var(name='src', min_value='16.0.0.1', max_value='16.0.0.3', size=4, op='inc')
        vm.write(fv_name='src', pkt_offset='IP.src')
        vm.fix_chksum(offset='IP')
        vm.fix_rocev2_icrc()
        stream = STLStream(packet=STLPktBuilder(pkt=packet, vm=vm),
                           mode=STLTXSingleBurst(total_pkts=3, pps=1))
        packets = self._simulate([stream])
        self.assertEqual([Ether(wire)[IP].src for wire in packets], ['16.0.0.1', '16.0.0.2', '16.0.0.3'])
        self.assertEqual(len({wire[-4:] for wire in packets}), 3)
        for wire in packets:
            self.assertEqual(wire[-4:], invariant_crc(wire))
            self.assertEqual(checksum(wire[14:34]), 0)

    def test_server_rejects_payload_stats_and_instructions_after_crc(self):
        stream = load_streams('--message-size', '64', '--flow-stats', '--stream-mode', 'burst')[0]
        stream.fields['flow_stats']['rule_type'] = 'latency'
        with self.assertRaises(Exception):
            self._simulate([stream])
        stream.fields['flow_stats']['rule_type'] = 'stats'
        stream.fields['vm']['instructions'].append({'type': 'fix_checksum_ipv4', 'pkt_offset': 14})
        with self.assertRaises(Exception):
            self._simulate([stream])


class RoCEv2StatsTest(unittest.TestCase):
    def setUp(self):
        streams = load_streams('--message-size', '16', '--stream-mode', 'burst',
                               '--total-pkts', '10', '--flow-stats')
        self.expected = rocev2_stats_expectations(streams)
        target = self.expected[0]
        self.stats = {'flow_stats': {0: {
            'tx_pkts': {0: 10}, 'rx_pkts': {1: 10},
            'tx_bytes': {0: target['tx_bytes']}, 'rx_bytes': {1: target['tx_bytes']},
        }, 'global': {'tx_err': {0: 0}, 'rx_err': {1: 0}}}}

    def check(self, stats=None, **kwargs):
        return validate_rocev2_stats(self.stats if stats is None else stats,
                                     self.expected, 0, 1, **kwargs)

    def test_success_and_json_key_round_trip(self):
        self.assertTrue(self.check(check_rx_bytes=True)['passed'])
        self.assertTrue(self.check(json.loads(json.dumps(self.stats)))['passed'])

    def test_missing_or_invalid_counters_fail(self):
        for key in ('tx_pkts', 'rx_pkts', 'tx_bytes'):
            for value in ({}, {0: -1, 1: -1}, {0: True, 1: True}, {0: None, 1: None}):
                stats = copy.deepcopy(self.stats)
                stats['flow_stats'][0][key] = value
                self.assertFalse(self.check(stats)['passed'])
        self.assertFalse(self.check({})['passed'])
        self.assertFalse(self.check({'flow_stats': []})['passed'])

    def test_loss_threshold_and_duplicates(self):
        self.stats['flow_stats'][0]['rx_pkts'][1] = 9
        self.assertFalse(self.check()['passed'])
        self.assertTrue(self.check(max_loss_pct=10)['passed'])
        self.assertFalse(self.check(max_loss_pct=9.9)['passed'])
        self.stats['flow_stats'][0]['rx_pkts'][1] = 11
        self.assertFalse(self.check(max_loss_pct=100)['passed'])

    def test_tx_count_and_bytes_must_match_expected(self):
        for key in ('tx_pkts', 'tx_bytes'):
            stats = copy.deepcopy(self.stats)
            stats['flow_stats'][0][key][0] -= 1
            self.assertFalse(self.check(stats)['passed'])

    def test_rx_bytes_are_optional_and_explicitly_checked(self):
        self.stats['flow_stats'][0]['rx_bytes'] = {}
        self.assertTrue(self.check()['passed'])
        self.assertFalse(self.check(check_rx_bytes=True)['passed'])

    def test_global_counter_errors_fail(self):
        self.stats['flow_stats']['global']['rx_err'][1] = 1
        self.assertFalse(self.check()['passed'])

    def test_malformed_global_counters_fail(self):
        for value in (None, [], {'tx_err': None}):
            stats = copy.deepcopy(self.stats)
            stats['flow_stats']['global'] = value
            self.assertFalse(self.check(stats)['passed'])

    def test_expectations_reject_continuous_and_uninstrumented_streams(self):
        for tunables in ((), ('--stream-mode', 'burst'), ('--flow-stats',)):
            with self.assertRaises(ValueError):
                rocev2_stats_expectations(load_streams(*tunables))
        with self.assertRaises(ValueError):
            rocev2_stats_expectations([])

    def test_invalid_loss_tolerances_are_rejected(self):
        for value in (-1, 101, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                self.check(max_loss_pct=value)


class RoCEv2ExampleTest(unittest.TestCase):
    def setUp(self):
        path = SCRIPTS / 'automation/trex_control_plane/interactive/trex/examples/stl/stl_rocev2.py'
        spec = importlib.util.spec_from_file_location('rocev2_example_test_module', str(path))
        self.example = importlib.util.module_from_spec(spec)
        with mock.patch.object(sys, 'path', [str(path.parent)] + sys.path):
            spec.loader.exec_module(self.example)
        self.args = SimpleNamespace(profile=str(PROFILE), packets=10, pps=100,
                                    qps=1, ip_version=4, message_size=16, mtu=9000,
                                    src_mac='00:00:00:00:01:00', dst_mac='00:00:00:00:02:00',
                                    tx_port=0, rx_port=1, server='localhost', timeout=10,
                                    rx_delay_ms=1000, max_loss_pct=0, check_rx_bytes=False)
        self.client = mock.Mock()
        self.client.get_warnings.return_value = []

    def invoke(self):
        with mock.patch.object(self.example, 'STLClient', return_value=self.client), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            code = self.example.run(self.args)
        return code, json.loads(output.getvalue())

    def test_run_sends_burst_validates_and_disconnects(self):
        length = load_streams('--message-size', '16')[0].get_pkt_len()
        self.client.get_rocev2_stats.return_value = {'flow_stats': {0: {
            'tx_pkts': {0: 10}, 'rx_pkts': {1: 10}, 'tx_bytes': {0: 10 * length},
        }}}
        code, result = self.invoke()
        self.assertEqual(code, 0)
        self.assertTrue(result['passed'])
        self.client.acquire.assert_called_once_with(ports=[0, 1])
        self.client.clear_stats.assert_called_once_with(ports=[0, 1])
        self.client.wait_on_traffic.assert_called_once_with(ports=[0], timeout=10, rx_delay_ms=1000)
        self.client.disconnect.assert_called_once_with()

    def test_missing_counters_fail_and_disconnect(self):
        self.client.get_rocev2_stats.return_value = {}
        code, result = self.invoke()
        self.assertEqual(code, 1)
        self.assertFalse(result['passed'])
        self.client.disconnect.assert_called_once_with()

    def test_timeout_disconnects(self):
        self.client.wait_on_traffic.side_effect = RuntimeError('timeout')
        with self.assertRaises(RuntimeError):
            self.invoke()
        self.client.disconnect.assert_called_once_with()

    def test_native_run_needs_no_profile_file(self):
        self.args.native = True
        self.args.profile = '/does/not/exist.py'
        self.client.add_rocev2_streams.return_value = {'expected_stats': {'0': {'tx_pkts': 10, 'tx_bytes': 1000}}}
        self.client.validate_rocev2_stats.return_value = {'passed': True, 'errors': [], 'flows': {}}
        code, result = self.invoke()
        self.assertEqual(code, 0)
        self.assertTrue(result['passed'])
        self.client.add_streams.assert_not_called()
        self.client.add_rocev2_streams.assert_called_once()
        self.client.validate_rocev2_stats.assert_called_once()
        self.client.disconnect.assert_called_once_with()


class RoCEv2NativeRpcTest(unittest.TestCase):
    def simulate(self, config, success=True):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / 'native.pcap'
            request = Path(directory) / 'request.json'
            commands = [{'id': 1, 'jsonrpc': '2.0', 'method': 'build_rocev2_streams', 'params': {'config': config}},
                        {'id': 2, 'jsonrpc': '2.0', 'method': 'add_rocev2_streams', 'params': {'port_id': 0, 'config': config}},
                        {'id': 3, 'jsonrpc': '2.0', 'method': 'get_rocev2_stats', 'params': {'pgids': [0, 1]}},
                        STLSim(str(SCRIPTS)).generate_start_cmd()]
            request.write_text(json.dumps(commands))
            result = subprocess.run([str(SCRIPTS / 'bp-sim-64-debug'), '--pcap', '--sl', '--cores', '1',
                                     '--limit', '100', '-f', str(request), '-o', str(capture)],
                                    cwd=str(SCRIPTS), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if not success:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('RoCEv2', result.stdout)
                return []
            self.assertEqual(result.returncode, 0, result.stdout)
            with RawPcapReader(str(capture)) as reader:
                return [wire for wire, _ in reader]

    def test_native_write_and_read_match_profile_wire_bytes(self):
        for version in (4, 6):
            for operation in ('write', 'read-request', 'read-response', 'read'):
                config = {'ip_version': version, 'operation': operation, 'qps': 2, 'message_size': 2000,
                          'mtu': 512, 'flow_stats': True, 'stream_mode': 'burst', 'total_pkts': 2}
                native = self.simulate(config)
                streams = load_streams('--ip-version', str(version), '--operation', operation, '--qps', '2',
                                       '--message-size', '2000', '--mtu', '512', '--flow-stats',
                                       '--stream-mode', 'burst', '--total-pkts', '2')
                reference = RoCEv2Test()._simulate(streams)
                self.assertEqual(Counter(native), Counter(reference), (version, operation))
                for wire in native:
                    self.assertEqual(wire[-4:], invariant_crc(wire))

    def test_native_vlan_gid_qp_and_psn_configuration(self):
        config = {'ip_version': 4, 'src_gid': '::ffff:16.0.0.1', 'dst_gid': '::ffff:48.0.0.1',
                  'direction': 1, 'vlan': 7, 'qps': 2, 'qp_base': 100, 'psn': 0xffffff,
                  'src_ip_step': 1, 'message_size': 17, 'stream_mode': 'chain', 'flow_stats': False}
        native = self.simulate(config)
        self.assertEqual(len(native), 2)
        self.assertEqual([Ether(wire)[BTH].dqpn for wire in native], [100, 101])
        self.assertEqual([Ether(wire)[IP].dst for wire in native], ['16.0.0.1', '16.0.0.2'])
        self.assertEqual(Ether(native[0])[BTH].psn, 0xffffff)
        self.assertEqual(Ether(native[0])[BTH].padcount, 3)

    def test_native_invalid_configs_fail_rpc_without_traffic(self):
        for config in ({'qps': 0}, {'pps': 0}, {'vlan': 4095}, {'qps': True}, {'unknown': 1},
                       {'src_ip': 'not-an-ip'}, {'mtu': 64, 'ip_version': 6},
                       {'flow_stats': True, 'pg_id_base': 0xffffffff, 'qps': 2}):
            self.simulate(config, success=False)


class RoCEv2ClientApiTest(unittest.TestCase):
    def setUp(self):
        self.client = STLClient()
        self.client.conn = mock.Mock()
        self.client.conn.is_connected.return_value = True

    def test_build_uses_native_rpc_without_profile_construction(self):
        result = {'streams': [], 'pgids': [100], 'expected_stats': {}}
        self.client._transmit = mock.Mock(return_value=RC_OK(result))
        self.assertEqual(self.client.build_rocev2_streams({'qps': 1}), result)
        self.client._transmit.assert_called_once_with('build_rocev2_streams',
                                                     {'config': {'qps': 1}, 'first_stream_id': 1})

    def test_add_native_uses_acquired_profile_and_returns_metadata(self):
        self.client.psv = mock.Mock()
        self.client.psv.validate.return_value = [PortProfileID('0.roce')]
        self.client.ports = {0: mock.Mock()}
        response = {'stream_ids': [1, 2], 'pgids': [100], 'expected_stats': {}}
        self.client.ports[0].add_rocev2_streams.return_value = RC_OK(response)
        self.assertEqual(self.client.add_rocev2_streams({'qps': 1}, ports=['0.roce']), response)
        self.client.ports[0].add_rocev2_streams.assert_called_once_with({'qps': 1}, profile_id='roce')

    def test_stats_get_clear_and_validation_use_native_counters(self):
        self.client.pgid_stats = mock.Mock()
        self.client.pgid_stats.get_stats.return_value = {'flow_stats': {100: {
            'tx_pkts': {0: 10}, 'rx_pkts': {1: 10}, 'tx_bytes': {0: 1000}}}}
        stats = self.client.get_rocev2_stats([100, 100])
        self.client.pgid_stats.get_stats.assert_called_once_with(pgid_list=[100], rpc_method='get_rocev2_stats')
        self.assertIn(100, stats['flow_stats'])
        result = self.client.validate_rocev2_stats({'100': {'tx_pkts': 10, 'tx_bytes': 1000}}, 0, 1)
        self.assertTrue(result['passed'])
        self.client.pgid_stats = mock.Mock()
        self.client.clear_rocev2_stats()
        self.client.pgid_stats.clear_stats.assert_called_once_with(clear_flow_stats=True, clear_latency_stats=False)

    def test_stats_missing_data_and_invalid_pgids_fail(self):
        self.client.pgid_stats = mock.Mock()
        self.client.pgid_stats.get_stats.return_value = {}
        result = self.client.validate_rocev2_stats({100: {'tx_pkts': 10, 'tx_bytes': 1000}}, 0, 1)
        self.assertFalse(result['passed'])
        for value in ([], [-1], [True], ['100'], [0x100000000]):
            with self.assertRaises(Exception):
                self.client.get_rocev2_stats(value)

    def test_native_stats_batching_never_adds_an_unfiltered_query(self):
        for count in (1, 1152, 1153, 2304):
            rpc = mock.Mock()
            rpc.transmit.return_value = RC_OK({'flow_stats': {}, 'latency': {}, 'ver_id': {}})
            CPgIdStats(rpc).get_stats(list(range(count)), rpc_method='get_rocev2_stats')
            calls = rpc.transmit.call_args_list
            self.assertEqual(len(calls), (count + 1151) // 1152)
            self.assertTrue(all(call.args[0] == 'get_rocev2_stats' and call.kwargs['params']['pgids'] for call in calls))
            self.assertEqual([pgid for call in calls for pgid in call.kwargs['params']['pgids']], list(range(count)))

    def test_native_stats_wire_translation_and_clear_baseline(self):
        rpc = mock.Mock()
        def sample(count):
            values = {'tp': {'0': count}, 'rp': {'1': count},
                      'tb': {'0': count * 100}, 'rb': {'1': count * 100}}
            for field in ('rbs', 'rps', 'tbs', 'tps'):
                values[field] = {'0': 0, '1': 0}
            return RC_OK({'flow_stats': {'100': values}, 'ver_id': {'100': 1}, 'latency': {}})
        current = [10]
        def query(method, params=None):
            if method == 'get_active_pgids':
                return RC_OK({'ids': {'flow_stats': [100], 'latency': []}})
            return sample(current[0])
        rpc.transmit.side_effect = query
        self.client.pgid_stats = CPgIdStats(rpc)
        self.assertEqual(self.client.get_rocev2_stats([100])['flow_stats'][100]['tx_pkts'][0], 10)
        self.client.clear_rocev2_stats()
        current[0] = 20
        result = self.client.validate_rocev2_stats({100: {'tx_pkts': 10, 'tx_bytes': 1000}}, 0, 1)
        self.assertTrue(result['passed'], result)
        self.assertEqual(rpc.transmit.call_args.args[0], 'get_rocev2_stats')

    def make_port(self):
        port = STLPort.__new__(STLPort)
        port.port_id = 1; port.handler = 'owned-handler'; port.ctx = mock.Mock()
        port.state = port.STATE_IDLE; port.streams = {}; port.profile_stream_list = {}; port.profile_state_list = {}
        port.next_available_id = 1; port.has_rx_streams = False
        port.is_acquired = mock.Mock(return_value=True); port.is_writeable = mock.Mock(return_value=True)
        port.transmit = mock.Mock(return_value=RC_OK({'stream_ids': [10], 'pgids': [100]}))
        return port

    def test_port_refreshes_cache_and_injects_ownership(self):
        port = self.make_port()
        port.profile_state_list = {'roce': port.STATE_IDLE}
        stream = load_streams('--flow-stats')[0]
        def sync():
            port.streams = {10: stream}; port.profile_stream_list = {'roce': [10]}
            return RC_OK()
        port.sync_streams = mock.Mock(side_effect=sync)
        self.assertTrue(port.add_rocev2_streams({'qps': 1}, profile_id='roce'))
        port.transmit.assert_called_once_with('add_rocev2_streams', {
            'handler': 'owned-handler', 'port_id': 1, 'profile_id': 'roce', 'config': {'qps': 1, 'direction': 1}})
        self.assertEqual(port.next_available_id, 11)
        self.assertTrue(port.has_rx_streams)
        self.assertEqual(port.state, port.STATE_STREAMS)

    def test_cache_sync_failure_rolls_back_only_new_ids(self):
        port = self.make_port()
        original = load_streams()[0]
        port.streams = {2: original}; port.profile_stream_list = {'old': [2]}
        port.profile_state_list = {'old': port.STATE_STREAMS}
        port.sync_streams = mock.Mock(return_value=RC_ERR('failed'))
        rc = port.add_rocev2_streams({}, profile_id='roce')
        self.assertFalse(rc)
        self.assertEqual(port.streams, {2: original})
        self.assertEqual(port.profile_stream_list, {'old': [2]})
        port.transmit.assert_called_with('remove_stream', {
            'handler': 'owned-handler', 'port_id': 1, 'profile_id': 'roce', 'stream_id': 10})

    def test_cache_rollback_transport_failure_is_reported(self):
        port = self.make_port()
        port.sync_streams = mock.Mock(return_value=RC_ERR('sync failed'))
        port.transmit.side_effect = [RC_OK({'stream_ids': [10], 'pgids': [100]}), RuntimeError('link lost')]
        result = port.add_rocev2_streams({})
        self.assertFalse(result)
        self.assertIn('link lost', result.err())
        self.assertEqual(port.streams, {})


if __name__ == '__main__':
    unittest.main()
