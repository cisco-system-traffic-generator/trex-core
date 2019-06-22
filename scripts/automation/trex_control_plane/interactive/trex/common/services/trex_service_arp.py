"""
ARP service implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>

Author:
  Itay Marom 

"""
from .trex_service import Service, ServiceFilter
from ..trex_vlan import VLAN
from ..trex_types import listify

from scapy.layers.l2 import Ether, ARP, Dot1Q, Dot1AD
from scapy.layers.inet import IP

from collections import defaultdict


class ServiceFilterARP(ServiceFilter):
    '''
        Service filter for ARP services
    '''
    def __init__ (self):
        self.services = defaultdict(list)

    def add (self, service):
        # forward packets according to the SRC/DST IP
        self.services[(service.src_ip, service.dst_ip, tuple(service.vlan))].append(service)

        
    def lookup (self, pkt):
        
        # use scapy to parse
        scapy_pkt = Ether(pkt)
        
        # not ARP
        if 'ARP' not in scapy_pkt:
            return []
            
        # forward only ARP response
        if scapy_pkt['ARP'].op != 2:
            return []
        
        vlans = VLAN.extract(scapy_pkt)
        
        # ignore VLAN 0 - hash as empty VLAN
        vlans = vlans if vlans != [0] else []
        
        return self.services.get( (scapy_pkt['ARP'].pdst, scapy_pkt['ARP'].psrc, tuple(vlans)), [] ) 

   
    def get_bpf_filter (self):
        # a simple BPF pattern for ARP (this is not duplicate, it is for QinQ)
        return 'arp or (vlan and arp) or (vlan and arp)'
        

class ServiceARP(Service):
    '''
        ARP service - generate ARP requests
    '''

    def __init__ (self, ctx, dst_ip, src_ip = '0.0.0.0', vlan = None, src_mac = None, timeout_sec = 3,  verbose_level = Service.ERROR,fmt=None,trigger_pkt=None):
        
        # init the base object
        super(ServiceARP, self).__init__(verbose_level)

        self.src_ip      = src_ip
        self.dst_ip      = dst_ip
        self.vlan        = VLAN(vlan)
        self.timeout_sec = timeout_sec
        self.fmt         = fmt
        self.trigger_pkt = trigger_pkt

        if src_mac != None:
            self.src_mac = src_mac
        else:
            self.src_mac     = ctx.get_src_mac()

        self.record = None
        
        
    def get_filter_type (self):
        return ServiceFilterARP


    def run (self, pipe):
        '''
            Will execute ARP request
        '''
        
        if self.trigger_pkt != None:
            self.log("ARP: ---> sending provided trigger packet from {0} -> {1}".format(self.trigger_pkt[IP].src, self.trigger_pkt[IP].dst))
            pipe.async_tx_pkt(self.trigger_pkt)

        else:
            self.log("ARP: ---> who has '{0}' ? tell '{1}' ".format(self.dst_ip, self.src_ip))
            pkt = Ether(dst="ff:ff:ff:ff:ff:ff",src=self.src_mac)/ARP(psrc  = self.src_ip, pdst = self.dst_ip, hwsrc = self.src_mac)
        
            # add VLAN to the packet if needed
            self.vlan.embed(pkt, self.fmt)
        
            # send the ARP request
            pipe.async_tx_pkt(pkt)
        
            # wait for RX packet
        pkts = yield pipe.async_wait_for_pkt(time_sec = self.timeout_sec)
        if not pkts:
            self.log("ARP: <--- timeout for '{0}'".format(self.dst_ip))
            self.record = ARPRecord(self.src_ip, self.dst_ip)
            return
            
        # parse record
        response = Ether(pkts[0]['pkt'])

        self.record = ARPRecord(self.src_ip, self.dst_ip, response)
        self.log("ARP: <--- '{0} is at '{1}'".format(self.record.dst_ip, self.record.dst_mac))
        

    def get_record (self):
        return self.record

        
class ARPRecord(object):
    def __init__ (self, src_ip = 'N/A', dst_ip = 'N/A', scapy_pkt = None):
        self.src_ip  = src_ip
        self.dst_ip  = dst_ip
        self.dst_mac = None
        
        if scapy_pkt:
            self.dst_mac = scapy_pkt['ARP'].hwsrc
        
    def __nonzero__ (self):
        return self.dst_mac is not None
        
    __bool__  = __nonzero__
    
    def __str__ (self):
        if self.dst_mac:
            return "Recieved ARP reply from: {0}, hw: {1}".format(self.dst_ip, self.dst_mac)
        else:
            return "Failed to receive ARP response from {0}".format(self.dst_ip)


