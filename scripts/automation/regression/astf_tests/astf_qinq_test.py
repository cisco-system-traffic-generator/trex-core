from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from trex.astf.api import *
import time
import pprint
import random
import string
from nose.tools import assert_raises, nottest
from scapy.all import *
import copy

def sortKey(val):
   return min(val)

class ASTFQINQ_Test(CASTFGeneral_Test):
    ''' Checks for QinQ in latency, traffic, resolve and ping '''

    @classmethod
    def setUpClass(cls):
        CASTFGeneral_Test.setUpClass()
        sort_map = copy.deepcopy(CTRexScenario.ports_map['bi'])
        sort_map.sort(key=sortKey)
        cls.tx_port, cls.rx_port = sort_map[0]
        if cls.tx_port > cls.rx_port:
            cls.tx_port, cls.rx_port = cls.rx_port, cls.tx_port
        cls.ports = [cls.tx_port, cls.rx_port]
        cls.inner_vlan = 30
        cls.outer_vlan = 40
        cls.exp_packets = 20


    @classmethod
    def tearDownClass(cls):
        c = CTRexScenario.astf_trex
        c.set_vlan(cls.ports, [cls.outer_vlan, cls.inner_vlan])
        c.remove_all_captures()


    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        if not self.is_loopback:
            self.skip('VLANs need DUT config')
        driver_params = self.get_driver_params()
        if 'no_vlan' in driver_params or 'no_vlan_even_in_software_mode' in driver_params:
            self.skip('Some driver issues with VLANs')


    @classmethod
    def conf_qinq(cls, ports, enabled):
        c = CTRexScenario.astf_trex
        if enabled:
            c.set_vlan(ports, vlan = [cls.outer_vlan, cls.inner_vlan])
        else:
            c.clear_vlan(ports)


    def conf_environment(self, vlan_enabled, bpf_ip):
        c = self.astf_trex
        self.conf_qinq(self.ports, vlan_enabled)
        if vlan_enabled:
            bpf = 'vlan %s && vlan %s && %s' % (self.outer_vlan, self.inner_vlan, bpf_ip)
        else:
            bpf = bpf_ip
        capt_id = c.start_capture(rx_ports = self.rx_port, limit = self.exp_packets, bpf_filter = bpf)['id']
        debug_capt_id = c.start_capture(rx_ports = self.rx_port, limit = self.exp_packets)['id']
        return capt_id, debug_capt_id


    def verify_qinq(self, with_qinq, cap_ids, is_ipv4 = True):
        c = self.astf_trex
        filtered_pkts = []
        capture_id, debug_capture_id = cap_ids
        c.stop_capture(capture_id, filtered_pkts)
        qinq_str = 'with' if with_qinq else 'without'
        ip_str = 'IPv4' if is_ipv4 else 'IPv6'
        print('Expected %s %s pkts %s QinQ, got %s' % (self.exp_packets, ip_str, qinq_str, len(filtered_pkts)))

        if len(filtered_pkts) < self.exp_packets:
            print('Got following packets:')
            all_pkts = []
            c.stop_capture(debug_capture_id, all_pkts)
            for pkt in all_pkts:
                print(Ether(pkt['binary']).command())
            self.fail('Did not get expected %s packets' % self.exp_packets)
        else:
            c.stop_capture(debug_capture_id)


    def randomString(self, stringLength=10):
        """Generate a random string of fixed length """
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(stringLength))


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


    def test_traffic_qinq(self):
        print('')
        c = self.astf_trex
        src_ip_start = '123.123.123.1'
        src_ip_end   = '123.123.123.250'
        src_ip_bpf   = 'ip src net 123.123.123.0/24'
        c.load_profile(self.get_profile(src_ip_start, src_ip_end))

        # no QinQ, IPv4
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_qinq(False, cap_ids)

        # with QinQ, IPv4
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000)
        time.sleep(1)
        c.stop()
        self.verify_qinq(True, cap_ids)

        src_ip_bpf   = 'ip6 src net ::123.123.123.0/120'
        # no QinQ, IPv6
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_qinq(False, cap_ids, is_ipv4 = False)

        # with QinQ, IPv6
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True)
        time.sleep(1)
        c.stop()
        self.verify_qinq(True, cap_ids, is_ipv4 = False)

    def test_traffic_qinq_dynamic_profile(self):
        print('')
        print('Creating random name for the dynamic profile')
        random_profile = self.randomString()
        print('Dynamic profile name : %s' % str(random_profile))

        c = self.astf_trex
        src_ip_start = '123.123.123.1'
        src_ip_end   = '123.123.123.250'
        src_ip_bpf   = 'ip src net 123.123.123.0/24'
        c.load_profile(self.get_profile(src_ip_start, src_ip_end), pid_input=str(random_profile))

        # no QinQ, IPv4
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000, pid_input=str(random_profile))
        time.sleep(1)
        c.stop(pid_input=str(random_profile))
        self.verify_qinq(False, cap_ids)

        # with QinQ, IPv4
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000, pid_input=str(random_profile))
        time.sleep(1)
        c.stop(pid_input=str(random_profile))
        self.verify_qinq(True, cap_ids)

        src_ip_bpf   = 'ip6 src net ::123.123.123.0/120'
        # no QinQ, IPv6
        cap_ids = self.conf_environment(False, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True, pid_input=str(random_profile))
        time.sleep(1)
        c.stop(pid_input=str(random_profile))
        self.verify_qinq(False, cap_ids, is_ipv4 = False)

        # with QinQ, IPv6
        cap_ids = self.conf_environment(True, src_ip_bpf)
        c.start(mult = 1000, ipv6 = True, pid_input=str(random_profile))
        time.sleep(1)
        c.stop(pid_input=str(random_profile))
        self.verify_qinq(True, cap_ids, is_ipv4 = False)


    def test_resolve_qinq(self):
        c = self.astf_trex
        if not c.get_resolvable_ports():
            self.skip('No resolvable ports')
        self.conf_qinq(self.ports, False)
        c.resolve()
        self.conf_qinq(self.tx_port, True)
        with assert_raises(TRexError):
            c.resolve()
        self.conf_qinq(self.ports, True)
        c.resolve()
        self.conf_qinq(self.tx_port, False)
        with assert_raises(TRexError):
            c.resolve()


    def test_ping_qinq(self):
        print('')
        c = self.astf_trex
        rx_port = c.get_port(self.rx_port)
        if not rx_port.is_l3_mode():
            self.skip('Not IPv4 config')

        dst_ip = rx_port.get_layer_cfg()['ipv4']['src']

        print('TX port without QinQ, RX port without QinQ')
        self.conf_qinq(self.ports, False)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            print('Got reply as expected')
        else:
            self.fail('Got no reply, ping record: %s' % rec)

        print('TX port with QinQ, RX port without QinQ')
        self.conf_qinq(self.tx_port, True)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            self.fail('Got unexpected reply, ping record: %s' % rec)
        else:
            print('Got no reply as expected')

        print('TX port with QinQ, RX port with QinQ')
        self.conf_qinq(self.ports, True)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            print('Got reply as expected')
        else:
            self.fail('Got no reply, ping record: %s' % rec)

        print('TX port without QinQ, RX port with QinQ')
        self.conf_qinq(self.tx_port, False)
        rec = c.ping_ip(self.tx_port, dst_ip, count = 1, timeout_sec = 0.5)[0]
        if rec.state == rec.SUCCESS:
            self.fail('Got unexpected reply, ping record: %s' % rec)
        else:
            print('Got no reply as expected')


    def test_config_qinq(self):
        with assert_raises(TRexError):
            self.astf_trex.set_vlan(self.tx_port, [5000, 6000])
        with assert_raises(TRexError):
            self.astf_trex.set_vlan(self.tx_port, [-1])
        try:
            self.astf_trex.set_vlan(self.tx_port, [12, 13])
        except TRexError:
            self.fail("Failed to configure QinQ")

