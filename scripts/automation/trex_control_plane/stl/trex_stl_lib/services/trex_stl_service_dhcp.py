"""
DHCP service implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>
    
Author:
  Itay Marom 

"""
from .trex_stl_service import STLService
from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP

import random
import struct
import re

class STLServiceDHCP(STLService):
    def __init__ (self, ctx, mac_range):

        super(STLServiceDHCP, self).__init__(ctx)

        if type(mac_range) not in [tuple, list]:
            self.err("mac_range should be either tuple or list")
        
        self.set_verbose(STLService.INFO)
        
        self.clients = {}
        for mac in mac_range:
            mac_bytes = self.__mac2bytes(mac)
            client = DHCPClient(ctx, mac_bytes, self.log)
            self.clients[client.get_xid()] = client
        
        
######### implement-needed functions #########
    def consume (self, scapy_pkt):
        # if not BOOTP 
        if 'BOOTP' not in scapy_pkt:
            return False
            
        # it's a DHCP packet - check if its with an XID that we own
        xid = scapy_pkt['BOOTP'].xid
        if not xid in self.clients.keys():
            return False
            
        # let the client consume the packet
        self.clients[xid].pipe.rx_pkt(scapy_pkt)
        return True
        
        
    def run (self):
        for k ,v in self.clients.items():
            self.ctx._add_task(v.run())
        
        
######### internal #########
    def __mac2bytes (self, mac):
        if type(mac) != str or not re.match("[0-9a-f]{2}([:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", mac.lower()):
            self.err('invalid MAC format: {}'.format(mac))
          
        return struct.pack('B' * 6, *[int(b, 16) for b in mac.split(':')])
        
        #if type(mac) in (list, tuple, bytes) and len(mac) == 6 and all([x <= 0xff for x in mac]):
        #    return bytes(mac)
    
        
################### internal ###################
class DHCPClient(object):
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    
    def __init__ (self, ctx, mac, log):
        self.mac_bytes  = mac
        self.ctx        = ctx
        self.pipe       = ctx.pipe()
        
        self.xid        = random.getrandbits(32)
        self.log        = log
        
        self.state = 'INIT'
        
    def get_xid (self):
        return self.xid
        
    def run (self):
        # main state machine loop
        self.state = 'INIT'
        
        while True:
            
            if self.state == 'INIT':
                self.log('DHCP: xid {0} sent DISCOVERY packet'.format(self.xid))
                
                # send a discover message
                self.pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                                 /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))
                
                # give a chance to all the servers to respond
                #yield self.pipe.async_wait(0.5)
                
                # wait until packet arrives or timeout occurs
                pkts = yield self.pipe.async_wait_for_pkt(3)
                if not pkts:
                    self.log('DHCP: xid {0} got timeout - retry'.format(self.xid))
                    continue
                     
                # so we got packets
                self.log('DHCP: xid {0} got OFFER of length {1}'.format(self.xid, len(pkts)))
                self.state = 'SELECTING'
                continue
                
            elif self.state == 'SELECTING':
                print('SELECTING')
                return
                
            elif self.state == 'DONE':
                break
            
