# $Id: udp.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""User Datagram Protocol."""

import dpkt

UDP_HDR_LEN = 8
UDP_PORT_MAX = 65535


class UDP(dpkt.Packet):
    __hdr__ = (
        ('sport', 'H', 0xdead),
        ('dport', 'H', 0),
        ('ulen', 'H', 8),
        ('sum', 'H', 0)
    )
