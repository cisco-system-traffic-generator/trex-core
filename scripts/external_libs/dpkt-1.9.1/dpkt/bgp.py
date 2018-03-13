# $Id: bgp.py 76 2011-01-06 15:51:30Z dugsong $
# -*- coding: utf-8 -*-
"""Border Gateway Protocol."""
from __future__ import print_function
from __future__ import absolute_import

import struct
import socket

from . import dpkt
from .decorators import deprecated


# Border Gateway Protocol 4 - RFC 4271
# Communities Attribute - RFC 1997
# Capabilities - RFC 3392
# Route Refresh - RFC 2918
# Route Reflection - RFC 4456
# Confederations - RFC 3065
# Cease Subcodes - RFC 4486
# NOPEER Community - RFC 3765
# Multiprotocol Extensions - 2858

# Message Types
OPEN = 1
UPDATE = 2
NOTIFICATION = 3
KEEPALIVE = 4
ROUTE_REFRESH = 5

# Attribute Types
ORIGIN = 1
AS_PATH = 2
NEXT_HOP = 3
MULTI_EXIT_DISC = 4
LOCAL_PREF = 5
ATOMIC_AGGREGATE = 6
AGGREGATOR = 7
COMMUNITIES = 8
ORIGINATOR_ID = 9
CLUSTER_LIST = 10
MP_REACH_NLRI = 14
MP_UNREACH_NLRI = 15

# Origin Types
ORIGIN_IGP = 0
ORIGIN_EGP = 1
INCOMPLETE = 2

# AS Path Types
AS_SET = 1
AS_SEQUENCE = 2
AS_CONFED_SEQUENCE = 3
AS_CONFED_SET = 4

# Reserved Communities Types
NO_EXPORT = 0xffffff01
NO_ADVERTISE = 0xffffff02
NO_EXPORT_SUBCONFED = 0xffffff03
NO_PEER = 0xffffff04

# Common AFI types
AFI_IPV4 = 1
AFI_IPV6 = 2

# Multiprotocol SAFI types
SAFI_UNICAST = 1
SAFI_MULTICAST = 2
SAFI_UNICAST_MULTICAST = 3

# OPEN Message Optional Parameters
AUTHENTICATION = 1
CAPABILITY = 2

# Capability Types
CAP_MULTIPROTOCOL = 1
CAP_ROUTE_REFRESH = 2

# NOTIFICATION Error Codes
MESSAGE_HEADER_ERROR = 1
OPEN_MESSAGE_ERROR = 2
UPDATE_MESSAGE_ERROR = 3
HOLD_TIMER_EXPIRED = 4
FSM_ERROR = 5
CEASE = 6

# Message Header Error Subcodes
CONNECTION_NOT_SYNCHRONIZED = 1
BAD_MESSAGE_LENGTH = 2
BAD_MESSAGE_TYPE = 3

# OPEN Message Error Subcodes
UNSUPPORTED_VERSION_NUMBER = 1
BAD_PEER_AS = 2
BAD_BGP_IDENTIFIER = 3
UNSUPPORTED_OPTIONAL_PARAMETER = 4
AUTHENTICATION_FAILURE = 5
UNACCEPTABLE_HOLD_TIME = 6
UNSUPPORTED_CAPABILITY = 7

# UPDATE Message Error Subcodes
MALFORMED_ATTRIBUTE_LIST = 1
UNRECOGNIZED_ATTRIBUTE = 2
MISSING_ATTRIBUTE = 3
ATTRIBUTE_FLAGS_ERROR = 4
ATTRIBUTE_LENGTH_ERROR = 5
INVALID_ORIGIN_ATTRIBUTE = 6
AS_ROUTING_LOOP = 7
INVALID_NEXT_HOP_ATTRIBUTE = 8
OPTIONAL_ATTRIBUTE_ERROR = 9
INVALID_NETWORK_FIELD = 10
MALFORMED_AS_PATH = 11

# Cease Error Subcodes
MAX_NUMBER_OF_PREFIXES_REACHED = 1
ADMINISTRATIVE_SHUTDOWN = 2
PEER_DECONFIGURED = 3
ADMINISTRATIVE_RESET = 4
CONNECTION_REJECTED = 5
OTHER_CONFIGURATION_CHANGE = 6
CONNECTION_COLLISION_RESOLUTION = 7
OUT_OF_RESOURCES = 8


