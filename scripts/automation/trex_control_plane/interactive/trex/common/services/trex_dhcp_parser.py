from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP
from scapy.packet import NoPayload
from collections import namedtuple, OrderedDict

from ..trex_client import  PacketBuffer

from .trex_service_fast_parser import FastParser

import struct

import pprint

class DHCPParser(FastParser):
    
    # message types
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    RELEASE  = 7
    
    
    def __init__ (self):
        base_pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67,chksum=0) \
                         /BOOTP(chaddr=b'123456',xid=55555,yiaddr='1.2.3.4')/DHCP(options=[("message-type","discover"), ("server_id", '1.2.3.4'),"end"])

        FastParser.__init__(self, base_pkt)

        self.add_field('Ethernet.dst', 'dstmac')
        self.add_field('Ethernet.src', 'srcmac')
        
        self.add_field('IP.ihl',      'ihl', fmt = "!B")
        self.add_field('IP.dst',      'dstip')
        self.add_field('IP.src',      'srcip')
        self.add_field('IP.chksum',   'chksum')
        
        self.add_field('BOOTP.xid', 'xid')
        self.add_field('BOOTP.chaddr', 'chaddr')
        self.add_field('BOOTP.ciaddr', 'ciaddr')
        self.add_field('BOOTP.yiaddr', 'yiaddr')
        self.add_field('DHCP options.options', 'options', getter = self.get_options, setter = self.set_options)

        msg_types = [{'id': 1, 'name': 'discover'},
                     {'id': 2, 'name': 'offer'},
                     {'id': 3, 'name': 'request'},
                    ]
        
        
        self.msg_types = {}
        
        for t in msg_types:
            self.msg_types[t['id']]   = t
            self.msg_types[t['name']] = t
            
        
        opts = [{'id': 53, 'name': 'message-type',   'type': 'byte'},
                {'id': 54, 'name': 'server_id',      'type': 'int'},
                {'id': 50, 'name': 'requested_addr', 'type': 'int'},
                {'id': 51, 'name': 'lease-time',     'type': 'int'},
                {'id': 58, 'name': 'renewal_time',   'type': 'int'},
                {'id': 59, 'name': 'rebinding_time', 'type': 'int'},
                {'id': 1,  'name': 'subnet_mask',    'type': 'int'},
                {'id': 15, 'name': 'domain',         'type': 'str'},
               ]
        
        self.opts = {}
        
        for opt in opts:
            self.opts[opt['id']]    = opt
            self.opts[opt['name']]  = opt
            

    def get_options (self, pkt_bytes, info):
        
        # min length
        if len(pkt_bytes) < info['offset']:
            return None
            
        options = pkt_bytes[info['offset']:]

        opt = OrderedDict()
        index = 0
        while index < len(options):

            o  = ord(options[index])
            index += 1

            # end
            if o == 255:
                break

            # pad
            elif o == 0:
                continue

            # fetch length
            olen = ord(options[index])
            index += 1

            # message type
            if o in self.opts:
                ot = self.opts[o]
                if ot['type'] == 'byte':
                    opt[ot['name']] = struct.unpack_from('!B', options, index)[0]
                    
                elif ot['type'] == 'int':
                    opt[ot['name']] = struct.unpack_from('!I', options, index)[0]
                    
                elif ot['type'] == 'str':
                    opt[ot['name']] = struct.unpack_from('!{0}s'.format(olen), options, index)[0]
                    
                else:
                    raise Exception('unknown type: {0}'.format(ot['type']))

            else:
                pass  # we should ignore oprions that we don't require for the protocol and not creash 

            # advance
            index += olen

        return opt


    def set_options (self, pkt_bytes, info, options):

        output = bytes()

        for o, v in options.items():
            if o in self.opts:
                ot = self.opts[o]
                
                # write tag
                output += struct.pack('!B', ot['id'])
                
                # write the size and value
                if ot['type'] == 'byte':
                    output += struct.pack('!B', 1)
                    output += struct.pack('!B', v)
                    
                elif ot['type'] == 'int':
                    output += struct.pack('!B', 4)
                    output += struct.pack('!I', v)
                    
                elif ot['type'] == 'str':
                    output += struct.pack('!B', len(v))
                    output += struct.pack('!{0}s'.format(len(v)), v)
                    
                
                
        # write end
        output += struct.pack('!B', 255)
        
        return pkt_bytes[:info['offset']] + output + pkt_bytes[info['offset'] + len(output):]
    
        
    def disc (self, xid, chaddr):
        '''
            generates a DHCP discovery packet
        '''
        
        # generate a new packet
        obj = self.clone()
        
        obj.options = {'message-type': 1}
        obj.xid = xid
        obj.chaddr = chaddr
        
        return PacketBuffer(obj.raw())
        

    def req (self, xid, chaddr, yiaddr):
        '''
            generate a new request packet
        '''
        
        # generate a new packet
        obj = self.clone()
        
        obj.options = {'message-type': 3, 'requested_addr': yiaddr}
        obj.xid = xid
        obj.chaddr = chaddr
        
        return PacketBuffer(obj.raw())
        
        

    def release (self, xid, client_mac, client_ip, server_mac, server_ip):
        '''
            generate a release request packet
        '''
        
        # generate a new packet
        obj = self.clone()
        
        obj.dstmac = server_mac
        obj.srcmac = client_mac
        
        obj.dstip  = server_ip
        obj.srcip  = client_ip
        
        obj.fix_chksum()
        
        obj.ciaddr = client_ip
        obj.chaddr = client_mac
        obj.xid    = xid
        
        obj.options = {'message-type': 7, 'server_id': server_ip}
        
        return PacketBuffer(obj.raw())
 

