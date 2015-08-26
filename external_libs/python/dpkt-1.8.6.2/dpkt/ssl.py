# $Id: ssl.py 90 2014-04-02 22:06:23Z andrewflnr@gmail.com $
# Portion Copyright 2012 Google Inc. All rights reserved.
# -*- coding: utf-8 -*-
"""Secure Sockets Layer / Transport Layer Security."""

import dpkt
import ssl_ciphersuites
import struct
import binascii

#
# Note from April 2011: cde...@gmail.com added code that parses SSL3/TLS messages more in depth.
#
# Jul 2012: afleenor@google.com modified and extended SSL support further.
#


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

SSL3_VERSION_BYTES = set(('\x03\x00', '\x03\x01', '\x03\x02', '\x03\x03'))


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
    padding = '\x00' if lenbytes == 3 else ''
    # read off the length
    size = struct.unpack(size_format, padding + buf[:lenbytes])[0]
    # read the actual data
    data = buf[lenbytes:lenbytes + size]
    # if len(data) != size: insufficient data
    return data, size + lenbytes


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
        # extensions


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
            # ignore extensions for now
        except struct.error:
            # probably data too short
            raise dpkt.NeedData


class TLSUnknownHandshake(dpkt.Packet):
    __hdr__ = tuple()


TLSCertificate = TLSUnknownHandshake
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
        return struct.unpack('!I', '\x00' + self.length_bytes)[0]


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
    while i < n:
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


class TestTLSRecord(object):

    """
    Test basic TLSRecord functionality
    For this test, the contents of the record doesn't matter, since we're not parsing the next layer.
    """

    @classmethod
    def setup_class(cls):
        # add some extra data, to make sure length is parsed correctly
        cls.p = TLSRecord('\x17\x03\x01\x00\x08abcdefghzzzzzzzzzzz')

    def test_content_type(self):
        assert (self.p.type == 23)

    def test_version(self):
        assert (self.p.version == 0x0301)

    def test_length(self):
        assert (self.p.length == 8)

    def test_data(self):
        assert (self.p.data == 'abcdefgh')

    def test_initial_flags(self):
        assert (self.p.compressed == True)
        assert (self.p.encrypted == True)

    def test_repack(self):
        p2 = TLSRecord(type=23, version=0x0301, data='abcdefgh')
        assert (p2.type == 23)
        assert (p2.version == 0x0301)
        assert (p2.length == 8)
        assert (p2.data == 'abcdefgh')
        assert (p2.pack() == self.p.pack())

    def test_total_length(self):
        # that len(p) includes header
        assert (len(self.p) == 13)

    def test_raises_need_data_when_buf_is_short(self):
        import pytest
        pytest.raises(dpkt.NeedData, TLSRecord, '\x16\x03\x01\x00\x10abc')


class TestTLSChangeCipherSpec(object):

    """It's just a byte. This will be quick, I promise"""

    @classmethod
    def setup_class(cls):
        cls.p = TLSChangeCipherSpec('\x01')

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
        cls.h = TLSHandshake('\x00\x00\x00\x01\xff')

    def test_created_inside_message(self):
        assert (isinstance(self.h.data, TLSHelloRequest) == True)

    def test_length(self):
        assert (self.h.length == 0x01)

    def test_raises_need_data(self):
        import pytest
        pytest.raises(dpkt.NeedData, TLSHandshake, '\x00\x00\x01\x01')


