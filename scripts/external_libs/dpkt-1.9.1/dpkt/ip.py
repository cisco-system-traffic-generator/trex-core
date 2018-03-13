# $Id: ip.py 87 2013-03-05 19:41:04Z andrewflnr@gmail.com $
# -*- coding: utf-8 -*-
"""Internet Protocol."""
from __future__ import print_function
from __future__ import absolute_import

from . import dpkt
from .decorators import deprecated
from .compat import iteritems

class IP(dpkt.Packet):
    """Internet Protocol.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of IP.
        TODO.
    """

    __hdr__ = (
        ('_v_hl', 'B', (4 << 4) | (20 >> 2)),
        ('tos', 'B', 0),
        ('len', 'H', 20),
        ('id', 'H', 0),
        ('off', 'H', 0),
        ('ttl', 'B', 64),
        ('p', 'B', 0),
        ('sum', 'H', 0),
        ('src', '4s', b'\x00' * 4),
        ('dst', '4s', b'\x00' * 4)
    )
    _protosw = {}
    opts = b''

    def __init__(self, *args, **kwargs):
        super(IP, self).__init__(*args, **kwargs)

        # If IP packet is not initialized by string and the len field has
        # been rewritten.
        if not args and 'len' not in kwargs:
            self.len = self.__len__()

    @property
    def v(self):
        return self._v_hl >> 4

    @v.setter
    def v(self, v):
        self._v_hl = (v << 4) | (self._v_hl & 0xf)

    @property
    def hl(self):
        return self._v_hl & 0xf

    @hl.setter
    def hl(self, hl):
        self._v_hl = (self._v_hl & 0xf0) | hl

    @property
    def rf(self):
        return (self.off >> 15) & 0x1

    @rf.setter
    def rf(self, rf):
        self.off = (self.off & ~IP_RF) | (rf << 15)

    @property
    def df(self):
        return (self.off >> 14) & 0x1

    @df.setter
    def df(self, df):
        self.off = (self.off & ~IP_DF) | (df << 14)

    @property
    def mf(self):
        return (self.off >> 13) & 0x1

    @mf.setter
    def mf(self, mf):
        self.off = (self.off & ~IP_MF) | (mf << 13)

    @property
    def offset(self):
        return (self.off & IP_OFFMASK) << 3

    @offset.setter
    def offset(self, offset):
        self.off = (self.off & ~IP_OFFMASK) | (offset >> 3)

    def __len__(self):
        return self.__hdr_len__ + len(self.opts) + len(self.data)

    def __bytes__(self):
        self.len = self.__len__()
        if self.sum == 0:
            self.sum = dpkt.in_cksum(self.pack_hdr() + bytes(self.opts))
            if (self.p == 6 or self.p == 17) and (self.off & (IP_MF | IP_OFFMASK)) == 0 and \
                    isinstance(self.data, dpkt.Packet) and self.data.sum == 0:
                # Set zeroed TCP and UDP checksums for non-fragments.
                p = bytes(self.data)
                s = dpkt.struct.pack('>4s4sxBH', self.src, self.dst,
                                     self.p, len(p))
                s = dpkt.in_cksum_add(0, s)
                s = dpkt.in_cksum_add(s, p)
                self.data.sum = dpkt.in_cksum_done(s)
                if self.p == 17 and self.data.sum == 0:
                    self.data.sum = 0xffff  # RFC 768
                    # XXX - skip transports which don't need the pseudoheader
        return self.pack_hdr() + bytes(self.opts) + bytes(self.data)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        ol = ((self._v_hl & 0xf) << 2) - self.__hdr_len__
        if ol < 0:
            raise dpkt.UnpackError('invalid header length')
        self.opts = buf[self.__hdr_len__:self.__hdr_len__ + ol]
        if self.len:
            buf = buf[self.__hdr_len__ + ol:self.len]
        else:  # very likely due to TCP segmentation offload
            buf = buf[self.__hdr_len__ + ol:]
        try:
            self.data = self._protosw[self.p](buf) if self.offset == 0 else buf
            setattr(self, self.data.__class__.__name__.lower(), self.data)
        except (KeyError, dpkt.UnpackError):
            self.data = buf

    @classmethod
    def set_proto(cls, p, pktclass):
        cls._protosw[p] = pktclass

    @classmethod
    def get_proto(cls, p):
        return cls._protosw[p]

