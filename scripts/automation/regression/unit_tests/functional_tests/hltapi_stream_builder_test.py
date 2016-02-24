#!/router/bin/python

import os
import unittest
from trex_stl_lib.trex_stl_hltapi import STLHltStream
from trex_stl_lib.trex_stl_types import validate_type
from nose.plugins.attrib import attr
from nose.tools import nottest

def compare_yamls(yaml1, yaml2):
    validate_type('yaml1', yaml1, str)
    validate_type('yaml2', yaml2, str)
    i = 0
    for line1, line2 in zip(yaml1.strip().split('\n'), yaml2.strip().split('\n')):
        i += 1
        assert line1 == line2, 'yamls are not equal starting from line %s:\n%s\n    Golden    <->    Generated\n%s' % (i, line1.strip(), line2.strip())


class CTRexHltApi_Test(unittest.TestCase):
    ''' Checks correct HLTAPI creation of packet/VM '''

    def setUp(self):
        self.golden_yaml = None
        self.test_yaml = None

    def tearDown(self):
        compare_yamls(self.golden_yaml, self.test_yaml)

    # Eth/IP/TCP, all values default, no VM instructions + test MACs correction
    def test_hlt_basic(self):
        STLHltStream(mac_src = 'a0:00:01:::01', mac_dst = '0d 00 01 00 00 01',
                     mac_src2 = '{00 b0 01 00 00 01}', mac_dst2 = 'd0.00.01.00.00.01')
        with self.assertRaises(Exception):
            STLHltStream(mac_src2 = '00:00:00:00:00:0k')
        with self.assertRaises(Exception):
            STLHltStream(mac_dst2 = '100:00:00:00:00:00')
        # wrong encap
        with self.assertRaises(Exception):
            STLHltStream(l2_encap = 'ethernet_sdfgsdfg')
        # all default values
        test_stream = STLHltStream()
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGusUAAAAAwAAAAQQAAFAAAAABAAAAAVAAD+U1/QAAISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions: []
      split_by_var: ''
