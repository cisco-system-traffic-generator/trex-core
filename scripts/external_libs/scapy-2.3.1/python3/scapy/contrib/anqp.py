from scapy.layers.dot11 import *
import struct

gas_types = {
    10: 'GAS Initial Request',
    11: 'GAS Initial Response',
    12: 'GAS Comeback Request',
    13: 'GAS Comeback Response'
}

class Dot11uGASAction(Packet):
    name = "802.11 GAS Action Frame"
    fields_desc = [
                    ByteField("category", 4), # Public action frame
                    ByteEnumField('action', 10, gas_types),
                    ByteField("dialogToken", 0),
                    ]

class Dot11uGASInitialRequest(Packet):
    name = "802.11 GAS Initial Request ANQP Frame"
    fields_desc = [
                    ByteField("tagNumber", 0x6c), # ANQP
                    ByteField("tagLength", 2), # Tag length as from 802.11u
                    BitField("pameBI", 0, 1),
                    BitField("queryResponse", 0, 7),
                    ByteField("advProtocol", 0), # ANQP
                    LenField("queryLength", 0, fmt='<H'),
                    ]
    def post_build(self, p, pay):
        if not self.queryLength:
            self.queryLength = len(pay)
            p = p[:-2]+struct.pack('<H', self.queryLength)
        return p+pay

class Dot11uGASInitialResponse(Packet):
    name = "802.11 GAS Initial Response ANQP Frame"
    fields_desc = [
                    LEShortField("status", 0),
                    LEShortField("comebackDelay", 0),
                    ByteField("tagNumber", 0x6c), # ANQP
                    ByteField("tagLength", 2), # Tag length as from 802.11u
                    BitField("pameBI", 0, 1),
                    BitField("queryResponse", 0, 7),
                    ByteField("advProtocol", 0), # ANQP
                    LenField("responseLength", 0, '<H'),
                    ]

class ANQPElementHeader(Packet):
    name = "ANQP Element"
    fields_desc = [
                    LEShortField("element_id", 0),
                    ]

class ANQPQueryList(Packet):
    name = "ANQP Query List"
    fields_desc = [
                    LEFieldLenField("length", None, length_of="element_ids"),
                    FieldListField("element_ids",[],LEShortField('element_id', 0), 
                                   length_from=lambda pkt:pkt.length)
                    ]

class ANQPCapabilityList(Packet):
    name = "ANQP Capability List"
    fields_desc = [
                    LEFieldLenField("length", None, length_of="element_ids"),
                    FieldListField("element_ids",[],LEShortField('element_id', 0), 
                                   length_from=lambda pkt:pkt.length)
                    ]

class ANQPVenueName(Packet):
    name = "ANQP Venue Name"
    fields_desc = [
                    FieldLenField("length", None, "data", "<H", adjust=lambda pkt,x:x+2),
                    ByteField("venue_group", 0),
                    ByteField("venue_type", 0),
                    StrLenField("data", "", length_from=lambda x: x.length-2)
                    ]

bind_layers( Dot11, Dot11uGASAction, subtype=0x0d, type=0)
bind_layers( Dot11uGASAction, Dot11uGASInitialRequest, category=4, action=10)
bind_layers( Dot11uGASAction, Dot11uGASInitialResponse, category=4, action=11)
bind_layers( Dot11uGASInitialResponse, ANQPElementHeader, advProtocol=0, tagNumber=0x6c)
bind_layers( Dot11uGASInitialRequest, ANQPElementHeader, advProtocol=0, tagNumber=0x6c)
bind_layers( ANQPElementHeader, ANQPQueryList, element_id=256)
bind_layers( ANQPElementHeader, ANQPCapabilityList, element_id=257)
bind_layers( ANQPElementHeader, ANQPVenueName, element_id=258)
bind_layers( ANQPCapabilityList, ANQPElementHeader)
bind_layers( ANQPVenueName, ANQPElementHeader)