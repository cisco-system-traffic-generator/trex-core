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
from .trex_stl_dhcp_parser import DHCPParser

from ..trex_stl_exceptions import STLError

from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP

from collections import defaultdict
import random
import struct
import socket
import re

# create a single global parser
parser = DHCPParser()

           
def ipv4_num_to_str (num):
    return socket.inet_ntoa(struct.pack('!L', num))
    
            
class STLServiceFilterDHCP(STLServiceFilter):
    '''
        Service filter for DHCP services
    '''
    def __init__ (self):
        self.services = defaultdict(list)
        
        
    def add (self, service):
        self.services[service.get_xid()].append(service)
        
        
    def lookup (self, pkt):
        # correct XID is enough to verify ownership
        xid = parser.parse(pkt).xid
        
        return self.services.get(xid, [])

        
    def get_bpf_filter (self):
        return 'udp port 67 or 68'
    
    
        

    
################### internal ###################
class STLServiceDHCP(STLService):
    
    # DHCP states
    INIT, SELECTING, REQUESTING, BOUND = range(4)
    
    def __init__ (self, mac, verbose_level = STLService.ERROR):

        # init the base object
        super(STLServiceDHCP, self).__init__(verbose_level)
        
        self.xid = random.getrandbits(32)
        
        self.mac        = mac
        self.mac_bytes  = self.mac2bytes(mac)
        
        self.record = None
        self.state  = 'INIT'
        
        
    def get_filter_type (self):
        return STLServiceFilterDHCP

        
    def get_xid (self):
        return self.xid
        
        
    def get_mac (self):
        return self.mac
        
        
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
        self.state   = 'INIT'
        self.record  = None
        self.retries = 5
        
        while True:
            
            # INIT state
            if self.state == 'INIT':

                self.retries -= 1
                if self.retries <= 0:
                    break
                    
                self.log('DHCP: {0} ---> DISCOVERY'.format(self.mac))
                
                # send a discover message
                yield pipe.async_tx_pkt(parser.disc(self.xid, self.mac_bytes))
                
                self.state = 'SELECTING'
                continue
                
                
            # SELECTING state
            elif self.state == 'SELECTING':
                
                # wait until packet arrives or timeout occurs
                pkts = yield pipe.async_wait_for_pkt(3)
                pkts = [pkt['pkt'] for pkt in pkts]
                
                # filter out the offer responses
                offers = [parser.parse(pkt) for pkt in pkts]
                offers = [offer for offer in offers if offer.options['message-type'] == parser.OFFER]
                        
                if not offers:
                    self.log('DHCP: {0} *** timeout on offers - retries left: {1}'.format(self.mac, self.retries), level = STLService.ERROR)
                    self.state = 'INIT'
                    continue
                    
                    
                offer = offers[0]
                self.log("DHCP: {0} <--- OFFER from '{1}' with address '{2}' ".format(self.mac, ipv4_num_to_str(offer.options['server_id']), ipv4_num_to_str(offer.yiaddr)))
                
                self.state = 'REQUESTING'
                continue
                
                
            # REQUEST state
            elif self.state == 'REQUESTING':
                self.retries = 5
                
                self.log('DHCP: {0} ---> REQUESTING'.format(self.mac))
                
                # send the request
                yield pipe.async_tx_pkt(parser.req(self.xid, self.mac_bytes, offer.yiaddr))
                
                # wait for response
                pkts = yield pipe.async_wait_for_pkt(3)
                pkts = [pkt['pkt'] for pkt in pkts]
                
                # filter out the offer responses
                acknacks = [parser.parse(pkt) for pkt in pkts]
                acknacks = [acknack for acknack in acknacks if acknack.options['message-type'] in (parser.ACK, parser.NACK)]
                
                if not acknacks:
                    self.log('DHCP: {0} *** timeout on ack - retries left: {1}'.format(self.mac, self.retries), level = STLService.ERROR)
                    self.state = 'INIT'
                    continue
                
                # by default we choose the first one... usually there should be only one response
                acknack = acknacks[0]
                
                
                if acknack.options['message-type'] == parser.ACK:
                    self.log("DHCP: {0} <--- ACK from '{1}' to address '{2}' ".format(self.mac, ipv4_num_to_str(offer.options['server_id']), ipv4_num_to_str(offer.yiaddr)))
                    self.state = 'BOUND'
                else:
                    self.log("DHCP: {0} <--- NACK from '{1}'".format(self.mac, ipv4_num_to_str(offer.options['server_ip'])))
                    self.state = 'INIT'
                    
                
                continue
                
                
            elif self.state == 'BOUND':
                
                # parse the offer and save it
                self.record = self.DHCPRecord(offer)
                break
            
            
          
    def _release (self, pipe):
        '''
            Release the DHCP lease
        '''
        self.log('DHCP: {0} ---> RELEASING'.format(self.mac))
        
        yield pipe.async_tx_pkt(Ether(dst=self.record.server_mac)/IP(src=self.record.client_ip,dst=self.record.server_ip)/UDP(sport=68,dport=67) \
                                /BOOTP(ciaddr=self.record.client_ip,chaddr=self.mac_bytes,xid=self.xid) \
                                /DHCP(options=[("message-type","release"),("server_id",self.record.server_ip), "end"]))
        
        self.record = None
        

    def get_record (self):
        '''
            Returns a DHCP record
        '''
        return self.record


    class DHCPRecord(object):
            
        def __init__ (self, offer):
            
            self.server_mac = offer.src
            self.client_mac = offer.dst
            
            options = offer.options
            
            self.server_ip = ipv4_num_to_str(options['server_id']) if 'server_id' in options else 'N/A'
            self.subnet    = ipv4_num_to_str(options['subnet_mask']) if 'subnet_mask' in options else 'N/A'
            self.client_ip = ipv4_num_to_str(offer.yiaddr)
            
            self.domain     = options.get('domain', 'N/A')
            self.lease      = options.get('lease-time', 'N/A')
            
            
        def __str__ (self):
            return "ip: {0}, server_ip: {1}, subnet: {2}, domain: {3}, lease_time: {4}".format(self.client_ip, self.server_ip, self.subnet, self.domain, self.lease)


