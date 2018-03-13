# -*- coding: utf-8 -*-
"""ATA over Ethernet Protocol."""
from __future__ import absolute_import

import struct

from . import dpkt
from .decorators import deprecated
from .compat import iteritems

class AOE(dpkt.Packet):
    """ATA over Ethernet Protocol.

    See more about the AOE on \
    https://en.wikipedia.org/wiki/ATA_over_Ethernet

    Attributes:
        __hdr__: Header fields of AOE.
        data: Message data.
    """

    __hdr__ = (
        ('ver_fl', 'B', 0x10),
        ('err', 'B', 0),
        ('maj', 'H', 0),
        ('min', 'B', 0),
        ('cmd', 'B', 0),
        ('tag', 'I', 0),
    )
    _cmdsw = {}

    @property
    def ver(self): return self.ver_fl >> 4

    @ver.setter
    def ver(self, ver): self.ver_fl = (ver << 4) | (self.ver_fl & 0xf)

    @property
    def fl(self): return self.ver_fl & 0xf

    @fl.setter
    def fl(self, fl): self.ver_fl = (self.ver_fl & 0xf0) | fl

    @classmethod
    def set_cmd(cls, cmd, pktclass):
        cls._cmdsw[cmd] = pktclass

    @classmethod
    def get_cmd(cls, cmd):
        return cls._cmdsw[cmd]

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        try:
            self.data = self._cmdsw[self.cmd](self.data)
            setattr(self, self.data.__class__.__name__.lower(), self.data)
        except (KeyError, struct.error, dpkt.UnpackError):
            pass

    def pack_hdr(self):
        try:
            return dpkt.Packet.pack_hdr(self)
        except struct.error as e:
            raise dpkt.PackError(str(e))


AOE_CMD_ATA = 0
AOE_CMD_CFG = 1
AOE_FLAG_RSP = 1 << 3


def __load_cmds():
    prefix = 'AOE_CMD_'
    g = globals()

    for k, v in iteritems(g):
        if k.startswith(prefix):
            name = 'aoe' + k[len(prefix):].lower()
            try:
                mod = __import__(name, g, level=1)
                AOE.set_cmd(v, getattr(mod, name.upper()))
            except (ImportError, AttributeError):
                continue


def _mod_init():
    """Post-initialization called when all dpkt modules are fully loaded"""
    if not AOE._cmdsw:
        __load_cmds()
