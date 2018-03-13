# $Id: loopback.py 38 2007-03-17 03:33:16Z dugsong $
# -*- coding: utf-8 -*-
"""Platform-dependent loopback header."""
from __future__ import absolute_import

from . import dpkt
from . import ethernet
from . import ip
from . import ip6


class Loopback(dpkt.Packet):
    """Platform-dependent loopback header.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of Loopback.
        TODO.
    """
    
    __hdr__ = (('family', 'I', 0), )
    __byte_order__ = '@'

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        if self.family == 2:
            self.data = ip.IP(self.data)
        elif self.family == 0x02000000:
            self.family = 2
            self.data = ip.IP(self.data)
        elif self.family in (24, 28, 30):
            self.data = ip6.IP6(self.data)
        elif self.family > 1500:
            self.data = ethernet.Ethernet(self.data)
