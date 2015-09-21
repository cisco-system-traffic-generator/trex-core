# $Id$

"""Snoop file format."""

import sys, time
import dpkt

# RFC 1761

SNOOP_MAGIC = 0x736E6F6F70000000L

SNOOP_VERSION = 2

SDL_8023   = 0
SDL_8024   = 1
SDL_8025   = 2
SDL_8026   = 3
SDL_ETHER  = 4
SDL_HDLC   = 5
SDL_CHSYNC = 6
SDL_IBMCC  = 7
SDL_FDDI   = 8
SDL_OTHER  = 9


dltoff = { SDL_ETHER:14 }

class PktHdr(dpkt.Packet):
    """snoop packet header."""
    __byte_order__ = '!'
    __hdr__ = (
        ('orig_len', 'I', 0),
        ('incl_len', 'I', 0),
        ('rec_len', 'I', 0),
        ('cum_drops', 'I', 0),
        ('ts_sec', 'I', 0),
        ('ts_usec', 'I', 0),
        )

class FileHdr(dpkt.Packet):
    """snoop file header."""
    __byte_order__ = '!'
    __hdr__ = (
        ('magic', 'Q', SNOOP_MAGIC),
        ('v', 'I', SNOOP_VERSION),
        ('linktype', 'I', SDL_ETHER),
        )

class Writer(object):
    """Simple snoop dumpfile writer."""
    def __init__(self, fileobj, linktype=SDL_ETHER):
        self.__f = fileobj
        fh = FileHdr(linktype=linktype)
        self.__f.write(str(fh))

    def writepkt(self, pkt, ts=None):
        if ts is None:
            ts = time.time()
        s = str(pkt)
        n = len(s)
        pad_len = 4 - n % 4 if n % 4 else 0
        ph = PktHdr(orig_len=n,incl_len=n,
                    rec_len=PktHdr.__hdr_len__+n+pad_len,
                    ts_sec=int(ts),
                    ts_usec=int((int(ts) - float(ts)) * 1000000.0))
        self.__f.write(str(ph))
        self.__f.write(s + '\0' * pad_len)

    def close(self):
        self.__f.close()

class Reader(object):
    """Simple pypcap-compatible snoop file reader."""
    
    def __init__(self, fileobj):
        self.name = fileobj.name
        self.fd = fileobj.fileno()
        self.__f = fileobj
        buf = self.__f.read(FileHdr.__hdr_len__)
        self.__fh = FileHdr(buf)
        self.__ph = PktHdr
        if self.__fh.magic != SNOOP_MAGIC:
            raise ValueError, 'invalid snoop header'
        self.dloff = dltoff[self.__fh.linktype]
        self.filter = ''

    def fileno(self):
        return self.fd
    
    def datalink(self):
        return self.__fh.linktype
    
    def setfilter(self, value, optimize=1):
        return NotImplementedError

    def readpkts(self):
        return list(self)
    
    def dispatch(self, cnt, callback, *args):
        if cnt > 0:
            for i in range(cnt):
                ts, pkt = self.next()
                callback(ts, pkt, *args)
        else:
            for ts, pkt in self:
                callback(ts, pkt, *args)

    def loop(self, callback, *args):
        self.dispatch(0, callback, *args)
    
    def __iter__(self):
        self.__f.seek(FileHdr.__hdr_len__)
        while 1:
            buf = self.__f.read(PktHdr.__hdr_len__)
            if not buf: break
            hdr = self.__ph(buf)
            buf = self.__f.read(hdr.rec_len - PktHdr.__hdr_len__)
            yield (hdr.ts_sec + (hdr.ts_usec / 1000000.0), buf[:hdr.incl_len])
