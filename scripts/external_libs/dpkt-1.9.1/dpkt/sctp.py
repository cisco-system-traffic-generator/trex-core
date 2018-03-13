# $Id: sctp.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Stream Control Transmission Protocol."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt
from . import crc32c

# Stream Control Transmission Protocol
# http://tools.ietf.org/html/rfc2960

# Chunk Types
DATA = 0
INIT = 1
INIT_ACK = 2
SACK = 3
HEARTBEAT = 4
HEARTBEAT_ACK = 5
ABORT = 6
SHUTDOWN = 7
SHUTDOWN_ACK = 8
ERROR = 9
COOKIE_ECHO = 10
COOKIE_ACK = 11
ECNE = 12
CWR = 13
SHUTDOWN_COMPLETE = 14


class SCTP(dpkt.Packet):
    """Stream Control Transmission Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of SCTP.
        TODO.
    """

    __hdr__ = (
        ('sport', 'H', 0),
        ('dport', 'H', 0),
        ('vtag', 'I', 0),
        ('sum', 'I', 0)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        l = []
        while self.data:
            chunk = Chunk(self.data)
            l.append(chunk)
            self.data = self.data[len(chunk):]
        self.data = self.chunks = l

    def __len__(self):
        return self.__hdr_len__ + sum(map(len, self.data))

    def __bytes__(self):
        l = [bytes(x) for x in self.data]
        if self.sum == 0:
            s = crc32c.add(0xffffffff, self.pack_hdr())
            for x in l:
                s = crc32c.add(s, x)
            self.sum = crc32c.done(s)
        return self.pack_hdr() + b''.join(l)


class Chunk(dpkt.Packet):
    __hdr__ = (
        ('type', 'B', INIT),
        ('flags', 'B', 0),
        ('len', 'H', 0)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.data = self.data[:self.len - self.__hdr_len__]


__s = b'\x80\x44\x00\x50\x00\x00\x00\x00\x30\xba\xef\x54\x01\x00\x00\x3c\x3b\xb9\x9c\x46\x00\x01\xa0\x00\x00\x0a\xff\xff\x2b\x2d\x7e\xb2\x00\x05\x00\x08\x9b\xe6\x18\x9b\x00\x05\x00\x08\x9b\xe6\x18\x9c\x00\x0c\x00\x06\x00\x05\x00\x00\x80\x00\x00\x04\xc0\x00\x00\x04\xc0\x06\x00\x08\x00\x00\x00\x00'


def test_sctp_pack():
    sctp = SCTP(__s)
    assert (__s == bytes(sctp))
    sctp.sum = 0
    assert (__s == bytes(sctp))


def test_sctp_unpack():
    sctp = SCTP(__s)
    assert (sctp.sport == 32836)
    assert (sctp.dport == 80)
    assert (len(sctp.chunks) == 1)
    assert (len(sctp) == 72)
    chunk = sctp.chunks[0]
    assert (chunk.type == INIT)
    assert (chunk.len == 60)


if __name__ == '__main__':
    test_sctp_pack()
    test_sctp_unpack()
    print('Tests Successful...')