# IP Headers
IP_ADDR_LEN = 0x04
IP_ADDR_BITS = 0x20

IP_HDR_LEN = 0x14
IP_OPT_LEN = 0x02
IP_OPT_LEN_MAX = 0x28
IP_HDR_LEN_MAX = IP_HDR_LEN + IP_OPT_LEN_MAX

IP_LEN_MAX = 0xffff
IP_LEN_MIN = IP_HDR_LEN

# Reserved Addresses
IP_ADDR_ANY = "\x00\x00\x00\x00"    # 0.0.0.0
IP_ADDR_BROADCAST = "\xff\xff\xff\xff"    # 255.255.255.255
IP_ADDR_LOOPBACK = "\x7f\x00\x00\x01"    # 127.0.0.1
IP_ADDR_MCAST_ALL = "\xe0\x00\x00\x01"    # 224.0.0.1
IP_ADDR_MCAST_LOCAL = "\xe0\x00\x00\xff"    # 224.0.0.255

# Type of service (ip_tos), RFC 1349 ("obsoleted by RFC 2474")
IP_TOS_DEFAULT = 0x00  # default
IP_TOS_LOWDELAY = 0x10  # low delay
IP_TOS_THROUGHPUT = 0x08  # high throughput
IP_TOS_RELIABILITY = 0x04  # high reliability
IP_TOS_LOWCOST = 0x02  # low monetary cost - XXX
IP_TOS_ECT = 0x02  # ECN-capable transport
IP_TOS_CE = 0x01  # congestion experienced

# IP precedence (high 3 bits of ip_tos), hopefully unused
IP_TOS_PREC_ROUTINE = 0x00
IP_TOS_PREC_PRIORITY = 0x20
IP_TOS_PREC_IMMEDIATE = 0x40
IP_TOS_PREC_FLASH = 0x60
IP_TOS_PREC_FLASHOVERRIDE = 0x80
IP_TOS_PREC_CRITIC_ECP = 0xa0
IP_TOS_PREC_INTERNETCONTROL = 0xc0
IP_TOS_PREC_NETCONTROL = 0xe0

# Fragmentation flags (ip_off)
IP_RF = 0x8000  # reserved
IP_DF = 0x4000  # don't fragment
IP_MF = 0x2000  # more fragments (not last frag)
IP_OFFMASK = 0x1fff  # mask for fragment offset

# Time-to-live (ip_ttl), seconds
IP_TTL_DEFAULT = 64  # default ttl, RFC 1122, RFC 1340
IP_TTL_MAX = 255  # maximum ttl

