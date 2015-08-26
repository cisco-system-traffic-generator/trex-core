# $Id: pppoe.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""PPP-over-Ethernet."""

import dpkt
import ppp
from decorators import deprecated

# RFC 2516 codes
PPPoE_PADI = 0x09
PPPoE_PADO = 0x07
PPPoE_PADR = 0x19
PPPoE_PADS = 0x65
PPPoE_PADT = 0xA7
PPPoE_SESSION = 0x00


class PPPoE(dpkt.Packet):
    __hdr__ = (
        ('v_type', 'B', 0x11),
        ('code', 'B', 0),
        ('session', 'H', 0),
        ('len', 'H', 0)  # payload length
    )

    @property
    def v(self): return self.v_type >> 4

    @v.setter
    def v(self, v): self.v_type = (v << 4) | (self.v_type & 0xf)

    @property
    def type(self): return self.v_type & 0xf

    @type.setter
    def type(self, t): self.v_type = (self.v_type & 0xf0) | t

    # Deprecated methods, will be removed in the future
    # =================================================
    @deprecated
    def _get_v(self): return self.v

    @deprecated
    def _set_v(self, v): self.v = v

    @deprecated
    def _get_type(self): return self.type

    @deprecated
    def _set_type(self, t): self.type = t
    # =================================================

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        try:
            if self.code == 0:
                self.data = self.ppp = ppp.PPP(self.data)
        except dpkt.UnpackError:
            pass

# XXX - TODO TLVs, etc.
