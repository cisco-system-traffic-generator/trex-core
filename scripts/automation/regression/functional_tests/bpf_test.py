import os

import functional_general_test
from trex import CTRexScenario

from scapy.all import *

from ctypes import CDLL, c_char_p, c_int, c_void_p, c_uint32, c_buffer

so_path = os.path.join(CTRexScenario.scripts_path, 'so')

libbpf_path = os.path.join(so_path, 'libbpf-64-debug.so')


try:
    libbpf = CDLL(libbpf_path)
    
    libbpf.bpf_compile.argtypes = [c_char_p]
    libbpf.bpf_compile.restype  = c_void_p
    
    libbpf.bpf_destroy.argtypes = [c_void_p]
    libbpf.bpf_destroy.restype  = None
    
    libbpf.bpf_run.argtypes = [c_void_p, c_char_p, c_uint32]
    libbpf.bpf_run.restype  = c_int
  

except OSError as e:
    libbpf = None
    #raise Exception('{0} is missing. please make sure the BPF shared library is built before executing the test'.format(libbpf_path))

    
class BPF_Test(functional_general_test.CGeneralFunctional_Test):
    def setUp (self):
        if not libbpf:
            self.skip('libbpf-64.so was not found...skipping test')
            
    def test_bpf_arp (self):
        
        pattern = c_buffer(b'arp')
        bpf_h = libbpf.bpf_compile(pattern)
        assert(bpf_h)
        
        try:
            arp = Ether()/ARP()
            c_arp = c_buffer(bytes(arp))
        
            rc = libbpf.bpf_run(bpf_h, c_arp, len(c_arp) - 1)
            assert(rc != 0)
    
        finally:
            libbpf.bpf_destroy(bpf_h)

    def test_bpf_ipv6 (self):

        pattern = c_buffer(b'ip6 src host 2001:0db8:85a3:0042:0000:8a2e:0371:7334')
        bpf_h = libbpf.bpf_compile(pattern)
        assert(bpf_h)

        try:
            c_ip6 = bytes(Ether()/IPv6(src='2001:0db8:85a3:0042:0000:8a2e:0371:7334'))

            rc = libbpf.bpf_run(bpf_h, c_ip6, len(c_ip6) - 1)
            assert(rc != 0)

        finally:
            libbpf.bpf_destroy(bpf_h)
    
            
    def test_bpf_multiple_matches (self):
        
        # build a or'ed filter
        pattern = c_buffer(b'arp or src 1.1.1.1 or udp dst port 112')
        bpf_h = libbpf.bpf_compile(pattern)
        assert(bpf_h)
        
        try:
            c_arp = bytes(Ether()/ARP())
            c_ip  = bytes(Ether()/IP(src='1.1.1.1'))
            c_udp = bytes(Ether()/IP()/UDP(dport=112))
            
            # positive
            for c_test in (c_arp, c_ip, c_udp):
                rc = libbpf.bpf_run(bpf_h, c_test, len(c_test) - 1)
                if not rc:
                    print("*** below packet failed to match pattern '{0}'".format(pattern))
                    Ether(c_test.raw).show2()
                assert(rc != 0)
            
            # negative
            c_dummy = bytes(Ether()/IP()/TCP())
            rc = libbpf.bpf_run(bpf_h, c_dummy, len(c_dummy) - 1)
            assert(rc == 0)
                
        finally:
            libbpf.bpf_destroy(bpf_h)

       
            
    def test_bpf_vlan (self):
        
        # this is not a bug...double vlan and simply adding another clause of 'vlan and arp'
        pattern = c_buffer(b'arp or (vlan and arp) or (vlan and arp)')
        bpf_h = libbpf.bpf_compile(pattern)
        assert(bpf_h)
        
        try:
            c_arp       = c_buffer(bytes(Ether()/ARP()))
            c_vlan_arp  = c_buffer(bytes(Ether()/Dot1Q(vlan=100)/ARP()))
            c_qinq_arp  = c_buffer(bytes(Ether()/Dot1AD(vlan=100)/Dot1Q(vlan=200)/ARP()))
            
            for c_test in (c_arp, c_vlan_arp, c_qinq_arp):
                rc = libbpf.bpf_run(bpf_h, c_test, len(c_test) - 1)
                if not rc:
                    print("*** below packet failed to match pattern '{0}'".format(pattern))
                    Ether(c_test.raw).show2()
                assert(rc != 0)
    
        finally:
            libbpf.bpf_destroy(bpf_h)
