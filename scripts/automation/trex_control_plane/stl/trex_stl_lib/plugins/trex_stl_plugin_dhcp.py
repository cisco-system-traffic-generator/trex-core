"""
DHCP plugin implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>
    
Author:
  Itay Marom 

"""
from trex_stl_plugin import STLPlugin
from scapy.layers.dhcp import DHCP, BOOTP
from scapy.all import *

import random
import re

class STLPluginDHCP(STLPlugin):
    def __init__ (self, ctx, mac_range):
        
        self.ctx = ctx
        
        if type(mac_range) not in [tuple, list]:
            self.err("mac_range should be either tuple or list")
        
        # register
        ctx._register_plugin(self)
        
        self.clients = {}
        for mac in mac_range:
            mac_bytes = self.__mac2bytes(mac)
            client = DHCPClient(ctx, mac_bytes)
            self.clients[client.get_xid()] = client
        
        
######### implement-needed functions #########
    def consume (self, pkt):
        # if not BOOTP 
        if 'BOOTP' not in scapy_pkt:
            return False
            
        # it's a DHCP packet - forward it
        xid = scapy_pkt['BOOTP'].xid
        if not xid in self.clients.keys():
            return False
            
        # let the client consume the packet
        self.clients[xid].pipe.forward_pkt(pkt)
        return True
        
        
    def run (self):
        for k ,v in self.clients.items():
            self.ctx._add_task(v.run())
        
        
######### internal #########
    def __mac2bytes (self, mac):
        if type(mac) == str and re.match("[0-9a-f]{2}([:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", mac.lower()):
            mac = [int(b, 16) for b in mac.split(':')]
                                     
        if type(mac) in (list, tuple, bytes) and len(mac) == 6 and all([x <= 0xff for x in mac]):
            return bytes(mac)
        
        self.err('invalid MAC format: {}'.format(mac))
    
        
################### internal ###################
class DHCPClient(object):
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    
    def __init__ (self, ctx, mac):
        self.mac_bytes  = mac
        self.ctx        = ctx
        self.pipe       = ctx.pipe()
        
        self.xid        = random.getrandbits(32)
        
        self.state = 'INIT'
        
    def get_xid (self):
        return self.xid
        
    def run (self):
        # main state machine loop
        self.state = 'INIT'
        
        while True:
            
            if self.state == 'INIT':
                print('client: {0} is sending discovery'.format(self.xid))
                
                # send a discover message
                self.pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                                 /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))
                
                # move to the next state
                #state = 'SELECTING'
                # wait for either response or a timeout
                next = yield self.pipe.wait_for_next_pkt(timeout_sec = 3)
                
                if self.pipe.is_timeout(next):
                    print('DHCP client: {0} got timeout on discovery - retry'.format(self.xid))
                    
                # packet
                else:
                    pass
                
                continue
                
            elif self.state == 'SELECTING':
                pass
                
            elif self.state == 'DONE':
                break
            
