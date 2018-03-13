# $Id: rip.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Routing Information Protocol."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt

# RIP v2 - RFC 2453
# http://tools.ietf.org/html/rfc2453

REQUEST = 1
RESPONSE = 2


class RIP(dpkt.Packet):
    """Routing Information Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of RIP.
        TODO.
    """

    __hdr__ = (
        ('cmd', 'B', REQUEST),
        ('v', 'B', 2),
        ('rsvd', 'H', 0)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        l = []
        self.auth = None
        while self.data:
            rte = RTE(self.data[:20])
            if rte.family == 0xFFFF:
                self.auth = Auth(self.data[:20])
            else:
                l.append(rte)
            self.data = self.data[20:]
        self.data = self.rtes = l

    def __len__(self):
        n = self.__hdr_len__
        if self.auth:
            n += len(self.auth)
        n += sum(map(len, self.rtes))
        return n

    def __bytes__(self):
        auth = b''
        if self.auth:
            auth = bytes(self.auth)
        return self.pack_hdr() + auth + b''.join(map(bytes, self.rtes))


class RTE(dpkt.Packet):
    __hdr__ = (
        ('family', 'H', 2),
        ('route_tag', 'H', 0),
        ('addr', 'I', 0),
        ('subnet', 'I', 0),
        ('next_hop', 'I', 0),
        ('metric', 'I', 1)
    )


class Auth(dpkt.Packet):
    __hdr__ = (
        ('rsvd', 'H', 0xFFFF),
        ('type', 'H', 2),
        ('auth', '16s', 0)
    )


__s = b'\x02\x02\x00\x00\x00\x02\x00\x00\x01\x02\x03\x00\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x02\x00\x00\xc0\xa8\x01\x08\xff\xff\xff\xfc\x00\x00\x00\x00\x00\x00\x00\x01'


def test_rtp_pack():
    r = RIP(__s)
    assert (__s == bytes(r))


def test_rtp_unpack():
    r = RIP(__s)
    assert (r.auth is None)
    assert (len(r.rtes) == 2)

    rte = r.rtes[1]
    assert (rte.family == 2)
    assert (rte.route_tag == 0)
    assert (rte.metric == 1)


if __name__ == '__main__':
    test_rtp_pack()
    test_rtp_unpack()
    print('Tests Successful...')
