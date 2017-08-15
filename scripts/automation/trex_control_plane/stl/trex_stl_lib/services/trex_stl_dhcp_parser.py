from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP
from scapy.packet import NoPayload
from collections import namedtuple

import struct

class DHCPParser(object):
    
    # messages types
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6
    
    
    def __init__ (self):
        self.base_pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67,chksum=0) \
                              /BOOTP(chaddr=b'123456',xid=55555,yiaddr='1.2.3.4')/DHCP(options=[("message-type","discover"),"end"])
        
        self.base_pkt.build()
         
        self.fields = {}
        for f in ['Ethernet.dst', 'Ethernet.src', 'BOOTP.xid', 'BOOTP.chaddr', 'BOOTP.yiaddr', 'DHCP options.options']:
            self.fields[f] = self.get_field_info(f)
    
        self.msg_types = {1: 'discover', 2: 'offer', 3: 'request'}
    
        self.opt_types = {53: {'name': 'message-type',   'fmt': 'B'},
                          54: {'name': 'server_id',      'fmt': 'I'},
                          50: {'name': 'requested_addr', 'fmt': 'I'},
                          51: {'name': 'lease-time',     'fmt': 'I'},
                          58: {'name': 'renewal_time',   'fmt': 'I'},
                          59: {'name': 'rebinding_time', 'fmt': 'I'},
                          1:  {'name': 'subnet_mask',    'fmt': 'I'},
                          15: {'name': 'domain',         'fmt': 's'},
                         }
           
        
    def get_srcmac(self, pkt):
        try:
            return struct.unpack_from('!6s', pkt, self.fields['Ethernet.src']['offset'])[0]
        except struct.error as e:
            return None
            
            
    def get_dstmac(self, pkt):
        try:
            return struct.unpack_from('!6s', pkt, self.fields['Ethernet.dst']['offset'])[0]
        except struct.error as e:
            return None
            
            
    def get_xid (self, pkt):
        try:
            return struct.unpack_from('!I', pkt, self.fields['BOOTP.xid']['offset'])[0]
        except struct.error as e:
            return None
    
    
    def get_chaddr (self, pkt):
        try:
            return struct.unpack_from('!16s', pkt, self.fields['BOOTP.chaddr']['offset'])[0]
        except struct.error as e:
            return None

            
    def get_yiaddr (self, pkt):
        try:
            return struct.unpack_from('!I', pkt, self.fields['BOOTP.yiaddr']['offset'])[0]
        except struct.error as e:
            return None
    
            
    def parse_as_offer (self, pkt):
        options = self.get_options(pkt)

        if options['message-type'] != self.OFFER:
            return None
        
        # it's an offer
        rc = options
        rc['yiaddr'] = self.get_yiaddr(pkt)
        rc['srcmac'] = self.get_srcmac(pkt)
        rc['dstmac'] = self.get_dstmac(pkt)
        
        return rc
        
        
    def parse_as_acknack (self, pkt):
        options = self.get_options(pkt)

        if options['message-type'] not in (self.ACK, self.NACK):
            return None
        
        return options
        
            
    def get_options (self, pkt):
        info = self.fields['DHCP options.options']
        options = pkt[info['offset']:]
        
        opt = {}
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
            if o in self.opt_types:
                ot = self.opt_types[o]
                fmt = "!{0}{1}".format(olen, ot['fmt'])
                opt[ot['name']] = struct.unpack_from(fmt, options, index)[0]
                
            else:
                raise Exception('unknown option: {0}'.format(o))
        
            # advance
            index += olen
            
        return opt    
        
    def get_field_info (self, field):
        p = self.base_pkt
        offset = 0
        while p is not None and not isinstance(p, NoPayload):
            offset = p._offset
            for f in p.fields_desc:
                if field == "{}.{}".format(p.name, f.name):
                    return {'offset': offset + f._offset, 'size': f.sz, 'fmt': f.fmt}

            p = p.payload

        raise STLError('unknown fields: {0}'.format(field))


class DHCPPacketGenerator(object):
    Field = namedtuple('Field', ['start', 'end'])
    
    XID = Field(46, 50)
    MAC = Field(70, 86)
    
    def __init__ (self):
        self.disc_pkt = bytes(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67,chksum=0) \
                              /BOOTP(chaddr=b'123456',xid=0xffffffff)/DHCP(options=[("message-type","discover"),"end"]))

        self.req_pkt = bytes(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67,chksum=0) \
                         /BOOTP(chaddr=b'123456',xid=0xffffffff)/DHCP(options=[("message-type","request"),("requested_addr", '8.8.8.8'),"end"]))

        #self.release_pkt = bytes(Ether(dst="ff:ff:ff:ff:ff:ff"))/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
        #                    /BOOTP(ciaddr=self.record.client_ip,chaddr=self.mac_bytes,xid=self.xid) \
        #                        /DHCP(options=[("message-type","release"),("server_id",self.record.server_ip), "end"]))
        

    def disc (self, xid, chaddr):
        bin = self.disc_pkt

        # XID
        bin = bin[:self.XID.start] + struct.pack('!I', xid) + bin[self.XID.end:]

        # MAC
        bin = bin[:self.MAC.start] + struct.pack('!16s', chaddr) + bin[self.MAC.end:]

        return bin    


    def req (self, xid, chaddr, req_addr):
        bin = self.req_pkt

        # XID
        bin = bin[:46] + struct.pack('!I', xid) + bin[50:]

        # MAC
        bin = bin[:70] + struct.pack('!16s', chaddr) + bin[86:]

        # req_addr
        bin = bin[:287] + struct.pack('!I', req_addr) + bin[291:]

        return bin
        

    def release (self, xid, chaddr, req_addr):
        bin = self.req_pkt
        
        # XID
        bin = bin[:46] + struct.pack('!I', xid) + bin[50:]

        # MAC
        bin = bin[:70] + struct.pack('!16s', chaddr) + bin[86:]
        
