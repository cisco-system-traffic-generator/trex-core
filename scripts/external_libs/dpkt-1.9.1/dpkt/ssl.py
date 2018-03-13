# $Id: ssl.py 90 2014-04-02 22:06:23Z andrewflnr@gmail.com $
# Portion Copyright 2012 Google Inc. All rights reserved.
# -*- coding: utf-8 -*-
"""Secure Sockets Layer / Transport Layer Security."""
from __future__ import absolute_import

import struct
import binascii

from . import dpkt
from . import ssl_ciphersuites

#
# Note from April 2011: cde...@gmail.com added code that parses SSL3/TLS messages more in depth.
#
# Jul 2012: afleenor@google.com modified and extended SSL support further.
#

# SSL 2.0 is deprecated in RFC 6176
class SSL2(dpkt.Packet):
    __hdr__ = (
        ('len', 'H', 0),
        ('msg', 's', ''),
        ('pad', 's', ''),
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        if self.len & 0x8000:
            n = self.len = self.len & 0x7FFF
            self.msg, self.data = self.data[:n], self.data[n:]
        else:
            n = self.len = self.len & 0x3FFF
            padlen = ord(self.data[0])
            self.msg = self.data[1:1 + n]
            self.pad = self.data[1 + n:1 + n + padlen]
            self.data = self.data[1 + n + padlen:]


# SSL 3.0 is deprecated in RFC 7568
# Use class TLS for >= SSL 3.0
class TLS(dpkt.Packet):
    __hdr__ = (
        ('type', 'B', ''),
        ('version', 'H', ''),
        ('len', 'H', ''),
    )

    def __init__(self, *args, **kwargs):
        self.records = []
        dpkt.Packet.__init__(self, *args, **kwargs)


    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        pointer = 0
        while len(self.data[pointer:]) > 0:
            end = pointer + 5 + struct.unpack("!H", buf[pointer+3:pointer+5])[0]
            self.records.append(TLSRecord(buf[pointer:end]))
            pointer = end
            self.data = self.data[pointer:]


# SSLv3/TLS versions
SSL3_V = 0x0300
TLS1_V = 0x0301
TLS11_V = 0x0302
TLS12_V = 0x0303

ssl3_versions_str = {
    SSL3_V: 'SSL3',
    TLS1_V: 'TLS 1.0',
    TLS11_V: 'TLS 1.1',
    TLS12_V: 'TLS 1.2'
}

SSL3_VERSION_BYTES = set((b'\x03\x00', b'\x03\x01', b'\x03\x02', b'\x03\x03'))


# Alert levels
SSL3_AD_WARNING = 1
SSL3_AD_FATAL = 2
alert_level_str = {
    SSL3_AD_WARNING: 'SSL3_AD_WARNING',
    SSL3_AD_FATAL: 'SSL3_AD_FATAL'
}

# SSL3 alert descriptions
SSL3_AD_CLOSE_NOTIFY = 0
SSL3_AD_UNEXPECTED_MESSAGE = 10  # fatal
SSL3_AD_BAD_RECORD_MAC = 20  # fatal
SSL3_AD_DECOMPRESSION_FAILURE = 30  # fatal
SSL3_AD_HANDSHAKE_FAILURE = 40  # fatal
SSL3_AD_NO_CERTIFICATE = 41
SSL3_AD_BAD_CERTIFICATE = 42
SSL3_AD_UNSUPPORTED_CERTIFICATE = 43
SSL3_AD_CERTIFICATE_REVOKED = 44
SSL3_AD_CERTIFICATE_EXPIRED = 45
SSL3_AD_CERTIFICATE_UNKNOWN = 46
SSL3_AD_ILLEGAL_PARAMETER = 47  # fatal

# TLS1 alert descriptions
TLS1_AD_DECRYPTION_FAILED = 21
TLS1_AD_RECORD_OVERFLOW = 22
TLS1_AD_UNKNOWN_CA = 48  # fatal
TLS1_AD_ACCESS_DENIED = 49  # fatal
TLS1_AD_DECODE_ERROR = 50  # fatal
TLS1_AD_DECRYPT_ERROR = 51
TLS1_AD_EXPORT_RESTRICTION = 60  # fatal
TLS1_AD_PROTOCOL_VERSION = 70  # fatal
TLS1_AD_INSUFFICIENT_SECURITY = 71  # fatal
TLS1_AD_INTERNAL_ERROR = 80  # fatal
TLS1_AD_USER_CANCELLED = 90
TLS1_AD_NO_RENEGOTIATION = 100
# /* codes 110-114 are from RFC3546 */
TLS1_AD_UNSUPPORTED_EXTENSION = 110
TLS1_AD_CERTIFICATE_UNOBTAINABLE = 111
TLS1_AD_UNRECOGNIZED_NAME = 112
TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE = 113
TLS1_AD_BAD_CERTIFICATE_HASH_VALUE = 114
TLS1_AD_UNKNOWN_PSK_IDENTITY = 115  # fatal


# Mapping alert types to strings
alert_description_str = {
    SSL3_AD_CLOSE_NOTIFY: 'SSL3_AD_CLOSE_NOTIFY',
    SSL3_AD_UNEXPECTED_MESSAGE: 'SSL3_AD_UNEXPECTED_MESSAGE',
    SSL3_AD_BAD_RECORD_MAC: 'SSL3_AD_BAD_RECORD_MAC',
    SSL3_AD_DECOMPRESSION_FAILURE: 'SSL3_AD_DECOMPRESSION_FAILURE',
    SSL3_AD_HANDSHAKE_FAILURE: 'SSL3_AD_HANDSHAKE_FAILURE',
    SSL3_AD_NO_CERTIFICATE: 'SSL3_AD_NO_CERTIFICATE',
    SSL3_AD_BAD_CERTIFICATE: 'SSL3_AD_BAD_CERTIFICATE',
    SSL3_AD_UNSUPPORTED_CERTIFICATE: 'SSL3_AD_UNSUPPORTED_CERTIFICATE',
    SSL3_AD_CERTIFICATE_REVOKED: 'SSL3_AD_CERTIFICATE_REVOKED',
    SSL3_AD_CERTIFICATE_EXPIRED: 'SSL3_AD_CERTIFICATE_EXPIRED',
    SSL3_AD_CERTIFICATE_UNKNOWN: 'SSL3_AD_CERTIFICATE_UNKNOWN',
    SSL3_AD_ILLEGAL_PARAMETER: 'SSL3_AD_ILLEGAL_PARAMETER',
    TLS1_AD_DECRYPTION_FAILED: 'TLS1_AD_DECRYPTION_FAILED',
    TLS1_AD_RECORD_OVERFLOW: 'TLS1_AD_RECORD_OVERFLOW',
    TLS1_AD_UNKNOWN_CA: 'TLS1_AD_UNKNOWN_CA',
    TLS1_AD_ACCESS_DENIED: 'TLS1_AD_ACCESS_DENIED',
    TLS1_AD_DECODE_ERROR: 'TLS1_AD_DECODE_ERROR',
    TLS1_AD_DECRYPT_ERROR: 'TLS1_AD_DECRYPT_ERROR',
    TLS1_AD_EXPORT_RESTRICTION: 'TLS1_AD_EXPORT_RESTRICTION',
    TLS1_AD_PROTOCOL_VERSION: 'TLS1_AD_PROTOCOL_VERSION',
    TLS1_AD_INSUFFICIENT_SECURITY: 'TLS1_AD_INSUFFICIENT_SECURITY',
    TLS1_AD_INTERNAL_ERROR: 'TLS1_AD_INTERNAL_ERROR',
    TLS1_AD_USER_CANCELLED: 'TLS1_AD_USER_CANCELLED',
    TLS1_AD_NO_RENEGOTIATION: 'TLS1_AD_NO_RENEGOTIATION',
    TLS1_AD_UNSUPPORTED_EXTENSION: 'TLS1_AD_UNSUPPORTED_EXTENSION',
    TLS1_AD_CERTIFICATE_UNOBTAINABLE: 'TLS1_AD_CERTIFICATE_UNOBTAINABLE',
    TLS1_AD_UNRECOGNIZED_NAME: 'TLS1_AD_UNRECOGNIZED_NAME',
    TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE: 'TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE',
    TLS1_AD_BAD_CERTIFICATE_HASH_VALUE: 'TLS1_AD_BAD_CERTIFICATE_HASH_VALUE',
    TLS1_AD_UNKNOWN_PSK_IDENTITY: 'TLS1_AD_UNKNOWN_PSK_IDENTITY'
}


# struct format strings for parsing buffer lengths
# don't forget, you have to pad a 3-byte value with \x00
_SIZE_FORMATS = ['!B', '!H', '!I', '!I']


def parse_variable_array(buf, lenbytes):
    """
    Parse an array described using the 'Type name<x..y>' syntax from the spec
    Read a length at the start of buf, and returns that many bytes
    after, in a tuple with the TOTAL bytes consumed (including the size). This
    does not check that the array is the right length for any given datatype.
    """
    # first have to figure out how to parse length
    assert lenbytes <= 4  # pretty sure 4 is impossible, too
    size_format = _SIZE_FORMATS[lenbytes - 1]
    padding = b'\x00' if lenbytes == 3 else b''
    # read off the length
    size = struct.unpack(size_format, padding + buf[:lenbytes])[0]
    # read the actual data
    data = buf[lenbytes:lenbytes + size]
    # if len(data) != size: insufficient data
    return data, size + lenbytes


def parse_extensions(buf):
    """
    Parse TLS extensions in passed buf. Returns an ordered list of extension tuples with
    ordinal extension type as first value and extension data as second value.
    Passed buf must start with the 2-byte extensions length TLV.
    http://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml
    """
    extensions_length = struct.unpack('!H', buf[:2])[0]
    extensions = []

    pointer = 2
    while pointer < extensions_length:
        ext_type = struct.unpack('!H', buf[pointer:pointer+2])[0]
        pointer += 2
        ext_data, parsed = parse_variable_array(buf[pointer:], 2)
        extensions.append((ext_type, ext_data))
        pointer += parsed
    return extensions


class SSL3Exception(Exception):
    pass


class TLSRecord(dpkt.Packet):

    """
    SSLv3 or TLSv1+ packet.

    In addition to the fields specified in the header, there are
    compressed and decrypted fields, indicating whether, in the language
    of the spec, this is a TLSPlaintext, TLSCompressed, or
    TLSCiphertext. The application will have to figure out when it's
    appropriate to change these values.
    """

    __hdr__ = (
        ('type', 'B', 0),
        ('version', 'H', 0),
        ('length', 'H', 0),
    )

    def __init__(self, *args, **kwargs):
        # assume plaintext unless specified otherwise in arguments
        self.compressed = kwargs.pop('compressed', False)
        self.encrypted = kwargs.pop('encrypted', False)
        # parent constructor
        dpkt.Packet.__init__(self, *args, **kwargs)
        # make sure length and data are consistent
        self.length = len(self.data)

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        header_length = self.__hdr_len__
        self.data = buf[header_length:header_length + self.length]
        # make sure buffer was long enough
        if len(self.data) != self.length:
            raise dpkt.NeedData('TLSRecord data was too short.')
        # assume compressed and encrypted when it's been parsed from
        # raw data
        self.compressed = True
        self.encrypted = True


class TLSChangeCipherSpec(dpkt.Packet):

    """
    ChangeCipherSpec message is just a single byte with value 1
    """

    __hdr__ = (('type', 'B', 1),)


class TLSAppData(str):

    """
    As far as TLSRecord is concerned, AppData is just an opaque blob.
    """

    pass


class TLSAlert(dpkt.Packet):
    __hdr__ = (
        ('level', 'B', 1),
        ('description', 'B', 0),
    )


class TLSHelloRequest(dpkt.Packet):
    __hdr__ = tuple()


class TLSClientHello(dpkt.Packet):
    __hdr__ = (
        ('version', 'H', 0x0301),
        ('random', '32s', '\x00' * 32),
    )  # the rest is variable-length and has to be done manually

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        # now session, cipher suites, extensions are in self.data
        self.session_id, pointer = parse_variable_array(self.data, 1)
        # print 'pointer',pointer
        # handle ciphersuites
        ciphersuites, parsed = parse_variable_array(self.data[pointer:], 2)
        pointer += parsed
        self.num_ciphersuites = len(ciphersuites) / 2
        # check len(ciphersuites) % 2 == 0 ?
        # compression methods
        compression_methods, parsed = parse_variable_array(
            self.data[pointer:], 1)
        pointer += parsed
        self.num_compression_methods = parsed - 1
        self.compression_methods = map(ord, compression_methods)
        # Parse extensions if present
        if len(self.data[pointer:]) >= 6:
            self.extensions = parse_extensions(self.data[pointer:])


class TLSServerHello(dpkt.Packet):
    __hdr__ = (
        ('version', 'H', '0x0301'),
        ('random', '32s', '\x00' * 32),
    )  # session is variable, forcing rest to be manual

    def unpack(self, buf):
        try:
            dpkt.Packet.unpack(self, buf)
            self.session_id, pointer = parse_variable_array(self.data, 1)
            # single cipher suite
            self.cipher_suite = struct.unpack('!H', self.data[pointer:pointer + 2])[0]
            pointer += 2
            # single compression method
            self.compression = struct.unpack('!B', self.data[pointer:pointer + 1])[0]
            pointer += 1
            # Parse extensions if present
            if len(self.data[pointer:]) >= 6:
                self.extensions = parse_extensions(self.data[pointer:])
        except struct.error:
            # probably data too short
            raise dpkt.NeedData


class TLSCertificate(dpkt.Packet):
    __hdr__ = tuple()

    def unpack(self, buf):
       try:
           dpkt.Packet.unpack(self, buf)
           all_certs, all_certs_len = parse_variable_array(self.data, 3)
           self.certificates = []
           pointer = 3
           while pointer < all_certs_len:
               cert, parsed = parse_variable_array(self.data[pointer:], 3)
               self.certificates.append((cert))
               pointer += parsed
       except struct.error:
            raise dpkt.NeedData


class TLSUnknownHandshake(dpkt.Packet):
    __hdr__ = tuple()


TLSServerKeyExchange = TLSUnknownHandshake
TLSCertificateRequest = TLSUnknownHandshake
TLSServerHelloDone = TLSUnknownHandshake
TLSCertificateVerify = TLSUnknownHandshake
TLSClientKeyExchange = TLSUnknownHandshake
TLSFinished = TLSUnknownHandshake


# mapping of handshake type ids to their names
# and the classes that implement them
HANDSHAKE_TYPES = {
    0: ('HelloRequest', TLSHelloRequest),
    1: ('ClientHello', TLSClientHello),
    2: ('ServerHello', TLSServerHello),
    11: ('Certificate', TLSCertificate),
    12: ('ServerKeyExchange', TLSServerKeyExchange),
    13: ('CertificateRequest', TLSCertificateRequest),
    14: ('ServerHelloDone', TLSServerHelloDone),
    15: ('CertificateVerify', TLSCertificateVerify),
    16: ('ClientKeyExchange', TLSClientKeyExchange),
    20: ('Finished', TLSFinished),
}


class TLSHandshake(dpkt.Packet):

    """
    A TLS Handshake message

    This goes for all messages encapsulated in the Record layer, but especially
    important for handshakes and app data: A message may be spread across a
    number of TLSRecords, in addition to the possibility of there being more
    than one in a given Record. You have to put together the contents of
    TLSRecord's yourself.
    """

    # struct.unpack can't handle the 3-byte int, so we parse it as bytes
    # (and store it as bytes so dpkt doesn't get confused), and turn it into
    # an int in a user-facing property
    __hdr__ = (
        ('type', 'B', 0),
        ('length_bytes', '3s', 0),
    )

    def unpack(self, buf):
        dpkt.Packet.unpack(self, buf)
        # Wait, might there be more than one message of self.type?
        embedded_type = HANDSHAKE_TYPES.get(self.type, None)
        if embedded_type is None:
            raise SSL3Exception('Unknown or invalid handshake type %d' %
                                self.type)
        # only take the right number of bytes
        self.data = self.data[:self.length]
        if len(self.data) != self.length:
            raise dpkt.NeedData
        # get class out of embedded_type tuple
        self.data = embedded_type[1](self.data)

    @property
    def length(self):
        return struct.unpack('!I', b'\x00' + self.length_bytes)[0]


RECORD_TYPES = {
    20: TLSChangeCipherSpec,
    21: TLSAlert,
    22: TLSHandshake,
    23: TLSAppData,
}


class SSLFactory(object):
    def __new__(cls, buf):
        v = buf[1:3]
        if v in SSL3_VERSION_BYTES:
            return TLSRecord(buf)
        # SSL2 has no characteristic header or magic bytes, so we just assume
        # that the msg is an SSL2 msg if it is not detected as SSL3+
        return SSL2(buf)


def tls_multi_factory(buf):
    """
    Attempt to parse one or more TLSRecord's out of buf

    Args:
      buf: string containing SSL/TLS messages. May have an incomplete record
        on the end

    Returns:
      [TLSRecord]
      int, total bytes consumed, != len(buf) if an incomplete record was left at
        the end.

    Raises SSL3Exception.
    """
    i, n = 0, len(buf)
    msgs = []
    while i + 5 <= n:
        v = buf[i + 1:i + 3]
        if v in SSL3_VERSION_BYTES:
            try:
                msg = TLSRecord(buf[i:])
                msgs.append(msg)
            except dpkt.NeedData:
                break
        else:
            raise SSL3Exception('Bad TLS version in buf: %r' % buf[i:i + 5])
        i += len(msg)
    return msgs, i

_hexdecode = binascii.a2b_hex


class TestTLS(object):

    """
    Test basic TLS functionality.
    Test that each TLSRecord is correctly discovered and added to TLS.records
    """

    @classmethod
    def setup_class(cls):
        cls.p = TLS(_hexdecode('1603000206010002020303585c2ff72a6599498771f59514f10af68c68f9ef30d0dadc9e1af64d1091476a000084c02bc02cc086c087c009c023c00ac024c072c073c008c007c02fc030c08ac08bc013c027c014c028c076c077c012c011009c009dc07ac07b002f003c0035003d004100ba008400c0000a00050004009e009fc07cc07d003300670039006b004500be008800c4001600a200a3c080c081003200400038006a004400bd008700c3001300660100015500050005010000000000000011000f00000c7777772e69616e612e6f7267ff0100010000230000000a000c000a00130015001700180019000b00020100000d001c001a0401040204030501050306010603030103020303020102020203001500f400f20000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'))

    def test_records_length(self):
        assert (len(self.p.records) == 1)

    def test_record_type(self):
        assert (self.p.records[0].type == 22)

    def test_record_version(self):
        assert (self.p.records[0].version == 768)


class TestTLSRecord(object):

    """
    Test basic TLSRecord functionality
    For this test, the contents of the record doesn't matter, since we're not parsing the next layer.
    """

    @classmethod
    def setup_class(cls):
        # add some extra data, to make sure length is parsed correctly
        cls.p = TLSRecord(b'\x17\x03\x01\x00\x08abcdefghzzzzzzzzzzz')

    def test_content_type(self):
        assert (self.p.type == 23)

    def test_version(self):
        assert (self.p.version == 0x0301)

    def test_length(self):
        assert (self.p.length == 8)

    def test_data(self):
        assert (self.p.data == b'abcdefgh')

    def test_initial_flags(self):
        assert (self.p.compressed == True)
        assert (self.p.encrypted == True)

    def test_repack(self):
        p2 = TLSRecord(type=23, version=0x0301, data=b'abcdefgh')
        assert (p2.type == 23)
        assert (p2.version == 0x0301)
        assert (p2.length == 8)
        assert (p2.data == b'abcdefgh')
        assert (p2.pack() == self.p.pack())

    def test_total_length(self):
        # that len(p) includes header
        assert (len(self.p) == 13)

    def test_raises_need_data_when_buf_is_short(self):
        import pytest
        pytest.raises(dpkt.NeedData, TLSRecord, b'\x16\x03\x01\x00\x10abc')


class TestTLSChangeCipherSpec(object):

    """It's just a byte. This will be quick, I promise"""

    @classmethod
    def setup_class(cls):
        cls.p = TLSChangeCipherSpec(b'\x01')

    def test_parses(self):
        assert (self.p.type == 1)

    def test_total_length(self):
        assert (len(self.p) == 1)


class TestTLSAppData(object):

    """AppData is basically just a string"""

    def test_value(self):
        d = TLSAppData('abcdefgh')
        assert (d == 'abcdefgh')


class TestTLSHandshake(object):
    @classmethod
    def setup_class(cls):
        cls.h = TLSHandshake(b'\x00\x00\x00\x01\xff')

    def test_created_inside_message(self):
        assert (isinstance(self.h.data, TLSHelloRequest) == True)

    def test_length(self):
        assert (self.h.length == 0x01)

    def test_raises_need_data(self):
        import pytest
        pytest.raises(dpkt.NeedData, TLSHandshake, b'\x00\x00\x01\x01')


class TestClientHello(object):

    """This data is extracted from and verified by Wireshark"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode(
            b"01000199"  # handshake header
            b"0301"  # version
            b"5008220ce5e0e78b6891afe204498c9363feffbe03235a2d9e05b7d990eb708d"  # rand
            b"2009bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed1"  # session id
            # cipher suites
            b"005400ffc00ac0140088008700390038c00fc00500840035c007c009c011c0130045004400330032c00cc00ec002c0040096004100050004002fc008c01200160013c00dc003feff000ac006c010c00bc00100020001"
            b"0100"  # compresssion methods
            # extensions
            b"00fc0000000e000c0000096c6f63616c686f7374000a00080006001700180019000b00020100002300d0a50b2e9f618a9ea9bf493ef49b421835cd2f6b05bbe1179d8edf70d58c33d656e8696d36d7e7e0b9d3ecc0e4de339552fa06c64c0fcb550a334bc43944e2739ca342d15a9ebbe981ac87a0d38160507d47af09bdc16c5f0ee4cdceea551539382333226048a026d3a90a0535f4a64236467db8fee22b041af986ad0f253bc369137cd8d8cd061925461d7f4d7895ca9a4181ab554dad50360ac31860e971483877c9335ac1300c5e78f3e56f3b8e0fc16358fcaceefd5c8d8aaae7b35be116f8832856ca61144fcdd95e071b94d0cf7233740000"
            b"FFFFFFFFFFFFFFFF")  # random garbage
        cls.p = TLSHandshake(cls.data)

    def test_client_hello_constructed(self):
        """Make sure the correct class was constructed"""
        # print self.p
        assert (isinstance(self.p.data, TLSClientHello) == True)

    #   def testClientDateCorrect(self):
    #       self.assertEqual(self.p.random_unixtime, 1342710284)

    def test_client_random_correct(self):
        assert (self.p.data.random == _hexdecode(b'5008220ce5e0e78b6891afe204498c9363feffbe03235a2d9e05b7d990eb708d'))

    def test_cipher_suite_length(self):
        # we won't bother testing the identity of each cipher suite in the list.
        assert (self.p.data.num_ciphersuites == 42)
        # self.assertEqual(len(self.p.ciphersuites), 42)

    def test_session_id(self):
        assert (self.p.data.session_id == _hexdecode(b'09bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed1'))

    def test_compression_methods(self):
        assert (self.p.data.num_compression_methods == 1)

    def test_total_length(self):
        assert (len(self.p) == 413)


class TestServerHello(object):

    """Again, from Wireshark"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode(
            b'0200004d03015008220c8ec43c5462315a7c99f5d5b6bff009ad285b51dc18485f352e9fdecd2009bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed10002000005ff01000100')
        cls.p = TLSHandshake(cls.data)

    def test_constructed(self):
        assert (isinstance(self.p.data, TLSServerHello) == True)

    #    def testDateCorrect(self):
    #        self.assertEqual(self.p.random_unixtime, 1342710284)

    def test_random_correct(self):
        assert (self.p.data.random == _hexdecode(b'5008220c8ec43c5462315a7c99f5d5b6bff009ad285b51dc18485f352e9fdecd'))

    def test_cipher_suite(self):
        assert (ssl_ciphersuites.BY_CODE[self.p.data.cipher_suite].name == 'TLS_RSA_WITH_NULL_SHA')

    def test_total_length(self):
        assert (len(self.p) == 81)


