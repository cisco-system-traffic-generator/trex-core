# $Id: dns.py 27 2006-11-21 01:22:52Z dahelder $
# -*- coding: utf-8 -*-
"""Domain Name System."""
from __future__ import print_function
from __future__ import absolute_import

import struct
import codecs

from . import dpkt
from .decorators import deprecated
from .compat import compat_ord

DNS_Q = 0
DNS_R = 1

# Opcodes
DNS_QUERY = 0
DNS_IQUERY = 1
DNS_STATUS = 2
DNS_NOTIFY = 4
DNS_UPDATE = 5

# Flags
DNS_CD = 0x0010  # checking disabled
DNS_AD = 0x0020  # authenticated data
DNS_Z = 0x0040  # unused
DNS_RA = 0x0080  # recursion available
DNS_RD = 0x0100  # recursion desired
DNS_TC = 0x0200  # truncated
DNS_AA = 0x0400  # authoritative answer
DNS_QR = 0x8000  # response ( query / response )

# Response codes
DNS_RCODE_NOERR = 0
DNS_RCODE_FORMERR = 1
DNS_RCODE_SERVFAIL = 2
DNS_RCODE_NXDOMAIN = 3
DNS_RCODE_NOTIMP = 4
DNS_RCODE_REFUSED = 5
DNS_RCODE_YXDOMAIN = 6
DNS_RCODE_YXRRSET = 7
DNS_RCODE_NXRRSET = 8
DNS_RCODE_NOTAUTH = 9
DNS_RCODE_NOTZONE = 10

# RR types
DNS_A = 1
DNS_NS = 2
DNS_CNAME = 5
DNS_SOA = 6
DNS_NULL = 10
DNS_PTR = 12
DNS_HINFO = 13
DNS_MX = 15
DNS_TXT = 16
DNS_AAAA = 28
DNS_SRV = 33
DNS_OPT = 41

# RR classes
DNS_IN = 1
DNS_CHAOS = 3
DNS_HESIOD = 4
DNS_ANY = 255


def pack_name(name, off, label_ptrs):
    name = codecs.encode(name, 'utf-8')
    if name:
        labels = name.split(b'.')
    else:
        labels = []
    labels.append(b'')
    buf = b''
    for i, label in enumerate(labels):
        key = b'.'.join(labels[i:]).upper()
        ptr = label_ptrs.get(key)
        if not ptr:
            if len(key) > 1:
                ptr = off + len(buf)
                if ptr < 0xc000:
                    label_ptrs[key] = ptr
            i = len(label)
            buf += struct.pack("B", i) + label
        else:
            buf += struct.pack('>H', (0xc000 | ptr))
            break
    return buf


def unpack_name(buf, off):
    name = []
    saved_off = 0
    start_off = off
    name_length = 0
    while True:
        if off >= len(buf):
            raise dpkt.NeedData()
        n = compat_ord(buf[off])
        if n == 0:
            off += 1
            break
        elif (n & 0xc0) == 0xc0:
            ptr = struct.unpack('>H', buf[off:off + 2])[0] & 0x3fff
            if ptr >= start_off:
                raise dpkt.UnpackError('Invalid label compression pointer')
            off += 2
            if not saved_off:
                saved_off = off
            start_off = off = ptr
        elif (n & 0xc0) == 0x00:
            off += 1
            name.append(buf[off:off + n])
            name_length += n + 1
            if name_length > 255:
                raise dpkt.UnpackError('name longer than 255 bytes')
            off += n
        else:
            raise dpkt.UnpackError('Invalid label length %02x' % n)
    if not saved_off:
        saved_off = off
    return codecs.decode(b'.'.join(name), 'utf-8'), saved_off