class TestClientHello(object):

    """This data is extracted from and verified by Wireshark"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode(
            "01000199"  # handshake header
            "0301"  # version
            "5008220ce5e0e78b6891afe204498c9363feffbe03235a2d9e05b7d990eb708d"  # rand
            "2009bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed1"  # session id
            # cipher suites
            "005400ffc00ac0140088008700390038c00fc00500840035c007c009c011c0130045004400330032c00cc00ec002c0040096004100050004002fc008c01200160013c00dc003feff000ac006c010c00bc00100020001"
            "0100"  # compresssion methods
            # extensions
            "00fc0000000e000c0000096c6f63616c686f7374000a00080006001700180019000b00020100002300d0a50b2e9f618a9ea9bf493ef49b421835cd2f6b05bbe1179d8edf70d58c33d656e8696d36d7e7e0b9d3ecc0e4de339552fa06c64c0fcb550a334bc43944e2739ca342d15a9ebbe981ac87a0d38160507d47af09bdc16c5f0ee4cdceea551539382333226048a026d3a90a0535f4a64236467db8fee22b041af986ad0f253bc369137cd8d8cd061925461d7f4d7895ca9a4181ab554dad50360ac31860e971483877c9335ac1300c5e78f3e56f3b8e0fc16358fcaceefd5c8d8aaae7b35be116f8832856ca61144fcdd95e071b94d0cf7233740000"
            "FFFFFFFFFFFFFFFF")  # random garbage
        cls.p = TLSHandshake(cls.data)

    def test_client_hello_constructed(self):
        """Make sure the correct class was constructed"""
        # print self.p
        assert (isinstance(self.p.data, TLSClientHello) == True)

    #   def testClientDateCorrect(self):
    #       self.assertEqual(self.p.random_unixtime, 1342710284)

    def test_client_random_correct(self):
        assert (self.p.data.random == _hexdecode('5008220ce5e0e78b6891afe204498c9363feffbe03235a2d9e05b7d990eb708d'))

    def test_cipher_suite_length(self):
        # we won't bother testing the identity of each cipher suite in the list.
        assert (self.p.data.num_ciphersuites == 42)
        # self.assertEqual(len(self.p.ciphersuites), 42)

    def test_session_id(self):
        assert (self.p.data.session_id == _hexdecode('09bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed1'))

    def test_compression_methods(self):
        assert (self.p.data.num_compression_methods == 1)

    def test_total_length(self):
        assert (len(self.p) == 413)


class TestServerHello(object):

    """Again, from Wireshark"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode(
            '0200004d03015008220c8ec43c5462315a7c99f5d5b6bff009ad285b51dc18485f352e9fdecd2009bc0192e008e6fa8fe47998fca91311ba30ddde14a9587dc674b11c3d3e5ed10002000005ff01000100')
        cls.p = TLSHandshake(cls.data)

    def test_constructed(self):
        assert (isinstance(self.p.data, TLSServerHello) == True)

    #    def testDateCorrect(self):
    #        self.assertEqual(self.p.random_unixtime, 1342710284)

    def test_random_correct(self):
        assert (self.p.data.random == _hexdecode('5008220c8ec43c5462315a7c99f5d5b6bff009ad285b51dc18485f352e9fdecd'))

    def test_cipher_suite(self):
        assert (ssl_ciphersuites.BY_CODE[self.p.data.cipher_suite].name == 'TLS_RSA_WITH_NULL_SHA')

    def test_total_length(self):
        assert (len(self.p) == 81)


class TestTLSMultiFactory(object):

    """Made up test data"""

    @classmethod
    def setup_class(cls):
        cls.data = _hexdecode('1703010010'  # header 1
                               'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'  # data 1
                               '1703010010'  # header 2
                               'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'  # data 2
                               '1703010010'  # header 3
                               'CCCCCCCC')  # data 3 (incomplete)
        cls.msgs, cls.bytes_parsed = tls_multi_factory(cls.data)

    def test_num_messages(self):
        # only complete messages should be parsed, incomplete ones left
        # in buffer
        assert (len(self.msgs) == 2)

    def test_bytes_parsed(self):
        assert (self.bytes_parsed == (5 + 16) * 2)

    def test_first_msg_data(self):
        assert (self.msgs[0].data == _hexdecode('AA' * 16))

    def test_second_msg_data(self):
        assert (self.msgs[1].data == _hexdecode('BB' * 16))

