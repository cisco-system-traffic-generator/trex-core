#!/router/bin/python

from client.trex_hltapi import CTRexHltApiPktBuilder
import os
import unittest

gen_stream = CTRexHltApiPktBuilder.generate_stream

def compare_yamls(yaml1, yaml2):
    if type(yaml1) is not str:
        raise Exception("yaml1 is '%s', expected str" % type(yaml1))
    if type(yaml2) is not str:
        raise Exception("yaml2 is '%s', expected str" % type(yaml2))
    i = 0
    for line1, line2 in zip(yaml1.strip().split('\n'), yaml2.strip().split('\n')):
        i += 1
        if line1 != line2:
            raise Exception('yamls are not equal starting from line %s:\n%s\n\t<->\n%s' % (i, line1.strip(), line2.strip()))

class CTRexHltApi_Test(unittest.TestCase):
    ''' Checks correct HLTAPI creation of packet/VM '''

    def setUp(self):
        self.golden_yaml = None
        self.test_yaml = None

    def tearDown(self):
        compare_yamls(self.golden_yaml, self.test_yaml)

    # Eth/IP/TCP, all values default, no VM instructions
    def test_default(self):
        test_stream = gen_stream()
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
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

    # Eth/IP/TCP, ip src and dest is changed by VM
    def test_ip_ranges(self):
        test_stream = gen_stream(ip_src_addr = '192.168.1.1',
                                 ip_src_mode = 'increment',
                                 ip_src_count = 5,
                                 ip_dst_addr = '5.5.5.5',
                                 ip_dst_count = 2,
                                 ip_dst_mode = 'random')
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGrxPAqAEBBQUFBQQAAFAAAAABAAAAAVAAD+UqSwAAISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 3232235777
        max_value: 3232235781
        min_value: 3232235777
        name: ip_src
        op: inc
        size: 4
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: ip_src
        pkt_offset: 26
        type: write_flow_var
      - init_value: 84215045
        max_value: 84215046
        min_value: 84215045
        name: ip_dst
        op: random
        size: 4
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: ip_dst
        pkt_offset: 30
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''

'''

    # Eth / IP / TCP, tcp ports are changed by VM
    def test_tcp_ranges(self):
        test_stream = gen_stream(tcp_src_port_mode = 'decrement',
                                 tcp_src_port_count = 10,
                                 tcp_dst_port_mode = 'random',
                                 tcp_dst_port_count = 10,
                                 tcp_dst_port = 1234)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGusUAAAAAwAAAAQQABNIAAAABAAAAAVAAD+UxewAAISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 1024
        max_value: 1024
        min_value: 1015
        name: tcp_src
        op: dec
        size: 2
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: tcp_src
        pkt_offset: 34
        type: write_flow_var
      - init_value: 1234
        max_value: 1243
        min_value: 1234
        name: tcp_dst
        op: random
        size: 2
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: tcp_dst
        pkt_offset: 36
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    # Eth / IP / UDP, udp ports are changed by VM
    def test_udp_ranges(self):
        # UDP is not set, expecting ignore of wrong UDP arguments
        gen_stream(udp_src_port_mode = 'qwerqwer',
                   udp_src_port_count = 'weqwer',
                   udp_src_port = 'qwerqwer',
                   udp_dst_port_mode = 'qwerqwe',
                   udp_dst_port_count = 'sfgsdfg',
                   udp_dst_port = 'sdfgsdfg')
        # UDP is set, expecting fail due to wrong UDP arguments
        with self.assertRaises(Exception):
            gen_stream(l4_protocol = 'udp',
                       udp_src_port_mode = 'qwerqwer',
                       udp_src_port_count = 'weqwer',
                       udp_src_port = 'qwerqwer',
                       udp_dst_port_mode = 'qwerqwe',
                       udp_dst_port_count = 'sfgsdfg',
                       udp_dst_port = 'sdfgsdfg')
        # generate it already with correct arguments
        test_stream = gen_stream(l4_protocol = 'udp',
                                 udp_src_port_mode = 'decrement',
                                 udp_src_port_count = 10,
                                 udp_src_port = 1234,
                                 udp_dst_port_mode = 'increment',
                                 udp_dst_port_count = 10,
                                 udp_dst_port = 1234)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEARuroAAAAAwAAAAQTSBNIAHsmgISEhISEhISEhISEhISEhISEhISEhIQ==
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 1234
        max_value: 1234
        min_value: 1225
        name: udp_src
        op: dec
        size: 2
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: udp_src
        pkt_offset: 34
        type: write_flow_var
      - init_value: 1234
        max_value: 1243
        min_value: 1234
        name: udp_dst
        op: inc
        size: 2
        type: flow_var
      - add_value: 0
        is_big_endian: true
        name: udp_dst
        pkt_offset: 36
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    # Eth/IP/TCP, packet length is changed in VM by frame_size
    def test_pkt_len_by_framesize(self):
        # frame_size_step should be 1 (as default)
        with self.assertRaises(Exception):
            test_stream = gen_stream(length_mode = 'decrement',
                                     frame_size_min = 100,
                                     frame_size_max = 3000,
                                     frame_size_step = 20)
        # just check errors, no compare to golden
        gen_stream(length_mode = 'increment',
                   frame_size_min = 100,
                   frame_size_max = 3000)
        test_stream = gen_stream(length_mode = 'decrement',
                                 frame_size_min = 100,
                                 frame_size_max = 3000)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEAGusUAAAAAwAAAAQQAAFAAAAABAAAAAVAAD+U8AwAAISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEh
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 3000
        max_value: 3000
        min_value: 100
        name: fv_rand
        op: dec
        size: 2
        type: flow_var
      - name: fv_rand
        type: trim_pkt_size
      - add_value: -14
        is_big_endian: true
        name: fv_rand
        pkt_offset: 16
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    # Eth/IP/UDP, packet length is changed in VM by l3_length
    def test_pkt_len_by_l3length(self):
        # l3_length_step should be 1
        with self.assertRaises(Exception):
            gen_stream(l4_protocol = 'udp',
                       length_mode = 'random',
                       l3_length_min = 100,
                       l3_length_max = 400,
                       l3_length_step = 20)
        test_stream = gen_stream(l4_protocol = 'udp',
                                 length_mode = 'random',
                                 l3_length_min = 100,
                                 l3_length_max = 400)
        self.test_yaml = test_stream.dump_to_yaml(self.yaml_save_location(), test_stream)
        self.golden_yaml = '''
- name: stream-0
  stream:
    enabled: true
    isg: 0.0
    mode:
      pps: 1
      type: continuous
    packet:
      binary: AAAAAAAAAAABAAABCABFAAAyAAAAAEARuroAAAAAwAAAAQQAAFABfCfxISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEhISEh
      meta: ''
    rx_stats:
      enabled: false
    self_start: true
    vm:
      instructions:
      - init_value: 114
        max_value: 414
        min_value: 114
        name: fv_rand
        op: random
        size: 2
        type: flow_var
      - name: fv_rand
        type: trim_pkt_size
      - add_value: -14
        is_big_endian: true
        name: fv_rand
        pkt_offset: 16
        type: write_flow_var
      - add_value: -34
        is_big_endian: true
        name: fv_rand
        pkt_offset: 38
        type: write_flow_var
      - pkt_offset: 14
        type: fix_checksum_ipv4
      split_by_var: ''
'''

    def yaml_save_location(self):
        return os.devnull
        # debug/deveopment, comment line above
        return '/tmp/%s.yaml' % self._testMethodName