class DNS(dpkt.Packet):
    """Domain Name System.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of DNS.
        TODO.
    """

    __hdr__ = (
        ('id', 'H', 0),
        ('op', 'H', DNS_RD),  # recursive query
        # XXX - lists of query, RR objects
        ('qd', 'H', []),
        ('an', 'H', []),
        ('ns', 'H', []),
        ('ar', 'H', [])
    )

    @property
    def qr(self):
        return int((self.op & DNS_QR) == DNS_QR)

    @qr.setter
    def qr(self, v):
        if v:
            self.op |= DNS_QR
        else:
            self.op &= ~DNS_QR

    @property
    def opcode(self):
        return (self.op >> 11) & 0xf

    @opcode.setter
    def opcode(self, v):
        self.op = (self.op & ~0x7800) | ((v & 0xf) << 11)

    @property
    def aa(self):
        return int((self.op & DNS_AA) == DNS_AA)

    @aa.setter
    def aa(self, v):
        if v:
            self.op |= DNS_AA
        else:
            self.op &= ~DNS_AA

    @property
    def tc(self):
        return int((self.op & DNS_TC) == DNS_TC)

    @tc.setter
    def tc(self, v):
        if v:
            self.op |= DNS_TC
        else:
            self.op &= ~DNS_TC

    @property
    def rd(self):
        return int((self.op & DNS_RD) == DNS_RD)

    @rd.setter
    def rd(self, v):
        if v:
            self.op |= DNS_RD
        else:
            self.op &= ~DNS_RD

    @property
    def ra(self):
        return int((self.op & DNS_RA) == DNS_RA)

    @ra.setter
    def ra(self, v):
        if v:
            self.op |= DNS_RA
        else:
            self.op &= ~DNS_RA

    @property
    def zero(self):
        return int((self.op & DNS_Z) == DNS_Z)

    @zero.setter
    def zero(self, v):
        if v:
            self.op |= DNS_Z
        else:
            self.op &= ~DNS_Z

    @property
    def rcode(self):
        return self.op & 0xf

    @rcode.setter
    def rcode(self, v):
        self.op = (self.op & ~0xf) | (v & 0xf)

    class Q(dpkt.Packet):
        """DNS question."""
        __hdr__ = (
            ('name', '1025s', b''),
            ('type', 'H', DNS_A),
            ('cls', 'H', DNS_IN)
        )

        # XXX - suk
        def __len__(self):
            raise NotImplementedError

        __str__ = __len__

        def unpack(self, buf):
            raise NotImplementedError

    class RR(Q):
        """DNS resource record."""
        __hdr__ = (
            ('name', '1025s', b''),
            ('type', 'H', DNS_A),
            ('cls', 'H', DNS_IN),
            ('ttl', 'I', 0),
            ('rlen', 'H', 4),
            ('rdata', 's', b'')
        )

        def pack_rdata(self, off, label_ptrs):
            # XXX - yeah, this sux
            if self.rdata:
                return self.rdata
            if self.type == DNS_A:
                return self.ip
            elif self.type == DNS_NS:
                return pack_name(self.nsname, off, label_ptrs)
            elif self.type == DNS_CNAME:
                return pack_name(self.cname, off, label_ptrs)
            elif self.type == DNS_PTR:
                return pack_name(self.ptrname, off, label_ptrs)
            elif self.type == DNS_SOA:
                l = []
                l.append(pack_name(self.mname, off, label_ptrs))
                l.append(pack_name(self.rname, off + len(l[0]), label_ptrs))
                l.append(struct.pack('>IIIII', self.serial, self.refresh,
                                     self.retry, self.expire, self.minimum))
                return b''.join(l)
            elif self.type == DNS_MX:
                return struct.pack('>H', self.preference) + \
                       pack_name(self.mxname, off + 2, label_ptrs)
            elif self.type == DNS_TXT or self.type == DNS_HINFO:
                return b''.join(struct.pack('B', len(x)) + x for x in self.text)
            elif self.type == DNS_AAAA:
                return self.ip6
            elif self.type == DNS_SRV:
                return struct.pack('>HHH', self.priority, self.weight, self.port) + \
                       pack_name(self.srvname, off + 6, label_ptrs)
            elif self.type == DNS_OPT:
                return b''  # self.rdata
            else:
                raise dpkt.PackError('RR type %s is not supported' % self.type)

        def unpack_rdata(self, buf, off):
            if self.type == DNS_A:
                self.ip = self.rdata
            elif self.type == DNS_NS:
                self.nsname, off = unpack_name(buf, off)
            elif self.type == DNS_CNAME:
                self.cname, off = unpack_name(buf, off)
            elif self.type == DNS_PTR:
                self.ptrname, off = unpack_name(buf, off)
            elif self.type == DNS_SOA:
                self.mname, off = unpack_name(buf, off)
                self.rname, off = unpack_name(buf, off)
                self.serial, self.refresh, self.retry, self.expire, \
                self.minimum = struct.unpack('>IIIII', buf[off:off + 20])
            elif self.type == DNS_MX:
                self.preference = struct.unpack('>H', self.rdata[:2])
                self.mxname, off = unpack_name(buf, off + 2)
            elif self.type == DNS_TXT or self.type == DNS_HINFO:
                self.text = []
                buf = self.rdata
                while buf:
                    n = compat_ord(buf[0])
                    self.text.append(codecs.decode(buf[1:1 + n], 'utf-8'))
                    buf = buf[1 + n:]
            elif self.type == DNS_AAAA:
                self.ip6 = self.rdata
            elif self.type == DNS_NULL:
                self.null = codecs.encode(self.rdata, 'hex')
            elif self.type == DNS_SRV:
                self.priority, self.weight, self.port = struct.unpack('>HHH', self.rdata[:6])
                self.srvname, off = unpack_name(buf, off + 6)
            elif self.type == DNS_OPT:
                pass  # RFC-6891: OPT is a pseudo-RR not carrying any DNS data
            else:
                raise dpkt.UnpackError('RR type %s is not supported' % self.type)

    def pack_q(self, buf, q):
        """Append packed DNS question and return buf."""
        return buf + pack_name(q.name, len(buf), self.label_ptrs) + struct.pack('>HH', q.type, q.cls)

    def unpack_q(self, buf, off):
        """Return DNS question and new offset."""
        q = self.Q()
        q.name, off = unpack_name(buf, off)
        q.type, q.cls = struct.unpack('>HH', buf[off:off + 4])
        off += 4
        return q, off

    def pack_rr(self, buf, rr):
        """Append packed DNS RR and return buf."""
        name = pack_name(rr.name, len(buf), self.label_ptrs)
        rdata = rr.pack_rdata(len(buf) + len(name) + 10, self.label_ptrs)
        return buf + name + struct.pack('>HHIH', rr.type, rr.cls, rr.ttl, len(rdata)) + rdata

    def unpack_rr(self, buf, off):
        """Return DNS RR and new offset."""
        rr = self.RR()
        rr.name, off = unpack_name(buf, off)
        rr.type, rr.cls, rr.ttl, rdlen = struct.unpack('>HHIH', buf[off:off + 10])
        off += 10
        rr.rdata = buf[off:off + rdlen]
        rr.rlen = rdlen
        rr.unpack_rdata(buf, off)
        off += rdlen
        return rr, off

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        off = self.__hdr_len__
        cnt = self.qd  # FIXME: This relies on this being properly set somewhere else
        self.qd = []
        for _ in range(cnt):
            q, off = self.unpack_q(buf, off)
            self.qd.append(q)
        for x in ('an', 'ns', 'ar'):
            cnt = getattr(self, x, 0)
            setattr(self, x, [])
            for _ in range(cnt):
                rr, off = self.unpack_rr(buf, off)
                getattr(self, x).append(rr)
        self.data = b''

    def __len__(self):
        # XXX - cop out
        return len(bytes(self))

    def __bytes__(self):
        # XXX - compress names on the fly
        self.label_ptrs = {}
        buf = struct.pack(self.__hdr_fmt__, self.id, self.op, len(self.qd),
                          len(self.an), len(self.ns), len(self.ar))
        for q in self.qd:
            buf = self.pack_q(buf, q)
        for x in ('an', 'ns', 'ar'):
            for rr in getattr(self, x):
                buf = self.pack_rr(buf, rr)
        del self.label_ptrs
        return buf


