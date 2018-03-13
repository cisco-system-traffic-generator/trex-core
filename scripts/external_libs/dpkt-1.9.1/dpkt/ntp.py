# $Id: ntp.py 48 2008-05-27 17:31:15Z yardley $
# -*- coding: utf-8 -*-
"""Network Time Protocol."""
from __future__ import print_function

from . import dpkt
from .decorators import deprecated

# NTP v4

# Leap Indicator (LI) Codes
NO_WARNING = 0
LAST_MINUTE_61_SECONDS = 1
LAST_MINUTE_59_SECONDS = 2
ALARM_CONDITION = 3

# Mode Codes
RESERVED = 0
SYMMETRIC_ACTIVE = 1
SYMMETRIC_PASSIVE = 2
CLIENT = 3
SERVER = 4
BROADCAST = 5
CONTROL_MESSAGE = 6
PRIVATE = 7


class NTP(dpkt.Packet):
    """Network Time Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of NTP.
        TODO.
    """

    __hdr__ = (
        ('flags', 'B', 0),
        ('stratum', 'B', 0),
        ('interval', 'B', 0),
        ('precision', 'B', 0),
        ('delay', 'I', 0),
        ('dispersion', 'I', 0),
        ('id', '4s', 0),
        ('update_time', '8s', 0),
        ('originate_time', '8s', 0),
        ('receive_time', '8s', 0),
        ('transmit_time', '8s', 0)
    )

    @property
    def v(self):
        return (self.flags >> 3) & 0x7

    @v.setter
    def v(self, v):
        self.flags = (self.flags & ~0x38) | ((v & 0x7) << 3)

    @property
    def li(self):
        return (self.flags >> 6) & 0x3

    @li.setter
    def li(self, li):
        self.flags = (self.flags & ~0xc0) | ((li & 0x3) << 6)

    @property
    def mode(self):
        return self.flags & 0x7

    @mode.setter
    def mode(self, mode):
        self.flags = (self.flags & ~0x7) | (mode & 0x7)


__s = b'\x24\x02\x04\xef\x00\x00\x00\x84\x00\x00\x33\x27\xc1\x02\x04\x02\xc8\x90\xec\x11\x22\xae\x07\xe5\xc8\x90\xf9\xd9\xc0\x7e\x8c\xcd\xc8\x90\xf9\xd9\xda\xc5\xb0\x78\xc8\x90\xf9\xd9\xda\xc6\x8a\x93'


def test_ntp_pack():
    n = NTP(__s)
    assert (__s == bytes(n))


def test_ntp_unpack():
    n = NTP(__s)
    assert (n.li == NO_WARNING)
    assert (n.v == 4)
    assert (n.mode == SERVER)
    assert (n.stratum == 2)
    assert (n.id == b'\xc1\x02\x04\x02')
    # test get/set functions
    n.li = ALARM_CONDITION
    n.v = 3
    n.mode = CLIENT
    assert (n.li == ALARM_CONDITION)
    assert (n.v == 3)
    assert (n.mode == CLIENT)

if __name__ == '__main__':
    test_ntp_pack()
    test_ntp_unpack()
    print('Tests Successful...')