class TestTLSCertificate(object):

    """We use a 2016 certificate record from iana.org as test data."""

    @classmethod
    def setup_class(cls):
        cls.p = TLSHandshake(_hexdecode('0b000b45000b42000687308206833082056ba003020102021009cabbe2191c8f569dd4b6dd250f21d8300d06092a864886f70d01010b05003070310b300906035504061302555331153013060355040a130c446967694365727420496e6331193017060355040b13107777772e64696769636572742e636f6d312f302d06035504031326446967694365727420534841322048696768204173737572616e636520536572766572204341301e170d3134313032373030303030305a170d3138303130333132303030305a3081a3310b3009060355040613025553311330110603550408130a43616c69666f726e6961311430120603550407130b4c6f7320416e67656c6573313c303a060355040a1333496e7465726e657420436f72706f726174696f6e20666f722041737369676e6564204e616d657320616e64204e756d6265727331163014060355040b130d4954204f7065726174696f6e733113301106035504030c0a2a2e69616e612e6f726730820222300d06092a864886f70d01010105000382020f003082020a02820201009dbdfddeb5cae53a559747e2fda63728e4aba60f18b79a69f03310bf0164e5ee7db6b15bf56df23fddbae6a1bb38449b8c883f18102bbd8bb655ac0e2dac2ee3ed5cf4315868d2c598068284854b24894dcd4bd37811f0ad3a282cd4b4e599ffd07d8d2d3f2478554f81020b320ee12f44948e2ea1edbc990b830ca5cca6b4a839fb27b51850c9847eac74f26609eb24365b9751fb1c3208f56913bacbcae49201347c78b7e54a9d99979404c37f00fb65db849fd75e3a68770c30f2abe65b33256fb59b450050b00d8139d4d80d36f7bc46daf303e48f0f0791b2fdd72ec60b2cb3ad533c3f288c9c194e49337a69c496731f086d4f1f9825900713e2a551d05cb6057567850d91e6001c4ce27176f0957873a95b880acbec19e7bd9bcf1286d0452b73789c41905dd470971cd73aea52c77b080cd779af58234f337225c26f87a8c13e2a65e9dd4e03a5b41d7e06b3353f38129b2327a531ec9627a21dc423733aa029d4989448ba3322891c1a5690ddf2d25c8ec8aaa894b14aa92130c6b6d969a21ff671b60c4c923a94a93ea1dd0492c93393ca6edd61f33ca77e9208d01d6bd15107662ec088733df4c876a7e1608b82973a0f7592e84ed15579d181e79024ae8a7e4b9f0078eb2005b23f9d09a1df1bbc7de2a5a6085a3646d9fadb0e9da273a5f403cdd42831ce6f0ca46889585602bb8bc36bb3be861ff6d1a62e350203010001a38201e3308201df301f0603551d230418301680145168ff90af0207753cccd9656462a212b859723b301d0603551d0e04160414c7d0acef898b20e4b914668933032394f6bf3a61301f0603551d1104183016820a2a2e69616e612e6f7267820869616e612e6f7267300e0603551d0f0101ff0404030205a0301d0603551d250416301406082b0601050507030106082b0601050507030230750603551d1f046e306c3034a032a030862e687474703a2f2f63726c332e64696769636572742e636f6d2f736861322d68612d7365727665722d67332e63726c3034a032a030862e687474703a2f2f63726c342e64696769636572742e636f6d2f736861322d68612d7365727665722d67332e63726c30420603551d20043b3039303706096086480186fd6c0101302a302806082b06010505070201161c68747470733a2f2f7777772e64696769636572742e636f6d2f43505330818306082b0601050507010104773075302406082b060105050730018618687474703a2f2f6f6373702e64696769636572742e636f6d304d06082b060105050730028641687474703a2f2f636163657274732e64696769636572742e636f6d2f446967694365727453484132486967684173737572616e636553657276657243412e637274300c0603551d130101ff04023000300d06092a864886f70d01010b0500038201010070314c38e7c02fd80810500b9df6dae85de9b23e29fbd68bfdb5f23411c89acfaf9ae05af9123a8aa6bce6954a4e68dc7cfc480a65d76f229c4bd5f5674b0c9ac6d06a37a1a1c145c3956120b8efe67c887ab4ff7d6aa950ff3698f27c4a19d59d93a39aca5a7b6d6c75e34974e50f5a590005b3cb665ddbd7074f9fcbcbf9c50228d5e25596b64ada160b48f77a93aaced22617bfe005e00fe20a532a0adcb818c878dc5d6649277777ca1a814e21d0b53308af4078be4554715e4ce4828b012f25ffa13a6ceb30d20a75deba8a344e41d627fa638feff38a3063a0187519b39b053f7134d9cd83e6091accf5d2e3a05edfa1dfbe181a87ad86ba24fe6b97fe0004b5308204b130820399a003020102021004e1e7a4dc5cf2f36dc02b42b85d159f300d06092a864886f70d01010b0500306c310b300906035504061302555331153013060355040a130c446967694365727420496e6331193017060355040b13107777772e64696769636572742e636f6d312b30290603550403132244696769436572742048696768204173737572616e636520455620526f6f74204341301e170d3133313032323132303030305a170d3238313032323132303030305a3070310b300906035504061302555331153013060355040a130c446967694365727420496e6331193017060355040b13107777772e64696769636572742e636f6d312f302d06035504031326446967694365727420534841322048696768204173737572616e63652053657276657220434130820122300d06092a864886f70d01010105000382010f003082010a0282010100b6e02fc22406c86d045fd7ef0a6406b27d22266516ae42409bcedc9f9f76073ec330558719b94f940e5a941f5556b4c2022aafd098ee0b40d7c4d03b72c8149eef90b111a9aed2c8b8433ad90b0bd5d595f540afc81ded4d9c5f57b786506899f58adad2c7051fa897c9dca4b182842dc6ada59cc71982a6850f5e44582a378ffd35f10b0827325af5bb8b9ea4bd51d027e2dd3b4233a30528c4bb28cc9aac2b230d78c67be65e71b74a3e08fb81b71616a19d23124de5d79208ac75a49cbacd17b21e4435657f532539d11c0a9a631b199274680a37c2c25248cb395aa2b6e15dc1dda020b821a293266f144a2141c7ed6d9bf2482ff303f5a26892532f5ee30203010001a38201493082014530120603551d130101ff040830060101ff020100300e0603551d0f0101ff040403020186301d0603551d250416301406082b0601050507030106082b06010505070302303406082b0601050507010104283026302406082b060105050730018618687474703a2f2f6f6373702e64696769636572742e636f6d304b0603551d1f044430423040a03ea03c863a687474703a2f2f63726c342e64696769636572742e636f6d2f4469676943657274486967684173737572616e63654556526f6f7443412e63726c303d0603551d200436303430320604551d2000302a302806082b06010505070201161c68747470733a2f2f7777772e64696769636572742e636f6d2f435053301d0603551d0e041604145168ff90af0207753cccd9656462a212b859723b301f0603551d23041830168014b13ec36903f8bf4701d498261a0802ef63642bc3300d06092a864886f70d01010b05000382010100188a958903e66ddf5cfc1d68ea4a8f83d6512f8d6b44169eac63f5d26e6c84998baa8171845bed344eb0b7799229cc2d806af08e20e179a4fe034713eaf586ca59717df404966bd359583dfed331255c183884a3e69f82fd8c5b98314ecd789e1afd85cb49aaf2278b9972fc3eaad5410bdad536a1bf1c6e47497f5ed9487c03d9fd8b49a098264240ebd69211a4640a5754c4f51dd6025e6baceec4809a1272fa5693d7ffbf30850630bf0b7f4eff57059d24ed85c32bfba675a8ac2d16ef7d7927b2ebc29d0b07eaaa85d301a3202841594328d281e3aaf6ec7b3b77b640628005414501ef17063edec0339b67d3612e7287e469fc120057401e70f51ec9b4'))

    def test_num_certs(self):
        assert (len(self.p.data.certificates) == 2)


