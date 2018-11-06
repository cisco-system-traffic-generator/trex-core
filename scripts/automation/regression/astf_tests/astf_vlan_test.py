from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from trex.astf.api import *
import time
import pprint
from nose.tools import assert_raises, nottest
from scapy.all import *


class ASTFVLAN_Test(CASTFGeneral_Test):
    ''' Checks for VLAN in latency and traffic '''

    @classmethod
    def setUpClass(cls):
        cls.tx_port, cls.rx_port = CTRexScenario.ports_map['bi'][0]
        cls.ports = [cls.tx_port, cls.rx_port]
        c = CTRexScenario.astf_trex
        tx_port_vlan = c.get_port(cls.tx_port).get_vlan_cfg()
        rx_port_vlan = c.get_port(cls.rx_port).get_vlan_cfg()
        assert tx_port_vlan == rx_port_vlan
        cls.port_vlan = tx_port_vlan
        cls.vlan = 30
        cls.exp_packets = 20
        cls.src_ip = '123.123.123.123'


    @classmethod
    def tearDownClass(cls):
        c = CTRexScenario.astf_trex
        if cls.port_vlan:
            c.set_vlan(cls.ports, cls.port_vlan)
        else:
            c.clear_vlan(cls.ports)
        c.remove_all_captures()


    def setup_vlan(self, with_vlan, bpf_ip):
        c = self.astf_trex
        if with_vlan:
            bpf = 'vlan %s and %s' % (self.vlan, bpf_ip)
            c.set_vlan(self.ports, vlan = self.vlan)
        else:
            bpf = bpf_ip
            c.clear_vlan(self.ports)

        capt_id = c.start_capture(rx_ports = self.rx_port, limit = self.exp_packets, bpf_filter = bpf)['id']
        debug_capt_id = c.start_capture(rx_ports = self.rx_port, limit = self.exp_packets)['id']
        return capt_id, debug_capt_id


    def verify_vlan(self, with_vlan, cap_ids, is_ipv4 = True):
        c = self.astf_trex
        filtered_pkts = []
        capture_id, debug_capture_id = cap_ids
        c.stop_capture(capture_id, filtered_pkts)
        vlan_str = 'with' if with_vlan else 'without'
        ip_str = 'IPv4' if is_ipv4 else 'IPv6'
        print('Expected %s %s pkts %s VLAN, got %s' % (self.exp_packets, ip_str, vlan_str, len(filtered_pkts)))

        if len(filtered_pkts) < self.exp_packets:
            print('Got following packets:')
            all_pkts = []
            c.stop_capture(debug_capture_id, all_pkts)
            for pkt in all_pkts:
                print(Ether(pkt['binary']).command())
            self.fail('Did not get expected %s packets' % self.exp_packets)
        else:
            c.stop_capture(debug_capture_id)


    def test_latency_vlan(self):
        print('')
        c = self.astf_trex
        src_ip = '123.123.123.123'
        bpf_ip = 'ip src %s' % src_ip

        # no VLAN
        cap_ids = self.setup_vlan(False, bpf_ip)
        c.start_latency(mult = 1000, src_ipv4 = src_ip)
        time.sleep(1)
        c.stop_latency()
        self.verify_vlan(False, cap_ids)

        # with VLAN
        cap_ids = self.setup_vlan(True, bpf_ip)
        c.start_latency(mult = 1000, src_ipv4 = src_ip)
        time.sleep(1)
        c.stop_latency()
        self.verify_vlan(True, cap_ids)


    def get_profile(self, src_ip_start, src_ip_end):
        prog_c = ASTFProgram(stream=False)
        prog_c.send_msg('req')
        prog_c.recv_msg(1)

        prog_s = ASTFProgram(stream=False)
        prog_s.recv_msg(1)
        prog_s.send_msg('resp')

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=[src_ip_start, src_ip_end], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="0.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, ip_gen=ip_gen)
        temp_s = ASTFTCPServerTemplate(program=prog_s)
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        return ASTFProfile(default_ip_gen=ip_gen, templates=template)


    def test_traffic_vlan(self):
        print('')
        c = self.astf_trex
        src_ip_start = '123.123.123.1'
        src_ip_end   = '123.123.123.250'
        src_ip_bpf   = 'ip src net 123.123.123.0/24'
        c.load_profile(self.get_profile(src_ip_start, src_ip_end))

        # no VLAN, IPv4
        cap_ids = self.setup_vlan(False, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_vlan(False, cap_ids)

        # with VLAN, IPv4
        cap_ids = self.setup_vlan(True, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_vlan(True, cap_ids)

        src_ip_bpf   = 'ip6 src net ::123.123.123.0/120'
        # no VLAN, IPv6
        cap_ids = self.setup_vlan(False, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_vlan(False, cap_ids, is_ipv4 = False)

        # with VLAN, IPv6
        cap_ids = self.setup_vlan(True, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_vlan(True, cap_ids, is_ipv4 = False)


