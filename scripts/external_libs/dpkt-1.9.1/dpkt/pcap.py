# $Id: pcap.py 77 2011-01-06 15:59:38Z dugsong $
# -*- coding: utf-8 -*-
"""Libpcap file format."""
from __future__ import print_function
from __future__ import absolute_import

import sys
import time
from decimal import Decimal

from . import dpkt

TCPDUMP_MAGIC = 0xa1b2c3d4
TCPDUMP_MAGIC_NANO = 0xa1b23c4d
PMUDPCT_MAGIC = 0xd4c3b2a1
PMUDPCT_MAGIC_NANO = 0x4d3cb2a1

PCAP_VERSION_MAJOR = 2
PCAP_VERSION_MINOR = 4

# see http://www.tcpdump.org/linktypes.html for explanations
DLT_NULL = 0
DLT_EN10MB = 1
DLT_EN3MB = 2
DLT_AX25 = 3
DLT_PRONET = 4
DLT_CHAOS = 5
DLT_IEEE802 = 6
DLT_ARCNET = 7
DLT_SLIP = 8
DLT_PPP = 9
DLT_FDDI = 10
DLT_PFSYNC = 18
DLT_PPP_SERIAL = 50
DLT_PPP_ETHER = 51
DLT_ATM_RFC1483 = 100
DLT_RAW = 101
DLT_C_HDLC = 104
DLT_IEEE802_11 = 105
DLT_FRELAY = 107
DLT_LOOP = 108
DLT_LINUX_SLL = 113
DLT_LTALK = 114
DLT_PFLOG = 117
DLT_PRISM_HEADER = 119
DLT_IP_OVER_FC = 122
DLT_SUNATM = 123
DLT_IEEE802_11_RADIO = 127
DLT_ARCNET_LINUX = 129
DLT_APPLE_IP_OVER_IEEE1394 = 138
DLT_MTP2_WITH_PHDR = 139
DLT_MTP2 = 140
DLT_MTP3 = 141
DLT_SCCP = 142
DLT_DOCSIS = 143
DLT_LINUX_IRDA = 144
DLT_USER0 = 147
DLT_USER1 = 148
DLT_USER2 = 149
DLT_USER3 = 150
DLT_USER4 = 151
DLT_USER5 = 152
DLT_USER6 = 153
DLT_USER7 = 154
DLT_USER8 = 155
DLT_USER9 = 156
DLT_USER10 = 157
DLT_USER11 = 158
DLT_USER12 = 159
DLT_USER13 = 160
DLT_USER14 = 161
DLT_USER15 = 162
DLT_IEEE802_11_RADIO_AVS = 163
DLT_BACNET_MS_TP = 165
DLT_PPP_PPPD = 166
DLT_GPRS_LLC = 169
DLT_GPF_T = 170
DLT_GPF_F = 171
DLT_LINUX_LAPD = 177
DLT_BLUETOOTH_HCI_H4 = 187
DLT_USB_LINUX = 189
DLT_PPI = 192
DLT_IEEE802_15_4 = 195
DLT_SITA = 196
DLT_ERF = 197
DLT_BLUETOOTH_HCI_H4_WITH_PHDR = 201
DLT_AX25_KISS = 202
DLT_LAPD = 203
DLT_PPP_WITH_DIR = 204
DLT_C_HDLC_WITH_DIR = 205
DLT_FRELAY_WITH_DIR = 206
DLT_IPMB_LINUX = 209
DLT_IEEE802_15_4_NONASK_PHY = 215
DLT_USB_LINUX_MMAPPED = 220
DLT_FC_2 = 224
DLT_FC_2_WITH_FRAME_DELIMS = 225
DLT_IPNET = 226
DLT_CAN_SOCKETCAN = 227
DLT_IPV4 = 228
DLT_IPV6 = 229
DLT_IEEE802_15_4_NOFCS = 230
DLT_DBUS = 231
DLT_DVB_CI = 235
DLT_MUX27010 = 236
DLT_STANAG_5066_D_PDU = 237
DLT_NFLOG = 239
DLT_NETANALYZER = 240
DLT_NETANALYZER_TRANSPARENT = 241
DLT_IPOIB = 242
DLT_MPEG_2_TS = 243
DLT_NG40 = 244
DLT_NFC_LLCP = 245
DLT_INFINIBAND = 247
DLT_SCTP = 248
DLT_USBPCAP = 249
DLT_RTAC_SERIAL = 250
DLT_BLUETOOTH_LE_LL = 251
DLT_NETLINK = 253
DLT_BLUETOOTH_LINUX_MONITOR = 253
DLT_BLUETOOTH_BREDR_BB = 255
DLT_BLUETOOTH_LE_LL_WITH_PHDR = 256
DLT_PROFIBUS_DL = 257
DLT_PKTAP = 258
DLT_EPON = 259
DLT_IPMI_HPM_2 = 260
DLT_ZWAVE_R1_R2 = 261
DLT_ZWAVE_R3 = 262
DLT_WATTSTOPPER_DLM = 263
DLT_ISO_14443 = 264