class TestTLSMultiFactory(object):

    """Made up test data"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode(b'1703010010'  # header 1
                               b'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'  # data 1
                               b'1703010010'  # header 2
                               b'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'  # data 2
                               b'1703010010'  # header 3
                               b'CCCCCCCC')  # data 3 (incomplete)
        cls.msgs, cls.bytes_parsed = tls_multi_factory(cls.data)

    def test_num_messages(self):
        # only complete messages should be parsed, incomplete ones left
        # in buffer
        assert (len(self.msgs) == 2)

    def test_bytes_parsed(self):
        assert (self.bytes_parsed == (5 + 16) * 2)

    def test_first_msg_data(self):
        assert (self.msgs[0].data == _hexdecode(b'AA' * 16))

    def test_second_msg_data(self):
        assert (self.msgs[1].data == _hexdecode(b'BB' * 16))

    def test_incomplete(self):
        msgs, n = tls_multi_factory(_hexdecode(b'17'))
        assert (len(msgs) == 0)
        assert (n == 0)
        msgs, n = tls_multi_factory(_hexdecode(b'1703'))
        assert (len(msgs) == 0)
        assert (n == 0)
        msgs, n = tls_multi_factory(_hexdecode(b'170301'))
        assert (len(msgs) == 0)
        assert (n == 0)
        msgs, n = tls_multi_factory(_hexdecode(b'17030100'))
        assert (len(msgs) == 0)
        assert (n == 0)
        msgs, n = tls_multi_factory(_hexdecode(b'1703010000'))
        assert (len(msgs) == 1)
        assert (n == 5)

