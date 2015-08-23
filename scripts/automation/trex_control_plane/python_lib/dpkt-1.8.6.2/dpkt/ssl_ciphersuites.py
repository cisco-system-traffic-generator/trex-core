# Copyright 2012 Google Inc. All rights reserved.
# -*- coding: utf-8 -*-
"""
Nicely formatted cipher suite definitions for TLS

A list of cipher suites in the form of CipherSuite objects.
These are supposed to be immutable; don't mess with them.
"""


class CipherSuite(object):
    """
    Encapsulates a cipher suite.

    Members/args:
    * code: two-byte ID code, as int
    * name: as in 'TLS_RSA_WITH_RC4_40_MD5'
    * kx: key exchange algorithm, string
    * auth: authentication algorithm, string
    * encoding: encoding algorithm
    * mac: message authentication code algorithm
    """

    def __init__(self, code, name, kx, auth, encoding, mac):
        self.code = code
        self.name = name
        self.kx = kx
        self.auth = auth
        self.encoding = encoding
        self.mac = mac

    def __repr__(self):
        return 'CipherSuite(%s)' % self.name

    MAC_SIZES = {
        'MD5': 16,
        'SHA': 20,
        'SHA256': 32,  # I guess
    }

    BLOCK_SIZES = {
        'AES_256_CBC': 16,
    }

    @property
    def mac_size(self):
        """In bytes. Default to 0."""
        return self.MAC_SIZES.get(self.mac, 0)

    @property
    def block_size(self):
        """In bytes. Default to 1."""
        return self.BLOCK_SIZES.get(self.encoding, 1)


# master list of CipherSuite Objects
CIPHERSUITES = [
    # not a real cipher suite, can be ignored, see RFC5746
    CipherSuite(0xff, 'TLS_EMPTY_RENEGOTIATION_INFO',
                'NULL', 'NULL', 'NULL', 'NULL'),
    CipherSuite(0x00, 'TLS_NULL_WITH_NULL_NULL',
                'NULL', 'NULL', 'NULL', 'NULL'),
    CipherSuite(0x01, 'TLS_RSA_WITH_NULL_MD5', 'RSA', 'RSA', 'NULL', 'MD5'),
    CipherSuite(0x02, 'TLS_RSA_WITH_NULL_SHA', 'RSA', 'RSA', 'NULL', 'SHA'),
    CipherSuite(0x0039, 'TLS_DHE_RSA_WITH_AES_256_CBC_SHA',
                'DHE', 'RSA', 'AES_256_CBC', 'SHA'),  # not sure I got the kx/auth thing right.
    CipherSuite(0xffff, 'UNKNOWN_CIPHER', '', '', '', '')
]

BY_CODE = dict(
    (cipher.code, cipher) for cipher in CIPHERSUITES)

BY_NAME = dict(
    (suite.name, suite) for suite in CIPHERSUITES)

NULL_SUITE = BY_CODE[0x00]
