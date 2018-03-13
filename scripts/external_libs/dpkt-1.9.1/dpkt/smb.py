# $Id: smb.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Server Message Block."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt


# https://msdn.microsoft.com/en-us/library/ee441774.aspx

SMB_FLAGS_LOCK_AND_READ_OK = 0x01
SMB_FLAGS_BUF_AVAIL = 0x02
SMB_FLAGS_CASE_INSENSITIVE = 0x08
SMB_FLAGS_CANONICALIZED_PATHS = 0x10
SMB_FLAGS_OPLOCK = 0x20
SMB_FLAGS_OPBATCH = 0x40
SMB_FLAGS_REPLY = 0x80

SMB_FLAGS2_LONG_NAMES = 0x0001
SMB_FLAGS2_EXTENDED_ATTRIBUTES = 0x0002
SMB_FLAGS2_SECURITY_SIGNATURES = 0x0004
SMB_FLAGS2_COMPRESSED = 0x0008
SMB_FLAGS2_SECURITY_SIGNATURES_REQUIRED = 0x0010
SMB_FLAGS2_IS_LONG_NAME = 0x0040
SMB_FLAGS2_REVERSE_PATH = 0x0400
SMB_FLAGS2_EXTENDED_SECURITY = 0x0800
SMB_FLAGS2_DFS = 0x1000
SMB_FLAGS2_PAGING_IO = 0x2000
SMB_FLAGS2_NT_STATUS = 0x4000
SMB_FLAGS2_UNICODE = 0x8000

SMB_STATUS_SUCCESS = 0x00000000


class SMB(dpkt.Packet):
    """Server Message Block.

    TODO: Longer class information....

    Attributes:
        __hdr__ = [
            ('proto', '4s', b'\xffSMB'),
            ('cmd', 'B', 0),
            ('status', 'I', SMB_STATUS_SUCCESS),
            ('flags', 'B', 0),
            ('flags2', 'H', 0),
            ('_pidhi', 'H', 0),
            ('security', '8s', b''),
            ('rsvd', 'H', 0),
            ('tid', 'H', 0),
            ('_pidlo', 'H', 0),
            ('uid', 'H', 0),
            ('mid', 'H', 0)
        ]
    """

    __byte_order__ = '<'
    __hdr__ = [
        ('proto', '4s', b'\xffSMB'),
        ('cmd', 'B', 0),
        ('status', 'I', SMB_STATUS_SUCCESS),
        ('flags', 'B', 0),
        ('flags2', 'H', 0),
        ('_pidhi', 'H', 0),
        ('security', '8s', b''),
        ('rsvd', 'H', 0),
        ('tid', 'H', 0),
        ('_pidlo', 'H', 0),
        ('uid', 'H', 0),
        ('mid', 'H', 0)
    ]

    @property
    def pid(self):
        return (self._pidhi << 16) | self._pidlo

    @pid.setter
    def pid(self, v):
        self._pidhi = v >> 16
        self._pidlo = v & 0xffff


def test_smb():
    buf = b'\xffSMB\xa0\x00\x00\x00\x00\x08\x03\xc8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\xfa\x7a\x00\x08\x53\x02'
    smb = SMB(buf)

    assert smb.flags == SMB_FLAGS_CASE_INSENSITIVE
    assert smb.flags2 == SMB_FLAGS2_UNICODE | SMB_FLAGS2_NT_STATUS | SMB_FLAGS2_EXTENDED_SECURITY | SMB_FLAGS2_EXTENDED_ATTRIBUTES | SMB_FLAGS2_LONG_NAMES
    assert smb.pid == 31482
    assert smb.uid == 2048
    assert smb.mid == 595
    print(repr(smb))

    smb = SMB()
    smb.pid = 0x00081020
    smb.uid = 0x800
    assert str(smb) == str(b'\xffSMB\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x10\x00\x08\x00\x00')


if __name__ == '__main__':
    test_smb()
    print('Tests Successful...')
