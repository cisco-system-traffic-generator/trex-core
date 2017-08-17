from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP
from scapy.packet import NoPayload
from collections import namedtuple

import struct


class ParserError(Exception):
    pass

    
class FastParser(object):
    def __init__ (self, base_pkt):
        self.base_pkt = base_pkt
        self.base_pkt.build()
        self.pkt_bytes = bytes(base_pkt)
        
        self.fields = {}
        
        
    def add_field (self, fullname, name, offset = None, sz = None, fmt = None, getter = None, setter = None):
        '''
            adds a new field to the parser
        '''
        
        info = self.__get_field_info(fullname)
        info['name']     = name
        info['fullname'] = fullname
        
        if offset is not None:
            info['offset'] = offset
            
        if sz is not None:
            info['sz'] = sz
            
        if fmt is not None:
            info['fmt'] = fmt
        
        # custom functions
        info['getter'] = self.def_getter if not getter else getter
        info['setter'] = self.def_setter if not setter else setter
            
        # add the field
        self.fields[name] = info
        
        
    def parse (self, pkt_bytes):
        return ParserInstance(pkt_bytes, self.fields)
    
        
    def clone (self):
        return ParserInstance(self.pkt_bytes, self.fields)
       
        
    def def_getter (self, pkt_bytes, info):
        min_size = info['offset'] + struct.calcsize(info['fmt'])
        
        if len(pkt_bytes) < min_size:
            return None
            #raise ParserError("packet length is '{0}' but parsing '{1}' requires at least '{2}' bytes".format(len(pkt_bytes), info['name'], min_size))
        
        return struct.unpack_from(info['fmt'], pkt_bytes, info['offset'])[0]
        
        
    def def_setter (self, pkt_bytes, info, value):
        
        # sanity
        min_size = info['offset'] + struct.calcsize(info['fmt'])
        
        if len(pkt_bytes) < min_size:
            raise ParserError("packet length is '{0}' but setting '{1}' requires at least '{2}' bytes".format(len(pkt_bytes), info['name'], min_size))
        
        packed = struct.pack(info['fmt'], value)    
        return pkt_bytes[:info['offset']] + packed + pkt_bytes[info['offset'] + len(packed):]
        
        
    def __get_field_info (self, field):
        p = self.base_pkt
        offset = 0
        while p is not None and not isinstance(p, NoPayload):
            offset = p._offset
            for f in p.fields_desc:
                if field == "{}.{}".format(p.name, f.name):
                    return {'offset': offset + f._offset, 'size': f.sz, 'fmt': f.fmt}

            p = p.payload

        raise ValueError('unknown field: {0}'.format(field))
        


class ParserInstance(object):
    def __init__ (self, pkt_bytes, fields):
        self.__dict__['pkt_bytes'] = pkt_bytes
        self.__dict__['fields']    = dict(fields)
        self.__dict__['cache']     = {}
        
        
    def __getattr__ (self, name):
        
        if not name in self.fields:
            raise ValueError("field '{0}' is not registered under the parser".format(name))
        
        # multiple gets will hit the cache
        if name in self.cache:
            return self.cache[name]
            
            
        info              = self.fields[name]
        value             = info['getter'](self.pkt_bytes, info) 
        self.cache[name]  = value
        
        return value
        
        
        
    def __setattr__ (self, name, value):
        
        if not name in self.fields:
            raise ValueError('field {0} is not registered under the parser'.format(name))
        
        # invalidate from the cache
        if name in self.cache:
            del self.cache[name]
            
        info = self.fields[name]
        self.__dict__['pkt_bytes'] = info['setter'](self.pkt_bytes, info, value)
        
        
    def raw (self):
        return self.pkt_bytes
        
        
    def show2 (self):
        Ether(self.pkt_bytes).show2()
        
                                        
                                
        