'''

    # Eth/IP/TCP, test MAC fields VM, wait for masking of variables for MAC
    @nottest
    def test_macs_vm(self):
        test_stream = STLHltStream(name = 'stream-0', )
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
TBD
'''


    # Eth/IP/TCP, ip src and dest is changed by VM
    def test_ip_ranges(self):
        # running on single core not implemented yet
        with self.assertRaises(Exception):
            test_stream = STLHltStream(split_by_cores = 'single',
                                       ip_src_addr = '192.168.1.1',
                                       ip_src_mode = 'increment',
                                       ip_src_count = 5,)
        # wrong type
        with self.assertRaises(Exception):
            test_stream = STLHltStream(split_by_cores = 12345,
                                       ip_src_addr = '192.168.1.1',
                                       ip_src_mode = 'increment',
                                       ip_src_count = 5,)

        test_stream = STLHltStream(split_by_cores = 'duplicate',
                                   ip_src_addr = '192.168.1.1',
                                   ip_src_mode = 'increment',
                                   ip_src_count = 5,
                                   ip_dst_addr = '5.5.5.5',
                                   ip_dst_count = 2,
                                   ip_dst_mode = 'random',
                                   name = 'test_ip_ranges',
                                   rate_pps = 1)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_ip_ranges
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      pps: 1.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGrxPAqAEBBQUFBQQAAFAAAAABAAAAAVAAD+UqSwAAISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 0
        max_value: 4
        min_value: 0
        name: inc_4_4_1
        op: inc
        size: 4
        step: 1
        type: flow_var
      - add_value: 3232235777
        is_big_endian: true
        name: inc_4_4_1
        pkt_offset: 26
        type: write_flow_var
      - init_value: 0
        max_value: 4294967295
        min_value: 0
        name: ip_dst_random
        op: random
        size: 4
        step: 1
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: ip_dst_random
        pkt_offset: 30
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    # Eth / IP / TCP, tcp ports are changed by VM
    def test_tcp_ranges(self):
        test_stream = STLHltStream(tcp_src_port_mode = 'decrement',
                                   tcp_src_port_count = 10,
                                   tcp_dst_port_mode = 'random',
                                   tcp_dst_port_count = 10,
                                   tcp_dst_port = 1234,
                                   name = 'test_tcp_ranges',
                                   rate_pps = '2')
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_tcp_ranges
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      pps: 2.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGusUAAAAAwAAAAQQABNIAAAABAAAAAVAAD+UxewAAISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 9
        max_value: 9
        min_value: 0
        name: dec_2_9_1
        op: dec
        size: 2
        step: 1
        type: flow_var
      - add_value: 1015
        is_big_endian: true
        name: dec_2_9_1
        pkt_offset: 34
        type: write_flow_var
      - init_value: 0
        max_value: 65535
        min_value: 0
        name: tcp_dst_random
        op: random
        size: 2
        step: 1
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: tcp_dst_random
        pkt_offset: 36
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: dec_2_9_1
'''

    # Eth / IP / UDP, udp ports are changed by VM
    def test_udp_ranges(self):
        # UDP is not set, expecting ignore of wrong UDP arguments
        STLHltStream(udp_src_port_mode = 'qwerqwer',
                     udp_src_port_count = 'weqwer',
                     udp_src_port = 'qwerqwer',
                     udp_dst_port_mode = 'qwerqwe',
                     udp_dst_port_count = 'sfgsdfg',
                     udp_dst_port = 'sdfgsdfg')
        # UDP is set, expecting fail due to wrong UDP arguments
        with self.assertRaises(Exception):
            STLHltStream(l4_protocol = 'udp',
                         udp_src_port_mode = 'qwerqwer',
                         udp_src_port_count = 'weqwer',
                         udp_src_port = 'qwerqwer',
                         udp_dst_port_mode = 'qwerqwe',
                         udp_dst_port_count = 'sfgsdfg',
                         udp_dst_port = 'sdfgsdfg')
        # generate it already with correct arguments
        test_stream = STLHltStream(l4_protocol = 'udp',
                                   udp_src_port_mode = 'decrement',
                                   udp_src_port_count = 10,
                                   udp_src_port = 1234,
                                   udp_dst_port_mode = 'increment',
                                   udp_dst_port_count = 10,
                                   udp_dst_port = 1234,
                                   name = 'test_udp_ranges',
                                   rate_percent = 20,)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_udp_ranges
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 20.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEARuroAAAAAwAAAAQTSBNIAHsmgISEhISEhISEhISEhISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 9
        max_value: 9
        min_value: 0
        name: dec_2_9_1
        op: dec
        size: 2
        step: 1
        type: flow_var
      - add_value: 1225
        is_big_endian: true
        name: dec_2_9_1
        pkt_offset: 34
        type: write_flow_var
      - init_value: 0
        max_value: 9
        min_value: 0
        name: inc_2_9_1
        op: inc
        size: 2
        step: 1
        type: flow_var
      - add_value: 1234
        is_big_endian: true
        name: inc_2_9_1
        pkt_offset: 36
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: dec_2_9_1
'''

    # Eth/IP/TCP, packet length is changed in VM by frame_size
    def test_pkt_len_by_framesize(self):
        # just check errors, no compare to golden
        STLHltStream(length_mode = 'increment',
                     frame_size_min = 100,
                     frame_size_max = 3000)
        test_stream = STLHltStream(length_mode = 'decrement',
                                   frame_size_min = 100,
                                   frame_size_max = 3000,
                                   name = 'test_pkt_len_by_framesize',
                                   rate_bps = 1000)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_pkt_len_by_framesize
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      bps_L2: 1000.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAuqAAAAAEAGr00AAAAAwAAAAQQAAFAAAAABAAAAAVAAD+UwiwAAISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEh
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 3000
        max_value: 3000
        min_value: 100
        name: pkt_len
        op: dec
        size: 2
        step: 1
        type: flow_var
      - name: pkt_len
        type: trim_pkt_size
      - add_value: -14
        is_big_endian: true
        name: pkt_len
        pkt_offset: 16
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: pkt_len
'''

    # Eth/IP/UDP, packet length is changed in VM by l3_length
    def test_pkt_len_by_l3length(self):
        test_stream = STLHltStream(l4_protocol = 'udp',
                                   length_mode = 'random',
                                   l3_length_min = 100,
                                   l3_length_max = 400,
                                   name = 'test_pkt_len_by_l3length')
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_pkt_len_by_l3length
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAGQAAAAAEARuVwAAAAAwAAAAQQAAFABfCaTISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEh
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 114
        max_value: 414
        min_value: 114
        name: pkt_len
        op: random
        size: 2
        step: 1
        type: flow_var
      - name: pkt_len
        type: trim_pkt_size
      - add_value: -14
        is_big_endian: true
        name: pkt_len
        pkt_offset: 16
        type: write_flow_var
      - add_value: -34
        is_big_endian: true
        name: pkt_len
        pkt_offset: 38
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    # Eth/IP/TCP, with vlan, no VM
    def test_vlan_basic(self):
        with self.assertRaises(Exception):
            STLHltStream(l2_encap = 'ethernet_ii',
                         vlan_id = 'sdfgsdgf')
        test_stream = STLHltStream(l2_encap = 'ethernet_ii')
        assert ':802.1Q:' not in test_stream.get_pkt_type(), 'Default packet should not include dot1q'

        test_stream = STLHltStream(name = 'test_vlan_basic', l2_encap = 'ethernet_ii_vlan')
        assert ':802.1Q:' in test_stream.get_pkt_type(), 'No dot1q in packet with encap ethernet_ii_vlan'
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_vlan_basic
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABgQAwAAgARQAALgAAAABABrrJAAAAAMAAAAEEAABQAAAAAQAAAAFQAA/leEMAACEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions: []
      split_by_var: ''
'''

    # Eth/IP/TCP, with 4 vlan
    def test_vlan_multiple(self):
        # default frame size should be not enough
        with self.assertRaises(Exception):
            STLHltStream(vlan_id = [1, 2, 3, 4])
        test_stream = STLHltStream(name = 'test_vlan_multiple', frame_size = 100,
                                   vlan_id = [1, 2, 3, 4], # can be either array or string separated by spaces
                                   vlan_protocol_tag_id = '8100 0x8100')
        pkt_layers = test_stream.get_pkt_type()
        assert '802.1Q:' * 4 in pkt_layers, 'No four dot1q layers in packet: %s' % pkt_layers
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_vlan_multiple
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABgQAwAYEAMAKBADADgQAwBAgARQAARgAAAABABrqxAAAAAMAAAAEEAABQAAAAAQAAAAFQAA/l6p0AACEhISEhISEhISEhISEhISEhISEhISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions: []
      split_by_var: ''
