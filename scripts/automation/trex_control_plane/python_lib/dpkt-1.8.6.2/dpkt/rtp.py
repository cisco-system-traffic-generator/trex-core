# $Id: rtp.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Real-Time Transport Protocol"""

from dpkt import Packet
from decorators import deprecated

# version 1100 0000 0000 0000 ! 0xC000  14
# p       0010 0000 0000 0000 ! 0x2000  13
# x       0001 0000 0000 0000 ! 0x1000  12
# cc      0000 1111 0000 0000 ! 0x0F00   8
# m       0000 0000 1000 0000 ! 0x0080   7
# pt      0000 0000 0111 1111 ! 0x007F   0
#

_VERSION_MASK = 0xC000
_P_MASK = 0x2000
_X_MASK = 0x1000
_CC_MASK = 0x0F00
_M_MASK = 0x0080
_PT_MASK = 0x007F
_VERSION_SHIFT = 14
_P_SHIFT = 13
_X_SHIFT = 12
_CC_SHIFT = 8
_M_SHIFT = 7
_PT_SHIFT = 0

VERSION = 2


class RTP(Packet):
    __hdr__ = (
        ('_type', 'H', 0x8000),
        ('seq', 'H', 0),
        ('ts', 'I', 0),
        ('ssrc', 'I', 0),
    )
    csrc = ''

    @property
    def version(self): return (self._type & _VERSION_MASK) >> _VERSION_SHIFT

    @version.setter
    def version(self, ver):
        self._type = (ver << _VERSION_SHIFT) | (self._type & ~_VERSION_MASK)

    @property
    def p(self): return (self._type & _P_MASK) >> _P_SHIFT

    @p.setter
    def p(self, p): self._type = (p << _P_SHIFT) | (self._type & ~_P_MASK)

    @property
    def x(self): return (self._type & _X_MASK) >> _X_SHIFT

    @x.setter
    def x(self, x): self._type = (x << _X_SHIFT) | (self._type & ~_X_MASK)

    @property
    def cc(self): return (self._type & _CC_MASK) >> _CC_SHIFT

    @cc.setter
    def cc(self, cc): self._type = (cc << _CC_SHIFT) | (self._type & ~_CC_MASK)

    @property
    def m(self): return (self._type & _M_MASK) >> _M_SHIFT

    @m.setter
    def m(self, m): self._type = (m << _M_SHIFT) | (self._type & ~_M_MASK)

    @property
    def pt(self): return (self._type & _PT_MASK) >> _PT_SHIFT

    @pt.setter
    def pt(self, m): self._type = (m << _PT_SHIFT) | (self._type & ~_PT_MASK)

    # Deprecated methods, will be removed in the future
    # =================================================
    @deprecated
    def _get_version(self): return self.version

    @deprecated
    def _set_version(self, ver): self.version = ver

    @deprecated
    def _get_p(self): return self.p

    @deprecated
    def _set_p(self, p): self.p = p

    @deprecated
    def _get_x(self): return self.x

    @deprecated
    def _set_x(self, x): self.x = x

    @deprecated
    def _get_cc(self): return self.cc

    @deprecated
    def _set_cc(self, cc): self.cc = cc

    @deprecated
    def _get_m(self): return self.m

    @deprecated
    def _set_m(self, m): self.m = m

    @deprecated
    def _get_pt(self): return self.pt

    @deprecated
    def _set_pt(self, pt): self.pt = pt
    # =================================================

    def __len__(self):
        return self.__hdr_len__ + len(self.csrc) + len(self.data)

    def __str__(self):
        return self.pack_hdr() + self.csrc + str(self.data)

    def unpack(self, buf):
        super(RTP, self).unpack(buf)
        self.csrc = buf[self.__hdr_len__:self.__hdr_len__ + self.cc * 4]
        self.data = buf[self.__hdr_len__ + self.cc * 4:]

