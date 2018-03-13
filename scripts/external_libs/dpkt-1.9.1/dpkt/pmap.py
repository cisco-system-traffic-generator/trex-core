# $Id: pmap.py 23 2006-11-08 15:45:33Z dugsong $
# -*- coding: utf-8 -*-
"""Portmap / rpcbind."""
from __future__ import absolute_import

from . import dpkt

PMAP_PROG = 100000
PMAP_PROCDUMP = 4
PMAP_VERS = 2


class Pmap(dpkt.Packet):
    """Portmap / rpcbind.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of Pmap.
        TODO.
    """
    
    __hdr__ = (
        ('prog', 'I', 0),
        ('vers', 'I', 0),
        ('prot', 'I', 0),
        ('port', 'I', 0),
    )
