# $Id: dtp.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Dynamic Trunking Protocol."""
from __future__ import absolute_import

import struct

from . import dpkt


class DTP(dpkt.Packet):
    """Dynamic Trunking Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of DTP.
        TODO.
    """
    
    __hdr__ = (
        ('v', 'B', 0),
    )  # rest is TLVs

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        buf = self.data
        tvs = []
        while buf:
            t, l = struct.unpack('>HH', buf[:4])
            v, buf = buf[4:4 + l], buf[4 + l:]
            tvs.append((t, v))
        self.data = tvs


TRUNK_NAME = 0x01
MAC_ADDR = 0x04