# Protocol (ip_p) - http://www.iana.org/assignments/protocol-numbers
IP_PROTO_IP = 0  # dummy for IP
IP_PROTO_HOPOPTS = IP_PROTO_IP  # IPv6 hop-by-hop options
IP_PROTO_ICMP = 1  # ICMP
IP_PROTO_IGMP = 2  # IGMP
IP_PROTO_GGP = 3  # gateway-gateway protocol
IP_PROTO_IPIP = 4  # IP in IP
IP_PROTO_ST = 5  # ST datagram mode
IP_PROTO_TCP = 6  # TCP
IP_PROTO_CBT = 7  # CBT
IP_PROTO_EGP = 8  # exterior gateway protocol
IP_PROTO_IGP = 9  # interior gateway protocol
IP_PROTO_BBNRCC = 10  # BBN RCC monitoring
IP_PROTO_NVP = 11  # Network Voice Protocol
IP_PROTO_PUP = 12  # PARC universal packet
IP_PROTO_ARGUS = 13  # ARGUS
IP_PROTO_EMCON = 14  # EMCON
IP_PROTO_XNET = 15  # Cross Net Debugger
IP_PROTO_CHAOS = 16  # Chaos
IP_PROTO_UDP = 17  # UDP
IP_PROTO_MUX = 18  # multiplexing
IP_PROTO_DCNMEAS = 19  # DCN measurement
IP_PROTO_HMP = 20  # Host Monitoring Protocol
IP_PROTO_PRM = 21  # Packet Radio Measurement
IP_PROTO_IDP = 22  # Xerox NS IDP
IP_PROTO_TRUNK1 = 23  # Trunk-1
IP_PROTO_TRUNK2 = 24  # Trunk-2
IP_PROTO_LEAF1 = 25  # Leaf-1
IP_PROTO_LEAF2 = 26  # Leaf-2
IP_PROTO_RDP = 27  # "Reliable Datagram" proto
IP_PROTO_IRTP = 28  # Inet Reliable Transaction
IP_PROTO_TP = 29  # ISO TP class 4
IP_PROTO_NETBLT = 30  # Bulk Data Transfer
IP_PROTO_MFPNSP = 31  # MFE Network Services
IP_PROTO_MERITINP = 32  # Merit Internodal Protocol
IP_PROTO_SEP = 33  # Sequential Exchange proto
IP_PROTO_3PC = 34  # Third Party Connect proto
IP_PROTO_IDPR = 35  # Interdomain Policy Route
IP_PROTO_XTP = 36  # Xpress Transfer Protocol
IP_PROTO_DDP = 37  # Datagram Delivery Proto
IP_PROTO_CMTP = 38  # IDPR Ctrl Message Trans
IP_PROTO_TPPP = 39  # TP++ Transport Protocol
IP_PROTO_IL = 40  # IL Transport Protocol
IP_PROTO_IP6 = 41  # IPv6
IP_PROTO_SDRP = 42  # Source Demand Routing
IP_PROTO_ROUTING = 43  # IPv6 routing header
IP_PROTO_FRAGMENT = 44  # IPv6 fragmentation header
IP_PROTO_RSVP = 46  # Reservation protocol
IP_PROTO_GRE = 47  # General Routing Encap
IP_PROTO_MHRP = 48  # Mobile Host Routing
IP_PROTO_ENA = 49  # ENA
IP_PROTO_ESP = 50  # Encap Security Payload
IP_PROTO_AH = 51  # Authentication Header
IP_PROTO_INLSP = 52  # Integated Net Layer Sec
IP_PROTO_SWIPE = 53  # SWIPE
IP_PROTO_NARP = 54  # NBMA Address Resolution
IP_PROTO_MOBILE = 55  # Mobile IP, RFC 2004
IP_PROTO_TLSP = 56  # Transport Layer Security
IP_PROTO_SKIP = 57  # SKIP
IP_PROTO_ICMP6 = 58  # ICMP for IPv6
IP_PROTO_NONE = 59  # IPv6 no next header
IP_PROTO_DSTOPTS = 60  # IPv6 destination options
IP_PROTO_ANYHOST = 61  # any host internal proto
IP_PROTO_CFTP = 62  # CFTP
IP_PROTO_ANYNET = 63  # any local network
IP_PROTO_EXPAK = 64  # SATNET and Backroom EXPAK
IP_PROTO_KRYPTOLAN = 65  # Kryptolan
IP_PROTO_RVD = 66  # MIT Remote Virtual Disk
IP_PROTO_IPPC = 67  # Inet Pluribus Packet Core
IP_PROTO_DISTFS = 68  # any distributed fs
IP_PROTO_SATMON = 69  # SATNET Monitoring
IP_PROTO_VISA = 70  # VISA Protocol
IP_PROTO_IPCV = 71  # Inet Packet Core Utility
IP_PROTO_CPNX = 72  # Comp Proto Net Executive
IP_PROTO_CPHB = 73  # Comp Protocol Heart Beat
IP_PROTO_WSN = 74  # Wang Span Network
IP_PROTO_PVP = 75  # Packet Video Protocol
IP_PROTO_BRSATMON = 76  # Backroom SATNET Monitor
IP_PROTO_SUNND = 77  # SUN ND Protocol
IP_PROTO_WBMON = 78  # WIDEBAND Monitoring
IP_PROTO_WBEXPAK = 79  # WIDEBAND EXPAK
IP_PROTO_EON = 80  # ISO CNLP
IP_PROTO_VMTP = 81  # Versatile Msg Transport
IP_PROTO_SVMTP = 82  # Secure VMTP
IP_PROTO_VINES = 83  # VINES
IP_PROTO_TTP = 84  # TTP
IP_PROTO_NSFIGP = 85  # NSFNET-IGP
IP_PROTO_DGP = 86  # Dissimilar Gateway Proto
IP_PROTO_TCF = 87  # TCF
IP_PROTO_EIGRP = 88  # EIGRP
IP_PROTO_OSPF = 89  # Open Shortest Path First
IP_PROTO_SPRITERPC = 90  # Sprite RPC Protocol
IP_PROTO_LARP = 91  # Locus Address Resolution
IP_PROTO_MTP = 92  # Multicast Transport Proto
IP_PROTO_AX25 = 93  # AX.25 Frames
IP_PROTO_IPIPENCAP = 94  # yet-another IP encap
IP_PROTO_MICP = 95  # Mobile Internet Ctrl
IP_PROTO_SCCSP = 96  # Semaphore Comm Sec Proto
IP_PROTO_ETHERIP = 97  # Ethernet in IPv4
IP_PROTO_ENCAP = 98  # encapsulation header
IP_PROTO_ANYENC = 99  # private encryption scheme
IP_PROTO_GMTP = 100  # GMTP
IP_PROTO_IFMP = 101  # Ipsilon Flow Mgmt Proto
IP_PROTO_PNNI = 102  # PNNI over IP
IP_PROTO_PIM = 103  # Protocol Indep Multicast
IP_PROTO_ARIS = 104  # ARIS
IP_PROTO_SCPS = 105  # SCPS
IP_PROTO_QNX = 106  # QNX
IP_PROTO_AN = 107  # Active Networks
IP_PROTO_IPCOMP = 108  # IP Payload Compression
IP_PROTO_SNP = 109  # Sitara Networks Protocol
IP_PROTO_COMPAQPEER = 110  # Compaq Peer Protocol
IP_PROTO_IPXIP = 111  # IPX in IP
IP_PROTO_VRRP = 112  # Virtual Router Redundancy
IP_PROTO_PGM = 113  # PGM Reliable Transport
IP_PROTO_ANY0HOP = 114  # 0-hop protocol
IP_PROTO_L2TP = 115  # Layer 2 Tunneling Proto
IP_PROTO_DDX = 116  # D-II Data Exchange (DDX)
IP_PROTO_IATP = 117  # Interactive Agent Xfer
IP_PROTO_STP = 118  # Schedule Transfer Proto
IP_PROTO_SRP = 119  # SpectraLink Radio Proto
IP_PROTO_UTI = 120  # UTI
IP_PROTO_SMP = 121  # Simple Message Protocol
IP_PROTO_SM = 122  # SM
IP_PROTO_PTP = 123  # Performance Transparency
IP_PROTO_ISIS = 124  # ISIS over IPv4
IP_PROTO_FIRE = 125  # FIRE
IP_PROTO_CRTP = 126  # Combat Radio Transport
IP_PROTO_CRUDP = 127  # Combat Radio UDP
IP_PROTO_SSCOPMCE = 128  # SSCOPMCE
IP_PROTO_IPLT = 129  # IPLT
IP_PROTO_SPS = 130  # Secure Packet Shield
IP_PROTO_PIPE = 131  # Private IP Encap in IP
IP_PROTO_SCTP = 132  # Stream Ctrl Transmission
IP_PROTO_FC = 133  # Fibre Channel
IP_PROTO_RSVPIGN = 134  # RSVP-E2E-IGNORE
IP_PROTO_RAW = 255  # Raw IP packets
IP_PROTO_RESERVED = IP_PROTO_RAW  # Reserved
IP_PROTO_MAX = 255

