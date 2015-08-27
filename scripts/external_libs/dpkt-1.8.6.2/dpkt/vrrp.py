# $Id: vrrp.py 88 2013-03-05 19:43:17Z andrewflnr@gmail.com $
# -*- coding: utf-8 -*-
"""Virtual Router Redundancy Protocol."""

import dpkt
from decorators import deprecated


class VRRP(dpkt.Packet):
    __hdr__ = (
        ('vtype', 'B', 0x21),
        ('vrid', 'B', 0),
        ('priority', 'B', 0),
        ('count', 'B', 0),
        ('atype', 'B', 0),
        ('advtime', 'B', 0),
        ('sum', 'H', 0),
    )
    addrs = ()
    auth = ''

    @property
    def v(self):
        return self.vtype >> 4

    @v.setter
    def v(self, v):
        self.vtype = (self.vtype & ~0xf) | (v << 4)

    @property
    def type(self):
        return self.vtype & 0xf

    @type.setter
    def type(self, v):
        self.vtype = (self.vtype & ~0xf0) | (v & 0xf)

    # Deprecated methods, will be removed in the future
    # =================================================
    @deprecated
    def _get_v(self): return self.v

    @deprecated
    def _set_v(self, v): self.v = v

    @deprecated
    def _get_type(self): return self.type

    @deprecated
    def _set_type(self, v): self.type = v
    # =================================================

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        l = []
        off = 0
        for off in range(0, 4 * self.count, 4):
            l.append(self.data[off:off + 4])
        self.addrs = l
        self.auth = self.data[off + 4:]
        self.data = ''

    def __len__(self):
        return self.__hdr_len__ + (4 * self.count) + len(self.auth)

    def __str__(self):
        data = ''.join(self.addrs) + self.auth
        if not self.sum:
            self.sum = dpkt.in_cksum(self.pack_hdr() + data)
        return self.pack_hdr() + data