class BGP(dpkt.Packet):
    """Border Gateway Protocol.

    BGP is an inter-AS routing protocol.
    See more about the BGP on \
    https://en.wikipedia.org/wiki/Border_Gateway_Protocol

    Attributes:
        __hdr__: Header fields of BGP.
        #TODO
    """

    __hdr__ = (
        ('marker', '16s', '\xff' * 16),
        ('len', 'H', 0),
        ('type', 'B', OPEN)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.data = self.data[:self.len - self.__hdr_len__]
        if self.type == OPEN:
            self.data = self.open = self.Open(self.data)
        elif self.type == UPDATE:
            self.data = self.update = self.Update(self.data)
        elif self.type == NOTIFICATION:
            self.data = self.notifiation = self.Notification(self.data)
        elif self.type == KEEPALIVE:
            self.data = self.keepalive = self.Keepalive(self.data)
        elif self.type == ROUTE_REFRESH:
            self.data = self.route_refresh = self.RouteRefresh(self.data)

    class Open(dpkt.Packet):
        __hdr__ = (
            ('v', 'B', 4),
            ('asn', 'H', 0),
            ('holdtime', 'H', 0),
            ('identifier', 'I', 0),
            ('param_len', 'B', 0)
        )
        __hdr_defaults__ = {
            'parameters': []
        }

        def unpack(self, buf):
            dpkt.Packet.unpack(self, buf)
            l = []
            plen = self.param_len
            while plen > 0:
                param = self.Parameter(self.data)
                self.data = self.data[len(param):]
                plen -= len(param)
                l.append(param)
            self.data = self.parameters = l

        def __len__(self):
            return self.__hdr_len__ + sum(map(len, self.parameters))

        def __bytes__(self):
            params = b''.join(map(bytes, self.parameters))
            self.param_len = len(params)
            return self.pack_hdr() + params

        class Parameter(dpkt.Packet):
            __hdr__ = (
                ('type', 'B', 0),
                ('len', 'B', 0)
            )

            def unpack(self, buf):
                dpkt.Packet.unpack(self, buf)
                self.data = self.data[:self.len]

                if self.type == AUTHENTICATION:
                    self.data = self.authentication = self.Authentication(self.data)
                elif self.type == CAPABILITY:
                    self.data = self.capability = self.Capability(self.data)

            class Authentication(dpkt.Packet):
                __hdr__ = (
                    ('code', 'B', 0),
                )

            class Capability(dpkt.Packet):
                __hdr__ = (
                    ('code', 'B', 0),
                    ('len', 'B', 0)
                )

                def unpack(self, buf):
                    dpkt.Packet.unpack(self, buf)
                    self.data = self.data[:self.len]

    class Update(dpkt.Packet):
        __hdr_defaults__ = {
            'withdrawn': [],
            'attributes': [],
            'announced': []
        }

        def unpack(self, buf):
            self.data = buf

            # Withdrawn Routes
            wlen = struct.unpack('>H', self.data[:2])[0]
            self.data = self.data[2:]
            l = []
            while wlen > 0:
                route = RouteIPV4(self.data)
                self.data = self.data[len(route):]
                wlen -= len(route)
                l.append(route)
            self.withdrawn = l

            # Path Attributes
            plen = struct.unpack('>H', self.data[:2])[0]
            self.data = self.data[2:]
            l = []
            while plen > 0:
                attr = self.Attribute(self.data)
                self.data = self.data[len(attr):]
                plen -= len(attr)
                l.append(attr)
            self.attributes = l

            # Announced Routes
            l = []
            while self.data:
                route = RouteIPV4(self.data)
                self.data = self.data[len(route):]
                l.append(route)
            self.announced = l

        def __len__(self):
            return 2 + sum(map(len, self.withdrawn)) + \
                   2 + sum(map(len, self.attributes)) + \
                   sum(map(len, self.announced))

        def __bytes__(self):
            return struct.pack('>H', sum(map(len, self.withdrawn))) + \
                   b''.join(map(bytes, self.withdrawn)) + \
                   struct.pack('>H', sum(map(len, self.attributes))) + \
                   b''.join(map(bytes, self.attributes)) + \
                   b''.join(map(bytes, self.announced))

        class Attribute(dpkt.Packet):
            __hdr__ = (
                ('flags', 'B', 0),
                ('type', 'B', 0)
            )

            @property
            def optional(self):
                return (self.flags >> 7) & 0x1

            @optional.setter
            def optional(self, o):
                self.flags = (self.flags & ~0x80) | ((o & 0x1) << 7)

            @property
            def transitive(self):
                return (self.flags >> 6) & 0x1

            @transitive.setter
            def transitive(self, t):
                self.flags = (self.flags & ~0x40) | ((t & 0x1) << 6)

            @property
            def partial(self):
                return (self.flags >> 5) & 0x1

            @partial.setter
            def partial(self, p):
                self.flags = (self.flags & ~0x20) | ((p & 0x1) << 5)

            @property
            def extended_length(self):
                return (self.flags >> 4) & 0x1

            @extended_length.setter
            def extended_length(self, e):
                self.flags = (self.flags & ~0x10) | ((e & 0x1) << 4)

            def unpack(self, buf):
                dpkt.Packet.unpack(self, buf)

                if self.extended_length:
                    self.len = struct.unpack('>H', self.data[:2])[0]
                    self.data = self.data[2:]
                else:
                    self.len = struct.unpack('B', self.data[:1])[0]
                    self.data = self.data[1:]

                self.data = self.data[:self.len]

                if self.type == ORIGIN:
                    self.data = self.origin = self.Origin(self.data)
                elif self.type == AS_PATH:
                    self.data = self.as_path = self.ASPath(self.data)
                elif self.type == NEXT_HOP:
                    self.data = self.next_hop = self.NextHop(self.data)
                elif self.type == MULTI_EXIT_DISC:
                    self.data = self.multi_exit_disc = self.MultiExitDisc(self.data)
                elif self.type == LOCAL_PREF:
                    self.data = self.local_pref = self.LocalPref(self.data)
                elif self.type == ATOMIC_AGGREGATE:
                    self.data = self.atomic_aggregate = self.AtomicAggregate(self.data)
                elif self.type == AGGREGATOR:
                    self.data = self.aggregator = self.Aggregator(self.data)
                elif self.type == COMMUNITIES:
                    self.data = self.communities = self.Communities(self.data)
                elif self.type == ORIGINATOR_ID:
                    self.data = self.originator_id = self.OriginatorID(self.data)
                elif self.type == CLUSTER_LIST:
                    self.data = self.cluster_list = self.ClusterList(self.data)
                elif self.type == MP_REACH_NLRI:
                    self.data = self.mp_reach_nlri = self.MPReachNLRI(self.data)
                elif self.type == MP_UNREACH_NLRI:
                    self.data = self.mp_unreach_nlri = self.MPUnreachNLRI(self.data)

            def __len__(self):
                if self.extended_length:
                    attr_len = 2
                else:
                    attr_len = 1
                return self.__hdr_len__ + attr_len + len(self.data)

            def __bytes__(self):
                if self.extended_length:
                    attr_len_str = struct.pack('>H', self.len)
                else:
                    attr_len_str = struct.pack('B', self.len)
                return self.pack_hdr() + attr_len_str + bytes(self.data)

            class Origin(dpkt.Packet):
                __hdr__ = (
                    ('type', 'B', ORIGIN_IGP),
                )

            class ASPath(dpkt.Packet):
                __hdr_defaults__ = {
                    'segments': []
                }

                def unpack(self, buf):
                    self.data = buf
                    l = []
                    while self.data:
                        seg = self.ASPathSegment(self.data)
                        self.data = self.data[len(seg):]
                        l.append(seg)
                    self.data = self.segments = l

                def __len__(self):
                    return sum(map(len, self.data))

                def __bytes__(self):
                    return b''.join(map(bytes, self.data))

                class ASPathSegment(dpkt.Packet):
                    __hdr__ = (
                        ('type', 'B', 0),
                        ('len', 'B', 0)
                    )

                    def unpack(self, buf):
                        dpkt.Packet.unpack(self, buf)
                        l = []
                        for i in range(self.len):
                            AS = struct.unpack('>H', self.data[:2])[0]
                            self.data = self.data[2:]
                            l.append(AS)
                        self.data = self.path = l

                    def __len__(self):
                        return self.__hdr_len__ + 2 * len(self.path)

                    def __bytes__(self):
                        as_str = b''
                        for AS in self.path:
                            as_str += struct.pack('>H', AS)
                        return self.pack_hdr() + as_str

            class NextHop(dpkt.Packet):
                __hdr__ = (
                    ('ip', 'I', 0),
                )

            class MultiExitDisc(dpkt.Packet):
                __hdr__ = (
                    ('value', 'I', 0),
                )

            class LocalPref(dpkt.Packet):
                __hdr__ = (
                    ('value', 'I', 0),
                )

            class AtomicAggregate(dpkt.Packet):
                def unpack(self, buf):
                    pass

                def __len__(self):
                    return 0

                def __bytes__(self):
                    return b''

            class Aggregator(dpkt.Packet):
                __hdr__ = (
                    ('asn', 'H', 0),
                    ('ip', 'I', 0)
                )

            class Communities(dpkt.Packet):
                __hdr_defaults__ = {
                    'list': []
                }

                def unpack(self, buf):
                    self.data = buf
                    l = []
                    while self.data:
                        val = struct.unpack('>I', self.data[:4])[0]
                        if (0x00000000 <= val <= 0x0000ffff) or (0xffff0000 <= val <= 0xffffffff):
                            comm = self.ReservedCommunity(self.data[:4])
                        else:
                            comm = self.Community(self.data[:4])
                        self.data = self.data[len(comm):]
                        l.append(comm)
                    self.data = self.list = l

                def __len__(self):
                    return sum(map(len, self.data))

                def __bytes__(self):
                    return b''.join(map(bytes, self.data))

                class Community(dpkt.Packet):
                    __hdr__ = (
                        ('asn', 'H', 0),
                        ('value', 'H', 0)
                    )

                class ReservedCommunity(dpkt.Packet):
                    __hdr__ = (
                        ('value', 'I', 0),
                    )

            class OriginatorID(dpkt.Packet):
                __hdr__ = (
                    ('value', 'I', 0),
                )

            class ClusterList(dpkt.Packet):
                __hdr_defaults__ = {
                    'list': []
                }

                def unpack(self, buf):
                    self.data = buf
                    l = []
                    while self.data:
                        id = struct.unpack('>I', self.data[:4])[0]
                        self.data = self.data[4:]
                        l.append(id)
                    self.data = self.list = l

                def __len__(self):
                    return 4 * len(self.list)

                def __bytes__(self):
                    cluster_str = b''
                    for val in self.list:
                        cluster_str += struct.pack('>I', val)
                    return cluster_str

            class MPReachNLRI(dpkt.Packet):
                __hdr__ = (
                    ('afi', 'H', AFI_IPV4),
                    ('safi', 'B', SAFI_UNICAST),
                )

                def unpack(self, buf):
                    dpkt.Packet.unpack(self, buf)

                    # Next Hop
                    nlen = struct.unpack('B', self.data[:1])[0]
                    self.data = self.data[1:]
                    self.next_hop = self.data[:nlen]
                    self.data = self.data[nlen:]

                    # SNPAs
                    l = []
                    num_snpas = struct.unpack('B', self.data[:1])[0]
                    self.data = self.data[1:]
                    for i in range(num_snpas):
                        snpa = self.SNPA(self.data)
                        self.data = self.data[len(snpa):]
                        l.append(snpa)
                    self.snpas = l

                    if self.afi == AFI_IPV4:
                        Route = RouteIPV4
                    elif self.afi == AFI_IPV6:
                        Route = RouteIPV6
                    else:
                        Route = RouteGeneric

                    # Announced Routes
                    l = []
                    while self.data:
                        route = Route(self.data)
                        self.data = self.data[len(route):]
                        l.append(route)
                    self.data = self.announced = l

                def __len__(self):
                    return self.__hdr_len__ + \
                           1 + len(self.next_hop) + \
                           1 + sum(map(len, self.snpas)) + \
                           sum(map(len, self.announced))

                def __bytes__(self):
                    return self.pack_hdr() + \
                           struct.pack('B', len(self.next_hop)) + \
                           bytes(self.next_hop) + \
                           struct.pack('B', len(self.snpas)) + \
                           b''.join(map(bytes, self.snpas)) + \
                           b''.join(map(bytes, self.announced))

                class SNPA(object):
                    __hdr__ = (
                        ('len', 'B', 0),
                    )

                    def unpack(self, buf):
                        dpkt.Packet.unpack(self, buf)
                        self.data = self.data[:(self.len + 1) // 2]

            class MPUnreachNLRI(dpkt.Packet):
                __hdr__ = (
                    ('afi', 'H', AFI_IPV4),
                    ('safi', 'B', SAFI_UNICAST),
                )

                def unpack(self, buf):
                    dpkt.Packet.unpack(self, buf)

                    if self.afi == AFI_IPV4:
                        Route = RouteIPV4
                    elif self.afi == AFI_IPV6:
                        Route = RouteIPV6
                    else:
                        Route = RouteGeneric

                    # Withdrawn Routes
                    l = []
                    while self.data:
                        route = Route(self.data)
                        self.data = self.data[len(route):]
                        l.append(route)
                    self.data = self.withdrawn = l

                def __len__(self):
                    return self.__hdr_len__ + sum(map(len, self.data))

                def __bytes__(self):
                    return self.pack_hdr() + b''.join(map(bytes, self.data))

    class Notification(dpkt.Packet):
        __hdr__ = (
            ('code', 'B', 0),
            ('subcode', 'B', 0),
        )

        def unpack(self, buf):
            dpkt.Packet.unpack(self, buf)
            self.error = self.data

    class Keepalive(dpkt.Packet):
        def unpack(self, buf):
            pass

        def __len__(self):
            return 0

        def __bytes__(self):
            return b''

    class RouteRefresh(dpkt.Packet):
        __hdr__ = (
            ('afi', 'H', AFI_IPV4),
            ('rsvd', 'B', 0),
            ('safi', 'B', SAFI_UNICAST)
        )


class RouteGeneric(dpkt.Packet):
    __hdr__ = (
        ('len', 'B', 0),
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.data = self.prefix = self.data[:(self.len + 7) // 8]


class RouteIPV4(dpkt.Packet):
    __hdr__ = (
        ('len', 'B', 0),
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        tmp = self.data[:(self.len + 7) // 8]
        tmp += (4 - len(tmp)) * b'\x00'
        self.data = self.prefix = tmp

    def __repr__(self):
        cidr = '%s/%d' % (socket.inet_ntoa(self.prefix), self.len)
        return '%s(%s)' % (self.__class__.__name__, cidr)

    def __len__(self):
        return self.__hdr_len__ + (self.len + 7) // 8

    def __bytes__(self):
        return self.pack_hdr() + self.prefix[:(self.len + 7) // 8]


class RouteIPV6(dpkt.Packet):
    __hdr__ = (
        ('len', 'B', 0),
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        tmp = self.data[:(self.len + 7) // 8]
        tmp += (16 - len(tmp)) * b'\x00'
        self.data = self.prefix = tmp

    def __len__(self):
        return self.__hdr_len__ + (self.len + 7) // 8

    def __bytes__(self):
        return self.pack_hdr() + self.prefix[:(self.len + 7) // 8]


__bgp1 = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x13\x04'
__bgp2 = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x63\x02\x00\x00\x00\x48\x40\x01\x01\x00\x40\x02\x0a\x01\x02\x01\xf4\x01\xf4\x02\x01\xfe\xbb\x40\x03\x04\xc0\xa8\x00\x0f\x40\x05\x04\x00\x00\x00\x64\x40\x06\x00\xc0\x07\x06\xfe\xba\xc0\xa8\x00\x0a\xc0\x08\x0c\xfe\xbf\x00\x01\x03\x16\x00\x04\x01\x54\x00\xfa\x80\x09\x04\xc0\xa8\x00\x0f\x80\x0a\x04\xc0\xa8\x00\xfa\x16\xc0\xa8\x04'
__bgp3 = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x79\x02\x00\x00\x00\x62\x40\x01\x01\x00\x40\x02\x00\x40\x05\x04\x00\x00\x00\x64\xc0\x10\x08\x00\x02\x01\x2c\x00\x00\x01\x2c\xc0\x80\x24\x00\x00\xfd\xe9\x40\x01\x01\x00\x40\x02\x04\x02\x01\x15\xb3\x40\x05\x04\x00\x00\x00\x2c\x80\x09\x04\x16\x05\x05\x05\x80\x0a\x04\x16\x05\x05\x05\x90\x0e\x00\x1e\x00\x01\x80\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x0c\x04\x04\x04\x00\x60\x18\x77\x01\x00\x00\x01\xf4\x00\x00\x01\xf4\x85'
__bgp4 = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x2d\x01\x04\x00\xed\x00\x5a\xc6\x6e\x83\x7d\x10\x02\x06\x01\x04\x00\x01\x00\x01\x02\x02\x80\x00\x02\x02\x02\x00'


def test_pack():
    assert (__bgp1 == bytes(BGP(__bgp1)))
    assert (__bgp2 == bytes(BGP(__bgp2)))
    assert (__bgp3 == bytes(BGP(__bgp3)))
    assert (__bgp4 == bytes(BGP(__bgp4)))


def test_unpack():
    b1 = BGP(__bgp1)
    assert (b1.len == 19)
    assert (b1.type == KEEPALIVE)
    assert (b1.keepalive is not None)

    b2 = BGP(__bgp2)
    assert (b2.type == UPDATE)
    assert (len(b2.update.withdrawn) == 0)
    assert (len(b2.update.announced) == 1)
    assert (len(b2.update.attributes) == 9)
    a = b2.update.attributes[1]
    assert (a.type == AS_PATH)
    assert (a.len == 10)
    assert (len(a.as_path.segments) == 2)
    s = a.as_path.segments[0]
    assert (s.type == AS_SET)
    assert (s.len == 2)
    assert (len(s.path) == 2)
    assert (s.path[0] == 500)

    a = b2.update.attributes[6]
    assert (a.type == COMMUNITIES)
    assert (a.len == 12)
    assert (len(a.communities.list) == 3)
    c = a.communities.list[0]
    assert (c.asn == 65215)
    assert (c.value == 1)
    r = b2.update.announced[0]
    assert (r.len == 22)
    assert (r.prefix == b'\xc0\xa8\x04\x00')

    b3 = BGP(__bgp3)
    assert (b3.type == UPDATE)
    assert (len(b3.update.withdrawn) == 0)
    assert (len(b3.update.announced) == 0)
    assert (len(b3.update.attributes) == 6)
    a = b3.update.attributes[0]
    assert (a.optional == False)
    assert (a.transitive == True)
    assert (a.partial == False)
    assert (a.extended_length == False)
    assert (a.type == ORIGIN)
    assert (a.len == 1)
    o = a.origin
    assert (o.type == ORIGIN_IGP)
    a = b3.update.attributes[5]
    assert (a.optional == True)
    assert (a.transitive == False)
    assert (a.partial == False)
    assert (a.extended_length == True)
    assert (a.type == MP_REACH_NLRI)
    assert (a.len == 30)
    m = a.mp_reach_nlri
    assert (m.afi == AFI_IPV4)
    assert (len(m.snpas) == 0)
    assert (len(m.announced) == 1)
    p = m.announced[0]
    assert (p.len == 96)

    b4 = BGP(__bgp4)
    assert (b4.len == 45)
    assert (b4.type == OPEN)
    assert (b4.open.asn == 237)
    assert (b4.open.param_len == 16)
    assert (len(b4.open.parameters) == 3)
    p = b4.open.parameters[0]
    assert (p.type == CAPABILITY)
    assert (p.len == 6)
    c = p.capability
    assert (c.code == CAP_MULTIPROTOCOL)
    assert (c.len == 4)
    assert (c.data == b'\x00\x01\x00\x01')
    c = b4.open.parameters[2].capability
    assert (c.code == CAP_ROUTE_REFRESH)
    assert (c.len == 0)


if __name__ == '__main__':
    test_pack()
    test_unpack()
    print('Tests Successful...')