# XXX - auto-load IP dispatch table from IP_PROTO_* definitions


def __load_protos():
    g = globals()
    for k, v in iteritems(g):
        if k.startswith('IP_PROTO_'):
            name = k[9:].lower()
            try:
                mod = __import__(name, g, level=1)
                IP.set_proto(v, getattr(mod, name.upper()))
            except (ImportError, AttributeError):
                continue


def _mod_init():
    """Post-initialization called when all dpkt modules are fully loaded"""
    if not IP._protosw:
        __load_protos()


def test_ip():
    from . import udp

    s = b'E\x00\x00"\x00\x00\x00\x00@\x11r\xc0\x01\x02\x03\x04\x01\x02\x03\x04\x00o\x00\xde\x00\x0e\xbf5foobar'
    ip = IP(id=0, src=b'\x01\x02\x03\x04', dst=b'\x01\x02\x03\x04', p=17)
    u = udp.UDP(sport=111, dport=222)
    u.data = b'foobar'
    u.ulen += len(u.data)
    ip.data = u
    ip.len += len(u)
    assert (bytes(ip) == s)
    assert (ip.v == 4)
    assert (ip.hl == 5)

    ip = IP(s)
    assert (bytes(ip) == s)
    assert (ip.udp.sport == 111)
    assert (ip.udp.data == b'foobar')