'''

    # Eth/IP/TCP, with 5 vlans and VMs on vlan_id
    def test_vlan_vm(self):
        test_stream = STLHltStream(name = 'test_vlan_vm', frame_size = 100,
                                   vlan_id = '1 2 1000 4 5',                          # 5 vlans
                                   vlan_id_mode = 'increment fixed decrement random', # 5th vlan will be default fixed
                                   vlan_id_step = 2,                                  # 1st vlan step will be 2, others - default 1
                                   vlan_id_count = [4, 1, 10],                        # 4th independent on count, 5th will be fixed
                                   )
        pkt_layers = test_stream.get_pkt_type()
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        assert '802.1Q:' * 5 in pkt_layers, 'No five dot1q layers in packet: %s' % pkt_layers
        self.golden_yaml = '''
- name: test_vlan_vm
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABgQAwAYEAMAKBADPogQAwBIEAMAUIAEUAAEIAAAAAQAa6tQAAAADAAAABBAAAUAAAAAEAAAABUAAP5SzkAAAhISEhISEhISEhISEhISEhISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 0
        max_value: 6
        min_value: 0
        name: dec_2_3_2
        op: inc
        size: 2
        step: 2
        type: flow_var
      - add_value: 1
        is_big_endian: true
        mask: 4095
        name: dec_2_3_2
        pkt_cast_size: 2
        pkt_offset: 14
        shift: 0
        type: write_mask_flow_var
      - init_value: 9
        max_value: 9
        min_value: 0
        name: dec_2_9_1
        op: dec
        size: 2
        step: 1
        type: flow_var
      - add_value: 991
        is_big_endian: true
        mask: 4095
        name: dec_2_9_1
        pkt_cast_size: 2
        pkt_offset: 22
        shift: 0
        type: write_mask_flow_var
      - init_value: 0
        max_value: 65535
        min_value: 0
        name: vlan_id_random
        op: random
        size: 2
        step: 1
        type: flow_var
      - add_value: 0
        is_big_endian: true
        mask: 4095
        name: vlan_id_random
        pkt_cast_size: 2
        pkt_offset: 26
        shift: 0
        type: write_mask_flow_var
      split_by_var: dec_2_9_1
