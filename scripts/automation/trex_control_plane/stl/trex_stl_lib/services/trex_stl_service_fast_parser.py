from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP
from scapy.packet import NoPayload
from collections import namedtuple

import struct

# a quick parser which uses scapy to generate an offline offset
# tables.
# this way, we get flexibility like scapy but much faster speed
#
# parsers can derive from this parser and enhance it


class ParserError(Exception):
    '''
        throwed when an error happens in the parser
    '''
    pass

    
class FastParser(object):
    
    '''
        A fast parser based on scapy
        it gets a base packet as a template
        and a list of fields to be used for parsing
        
        any packet being parsed by this parser should
        match the template packet
    '''
    def __init__ (self, base_pkt):
        self.base_pkt = base_pkt

        # build the base packet to generate fields offsets
        self.base_pkt.build()
        
        self.pkt_bytes = bytes(base_pkt)
        
        # by default, no fields are monitored
        self.fields = {}
        
        
    def add_field (self, fullname, name, offset = None, sz = None, fmt = None, getter = None, setter = None):
        '''
            adds a new field to the parser
            this field will be accessible when doing 'parse'
        '''
        
        info = self.__get_field_info(fullname)
        info['name']     = name
        info['fullname'] = fullname
        
        # override offset if specified by user
        if offset is not None:
            info['offset'] = offset
        
        # override size if specified by user
        if sz is not None:
            info['sz'] = sz
            
        # override format if specified by user
        if fmt is not None:
            info['fmt'] = fmt
        
        # use custom functions if those were defined
        info['getter'] = ParserInstance.def_getter if not getter else getter
        info['setter'] = ParserInstance.def_setter if not setter else setter
            
        # add the field
        self.fields[name] = info
        
        
    def parse (self, pkt_bytes):
        '''
            Parse a packet based on the template
            returns a parser instace object with all the monitored fields
        '''
        return ParserInstance(pkt_bytes, self.fields)
    
        
    def clone (self):
        '''
            Clones the base packet (template)
            used for manipulating the base packet for packet generation
        '''
        return ParserInstance(self.pkt_bytes, self.fields)
       
 
        
    def __get_field_info (self, field):
        '''
            Internal function
            used to generate all the data per field
        '''
        
        p = self.base_pkt
        while p is not None and not isinstance(p, NoPayload):
            for f in p.fields_desc:
                if field == "{}.{}".format(p.name, f.name):
                    return {'offset': p._offset + f._offset, 'size': f.sz, 'fmt': f.fmt}

            p = p.payload

        raise ValueError('unknown field: {0}'.format(field))
        


class ParserInstance(object):
    '''
        Parser instance.
        generated when a packet is parsed or cloned.
        
        Contains all the monitored fields as attributes which can be read/write
    '''
    
    def __init__ (self, pkt_bytes, fields):
        self.__dict__['pkt_bytes'] = pkt_bytes
        self.__dict__['fields']    = dict(fields)
        self.__dict__['cache']     = {}
        
        
    def __getattr__ (self, name):
        
        if not name in self.fields:
            raise ValueError("field '{0}' is not registered under the parser".format(name))
        
        # multiple gets will hit the cache - no need to parse again
        if name in self.cache:
            return self.cache[name]


        # get the record and fetch the value, then save it to the cache
        info              = self.fields[name]
        value             = info['getter'](self.pkt_bytes, info) 
        self.cache[name]  = value
        
        return value
        
        
        
    def __setattr__ (self, name, value):
        
        if not name in self.fields:
            raise ValueError('field {0} is not registered under the parser'.format(name))
        
        # invalidate from the cache as we are writing
        if name in self.cache:
            del self.cache[name]
            
        info = self.fields[name]
        self.__dict__['pkt_bytes'] = info['setter'](self.pkt_bytes, info, value)
        
        
    def raw (self):
        return self.pkt_bytes
        
        
    def show2 (self):
        Ether(self.pkt_bytes).show2()
        
                                        
    
    @staticmethod
    def def_getter (pkt_bytes, info):
        '''
            Default field getter
            returns None if the offset is outside the boundary
        '''
        
        min_size = info['offset'] + struct.calcsize(info['fmt'])

        if len(pkt_bytes) < min_size:
            return None

        return struct.unpack_from(info['fmt'], pkt_bytes, info['offset'])[0]


    @staticmethod
    def def_setter (pkt_bytes, info, value):
        '''
            Default field setter
        '''
        
        # sanity
        min_size = info['offset'] + struct.calcsize(info['fmt'])

        if len(pkt_bytes) < min_size:
            raise ParserError("packet length is '{0}' but setting '{1}' requires at least '{2}' bytes".format(len(pkt_bytes), info['name'], min_size))

        packed = struct.pack(info['fmt'], value)    
        return pkt_bytes[:info['offset']] + packed + pkt_bytes[info['offset'] + len(packed):]