def test_basic():
    from . import ip

    s = b'E\x00\x02\x08\xc15\x00\x00\x80\x11\x92aBk0\x01Bk0w\x005\xc07\x01\xf4\xda\xc2d\xd2\x81\x80\x00\x01\x00\x03\x00\x0b\x00\x0b\x03www\x06google\x03com\x00\x00\x01\x00\x01\xc0\x0c\x00\x05\x00\x01\x00\x00\x03V\x00\x17\x03www\x06google\x06akadns\x03net\x00\xc0,\x00\x01\x00\x01\x00\x00\x01\xa3\x00\x04@\xe9\xabh\xc0,\x00\x01\x00\x01\x00\x00\x01\xa3\x00\x04@\xe9\xabc\xc07\x00\x02\x00\x01\x00\x00KG\x00\x0c\x04usw5\x04akam\xc0>\xc07\x00\x02\x00\x01\x00\x00KG\x00\x07\x04usw6\xc0t\xc07\x00\x02\x00\x01\x00\x00KG\x00\x07\x04usw7\xc0t\xc07\x00\x02\x00\x01\x00\x00KG\x00\x08\x05asia3\xc0t\xc07\x00\x02\x00\x01\x00\x00KG\x00\x05\x02za\xc07\xc07\x00\x02\x00\x01\x00\x00KG\x00\x0f\x02zc\x06akadns\x03org\x00\xc07\x00\x02\x00\x01\x00\x00KG\x00\x05\x02zf\xc07\xc07\x00\x02\x00\x01\x00\x00KG\x00\x05\x02zh\xc0\xd5\xc07\x00\x02\x00\x01\x00\x00KG\x00\x07\x04eur3\xc0t\xc07\x00\x02\x00\x01\x00\x00KG\x00\x07\x04use2\xc0t\xc07\x00\x02\x00\x01\x00\x00KG\x00\x07\x04use4\xc0t\xc0\xc1\x00\x01\x00\x01\x00\x00\xfb4\x00\x04\xd0\xb9\x84\xb0\xc0\xd2\x00\x01\x00\x01\x00\x001\x0c\x00\x04?\xf1\xc76\xc0\xed\x00\x01\x00\x01\x00\x00\xfb4\x00\x04?\xd7\xc6S\xc0\xfe\x00\x01\x00\x01\x00\x001\x0c\x00\x04?\xd00.\xc1\x0f\x00\x01\x00\x01\x00\x00\n\xdf\x00\x04\xc1-\x01g\xc1"\x00\x01\x00\x01\x00\x00\x101\x00\x04?\xd1\xaa\x88\xc15\x00\x01\x00\x01\x00\x00\r\x1a\x00\x04PCC\xb6\xc0o\x00\x01\x00\x01\x00\x00\x10\x7f\x00\x04?\xf1I\xd6\xc0\x87\x00\x01\x00\x01\x00\x00\n\xdf\x00\x04\xce\x84dl\xc0\x9a\x00\x01\x00\x01\x00\x00\n\xdf\x00\x04A\xcb\xea\x1b\xc0\xad\x00\x01\x00\x01\x00\x00\x0b)\x00\x04\xc1l\x9a\t'
    ip = ip.IP(s)
    my_dns = DNS(ip.udp.data)
    assert my_dns.qd[0].name == 'www.google.com' and my_dns.an[1].name == 'www.google.akadns.net'
    s = b'\x05\xf5\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03www\x03cnn\x03com\x00\x00\x01\x00\x01'
    my_dns = DNS(s)
    assert s == bytes(my_dns)


