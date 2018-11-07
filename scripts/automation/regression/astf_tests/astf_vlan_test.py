from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from trex.astf.api import *
import time
import pprint
from nose.tools import assert_raises, nottest
from scapy.all import *


class ASTFVLAN_Test(CASTFGeneral_Test):
    ''' Checks for VLAN in latency, traffic, resolve and ping '''

    @classmethod
    def setUpClass(cls):
        CASTFGeneral_Test.setUpClass()
        cls.tx_port, cls.rx_port = CTRexScenario.ports_map['bi'][0]
        cls.ports = [cls.tx_port, cls.rx_port]
        c = CTRexScenario.astf_trex
        tx_port_vlan = c.get_port(cls.tx_port).get_vlan_cfg()
        rx_port_vlan = c.get_port(cls.rx_port).get_vlan_cfg()
        if tx_port_vlan != rx_port_vlan:
            print('TX port VLAN != RX port VLAN, clearing')
            cls.conf_vlan(cls.ports, False)
            cls.vlan_prev = None
        else:
            cls.vlan_prev = tx_port_vlan
        cls.vlan = 30
        cls.exp_packets = 20


    @classmethod
    def tearDownClass(cls):
        c = CTRexScenario.astf_trex
        if cls.vlan_prev:
            c.set_vlan(cls.ports, cls.vlan_prev)
        else:
            c.clear_vlan(cls.ports)
        c.remove_all_captures()


    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        if not self.is_loopback:
            self.skip('VLANs need DUT config')
        driver_params = self.get_driver_params()
        if 'no_vlan' in driver_params or 'no_vlan_even_in_software_mode' in driver_params:
            self.skip('Some driver issues with VLANs')


    @classmethod
    def conf_vlan(cls, ports, enabled):
        c = CTRexScenario.astf_trex
        if enabled:
            c.set_vlan(ports, vlan = cls.vlan)
        else:
            c.clear_vlan(ports)


    def conf_environment(self, vlan_enabled, bpf_ip):
        c = self.astf_trex
        self.conf_vlan(self.ports, vlan_enabled)
        if vlan_enabled:
            bpf = 'vlan %s and %s' % (self.vlan, bpf_ip)
        else:
            bpf = bpf_ip

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
        cap_ids = self.conf_environment(False, bpf_ip)
        c.start_latency(mult = 1000, src_ipv4 = src_ip)
        time.sleep(1)
        c.stop_latency()
        self.verify_vlan(False, cap_ids)

        # with VLAN
        cap_ids = self.conf_environment(True, bpf_ip)
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
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_vlan(False, cap_ids)

        # with VLAN, IPv4
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_vlan(True, cap_ids)

        src_ip_bpf   = 'ip6 src net ::123.123.123.0/120'
        # no VLAN, IPv6
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_vlan(False, cap_ids, is_ipv4 = False)

        # with VLAN, IPv6
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_vlan(True, cap_ids, is_ipv4 = False)


    def test_resolve_vlan(self):
        c = self.astf_trex
        self.conf_vlan(self.ports, False)
        c.resolve()
        self.conf_vlan(self.tx_port, True)
        with assert_raises(TRexError):
            c.resolve()
        self.conf_vlan(self.ports, True)
        c.resolve()
        self.conf_vlan(self.tx_port, False)
        with assert_raises(TRexError):
            c.resolve()


    def test_ping_vlan(self):
        print('')
        c = self.astf_trex
        rx_port = c.get_port(self.rx_port)
        if not rx_port.is_l3_mode():
            self.skip('Not IPv4 config')

        dst_ip = rx_port.get_layer_cfg()['ipv4']['src']

        print('TX port without VLAN, RX port without VLAN')
        self.conf_vlan(self.ports, False)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            print('Got reply as expected')
        else:
            self.fail('Got no reply, ping record: %s' % rec)

        print('TX port with VLAN, RX port without VLAN')
        self.conf_vlan(self.tx_port, True)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            self.fail('Got unexpected reply, ping record: %s' % rec)
        else:
            print('Got no reply as expected')

        print('TX port with VLAN, RX port with VLAN')
        self.conf_vlan(self.ports, True)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            print('Got reply as expected')
        else:
            self.fail('Got no reply, ping record: %s' % rec)

        print('TX port without VLAN, RX port with VLAN')
        self.conf_vlan(self.tx_port, False)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            self.fail('Got unexpected reply, ping record: %s' % rec)
        else:
            print('Got no reply as expected')


    def test_config_vlan(self):
        with assert_raises(TRexError):
            self.astf_trex.set_vlan(self.tx_port, [5000])
        with assert_raises(TRexError):
            self.astf_trex.set_vlan(self.tx_port, [-1])
        with assert_raises(TRexError):
            self.astf_trex.set_vlan(self.tx_port, [12, 13]) # astf allows only single VLAN

