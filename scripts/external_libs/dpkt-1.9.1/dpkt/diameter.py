# $Id: diameter.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Diameter."""
from __future__ import print_function
from __future__ import absolute_import

import struct

from . import dpkt
from .decorators import deprecated
from .compat import compat_ord

# Diameter Base Protocol - RFC 3588
# http://tools.ietf.org/html/rfc3588

# Request/Answer Command Codes
ABORT_SESSION = 274
ACCOUTING = 271
CAPABILITIES_EXCHANGE = 257
DEVICE_WATCHDOG = 280
DISCONNECT_PEER = 282
RE_AUTH = 258
SESSION_TERMINATION = 275


class Diameter(dpkt.Packet):
    """Diameter.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of Diameter.
        TODO.
    """

    __hdr__ = (
        ('v', 'B', 1),
        ('len', '3s', 0),
        ('flags', 'B', 0),
        ('cmd', '3s', 0),
        ('app_id', 'I', 0),
        ('hop_id', 'I', 0),
        ('end_id', 'I', 0)
    )

    @property
    def request_flag(self):
        return (self.flags >> 7) & 0x1

    @request_flag.setter
    def request_flag(self, r):
        self.flags = (self.flags & ~0x80) | ((r & 0x1) << 7)

    @property
    def proxiable_flag(self):
        return (self.flags >> 6) & 0x1

    @proxiable_flag.setter
    def proxiable_flag(self, p):
        self.flags = (self.flags & ~0x40) | ((p & 0x1) << 6)

    @property
    def error_flag(self):
        return (self.flags >> 5) & 0x1

    @error_flag.setter
    def error_flag(self, e):
        self.flags = (self.flags & ~0x20) | ((e & 0x1) << 5)

    @property
    def retransmit_flag(self):
        return (self.flags >> 4) & 0x1

    @retransmit_flag.setter
    def retransmit_flag(self, t):
        self.flags = (self.flags & ~0x10) | ((t & 0x1) << 4)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.cmd = (compat_ord(self.cmd[0]) << 16) | \
                    (compat_ord(self.cmd[1]) << 8) | \
                    (compat_ord(self.cmd[2]))
        self.len = (compat_ord(self.len[0]) << 16) | \
                    (compat_ord(self.len[1]) << 8) | \
                    (compat_ord(self.len[2]))
        self.data = self.data[:self.len - self.__hdr_len__]

        l = []
        while self.data:
            avp = AVP(self.data)
            l.append(avp)
            self.data = self.data[len(avp):]
        self.data = self.avps = l

    def pack_hdr(self):
        self.len = struct.pack("BBB", (self.len >> 16) & 0xff, (self.len >> 8) & 0xff, self.len & 0xff)
        self.cmd = struct.pack("BBB", (self.cmd >> 16) & 0xff, (self.cmd >> 8) & 0xff, self.cmd & 0xff)
        return dpkt.Packet.pack_hdr(self)

    def __len__(self):
        return self.__hdr_len__ + sum(map(len, self.data))

    def __bytes__(self):
        return self.pack_hdr() + b''.join(map(bytes, self.data))

class AVP(dpkt.Packet):
    __hdr__ = (
        ('code', 'I', 0),
        ('flags', 'B', 0),
        ('len', '3s', 0),
    )

    @property
    def vendor_flag(self):
        return (self.flags >> 7) & 0x1

    @vendor_flag.setter
    def vendor_flag(self, v):
        self.flags = (self.flags & ~0x80) | ((v & 0x1) << 7)

    @property
    def mandatory_flag(self):
        return (self.flags >> 6) & 0x1

    @mandatory_flag.setter
    def mandatory_flag(self, m):
        self.flags = (self.flags & ~0x40) | ((m & 0x1) << 6)

    @property
    def protected_flag(self):
        return (self.flags >> 5) & 0x1

    @protected_flag.setter
    def protected_flag(self, p):
        self.flags = (self.flags & ~0x20) | ((p & 0x1) << 5)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.len = (compat_ord(self.len[0]) << 16) | \
                    (compat_ord(self.len[1]) << 8) | \
                    (compat_ord(self.len[2]))

        if self.vendor_flag:
            self.vendor = struct.unpack('>I', self.data[:4])[0]
            self.data = self.data[4:self.len - self.__hdr_len__]
        else:
            self.data = self.data[:self.len - self.__hdr_len__]

    def pack_hdr(self):
        self.len = struct.pack("BBB", (self.len >> 16) & 0xff, (self.len >> 8) & 0xff, self.len & 0xff)
        data = dpkt.Packet.pack_hdr(self)
        if self.vendor_flag:
            data += struct.pack('>I', self.vendor)
        return data

    def __len__(self):
        length = self.__hdr_len__ + len(self.data)
        if self.vendor_flag:
            length += 4
        return length


__s = b'\x01\x00\x00\x28\x80\x00\x01\x18\x00\x00\x00\x00\x00\x00\x41\xc8\x00\x00\x00\x0c\x00\x00\x01\x08\x40\x00\x00\x0c\x68\x30\x30\x32\x00\x00\x01\x28\x40\x00\x00\x08'
__t = b'\x01\x00\x00\x2c\x80\x00\x01\x18\x00\x00\x00\x00\x00\x00\x41\xc8\x00\x00\x00\x0c\x00\x00\x01\x08\xc0\x00\x00\x10\xde\xad\xbe\xef\x68\x30\x30\x32\x00\x00\x01\x28\x40\x00\x00\x08'


def test_pack():
    d = Diameter(__s)
    assert (__s == bytes(d))
    d = Diameter(__t)
    assert (__t == bytes(d))


def test_unpack():
    d = Diameter(__s)
    assert (d.len == 40)
    # assert (d.cmd == DEVICE_WATCHDOG_REQUEST)
    assert (d.request_flag == 1)
    assert (d.error_flag == 0)
    assert (len(d.avps) == 2)

    avp = d.avps[0]
    # assert (avp.code == ORIGIN_HOST)
    assert (avp.mandatory_flag == 1)
    assert (avp.vendor_flag == 0)
    assert (avp.len == 12)
    assert (len(avp) == 12)
    assert (avp.data == b'\x68\x30\x30\x32')

    # also test the optional vendor id support
    d = Diameter(__t)
    assert (d.len == 44)
    avp = d.avps[0]
    assert (avp.vendor_flag == 1)
    assert (avp.len == 16)
    assert (len(avp) == 16)
    assert (avp.vendor == 3735928559)
    assert (avp.data == b'\x68\x30\x30\x32')


if __name__ == '__main__':
    test_pack()
    test_unpack()
    print('Tests Successful...')