def test_PTR():
    s = b'g\x02\x81\x80\x00\x01\x00\x01\x00\x03\x00\x00\x011\x011\x03211\x03141\x07in-addr\x04arpa\x00\x00\x0c\x00\x01\xc0\x0c\x00\x0c\x00\x01\x00\x00\r6\x00$\x07default\nv-umce-ifs\x05umnet\x05umich\x03edu\x00\xc0\x0e\x00\x02\x00\x01\x00\x00\r6\x00\r\x06shabby\x03ifs\xc0O\xc0\x0e\x00\x02\x00\x01\x00\x00\r6\x00\x0f\x0cfish-license\xc0m\xc0\x0e\x00\x02\x00\x01\x00\x00\r6\x00\x0b\x04dns2\x03itd\xc0O'
    my_dns = DNS(s)
    assert my_dns.qd[0].name == '1.1.211.141.in-addr.arpa' and \
           my_dns.an[0].ptrname == 'default.v-umce-ifs.umnet.umich.edu' and \
           my_dns.ns[0].nsname == 'shabby.ifs.umich.edu' and \
           my_dns.ns[1].ttl == 3382 and \
           my_dns.ns[2].nsname == 'dns2.itd.umich.edu'
    assert s == bytes(my_dns)


def test_OPT():
    s = b'\x8dn\x01\x10\x00\x01\x00\x00\x00\x00\x00\x01\x04x111\x06xxxx11\x06akamai\x03net\x00\x00\x01\x00\x01\x00\x00)\x0f\xa0\x00\x00\x80\x00\x00\x00'
    my_dns = DNS(s)
    my_rr = my_dns.ar[0]
    assert my_rr.type == DNS_OPT
    assert my_rr.rlen == 0 and my_rr.rdata == b''
    assert bytes(my_dns) == s

    my_rr.rdata = b'\x00\x00\x00\x02\x00\x00'  # add 1 attribute tlv
    my_dns2 = DNS(bytes(my_dns))
    my_rr2 = my_dns2.ar[0]
    assert my_rr2.rlen == 6 and my_rr2.rdata == b'\x00\x00\x00\x02\x00\x00'


