# $Id: ipx.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Internetwork Packet Exchange."""
from __future__ import absolute_import

from . import dpkt

IPX_HDR_LEN = 30


class IPX(dpkt.Packet):
    """Internetwork Packet Exchange.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of IPX.
        TODO.
    """
    
    __hdr__ = (
        ('sum', 'H', 0xffff),
        ('len', 'H', IPX_HDR_LEN),
        ('tc', 'B', 0),
        ('pt', 'B', 0),
        ('dst', '12s', ''),
        ('src', '12s', '')
    )
