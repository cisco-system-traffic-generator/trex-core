# $Id: aim.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-

"""AOL Instant Messenger."""
from __future__ import absolute_import

import struct

from . import dpkt

# OSCAR: http://iserverd1.khstu.ru/oscar/


class FLAP(dpkt.Packet):
    """Frame Layer Protocol.

    See more about the FLAP on \
    https://en.wikipedia.org/wiki/OSCAR_protocol#FLAP_header

    Attributes:
        __hdr__: Header fields of FLAP.
        data: Message data.
    """
    
    __hdr__ = (
        ('ast', 'B', 0x2a),  # '*'
        ('type', 'B', 0),
        ('seq', 'H', 0),
        ('len', 'H', 0)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        if self.ast != 0x2a:
            raise dpkt.UnpackError('invalid FLAP header')
        if len(self.data) < self.len:
            raise dpkt.NeedData('%d left, %d needed' % (len(self.data), self.len))


class SNAC(dpkt.Packet):
    """Simple Network Atomic Communication.

    See more about the SNAC on \
    https://en.wikipedia.org/wiki/OSCAR_protocol#SNAC_data

    Attributes:
        __hdr__: Header fields of SNAC.
    """
    
    __hdr__ = (
        ('family', 'H', 0),
        ('subtype', 'H', 0),
        ('flags', 'H', 0),
        ('reqid', 'I', 0)
    )


def tlv(buf):
    n = 4
    try:
        t, l = struct.unpack('>HH', buf[:n])
    except struct.error:
        raise dpkt.UnpackError('invalid type, length fields')
    v = buf[n:n + l]
    if len(v) < l:
        raise dpkt.NeedData('%d left, %d needed' % (len(v), l))
    buf = buf[n + l:]
    return t, l, v, buf

# TOC 1.0: http://jamwt.com/Py-TOC/PROTOCOL

# TOC 2.0: http://www.firestuff.org/projects/firetalk/doc/toc2.txt

def testAIM():
    testdata = b'*\x02\xac\xf3\x00\x81\x00\x03\x00\x0b\x00\x00\xfaEUd\x0eusrnameremoved\x00\x00\x00\n\x00\x01\x00\x02\x12\x90\x00D\x00\x01\x00\x00\x03\x00\x04X\x90T6\x00E\x00\x04\x00\x00\x0f\x93\x00!\x00\x08\x00\x85\x00}\x00}\x00\x00\x00A\x00\x01\x00\x007\x00\x04\x00\x00\x00\x00\x00\r\x00\x00\x00\x19\x00\x00\x00\x1d\x00$\x00\x00\x00\x05\x02\x01\xd2\x04r\x00\x01\x00\x05\x02\x01\xd2\x04r\x00\x03\x00\x05+\x00\x00*\xcc\x00\x81\x00\x05+\x00\x00\x13\xf1'

    flap = FLAP(testdata)
    assert flap.ast == 0x2a
    assert flap.type == 0x02
    assert flap.seq == 44275
    assert flap.len == 129
    assert flap.data == b'\x00\x03\x00\x0b\x00\x00\xfaEUd\x0eusrnameremoved\x00\x00\x00\n\x00\x01\x00\x02\x12\x90\x00D\x00\x01\x00\x00\x03\x00\x04X\x90T6\x00E\x00\x04\x00\x00\x0f\x93\x00!\x00\x08\x00\x85\x00}\x00}\x00\x00\x00A\x00\x01\x00\x007\x00\x04\x00\x00\x00\x00\x00\r\x00\x00\x00\x19\x00\x00\x00\x1d\x00$\x00\x00\x00\x05\x02\x01\xd2\x04r\x00\x01\x00\x05\x02\x01\xd2\x04r\x00\x03\x00\x05+\x00\x00*\xcc\x00\x81\x00\x05+\x00\x00\x13\xf1'

    snac = SNAC(flap.data)
    assert snac.family == 3
    assert snac.subtype == 11
    assert snac.flags == 0
    assert snac.reqid == 0xfa455564
    assert snac.data == b'\x0eusrnameremoved\x00\x00\x00\n\x00\x01\x00\x02\x12\x90\x00D\x00\x01\x00\x00\x03\x00\x04X\x90T6\x00E\x00\x04\x00\x00\x0f\x93\x00!\x00\x08\x00\x85\x00}\x00}\x00\x00\x00A\x00\x01\x00\x007\x00\x04\x00\x00\x00\x00\x00\r\x00\x00\x00\x19\x00\x00\x00\x1d\x00$\x00\x00\x00\x05\x02\x01\xd2\x04r\x00\x01\x00\x05\x02\x01\xd2\x04r\x00\x03\x00\x05+\x00\x00*\xcc\x00\x81\x00\x05+\x00\x00\x13\xf1'

    #skip over the buddyname and TLV count in Oncoming Buddy message
    tlvdata = snac.data[19:]

    tlvCount = 0
    while tlvdata:
        t, l, v, tlvdata = tlv(tlvdata)
        tlvCount += 1
        if tlvCount == 1:
            # just check function return for first TLV
            assert t == 0x01
            assert l == 2
            assert v == b'\x12\x90'
            assert tlvdata == b'\x00D\x00\x01\x00\x00\x03\x00\x04X\x90T6\x00E\x00\x04\x00\x00\x0f\x93\x00!\x00\x08\x00\x85\x00}\x00}\x00\x00\x00A\x00\x01\x00\x007\x00\x04\x00\x00\x00\x00\x00\r\x00\x00\x00\x19\x00\x00\x00\x1d\x00$\x00\x00\x00\x05\x02\x01\xd2\x04r\x00\x01\x00\x05\x02\x01\xd2\x04r\x00\x03\x00\x05+\x00\x00*\xcc\x00\x81\x00\x05+\x00\x00\x13\xf1'

    # make sure we extracted 10 TLVs
    assert tlvCount == 10


def testExceptions():
    testdata = b'xxxxxx'
    try:
        flap = FLAP(testdata)
    except dpkt.UnpackError as e:
        assert str(e) == 'invalid FLAP header'
    testdata = b'*\x02\x12\x34\x00\xff'
    try:
        flap = FLAP(testdata)
    except dpkt.NeedData as e:
        assert str(e) == '0 left, 255 needed'
    try:
        t, l, v, _ = tlv(b'x')
    except dpkt.UnpackError as e:
        assert str(e) == 'invalid type, length fields'

    try:
        t, l, v, _ = tlv(b'\x00\x01\x00\xff')
    except dpkt.NeedData as e:
        assert str(e) == '0 left, 255 needed'