def test_pack_name():
    # Empty name is \0
    x = pack_name('', 0, {})
    assert x == b'\0'


def test_random_data():
    try:
        DNS(b'\x83z0\xd2\x9a\xec\x94_7\xf3\xb7+\x85"?\xf0\xfb')
    except dpkt.UnpackError:
        pass
    except:
        assert False
    else:
        assert False


def test_circular_pointers():
    try:
        DNS(b'\xc0\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x07example\x03com\xc0\x00')
    except dpkt.UnpackError:
        pass
    except:
        assert False
    else:
        assert False


def test_very_long_name():
    try:
        DNS(b'\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00' + (b'\x10abcdef0123456789' * 16) + b'\x00')
    except dpkt.UnpackError:
        pass
    except:
        assert False
    else:
        assert False


def test_null_response():
    s = b'\x12\xb0\x84\x00\x00\x01\x00\x01\x00\x00\x00\x00\x0bblahblah666\x06pirate\x03sea\x00\x00\n\x00\x01\xc0\x0c\x00\n\x00\x01\x00\x00\x00\x00\x00\tVACKD\x03\xc5\xe9\x01'
    my_dns = DNS(s)
    assert my_dns.qd[0].name == 'blahblah666.pirate.sea' and \
           my_dns.an[0].null == b'5641434b4403c5e901'
    assert str(s) == str(my_dns)


def test_txt_response():
    buf = (
        b'\x10\x32\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\x06\x67\x6f\x6f\x67\x6c\x65\x03\x63\x6f'
        b'\x6d\x00\x00\x10\x00\x01\xc0\x0c\x00\x10\x00\x01\x00\x00\x01\x0e\x00\x10\x0f\x76\x3d\x73'
        b'\x70\x66\x31\x20\x70\x74\x72\x20\x3f\x61\x6c\x6c')
    my_dns = DNS(buf)
    my_rr = my_dns.an[0]
    assert my_rr.type == DNS_TXT
    assert my_rr.name == 'google.com'
    assert my_rr.text == ['v=spf1 ptr ?all']
    assert str(my_dns) == str(buf)
    assert bytes(my_dns) == buf


def test_rdata_TXT():
    rr = DNS.RR(
        type=DNS_TXT,
        text=[b'v=spf1 ptr ?all', b'a=something']
    )
    packdata = rr.pack_rdata(0, {})
    correct = b'\x0fv=spf1 ptr ?all\x0ba=something'
    assert packdata == correct


def test_rdata_HINFO():
    rr = DNS.RR(
        type=DNS_HINFO,
        text=[b'v=spf1 ptr ?all', b'a=something']
    )
    packdata = rr.pack_rdata(0, {})
    correct = b'\x0fv=spf1 ptr ?all\x0ba=something'
    assert packdata == correct


def test_dns_len():
    my_dns = DNS()
    assert len(my_dns) == 12


if __name__ == '__main__':
    # Runs all the test associated with this class/file
    test_basic()
    test_PTR()
    test_OPT()
    test_pack_name()
    test_random_data()
    test_circular_pointers()
    test_very_long_name()
    test_null_response()
    test_txt_response()
    test_deprecated_methods()
    test_deprecated_method_performance()
    print('Tests Successful...')
