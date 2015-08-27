# $Id: pim.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Protocol Independent Multicast."""

import dpkt
from decorators import deprecated


class PIM(dpkt.Packet):
    __hdr__ = (
        ('v_type', 'B', 0x20),
        ('rsvd', 'B', 0),
        ('sum', 'H', 0)
    )

    @property
    def v(self): return self.v_type >> 4

    @v.setter
    def v(self, v): self.v_type = (v << 4) | (self.v_type & 0xf)

    @property
    def type(self): return self.v_type & 0xf

    @type.setter
    def type(self, type): self.v_type = (self.v_type & 0xf0) | type

    # Deprecated methods, will be removed in the future
    # =================================================
    @deprecated
    def _get_v(self): return self.v

    @deprecated
    def _set_v(self, v): self.v = v

    @deprecated
    def _get_type(self): return self.type

    @deprecated
    def _set_type(self, type): self.type = type
    # =================================================

    def __str__(self):
        if not self.sum:
            self.sum = dpkt.in_cksum(dpkt.Packet.__str__(self))
        return dpkt.Packet.__str__(self)