if sys.platform.find('openbsd') != -1:
    DLT_LOOP = 12
    DLT_RAW = 14
else:
    DLT_LOOP = 108
    DLT_RAW = 12

dltoff = {DLT_NULL: 4, DLT_EN10MB: 14, DLT_IEEE802: 22, DLT_ARCNET: 6,
          DLT_SLIP: 16, DLT_PPP: 4, DLT_FDDI: 21, DLT_PFLOG: 48, DLT_PFSYNC: 4,
          DLT_LOOP: 4, DLT_LINUX_SLL: 16}


class PktHdr(dpkt.Packet):
    """pcap packet header.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of pcap header.
        TODO.
    """
    __hdr__ = (
        ('tv_sec', 'I', 0),
        ('tv_usec', 'I', 0),
        ('caplen', 'I', 0),
        ('len', 'I', 0),
    )


class LEPktHdr(PktHdr):
    __byte_order__ = '<'


class FileHdr(dpkt.Packet):
    """pcap file header.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of pcap file header.
        TODO.
    """

    __hdr__ = (
        ('magic', 'I', TCPDUMP_MAGIC),
        ('v_major', 'H', PCAP_VERSION_MAJOR),
        ('v_minor', 'H', PCAP_VERSION_MINOR),
        ('thiszone', 'I', 0),
        ('sigfigs', 'I', 0),
        ('snaplen', 'I', 1500),
        ('linktype', 'I', 1),
    )


class LEFileHdr(FileHdr):
    __byte_order__ = '<'


class Writer(object):
    """Simple pcap dumpfile writer.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of simple pcap dumpfile writer.
        TODO.
    """

    def __init__(self, fileobj, snaplen=1500, linktype=DLT_EN10MB, nano=False):
        self.__f = fileobj
        self._precision = 9 if nano else 6
        magic = TCPDUMP_MAGIC_NANO if nano else TCPDUMP_MAGIC
        if sys.byteorder == 'little':
            fh = LEFileHdr(snaplen=snaplen, linktype=linktype, magic=magic)
        else:
            fh = FileHdr(snaplen=snaplen, linktype=linktype, magic=magic)
        self.__f.write(bytes(fh))

    def writepkt(self, pkt, ts=None):
        if ts is None:
            ts = time.time()
        s = bytes(pkt)
        n = len(s)
        sec = int(ts)
        usec = int(round(ts % 1 * 10 ** self._precision))
        if sys.byteorder == 'little':
            ph = LEPktHdr(tv_sec=sec,
                          tv_usec=usec,
                          caplen=n, len=n)
        else:
            ph = PktHdr(tv_sec=sec,
                        tv_usec=usec,
                        caplen=n, len=n)
        self.__f.write(bytes(ph))
        self.__f.write(s)

    def close(self):
        self.__f.close()


