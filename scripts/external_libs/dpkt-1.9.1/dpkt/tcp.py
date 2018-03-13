# $Id: tcp.py 42 2007-08-02 22:38:47Z jon.oberheide $
# -*- coding: utf-8 -*-
"""Transmission Control Protocol."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt
from .decorators import deprecated
from .compat import compat_ord

# TCP control flags
TH_FIN = 0x01  # end of data
TH_SYN = 0x02  # synchronize sequence numbers
TH_RST = 0x04  # reset connection
TH_PUSH = 0x08  # push
TH_ACK = 0x10  # acknowledgment number set
TH_URG = 0x20  # urgent pointer set
TH_ECE = 0x40  # ECN echo, RFC 3168
TH_CWR = 0x80  # congestion window reduced

TCP_PORT_MAX = 65535  # maximum port
TCP_WIN_MAX = 65535  # maximum (unscaled) window


class TCP(dpkt.Packet):
    """Transmission Control Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of TCP.
        TODO.
    """

    __hdr__ = (
        ('sport', 'H', 0xdead),
        ('dport', 'H', 0),
        ('seq', 'I', 0xdeadbeef),
        ('ack', 'I', 0),
        ('_off', 'B', ((5 << 4) | 0)),
        ('flags', 'B', TH_SYN),
        ('win', 'H', TCP_WIN_MAX),
        ('sum', 'H', 0),
        ('urp', 'H', 0)
    )
    opts = b''

    @property
    def off(self):
        return self._off >> 4

    @off.setter
    def off(self, off):
        self._off = (off << 4) | (self._off & 0xf)

    def __len__(self):
        return self.__hdr_len__ + len(self.opts) + len(self.data)

    def __bytes__(self):
        return self.pack_hdr() + bytes(self.opts) + bytes(self.data)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        ol = ((self._off >> 4) << 2) - self.__hdr_len__
        if ol < 0:
            raise dpkt.UnpackError('invalid header length')
        self.opts = buf[self.__hdr_len__:self.__hdr_len__ + ol]
        self.data = buf[self.__hdr_len__ + ol:]

# Options (opt_type) - http://www.iana.org/assignments/tcp-parameters
TCP_OPT_EOL = 0  # end of option list
TCP_OPT_NOP = 1  # no operation
TCP_OPT_MSS = 2  # maximum segment size
TCP_OPT_WSCALE = 3  # window scale factor, RFC 1072
TCP_OPT_SACKOK = 4  # SACK permitted, RFC 2018
TCP_OPT_SACK = 5  # SACK, RFC 2018
TCP_OPT_ECHO = 6  # echo (obsolete), RFC 1072
TCP_OPT_ECHOREPLY = 7  # echo reply (obsolete), RFC 1072
TCP_OPT_TIMESTAMP = 8  # timestamp, RFC 1323
TCP_OPT_POCONN = 9  # partial order conn, RFC 1693
TCP_OPT_POSVC = 10  # partial order service, RFC 1693
TCP_OPT_CC = 11  # connection count, RFC 1644
TCP_OPT_CCNEW = 12  # CC.NEW, RFC 1644
TCP_OPT_CCECHO = 13  # CC.ECHO, RFC 1644
TCP_OPT_ALTSUM = 14  # alt checksum request, RFC 1146
TCP_OPT_ALTSUMDATA = 15  # alt checksum data, RFC 1146
TCP_OPT_SKEETER = 16  # Skeeter
TCP_OPT_BUBBA = 17  # Bubba
TCP_OPT_TRAILSUM = 18  # trailer checksum
TCP_OPT_MD5 = 19  # MD5 signature, RFC 2385
TCP_OPT_SCPS = 20  # SCPS capabilities
TCP_OPT_SNACK = 21  # selective negative acks
TCP_OPT_REC = 22  # record boundaries
TCP_OPT_CORRUPT = 23  # corruption experienced
TCP_OPT_SNAP = 24  # SNAP
TCP_OPT_TCPCOMP = 26  # TCP compression filter
TCP_OPT_MAX = 27


def parse_opts(buf):
    """Parse TCP option buffer into a list of (option, data) tuples."""
    opts = []
    while buf:
        o = compat_ord(buf[0])
        if o > TCP_OPT_NOP:
            try:
                # advance buffer at least 2 bytes = 1 type + 1 length
                l = max(2, compat_ord(buf[1]))
                d, buf = buf[2:l], buf[l:]
            except (IndexError, ValueError):
                # print 'bad option', repr(str(buf))
                opts.append(None)  # XXX
                break
        else:
            # options 0 and 1 are not followed by length byte
            d, buf = b'', buf[1:]
        opts.append((o, d))
    return opts


def test_parse_opts():
    # normal scenarios
    buf = b'\x02\x04\x23\x00\x01\x01\x04\x02'
    opts = parse_opts(buf)
    assert opts == [
        (TCP_OPT_MSS, b'\x23\x00'),
        (TCP_OPT_NOP, b''),
        (TCP_OPT_NOP, b''),
        (TCP_OPT_SACKOK, b'')
    ]

    buf = b'\x01\x01\x05\x0a\x37\xf8\x19\x70\x37\xf8\x29\x78'
    opts = parse_opts(buf)
    assert opts == [
        (TCP_OPT_NOP, b''),
        (TCP_OPT_NOP, b''),
        (TCP_OPT_SACK, b'\x37\xf8\x19\x70\x37\xf8\x29\x78')
    ]

    # test a zero-length option
    buf = b'\x02\x00\x01'
    opts = parse_opts(buf)
    assert opts == [
        (TCP_OPT_MSS, b''),
        (TCP_OPT_NOP, b'')
    ]

    # test a one-byte malformed option
    buf = b'\xff'
    opts = parse_opts(buf)
    assert opts == [None]

def test_offset():
    tcpheader = TCP(b'\x01\xbb\xc0\xd7\xb6\x56\xa8\xb9\xd1\xac\xaa\xb1\x50\x18\x40\x00\x56\xf8\x00\x00')
    assert tcpheader.off == 5

    # test setting header offset
    tcpheader.off = 8
    assert bytes(tcpheader) == b'\x01\xbb\xc0\xd7\xb6\x56\xa8\xb9\xd1\xac\xaa\xb1\x80\x18\x40\x00\x56\xf8\x00\x00'

if __name__ == '__main__':
    # Runs all the test associated with this class/file
    test_parse_opts()
    test_offset()
    print('Tests Successful...')