def test_hl():  # Todo chack this test method
    s = b'BB\x03\x00\x00\x00\x00\x00\x00\x00\xd0\x00\xec\xbc\xa5\x00\x00\x00\x03\x80\x00\x00\xd0\x01\xf2\xac\xa5"0\x01\x00\x14\x00\x02\x00\x0f\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    try:
        IP(s)
    except dpkt.UnpackError:
        pass


def test_opt():
    s = b'\x4f\x00\x00\x3c\xae\x08\x00\x00\x40\x06\x18\x10\xc0\xa8\x0a\x26\xc0\xa8\x0a\x01\x07\x27\x08\x01\x02\x03\x04\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    ip = IP(s)
    ip.sum = 0
    assert (bytes(ip) == s)


def test_zerolen():
    from . import tcp
    d = b'X' * 2048
    s = b'E\x00\x00\x004\xce@\x00\x80\x06\x00\x00\x7f\x00\x00\x01\x7f\x00\x00\x01\xccN\x0c8`\xff\xc6N_\x8a\x12\x98P\x18@):\xa3\x00\x00' + d
    ip = IP(s)
    assert (isinstance(ip.data, tcp.TCP))
    assert (ip.tcp.data == d)


def test_constuctor():
    ip1 = IP(data = b"Hello world!")
    ip2 = IP(data = b"Hello world!", len = 0)
    ip3 = IP(bytes(ip1))
    ip4 = IP(bytes(ip2))
    assert (bytes(ip1) == bytes(ip3))
    assert (bytes(ip1) == b'E\x00\x00 \x00\x00\x00\x00@\x00z\xdf\x00\x00\x00\x00\x00\x00\x00\x00Hello world!')
    assert (bytes(ip2) == bytes(ip4))
    assert (bytes(ip2) == b'E\x00\x00 \x00\x00\x00\x00@\x00z\xdf\x00\x00\x00\x00\x00\x00\x00\x00Hello world!')

def test_frag():
    from . import ethernet
    s = b"\x00\x23\x20\xd4\x2a\x8c\x00\x23\x20\xd4\x2a\x8c\x08\x00\x45\x00\x00\x54\x00\x00\x40\x00\x40\x01\x25\x8d\x0a\x00\x00\x8f\x0a\x00\x00\x8e\x08\x00\x2e\xa0\x01\xff\x23\x73\x20\x48\x4a\x4d\x00\x00\x00\x00\x78\x85\x02\x00\x00\x00\x00\x00\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37"
    ip = ethernet.Ethernet(s).ip
    assert (ip.rf == 0)
    assert (ip.df == 1)
    assert (ip.mf == 0)
    assert (ip.offset == 0)

    # test setters of fragmentation related attributes.
    ip.rf = 1
    ip.df = 0
    ip.mf = 1
    ip.offset = 1480
    assert (ip.rf == 1)
    assert (ip.df == 0)
    assert (ip.mf == 1)
    assert (ip.offset == 1480)


if __name__ == '__main__':
    test_ip()
    test_hl()
    test_opt()
    test_zerolen()
    test_constuctor()
    test_frag()
    print('Tests Successful...')
