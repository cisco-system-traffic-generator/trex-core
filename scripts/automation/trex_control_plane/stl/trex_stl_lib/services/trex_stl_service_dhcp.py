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
            client = DHCPClient(ctx, mac, self.log)
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
        
    
    def results (self):
        dhcp_results = {}
        for client in self.clients.values():
            dhcp_results[client.mac] = dict(client.results())
            
        return dhcp_results

        
################### internal ###################
class DHCPClient(object):
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    
    def __init__ (self, ctx, mac, log):
        self.mac        = mac
        self.mac_bytes  = self.mac2bytes(mac)
        self.ctx        = ctx
        self.pipe       = ctx.pipe()
        
        self._results   = {'yaddr': None, 'server' : None, 'dg' : None, 'lease' : None, 'domain': None, 'subnet': None}
        self.success    = False
        
        self.xid        = random.getrandbits(32)
        self.log        = log
        
        self.state = 'INIT'
      
        
    def mac2bytes (self, mac):
        if type(mac) != str or not re.match("[0-9a-f]{2}([:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", mac.lower()):
            self.err('invalid MAC format: {}'.format(mac))
          
        return struct.pack('B' * 6, *[int(b, 16) for b in mac.split(':')])
        
        
    def get_xid (self):
        return self.xid
        
    def run (self):
        # main state machine loop
        self.state = 'INIT'
        self.retries = 5
        
        while True:
            
            # INIT state
            if self.state == 'INIT':
                self.log('DHCP: xid {:10} sent DISCOVERY packet'.format(self.xid))
                
                # send a discover message
                self.pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                                 /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))
                
                # give a chance to all the servers to respond
                yield self.pipe.async_wait(0.5)
                
                # wait until packet arrives or timeout occurs
                pkts = yield self.pipe.async_wait_for_pkt(3)
                offers = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] == self.OFFER]
                if not offers:
                    if self.retries > 0:
                        self.log('DHCP: xid {:10} got timeout - retries left: {}'.format(self.xid, self.retries))
                        self.retries -= 1
                        continue
                    else:
                        break
                
                    
                offer = offers[0]
                options = {x[0]:x[1] for x in offer['DHCP options'].options if isinstance(x, tuple)}
                self.log("DHCP: xid {:10} got OFFER from server '{}' of address '{}'".format(self.xid, options['server_id'], offer['BOOTP'].yiaddr))
                
                self._results['server'] = options['server_id']
                self._results['subnet'] = options['subnet_mask']
                self._results['domain'] = options['domain']
                self._results['lease']  = options['lease_time']
                self._results['yaddr']  = offer['BOOTP'].yiaddr
                
                self.state = 'REQUESTING'
                continue
                
                
            # REQUEST state
            elif self.state == 'REQUESTING':
                self.log('DHCP: xid {0} sent REQUEST packet'.format(self.xid))
                
                self.pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                                 /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","request"),("requested_addr", self._results['yaddr']),"end"]))
                
                pkts = yield self.pipe.async_wait_for_pkt(3)
                
                acknacks = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] in (self.ACK, self.NACK)]
                if not acknacks:
                    self.log('DHCP: xid {0} got timeout - retry'.format(self.xid))
                    continue
                
                assert(len(acknacks) == 1)
                
                acknack = acknacks[0]
                options = {x[0]:x[1] for x in acknack['DHCP options'].options if isinstance(x, tuple)}
                
                if options['message-type'] == self.ACK:
                    self.success = True
                else:
                    self.success = False
                    
                self.state = 'DONE'
                
                continue
                
                
            elif self.state == 'DONE':
                break
            
                
    def results (self):
        return self._results if self.success else {}
