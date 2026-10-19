"""Contracts required by TRex's vendored Scapy integration.

Run directly with unittest, or through the functional regression runner.
"""
import contextlib
import base64
import importlib
import io
import os
from pathlib import Path
import pickle
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from trex.stl.api import Ether, IP, IPv6, UDP, STLPktBuilder, STLProfile, STLStream
from trex.utils.pcap import merge_cap_files
import scapy
from scapy.config import conf
from scapy.data import _CacheUnpickler, scapy_data_cache
from scapy.fields import BitField, ConditionalField, FieldLenField, ShortField, StrLenField
from scapy.packet import Packet, Raw
from scapy.utils import RawPcapReader, RawPcapWriter, int2str, str2bytes, str2int, str2ip


class OffsetTestPacket(Packet):
    fields_desc = [BitField('flags', 1, 4), BitField('kind', 2, 4),
                   FieldLenField('length', None, length_of='data', fmt='B'),
                   StrLenField('data', b'', length_from=lambda pkt: pkt.length),
                   ConditionalField(ShortField('tail', 0x1234), lambda pkt: pkt.flags == 1)]


class ScapyUpgradeTest(unittest.TestCase):
    def test_vendored_release(self):
        self.assertEqual(scapy.__version__, '2.8.0')
        self.assertEqual(Path(scapy.__file__).parents[1].name, 'scapy-2.8.0')

    def test_template_defaults_do_not_resolve_network_addresses(self):
        fail = AssertionError('Packet generation attempted network/interface resolution')
        with mock.patch.object(socket, 'getaddrinfo', side_effect=fail), \
                mock.patch.object(conf.route, 'route', side_effect=fail), \
                mock.patch.object(conf.route6, 'route', side_effect=fail), \
                mock.patch.object(conf.neighbor, 'resolve', side_effect=fail), \
                mock.patch('scapy.layers.l2.resolve_iface', side_effect=fail):
            packet = Ether()/IP()/UDP()/b'abc'
            self.assertEqual(bytes(packet).hex(),
                             '00000001000000000002000008004500001f0001000040113acc'
                             '100000013000000100350035000bfb09616263')
            self.assertEqual(bytes(Ether()/IPv6()/UDP())[14 + 8:14 + 24],
                             socket.inet_pton(socket.AF_INET6, '::2'))
            self.assertEqual(UDP().sprintf('%UDP.sport%'), '53')

    def test_variable_and_bit_field_offsets_survive_dissection(self):
        packet = OffsetTestPacket(data=b'abc')/Raw(b'payload')
        wire = bytes(packet)
        self.assertEqual(wire, b'\x12\x03abc\x12\x34payload')
        self.assertEqual(packet.offset_fields,
                         {'flags': 0, 'kind': 0, 'length': 1, 'data': 2, 'tail': 5})
        self.assertEqual(packet.get_field('flags').get_size_bytes(), 0)
        self.assertEqual(packet.get_field('flags').get_size_bits(), 4)
        self.assertEqual(packet.get_field('tail').get_size_bytes(), 2)
        parsed = OffsetTestPacket(wire)
        self.assertEqual(parsed.offset_fields, packet.offset_fields)
        self.assertEqual(parsed._length, 7)
        self.assertEqual(parsed.payload._offset, 7)
        self.assertEqual(bytes(parsed), wire)
        self.assertEqual(bytes(OffsetTestPacket(flags=0, data=b'a')), b'\x02\x01a')

    def test_cached_offsets_are_restored_after_another_packet_build(self):
        packet = OffsetTestPacket(bytes(OffsetTestPacket(data=b'a')))
        expected = packet.offset_fields.copy()
        bytes(OffsetTestPacket(data=b'longer'))
        self.assertNotEqual(packet.get_field('tail')._offset, expected['tail'])
        self.assertEqual(bytes(packet), b'\x12\x01a\x12\x34')
        self.assertEqual(packet.get_field('tail')._offset, expected['tail'])

    def test_copy_and_clone_retain_independent_offset_metadata(self):
        packet = Ether()/IP(options=b'\x01\x01\x01\x01')/UDP()/b'x'
        wire = bytes(packet)
        parsed = Ether(wire)
        for copied in (parsed.copy(), parsed.clone_with(payload=parsed.payload.copy(), **parsed.fields)):
            self.assertEqual(bytes(copied), wire)
            self.assertEqual(copied[UDP]._offset, 38)
            self.assertEqual(copied[IP]._length, 24)
            self.assertEqual(copied.offset_fields, parsed.offset_fields)
            self.assertIsNot(copied.offset_fields, parsed.offset_fields)
        parsed[IP].options = []
        bytes(parsed)
        self.assertEqual(parsed[UDP]._offset, 34)
        builder = STLPktBuilder(pkt=parsed)
        builder.compile()

    def test_payload_offsets_include_post_build_option_padding(self):
        from scapy.layers.inet import IPOption_NOP, TCP
        packet = Ether()/IP(options=[IPOption_NOP()])/TCP(options=[('NOP', None)])/b'x'
        wire = bytes(packet)
        parsed = Ether(wire)
        self.assertEqual(packet[IP]._length, 24)
        self.assertEqual(packet[TCP]._length, 24)
        self.assertEqual(packet[TCP]._offset, 38)
        self.assertEqual(packet[Raw]._offset, 62)
        self.assertEqual(packet[Raw]._offset, parsed[Raw]._offset)

    def test_legacy_binary_conversion_and_mac_inputs(self):
        self.assertEqual(str2bytes('\xff\x01'), b'\xff\x01')
        self.assertEqual(str2int('\xff\x01'), 65281)
        self.assertEqual(int2str(65281), b'\xff\x01')
        self.assertEqual(str2ip(b'\xc0\x00\x02\x01'), '192.0.2.1')
        self.assertEqual(Ether(src='\x00\x01\x02\x03\x04\xff').src, '00:01:02:03:04:ff')

    def test_custom_protocols_and_legacy_mldv2_names(self):
        from scapy.contrib.ipfix import IPFIX
        from scapy.contrib.anqp import ANQPQueryList
        from scapy.layers.iapp import IAPP
        from scapy.layers.inet6 import ICMPv6MLReportV2, MLDv2Addr, ICMPv6MLReport2
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            capwap = importlib.import_module('scapy.contrib.capwap')
        self.assertTrue(bytes(IPFIX()))
        self.assertEqual(bytes(ANQPQueryList(element_ids=[256])), b'\x02\x00\x00\x01')
        self.assertEqual(len(bytes(IAPP())), 16)
        self.assertTrue(bytes(capwap.CAPWAP_CTRL()))
        packet = IPv6()/ICMPv6MLReportV2()/MLDv2Addr(type=4, multicast_addr='ff02::2')
        parsed = IPv6(bytes(packet))
        self.assertEqual(parsed[ICMPv6MLReport2].records_number, 1)
        self.assertEqual(parsed[ICMPv6MLReport2].records[0].dst, 'ff02::2')

    def test_pcap_profile_and_merge_preserve_bytes_and_timestamps(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [str(Path(directory) / name) for name in ('late.pcap', 'early.pcap')]
            packets = [bytes(Ether()/IP(id=2)/UDP()), bytes(Ether()/IP(id=1)/UDP())]
            for path, packet, second in zip(paths, packets, (2, 1)):
                with RawPcapWriter(path, linktype=1) as writer:
                    writer.write_header(None)
                    writer.write_packet(packet, sec=second, usec=123456)
            output = str(Path(directory) / 'merged.pcap')
            merge_cap_files(paths, output)
            with RawPcapReader(output) as reader:
                merged = list(reader)
            self.assertEqual([data for data, _ in merged], list(reversed(packets)))
            self.assertEqual([(meta.sec, meta.usec) for _, meta in merged],
                             [(1, 123456), (2, 123456)])
            self.assertEqual(len(STLProfile.load_pcap(output, ipg_usec=10).get_streams()), 2)

    def test_capture_fetch_preserves_truncated_packet_wire_length(self):
        from trex.common.trex_client import TRexClient
        from trex.common.trex_types import RC_OK
        packet = bytes(Ether()/IP()/UDP())[:20]
        client = TRexClient.__new__(TRexClient)
        client.ctx = mock.Mock()
        client.conn = mock.Mock()
        client.conn.is_connected.return_value = True
        client._transmit = mock.Mock(return_value=RC_OK({
            'pkts': [{'binary': base64.b64encode(packet).decode('ascii'),
                      'ts': 100.25, 'wirelen': 1500}],
            'pending': 0, 'start_ts': 100.0,
        }))
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / 'capture.pcap')
            client.fetch_capture_packets(1, output, pkt_count=1, snaplen=20)
            with RawPcapReader(output) as reader:
                captured = list(reader)
        self.assertEqual(captured[0][0], packet)
        self.assertEqual(captured[0][1].wirelen, 1500)
        self.assertEqual(captured[0][1].caplen, 20)
        self.assertEqual((captured[0][1].sec, captured[0][1].usec), (0, 250000))

    def test_capture_monitor_writes_packet_timestamps(self):
        from trex.console.trex_capture import CaptureMonitorWriterPipe
        packet = bytes(Ether()/IP()/UDP())
        monitor = CaptureMonitorWriterPipe.__new__(CaptureMonitorWriterPipe)
        monitor.start_ts = 100.0
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / 'monitor.pcap')
            with RawPcapWriter(output, linktype=1) as writer:
                writer.write_header(None)
                monitor.writer = writer
                count = monitor.handle_pkts_internal([
                    {'binary': base64.b64encode(packet).decode('ascii'), 'ts': 100.25}])
            with RawPcapReader(output) as reader:
                captured = list(reader)
        self.assertEqual(count, len(packet))
        self.assertEqual(captured[0][0], packet)
        self.assertEqual((captured[0][1].sec, captured[0][1].usec), (0, 250000))

    def test_profile_export_preserves_payload_misidentified_as_dns(self):
        packet = Ether()/IP()/UDP(chksum=0)/(b'x' * 26)
        profile = STLProfile([STLStream(packet=STLPktBuilder(pkt=packet))])
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / 'exported.py')
            profile.dump_to_code(output)
            exported = STLProfile.load_py(output)
        self.assertEqual(profile.to_json(), exported.to_json())


class ScapyCacheTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.cache = Path(self.temp.name) / 'cache'
        self.patch = mock.patch('scapy.main.SCAPY_CACHE_FOLDER', self.cache)
        self.patch.start()
        self.calls = []

        @scapy_data_cache('trex-test')
        def load(filename=None):
            self.calls.append(filename)
            return {'loaded': filename}

        self.load = load
        self.path = self.cache / 'trex-test.pickle'

    def tearDown(self):
        self.patch.stop()
        self.temp.cleanup()

    def test_valid_cache_is_reused(self):
        self.assertEqual(self.load('input'), {'loaded': 'input'})
        self.assertEqual(self.load('input'), {'loaded': 'input'})
        self.assertEqual(self.calls, ['input'])

    def test_cache_stat_permission_error_is_nonfatal(self):
        with mock.patch.object(Path, 'exists', side_effect=PermissionError('stat denied')):
            self.assertEqual(self.load(), {'loaded': None})
        self.assertEqual(self.calls, [None])

    def test_cache_read_and_unlink_permission_errors_are_nonfatal(self):
        self.load()
        original_open = Path.open

        def denied_read(path, mode='r', *args, **kwargs):
            if mode == 'rb':
                raise PermissionError('read denied')
            return original_open(path, mode, *args, **kwargs)

        with mock.patch.object(Path, 'open', denied_read), \
                mock.patch.object(Path, 'unlink', side_effect=PermissionError('unlink denied')):
            self.assertEqual(self.load(), {'loaded': None})
        self.assertEqual(self.calls, [None, None])

    def test_cache_write_permission_error_is_nonfatal(self):
        with mock.patch.object(Path, 'open', side_effect=PermissionError('write denied')):
            self.assertEqual(self.load(), {'loaded': None})
        self.assertFalse(self.path.exists())

    def test_corrupt_cache_falls_back_to_source(self):
        self.cache.mkdir()
        self.path.write_bytes(b'invalid pickle')
        self.assertEqual(self.load(), {'loaded': None})
        self.assertEqual(self.calls, [None])

    def test_restricted_unpickler_rejects_unexpected_classes(self):
        payload = pickle.dumps(complex(1, 2))
        with self.assertRaises(pickle.UnpicklingError):
            _CacheUnpickler(io.BytesIO(payload)).load()
        self.cache.mkdir()
        self.path.write_bytes(payload)
        self.assertEqual(self.load(), {'loaded': None})

    def test_fresh_api_import_survives_inaccessible_cache(self):
        interactive = Path(__file__).resolve().parents[2] / 'trex_control_plane/interactive'
        code = '''
import sys
sys.path.insert(0, sys.argv[1])
import trex
from pathlib import Path
original_exists = Path.exists
def inaccessible(path):
    if path.suffix == '.pickle':
        raise PermissionError('inaccessible cache')
    return original_exists(path)
Path.exists = inaccessible
from trex.stl.api import Ether, IP, UDP, STLPktBuilder
builder = STLPktBuilder(pkt=Ether()/IP()/UDP())
builder.compile()
'''
        environment = dict(os.environ, XDG_CACHE_HOME=str(self.cache))
        result = subprocess.run([sys.executable, '-c', code, str(interactive)],
                                env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))


if __name__ == '__main__':
    unittest.main()
