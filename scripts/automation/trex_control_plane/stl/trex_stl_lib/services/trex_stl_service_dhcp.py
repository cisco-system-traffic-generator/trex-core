"""
DHCP service implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>
    
Author:
  Itay Marom 

"""
from .trex_stl_service import STLService, STLServiceFilter
from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP

import random
import struct
import re

class STLServiceFilterDHCP(STLServiceFilter):
    '''
        Service filter for DHCP services
    '''
    def __init__ (self):
        self.services = {}
        
        
    def add (self, service):
        self.services[service.get_xid()] = service
        
        
    def lookup (self, scapy_pkt):
        # if not BOOTP 
        if 'BOOTP' not in scapy_pkt:
            return None
            
        # it's a DHCP packet - check if its with an XID that we own
        xid = scapy_pkt['BOOTP'].xid
        
        return self.services.get(xid, None)

        
################### internal ###################
class STLServiceDHCP(STLService):
    
    # DHCP states
    INIT, SELECTING, REQUESTING, BOUND = range(4)
    
    # messages types
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    
    def __init__ (self, mac, verbose_level = STLService.ERROR):

        # init the base object
        super(STLServiceDHCP, self).__init__(verbose_level)
        
        self.xid = random.getrandbits(32)

        # set the run function to 'acquire'
        self.run        = self._acquire
        
        self.mac        = mac
        self.mac_bytes  = self.mac2bytes(mac)
        
        self._results   = {'yiaddr': None, 'server' : None, 'dg' : None, 'lease' : None, 'domain': None, 'subnet': None}
        self.success    = False
        
        
        
    def get_filter_type (self):
        return STLServiceFilterDHCP

        
    def get_xid (self):
        return self.xid
        
    
    def mac2bytes (self, mac):
        if type(mac) != str or not re.match("[0-9a-f]{2}([:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", mac.lower()):
            self.err('invalid MAC format: {}'.format(mac))
          
        return struct.pack('B' * 6, *[int(b, 16) for b in mac.split(':')])
        

    #########################  protocol state machines  #########################
    
    def run (self, pipe):
        # while running under 'INIT' - perform acquire
        if self.state == 'INIT':
            return self._acquire(pipe)
        elif self.state == 'BOUND':
            return self._release(pipe)
            
        
    def _acquire (self, pipe):
        '''
            Acquire DHCP lease protocol
        '''
        
        # main state machine loop
        self.state = 'INIT'
        self.retries = 5
        
        while True:
            
            # INIT state
            if self.state == 'INIT':

                self.retries -= 1
                if self.retries <= 0:
                    self.state = 'FAIL'
                    break
                    
                self.log('DHCP: {0} ---> DISCOVERY'.format(self.mac))
                
                # send a discover message
                pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                            /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))
                
                self.state = 'SELECTING'
                continue
                
                
            # SELECTING state
            elif self.state == 'SELECTING':
                
                # give a chance to all the servers to respond
                yield pipe.async_wait(0.5)
                
                # wait until packet arrives or timeout occurs
                pkts = yield pipe.async_wait_for_pkt(3)
                
                offers = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] == self.OFFER]
                if not offers:
                    self.log('DHCP: {0} *** timeout - retries left: {1}'.format(self.mac, self.retries))
                    self.state = 'INIT'
                    continue
                    
                    
                offer = offers[0]
                options = {x[0]:x[1] for x in offer['DHCP options'].options if isinstance(x, tuple)}
                self.log("DHCP: {0} <--- OFFER from '{1}' with address '{2}' ".format(self.mac, options['server_id'], offer['BOOTP'].yiaddr))
                
                self._results['server'] = options['server_id']
                self._results['subnet'] = options['subnet_mask']
                self._results['domain'] = options['domain']
                self._results['lease']  = options['lease_time']
                self._results['yiaddr'] = offer['BOOTP'].yiaddr
                
                self.state = 'REQUESTING'
                continue
                
                
            # REQUEST state
            elif self.state == 'REQUESTING':
                self.retries = 5
                
                self.log('DHCP: {0} ---> REQUESTING'.format(self.mac))
                
                pipe.tx_pkt(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                            /BOOTP(chaddr=self.mac_bytes,xid=self.xid)/DHCP(options=[("message-type","request"),("requested_addr", self._results['yiaddr']),"end"]))
                
                pkts = yield pipe.async_wait_for_pkt(3)
                
                acknacks = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] in (self.ACK, self.NACK)]
                if not acknacks:
                    self.log('DHCP: {0} *** timeout - retries left: {1}'.format(self.mac, self.retries))
                    self.state = 'INIT'
                    continue
                
                assert(len(acknacks) == 1)
                
                acknack = acknacks[0]
                options = {x[0]:x[1] for x in acknack['DHCP options'].options if isinstance(x, tuple)}
                
                if options['message-type'] == self.ACK:
                    self.log("DHCP: {0} <--- ACK from '{1}' to address '{2}' ".format(self.mac, self._results['server'], self._results['yiaddr']))
                    self.state = 'BOUND'
                else:
                    self.log("DHCP: {0} <--- NACK from '{1}'".format(self.mac, self._results['server']))
                    self.state = 'INIT'
                    
                
                continue
                
                
            elif self.state == 'BOUND':
                break
            
            
          
    def _release (self, pipe):
        '''
            Release the DHCP lease
        '''
        
    def results (self):
        return self._results if self.success else {}
