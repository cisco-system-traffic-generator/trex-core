#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *

from trex.common.services.trex_service import Service
from trex.common.services.trex_service_arp import ServiceARP
from trex.common.services.trex_service_icmp import ServiceICMP
from trex.common.services.trex_service_dhcp import ServiceDHCP

def ip2num (ip_str):
    return struct.unpack('>L', socket.inet_pton(socket.AF_INET, ip_str))[0]

def num2ip (ip_num):
    return socket.inet_ntoa(struct.pack('>L', ip_num))

def ip_add (ip_str, cnt):
    return num2ip(ip2num(ip_str) + cnt)


class STLServices_Test(CStlGeneral_Test):
    """Tests for services"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)

        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        self.c.connect()
        self.c.reset(ports = [self.tx_port, self.rx_port])

        self.ctx = self.c.create_service_ctx(self.tx_port)
        
        self.percentage = 5 if self.is_virt_nics else 50
        
        # change this for verbose level
        self.vl = Service.ERROR
        #self.vl = Service.INFO
        
        
    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()
            
             
    def test_arp_service (self):
        '''
            test for the ARP service
        '''
        
        attr = self.c.get_port_attr(port = self.tx_port)
        if attr['layer_mode'] != 'IPv4':
            return self.skip('ARP: skipping test for non IPv4 configuration')
        
        dst_ipv4 = attr['dest']
        src_ipv4 = attr['src_ipv4']
        
        assert(is_valid_ipv4(src_ipv4))
        assert(is_valid_ipv4(dst_ipv4))
        
        self.c.set_service_mode(ports = [self.tx_port, self.rx_port])
        
        try:
            # single ARP
            arp = ServiceARP(self.ctx, src_ip = src_ipv4, dst_ip = dst_ipv4, verbose_level = self.vl)
            self.ctx.run(arp)
            
            rec = arp.get_record()
            
            assert rec.src_ip == src_ipv4
            assert rec.dst_ip == dst_ipv4
            assert is_valid_mac(rec.dst_mac)
            
            # timeout ARP
            arp = ServiceARP(self.ctx, src_ip = src_ipv4, dst_ip = '1.2.3.4', verbose_level = self.vl)
            self.ctx.run(arp)

            rec = arp.get_record()
            assert not rec.dst_mac
            
            # multiple ARPs - all the subnet mask
            src_ips = [num2ip((ip2num(src_ipv4) & 0xFFFFFFF0) + i) for i in range(256)]
            src_ips = [x for x in src_ips if x not in (src_ipv4, dst_ipv4)]
            
            arps = [ServiceARP(self.ctx, src_ip = x, dst_ip = dst_ipv4, verbose_level = self.vl) for x in src_ips]
            self.ctx.run(arps)
        
            for arp in arps:
                rec = arp.get_record()
                assert rec.dst_ip == dst_ipv4
                assert is_valid_mac(rec.dst_mac)
                assert rec.dst_mac == arps[0].get_record().dst_mac
                
            
        finally:
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port], enabled = False)

            
            
            
        def test_ping_service (self):
            '''
                test for the Ping IPv4 service
            '''
            pass