'''


    # Eth/IPv6/TCP, no VM
    def test_ipv6_basic(self):
        # default frame size should be not enough
        with self.assertRaises(Exception):
            STLHltStream(l3_protocol = 'ipv6')
        # error should not affect
        STLHltStream(ipv6_src_addr = 'asdfasdfasgasdf')
        # error should affect
        with self.assertRaises(Exception):
            STLHltStream(l3_protocol = 'ipv6', ipv6_src_addr = 'asdfasdfasgasdf')
        test_stream = STLHltStream(name = 'test_ipv6_basic', l3_protocol = 'ipv6', length_mode = 'fixed', l3_length = 150, )
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_ipv6_basic
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABht1gAAAAAG4GQP6AAAAAAAAAAAAAAAAAABL+gAAAAAAAAAAAAAAAAAAiBAAAUAAAAAEAAAABUAAP5ctLAAAhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISE=
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions: []
      split_by_var: ''
'''

    # Eth/IPv6/UDP, VM on ipv6 fields
    def test_ipv6_src_dst_ranges(self):
        test_stream = STLHltStream(name = 'test_ipv6_src_dst_ranges', l3_protocol = 'ipv6', l3_length = 150, l4_protocol = 'udp',
                                   ipv6_src_addr = '1111:2222:3333:4444:5555:6666:7777:8888',
                                   ipv6_dst_addr = '1111:1111:1111:1111:1111:1111:1111:1111',
                                   ipv6_src_mode = 'increment', ipv6_src_step = 5, ipv6_src_count = 10,
                                   ipv6_dst_mode = 'decrement', ipv6_dst_step = '1111:1111:1111:1111:1111:1111:0000:0011', ipv6_dst_count = 150,
                                   )
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location())
        self.golden_yaml = '''
- name: test_ipv6_src_dst_ranges
  stream:
    action_count: 0
    enabled: true
    flags: 3
    isg: 0.0
    mode:
      percentage: 10.0
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABht1gAAAAAG4RQBERIiIzM0REVVVmZnd3iIgRERERERERERERERERERERBAAAUABucjohISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISE=
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 0
        max_value: 45
        min_value: 0
        name: inc_4_9_5
        op: inc
        size: 4
        step: 5
        type: flow_var
      - add_value: 2004322440
        is_big_endian: true
        name: inc_4_9_5
        pkt_offset: 34
        type: write_flow_var
      - init_value: 2533
        max_value: 2533
        min_value: 0
        name: dec_4_149_17
        op: dec
        size: 4
        step: 17
        type: flow_var
      - add_value: 286328620
        is_big_endian: true
        name: dec_4_149_17
        pkt_offset: 50
        type: write_flow_var
      split_by_var: dec_4_149_17
'''





    def yaml_save_location(self):
        #return os.devnull
        # debug/deveopment, comment line above
        return '/tmp/%s.yaml' % self._testMethodName


