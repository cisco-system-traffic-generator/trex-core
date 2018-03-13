# $Id: ppp.py 65 2010-03-26 02:53:51Z dugsong $
# -*- coding: utf-8 -*-
"""Point-to-Point Protocol."""
from __future__ import absolute_import

import struct

from . import dpkt

# XXX - finish later

# http://www.iana.org/assignments/ppp-numbers
PPP_IP = 0x21  # Internet Protocol
PPP_IP6 = 0x57  # Internet Protocol v6

# Protocol field compression
PFC_BIT = 0x01


class PPP(dpkt.Packet):
    # Note: This class is subclassed in PPPoE
    """Point-to-Point Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of PPP.
        TODO.
    """

    __hdr__ = (
        ('addr', 'B', 0xff),
        ('cntrl', 'B', 3),
        ('p', 'B', PPP_IP),
    )
    _protosw = {}

    @classmethod
    def set_p(cls, p, pktclass):
        cls._protosw[p] = pktclass

    @classmethod
    def get_p(cls, p):
        return cls._protosw[p]

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        if self.p & PFC_BIT == 0:
            try:
                self.p = struct.unpack('>H', buf[2:4])[0]
            except struct.error:
                raise dpkt.NeedData
            self.data = self.data[1:]
        try:
            self.data = self._protosw[self.p](self.data)
            setattr(self, self.data.__class__.__name__.lower(), self.data)
        except (KeyError, struct.error, dpkt.UnpackError):
            pass

    def pack_hdr(self):
        try:
            if self.p > 0xff:
                return struct.pack('>BBH', self.addr, self.cntrl, self.p)
            return dpkt.Packet.pack_hdr(self)
        except struct.error as e:
            raise dpkt.PackError(str(e))


def __load_protos():
    g = globals()
    for k, v in g.items():
        if k.startswith('PPP_'):
            name = k[4:]
            modname = name.lower()
            try:
                mod = __import__(modname, g, level=1)
                PPP.set_p(v, getattr(mod, name))
            except (ImportError, AttributeError):
                continue


def _mod_init():
    """Post-initialization called when all dpkt modules are fully loaded"""
    if not PPP._protosw:
        __load_protos()


def test_ppp():
    # Test protocol compression
    s = b"\xff\x03\x21"
    p = PPP(s)
    assert p.p == 0x21

    s = b"\xff\x03\x00\x21"
    p = PPP(s)
    assert p.p == 0x21


def test_ppp_short():
    s = b"\xff\x03\x00"

    import pytest
    pytest.raises(dpkt.NeedData, PPP, s)


def test_packing():
    p = PPP()
    assert p.pack_hdr() == b"\xff\x03\x21"

    p.p = 0xc021  # LCP
    assert p.pack_hdr() == b"\xff\x03\xc0\x21"


if __name__ == '__main__':
    # Runs all the test associated with this class/file
    test_ppp()
