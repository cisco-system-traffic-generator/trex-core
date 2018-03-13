# $Id: ip6.py 87 2013-03-05 19:41:04Z andrewflnr@gmail.com $
# -*- coding: utf-8 -*-
"""Internet Protocol, version 6."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt
from . import ip
from .compat import compat_ord


class IP6(dpkt.Packet):
    """Internet Protocol, version 6.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of IPv6.
        TODO.
    """

    __hdr__ = (
        ('_v_fc_flow', 'I', 0x60000000),
        ('plen', 'H', 0),  # payload length (not including header)
        ('nxt', 'B', 0),  # next header protocol
        ('hlim', 'B', 0),  # hop limit
        ('src', '16s', ''),
        ('dst', '16s', '')
    )

    _protosw = ip.IP._protosw

    @property
    def v(self):
        return self._v_fc_flow >> 28

    @v.setter
    def v(self, v):
        self._v_fc_flow = (self._v_fc_flow & ~0xf0000000) | (v << 28)

    @property
    def fc(self):
        return (self._v_fc_flow >> 20) & 0xff

    @fc.setter
    def fc(self, v):
        self._v_fc_flow = (self._v_fc_flow & ~0xff00000) | (v << 20)

    @property
    def flow(self):
        return self._v_fc_flow & 0xfffff

    @flow.setter
    def flow(self, v):
        self._v_fc_flow = (self._v_fc_flow & ~0xfffff) | (v & 0xfffff)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.extension_hdrs = {}
        # NOTE: self.extension_hdrs is not accurate, as it doesn't support duplicate header types.
        # According to RFC-1883 "Each extension header should occur at most once, except for the
        # Destination Options header which should occur at most twice".
        # Secondly, the .headers_str() method attempts to pack the extension headers in order as
        # defined in the RFC, however it doesn't adjust the next header (nxt) pointer accordingly.
        # Here we introduce the new field .all_extension_headers; it allows duplicate types and
        # keeps the original order.
        self.all_extension_headers = []

        if self.plen:
            buf = self.data[:self.plen]
        else:  # due to jumbo payload or TSO
            buf = self.data

        next_ext_hdr = self.nxt

        while next_ext_hdr in ext_hdrs:
            ext = ext_hdrs_cls[next_ext_hdr](buf)
            self.extension_hdrs[next_ext_hdr] = ext
            self.all_extension_headers.append(ext)
            buf = buf[ext.length:]
            next_ext_hdr = getattr(ext, 'nxt', None)

        # set the payload protocol id
        if next_ext_hdr is not None:
            self.p = next_ext_hdr

        try:
            self.data = self._protosw[next_ext_hdr](buf)
            setattr(self, self.data.__class__.__name__.lower(), self.data)
        except (KeyError, dpkt.UnpackError):
            self.data = buf

    def headers_str(self):
        # If all_extension_headers is available, return the headers as they originally appeared
        if self.all_extension_headers:
            return b''.join(bytes(ext) for ext in self.all_extension_headers)

        # Output extension headers in order defined in RFC1883 (except dest opts)
        header_str = b""
        for hdr in ext_hdrs:
            if hdr in self.extension_hdrs:
                header_str += bytes(self.extension_hdrs[hdr])
        return header_str

    def __bytes__(self):
        if (self.p == 6 or self.p == 17 or self.p == 58) and not self.data.sum:
            # XXX - set TCP, UDP, and ICMPv6 checksums
            p = bytes(self.data)
            s = dpkt.struct.pack('>16s16sxBH', self.src, self.dst, self.nxt, len(p))
            s = dpkt.in_cksum_add(0, s)
            s = dpkt.in_cksum_add(s, p)
            try:
                self.data.sum = dpkt.in_cksum_done(s)
            except AttributeError:
                pass
        return self.pack_hdr() + self.headers_str() + bytes(self.data)

    @classmethod
    def set_proto(cls, p, pktclass):
        cls._protosw[p] = pktclass

    @classmethod
    def get_proto(cls, p):
        return cls._protosw[p]


class IP6ExtensionHeader(dpkt.Packet):
    """
    An extension header is very similar to a 'sub-packet'.
    We just want to re-use all the hdr unpacking etc.
    """
    pass


class IP6OptsHeader(IP6ExtensionHeader):
    __hdr__ = (
        ('nxt', 'B', 0),  # next extension header protocol
        ('len', 'B', 0)  # option data length in 8 octect units (ignoring first 8 octets) so, len 0 == 64bit header
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.length = (self.len + 1) * 8
        options = []

        index = 0

        while index < self.length - 2:
            opt_type = compat_ord(self.data[index])

            # PAD1 option
            if opt_type == 0:
                index += 1
                continue

            opt_length = compat_ord(self.data[index + 1])

            if opt_type == 1:  # PADN option
                # PADN uses opt_length bytes in total
                index += opt_length + 2
                continue

            options.append(
                {'type': opt_type, 'opt_length': opt_length, 'data': self.data[index + 2:index + 2 + opt_length]})

            # add the two chars and the option_length, to move to the next option
            index += opt_length + 2

        self.options = options
        self.data = buf[2:self.length]  # keep raw data with all pad options, but not the following data


class IP6HopOptsHeader(IP6OptsHeader):
    pass


class IP6DstOptsHeader(IP6OptsHeader):
    pass


class IP6RoutingHeader(IP6ExtensionHeader):
    __hdr__ = (
        ('nxt', 'B', 0),  # next extension header protocol
        ('len', 'B', 0),  # extension data length in 8 octect units (ignoring first 8 octets) (<= 46 for type 0)
        ('type', 'B', 0),  # routing type (currently, only 0 is used)
        ('segs_left', 'B', 0),  # remaining segments in route, until destination (<= 23)
        ('rsvd_sl_bits', 'I', 0),  # reserved (1 byte), strict/loose bitmap for addresses
    )

    @property
    def sl_bits(self):
        return self.rsvd_sl_bits & 0xffffff

    @sl_bits.setter
    def sl_bits(self, v):
        self.rsvd_sl_bits = (self.rsvd_sl_bits & ~0xfffff) | (v & 0xfffff)

    def unpack(self, buf):
        hdr_size = 8
        addr_size = 16

        dpkt.Packet.unpack(self, buf)

        addresses = []
        num_addresses = self.len // 2
        buf = buf[hdr_size:hdr_size + num_addresses * addr_size]

        for i in range(num_addresses):
            addresses.append(buf[i * addr_size: i * addr_size + addr_size])

        self.data = buf
        self.addresses = addresses
        self.length = self.len * 8 + 8


class IP6FragmentHeader(IP6ExtensionHeader):
    __hdr__ = (
        ('nxt', 'B', 0),  # next extension header protocol
        ('resv', 'B', 0),  # reserved, set to 0
        ('frag_off_resv_m', 'H', 0),  # frag offset (13 bits), reserved zero (2 bits), More frags flag
        ('id', 'I', 0)  # fragments id
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.length = self.__hdr_len__
        self.data = b''

    @property
    def frag_off(self):
        return self.frag_off_resv_m >> 3

    @frag_off.setter
    def frag_off(self, v):
        self.frag_off_resv_m = (self.frag_off_resv_m & ~0xfff8) | (v << 3)

    @property
    def m_flag(self):
        return self.frag_off_resv_m & 1

    @m_flag.setter
    def m_flag(self, v):
        self.frag_off_resv_m = (self.frag_off_resv_m & ~0xfffe) | v


class IP6AHHeader(IP6ExtensionHeader):
    __hdr__ = (
        ('nxt', 'B', 0),  # next extension header protocol
        ('len', 'B', 0),  # length of header in 4 octet units (ignoring first 2 units)
        ('resv', 'H', 0),  # reserved, 2 bytes of 0
        ('spi', 'I', 0),  # SPI security parameter index
        ('seq', 'I', 0)  # sequence no.
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.length = (self.len + 2) * 4
        self.auth_data = self.data[:(self.len - 1) * 4]


class IP6ESPHeader(IP6ExtensionHeader):
    __hdr__ = (
        ('spi', 'I', 0),
        ('seq', 'I', 0)
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        self.length = self.__hdr_len__ + len(self.data)


ext_hdrs = [ip.IP_PROTO_HOPOPTS, ip.IP_PROTO_ROUTING, ip.IP_PROTO_FRAGMENT, ip.IP_PROTO_AH, ip.IP_PROTO_ESP,
            ip.IP_PROTO_DSTOPTS]
ext_hdrs_cls = {ip.IP_PROTO_HOPOPTS: IP6HopOptsHeader,
                ip.IP_PROTO_ROUTING: IP6RoutingHeader,
                ip.IP_PROTO_FRAGMENT: IP6FragmentHeader,
                ip.IP_PROTO_ESP: IP6ESPHeader,
                ip.IP_PROTO_AH: IP6AHHeader,
                ip.IP_PROTO_DSTOPTS: IP6DstOptsHeader}


# Unit tests

def test_ipg():
    s = (b'\x60\x00\x00\x00\x00\x28\x06\x40\xfe\x80\x00\x00\x00\x00\x00\x00\x02\x11\x24\xff\xfe\x8c'
         b'\x11\xde\xfe\x80\x00\x00\x00\x00\x00\x00\x02\xb0\xd0\xff\xfe\xe1\x80\x72\xcd\xca\x00\x16'
         b'\x04\x84\x46\xd5\x00\x00\x00\x00\xa0\x02\xff\xff\xf8\x09\x00\x00\x02\x04\x05\xa0\x01\x03'
         b'\x03\x00\x01\x01\x08\x0a\x7d\x18\x35\x3f\x00\x00\x00\x00')
    _ip = IP6(s)

    # basic properties
    assert _ip.v == 6
    assert _ip.fc == 0
    assert _ip.flow == 0

    _ip.data.sum = 0
    s2 = bytes(_ip)
    assert s == s2


def test_ip6_routing_header():
    s = (b'\x60\x00\x00\x00\x00\x3c\x2b\x40\x20\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
         b'\xde\xca\x20\x47\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xca\xfe\x06\x04\x00\x02'
         b'\x00\x00\x00\x00\x20\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xde\xca\x20\x22'
         b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xde\xca\x00\x14\x00\x50\x00\x00\x00\x00'
         b'\x00\x00\x00\x00\x50\x02\x20\x00\x91\x7f\x00\x00')
    _ip = IP6(s)
    s2 = bytes(_ip)
    # 43 is Routing header id
    assert len(_ip.extension_hdrs[43].addresses) == 2
    assert _ip.tcp
    assert s == s2


def test_ip6_fragment_header():
    s = b'\x06\xee\xff\xfb\x00\x00\xff\xff'
    fh = IP6FragmentHeader(s)
    # s2 = str(fh) variable 's2' is not used
    assert fh.nxt == 6
    assert fh.id == 65535
    assert fh.frag_off == 8191
    assert fh.m_flag == 1
    assert bytes(fh) == s

    # IP6 with fragment header
    s = (b'\x60\x00\x00\x00\x00\x10\x2c\x00\x02\x22\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
         b'\x00\x02\x03\x33\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x29\x00\x00\x01'
         b'\x00\x00\x00\x00\x60\x00\x00\x00\x00\x10\x2c\x00')
    _ip = IP6(s)
    assert bytes(_ip) == s


def test_ip6_options_header():
    s = (b'\x3b\x04\x01\x02\x00\x00\xc9\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
         b'\x00\x00\x01\x00\xc2\x04\x00\x00\x00\x00\x05\x02\x00\x00\x01\x02\x00\x00')
    options = IP6OptsHeader(s).options
    assert len(options) == 3
    assert bytes(IP6OptsHeader(s)) == s


def test_ip6_ah_header():
    s = b'\x3b\x04\x00\x00\x02\x02\x02\x02\x01\x01\x01\x01\x78\x78\x78\x78\x78\x78\x78\x78'
    ah = IP6AHHeader(s)
    assert ah.length == 24
    assert ah.auth_data == b'xxxxxxxx'
    assert ah.spi == 0x2020202
    assert ah.seq == 0x1010101
    assert bytes(ah) == s


def test_ip6_esp_header():
    s = (b'\x00\x00\x01\x00\x00\x00\x00\x44\xe2\x4f\x9e\x68\xf3\xcd\xb1\x5f\x61\x65\x42\x8b\x78\x0b'
         b'\x4a\xfd\x13\xf0\x15\x98\xf5\x55\x16\xa8\x12\xb3\xb8\x4d\xbc\x16\xb2\x14\xbe\x3d\xf9\x96'
         b'\xd4\xa0\x39\x1f\x85\x74\x25\x81\x83\xa6\x0d\x99\xb6\xba\xa3\xcc\xb6\xe0\x9a\x78\xee\xf2'
         b'\xaf\x9a')
    esp = IP6ESPHeader(s)
    assert esp.length == 68
    assert esp.spi == 256
    assert bytes(esp) == s


def test_ip6_extension_headers():
    p = (b'\x60\x00\x00\x00\x00\x3c\x2b\x40\x20\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
         b'\xde\xca\x20\x47\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xca\xfe\x06\x04\x00\x02'
         b'\x00\x00\x00\x00\x20\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xde\xca\x20\x22'
         b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xde\xca\x00\x14\x00\x50\x00\x00\x00\x00'
         b'\x00\x00\x00\x00\x50\x02\x20\x00\x91\x7f\x00\x00')
    _ip = IP6(p)
    o = (b'\x3b\x04\x01\x02\x00\x00\xc9\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
         b'\x00\x00\x01\x00\xc2\x04\x00\x00\x00\x00\x05\x02\x00\x00\x01\x02\x00\x00')
    _ip.extension_hdrs[0] = IP6HopOptsHeader(o)
    fh = b'\x06\xee\xff\xfb\x00\x00\xff\xff'
    _ip.extension_hdrs[44] = IP6FragmentHeader(fh)
    ah = b'\x3b\x04\x00\x00\x02\x02\x02\x02\x01\x01\x01\x01\x78\x78\x78\x78\x78\x78\x78\x78'
    _ip.extension_hdrs[51] = IP6AHHeader(ah)
    do = b'\x3b\x02\x01\x02\x00\x00\xc9\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    _ip.extension_hdrs[60] = IP6DstOptsHeader(do)
    assert len(_ip.extension_hdrs) == 5


def test_ip6_all_extension_headers():  # https://github.com/kbandla/dpkt/pull/403
    s = (b'\x60\x00\x00\x00\x00\x47\x3c\x40\xfe\xd0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01'
         b'\x00\x02\xfe\xd0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x01\x3c\x00\x01\x04'
         b'\x00\x00\x00\x00\x3c\x00\x01\x04\x00\x00\x00\x00\x2c\x00\x01\x04\x00\x00\x00\x00\x2c\x00'
         b'\x00\x00\x00\x00\x00\x00\x3c\x00\x00\x00\x00\x00\x00\x00\x2c\x00\x01\x04\x00\x00\x00\x00'
         b'\x3a\x00\x00\x00\x00\x00\x00\x00\x80\x00\xd8\xe5\x0c\x1a\x00\x00\x50\x61\x79\x4c\x6f\x61'
         b'\x64')
    _ip = IP6(s)
    assert _ip.p == 58  # ICMPv6
    hdrs = _ip.all_extension_headers
    assert len(hdrs) == 7
    assert isinstance(hdrs[0], IP6DstOptsHeader)
    assert isinstance(hdrs[3], IP6FragmentHeader)
    assert isinstance(hdrs[5], IP6DstOptsHeader)
    assert bytes(_ip) == s


if __name__ == '__main__':
    test_ipg()
    test_ip6_routing_header()
    test_ip6_fragment_header()
    test_ip6_options_header()
    test_ip6_ah_header()
    test_ip6_esp_header()
    test_ip6_extension_headers()
    test_ip6_all_extension_headers()
    print('Tests Successful...')