class Reader(object):
    """Simple pypcap-compatible pcap file reader.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of simple pypcap-compatible pcap file reader.
        TODO.
    """

    def __init__(self, fileobj):
        self.name = getattr(fileobj, 'name', '<%s>' % fileobj.__class__.__name__)
        self.__f = fileobj
        buf = self.__f.read(FileHdr.__hdr_len__)
        self.__fh = FileHdr(buf)
        self.__ph = PktHdr
        if self.__fh.magic in (PMUDPCT_MAGIC, PMUDPCT_MAGIC_NANO):
            self.__fh = LEFileHdr(buf)
            self.__ph = LEPktHdr
        elif self.__fh.magic not in (TCPDUMP_MAGIC, TCPDUMP_MAGIC_NANO):
            raise ValueError('invalid tcpdump header')
        if self.__fh.linktype in dltoff:
            self.dloff = dltoff[self.__fh.linktype]
        else:
            self.dloff = 0
        self._divisor = 1E6 if self.__fh.magic in (TCPDUMP_MAGIC, PMUDPCT_MAGIC) else Decimal('1E9')
        self.snaplen = self.__fh.snaplen
        self.filter = ''
        self.__iter = iter(self)

    @property
    def fd(self):
        return self.__f.fileno()

    def fileno(self):
        return self.fd

    def datalink(self):
        return self.__fh.linktype

    def setfilter(self, value, optimize=1):
        return NotImplementedError

    def readpkts(self):
        return list(self)

    def __next__(self):
        return next(self.__iter)

    def dispatch(self, cnt, callback, *args):
        """Collect and process packets with a user callback.

        Return the number of packets processed, or 0 for a savefile.

        Arguments:

        cnt      -- number of packets to process;
                    or 0 to process all packets until EOF
        callback -- function with (timestamp, pkt, *args) prototype
        *args    -- optional arguments passed to callback on execution
        """
        processed = 0
        if cnt > 0:
            for _ in range(cnt):
                try:
                    ts, pkt = next(iter(self))
                except StopIteration:
                    break
                callback(ts, pkt, *args)
                processed += 1
        else:
            for ts, pkt in self:
                callback(ts, pkt, *args)
                processed += 1
        return processed

    def loop(self, callback, *args):
        self.dispatch(0, callback, *args)

    def __iter__(self):
        while 1:
            buf = self.__f.read(PktHdr.__hdr_len__)
            if not buf:
                break
            hdr = self.__ph(buf)
            buf = self.__f.read(hdr.caplen)
            yield (hdr.tv_sec + (hdr.tv_usec / self._divisor), buf)


def test_pcap_endian():
    be = b'\xa1\xb2\xc3\xd4\x00\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x60\x00\x00\x00\x01'
    le = b'\xd4\xc3\xb2\xa1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x60\x00\x00\x00\x01\x00\x00\x00'
    befh = FileHdr(be)
    lefh = LEFileHdr(le)
    assert (befh.linktype == lefh.linktype)


def test_reader():
    data = (  # full libpcap file with one packet
        b'\xd4\xc3\xb2\xa1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x01\x00\x00\x00'
        b'\xb2\x67\x4a\x42\xae\x91\x07\x00\x46\x00\x00\x00\x46\x00\x00\x00\x00\xc0\x9f\x32\x41\x8c\x00\xe0'
        b'\x18\xb1\x0c\xad\x08\x00\x45\x00\x00\x38\x00\x00\x40\x00\x40\x11\x65\x47\xc0\xa8\xaa\x08\xc0\xa8'
        b'\xaa\x14\x80\x1b\x00\x35\x00\x24\x85\xed'
    )

    # --- BytesIO tests ---
    from .compat import BytesIO

    # BytesIO
    fobj = BytesIO(data)
    reader = Reader(fobj)
    assert reader.name == '<BytesIO>'
    _, buf1 = next(iter(reader))
    assert buf1 == data[FileHdr.__hdr_len__ + PktHdr.__hdr_len__:]

    # --- dispatch() tests ---

    # test count = 0
    fobj.seek(0)
    reader = Reader(fobj)
    assert reader.dispatch(0, lambda ts, pkt: None) == 1

    # test count > 0
    fobj.seek(0)
    reader = Reader(fobj)
    assert reader.dispatch(4, lambda ts, pkt: None) == 1

    # test iterative dispatch
    fobj.seek(0)
    reader = Reader(fobj)
    assert reader.dispatch(1, lambda ts, pkt: None) == 1
    assert reader.dispatch(1, lambda ts, pkt: None) == 0


def test_writer_precision():
    data = b'foo'
    from .compat import BytesIO

    # default precision
    fobj = BytesIO()
    writer = Writer(fobj)
    writer.writepkt(data, ts=1454725786.526401)
    fobj.flush()
    fobj.seek(0)

    reader = Reader(fobj)
    ts, buf1 = next(iter(reader))
    assert ts == 1454725786.526401
    assert buf1 == b'foo'

    # nano precision
    from decimal import Decimal

    fobj = BytesIO()
    writer = Writer(fobj, nano=True)
    writer.writepkt(data, ts=Decimal('1454725786.010203045'))
    fobj.flush()
    fobj.seek(0)

    reader = Reader(fobj)
    ts, buf1 = next(iter(reader))
    assert ts == Decimal('1454725786.010203045')
    assert buf1 == b'foo'


if __name__ == '__main__':
    test_pcap_endian()
    test_reader()
    test_writer_precision()

    print('Tests Successful...')
