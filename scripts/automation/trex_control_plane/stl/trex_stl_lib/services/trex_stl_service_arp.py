"""
ARP service implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>

Author:
  Itay Marom 

"""
from .trex_stl_service import STLService, STLServiceFilter
from scapy.layers.l2 import Ether, ARP

from collections import defaultdict

class STLServiceFilterARP(STLServiceFilter):
    '''
        Service filter for ARP services
    '''
    def __init__ (self):
        self.services = defaultdict(list)


    def add (self, service):
        # forward packets according to the DST IP being resolved
        self.services[service.dst_ip].append(service)


    def lookup (self, scapy_pkt):
        # not ARP
        if 'ARP' not in scapy_pkt:
            return []
            
        # forward only ARP response
        if scapy_pkt['ARP'].op != 2:
            return []
        
        return self.services.get( scapy_pkt['ARP'].psrc, [] )


class STLServiceARP(STLService):
    '''
        ARP service - generate ARP requests
    '''

    def __init__ (self, ctx, dst_ip, src_ip = '0.0.0.0', timeout_sec = 3, verbose_level = STLService.ERROR):
        
        # init the base object
        super(STLServiceARP, self).__init__(verbose_level)

        self.src_mac     = ctx.get_src_mac()
        self.src_ip      = src_ip
        self.dst_ip      = dst_ip
        self.timeout_sec = timeout_sec
        
        self.record = None


    def get_filter_type (self):
        return STLServiceFilterARP


    def run (self, pipe):
        '''
            Will execute ARP request
        '''
        
        self.log("ARP: ---> who has '{0}' ? tell '{1}' ".format(self.dst_ip, self.src_ip))

        pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(psrc  = self.src_ip, pdst  = self.dst_ip, hwsrc = self.src_mac)
        
        # send the ARP request
        pipe.async_tx_pkt(pkt)
        
        # wait for RX packet
        pkts = yield pipe.async_wait_for_pkt(time_sec = self.timeout_sec)
        if not pkts:
            self.log("ARP: <--- timeout for '{0}'".format(self.dst_ip))
            self.record = ARPRecord(self.src_ip, self.dst_ip)
            return
            
        # parse record
        response = pkts[0]['pkt']
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

  