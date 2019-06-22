#!/bin/python

import base64
import copy

from trex.stl.api import *
from trex.common.services.trex_service_ap import *

from trex.utils import text_tables, parsing_opts
from trex.utils.common import natural_sorted_key
from trex.utils.parsing_opts import ArgumentPack, ArgumentGroup, is_valid_file, check_mac_addr, check_ipv4_addr, MUTEX

from scapy.contrib.capwap import *
from scapy.contrib.capwap import CAPWAP_PKTS # ensure capwap_internal is imported
from trex_openssl import *
from texttable import ansi_len
from collections import deque
import threading
import re
import os
import yaml
import ctypes
import struct
import binascii


def base64encode(buf):
    try:
        return base64.b64encode(buf).decode()
    except:
        print('Could not encode: %s' % buf)
        raise

def base64decode(buf):
    try:
        return base64.b64decode(buf)
    except:
        print('Could not decode: %s' % buf)
        raise


class SSL_Context:
    ''' Shared among all APs '''
    def __init__(self):
        self.ctx = None
        self.evp = None
        bne = None
        rsa = None

        self.ctx = libssl.SSL_CTX_new(libssl.DTLSv1_method())
        if self.ctx is None:
            raise TRexError('Could not create SSL Context')

        try:
            bne = libcrypto.BN_new()
            libcrypto.BN_set_word(bne, SSL_CONST.RSA_F4)
            rsa = libcrypto.RSA_new()
            if libcrypto.RSA_generate_key_ex(rsa, 1024, bne, None) != 1:
                raise TRexError('Could not generate RSA key in SSL Context')

            if libssl.SSL_CTX_use_RSAPrivateKey(self.ctx, rsa) != 1:
                raise TRexError('Could not set RSA key into SSL Context')

            self.evp = libcrypto.EVP_PKEY_new()
            if libcrypto.EVP_PKEY_set1_RSA(self.evp, rsa) != 1:
                raise TRexError('Could not create EVP_PKEY in SSL Context')

            libssl.SSL_CTX_set_options(self.ctx, SSL_CONST.SSL_OP_NO_TICKET) # optimization

        finally:
            if bne:
                libcrypto.BN_free(bne)
            if rsa:
                libcrypto.RSA_free(rsa)

    def __del__(self):
        if libssl and self.ctx:
            libssl.SSL_CTX_free(self.ctx)
        if libcrypto and self.evp:
            libcrypto.EVP_PKEY_free(self.evp)

class VAP(object):
    """ A VAP Entry (BSSID) representing a SSID on AP on a given frequency """
    def __init__(self, ssid, slot_id, vap_id):
        self.ssid = ssid
        self.slot_id = slot_id
        self.vap_id = vap_id
        self.encrypt_policy = 1 # Open Auth

    def __str__(self):
        return "VAP(slotId={},vapId={},ssid={})".format(self.slot_id, self.vap_id, str(self.ssid))

class AP:
    VERB_QUIET = 0
    VERB_ERR   = 1
    VERB_WARN  = 2 # default
    VERB_INFO  = 3
    VERB_DEBUG = 4
    _scapy_cache_static = {}

    def __init__(self, ssl_ctx, logger, trex_port, mac, ip, wlc_ip, verbose_level = VERB_WARN, rsa_ca_priv_file = None, rsa_priv_file = None, rsa_cert_file = None):
        self.ssl_ctx = ssl_ctx
        self.logger = logger
        self.trex_port = trex_port
        self.port_id = trex_port.port_id
        check_mac_addr(mac)
        check_ipv4_addr(ip)
        try:
            self.mac_bytes = mac2str(mac)
        except:
            raise TRexError('Bad MAC format, expected aa:bb:cc:dd:ee:ff')
        self.mac = mac
        self.name = 'AP%s%s.%s%s.%s%s' % (mac[:2], mac[3:5], mac[6:8], mac[9:11], mac[12:14], mac[15:17])
        self.name_bytes = self.name.encode('ascii')
        assert '.' in ip, 'Bad IP format, expected x.x.x.x'
        self.ip_bytes = is_valid_ipv4_ret(ip)
        self.ip = ip
        self.udp_port = 50000 # should not matter
        self.udp_port_str = int2str(self.udp_port, 2)
        self.radio_mac_bytes = b'\x94' + self.ip_bytes + b'\x00'
        self.radio_mac = str2mac(self.radio_mac_bytes)
        self.ssl = None
        self.in_bio = None
        self.out_bio = None
        self.serial_number = 'FCZ1853QQQ'
        self.country = 'CH '
        self.echo_req_interval = 60
        self.last_echo_req_ts = 0
        self.verbose_level = verbose_level
        self.clients = []
        self.ap_mode = 'Local'
        self.rsa_priv_file = rsa_priv_file
        self.rsa_cert_file = rsa_cert_file
        self.rsa_ca_priv_file = rsa_ca_priv_file
        self.capwap_MaxRetransmit = 5
        self.capwap_RetransmitInterval = 0.5
        self.ssl_lock = threading.RLock()
        self.reset_vars()
        if wlc_ip and wlc_ip != '255.255.255.255':
            self.ip_dst = wlc_ip
            self.wlc_ip_bytes = is_valid_ipv4_ret(wlc_ip)


    def reset_vars(self):
        self.rx_buffer = deque(maxlen = 100)
        self.capwap_assemble = {}
        self.wlc_name = ''
        self.wlc_sw_ver = []
        self.is_connected = False
        self.echo_resp_timer = None
        self.echo_resp_retry = 0
        self.echo_resp_timeout = 0
        self.SSID = {}
        self.session_id = None
        self.mac_dst = None
        self.mac_dst_bytes = None
        self.ip_dst = None
        self.wlc_ip_bytes = None
        self.dot11_seq = 0
        self.__capwap_seq = 0
        self._scapy_cache = {}
        self.last_recv_ts = None
        self.is_handshake_done = False
        self.is_dtls_closed = False
        self.got_keep_alive = False
        self.rx_responses = {}
        self._create_ssl()

    def debug(self, msg):
        if self.is_debug:
            self.logger.urgent_async_log(msg)


    def info(self, msg):
        if self.verbose_level >= self.VERB_INFO:
            self.logger.urgent_async_log(msg)


    def warn(self, msg):
        if self.verbose_level >= self.VERB_WARN:
            self.logger.urgent_async_log(msg)

    def err(self, msg):
        if self.verbose_level >= self.VERB_ERR:
            self.logger.urgent_async_log(msg)


    def fatal(self, msg):
        raise TRexError('%s: %s' % (self.name, msg))


    @property
    def is_debug(self):
        return self.verbose_level >= self.VERB_DEBUG


    def __del__(self):
        if getattr(self, 'ssl', None) and libssl:
            libssl.SSL_free(self.ssl)


    def _create_ssl(self):
        if self.ssl:
            libssl.SSL_free(self.ssl)

        self.ssl = libssl.SSL_new(self.ssl_ctx.ctx)
        self.openssl_buf = c_buffer(9999)
        self.in_bio = libcrypto.BIO_new(libcrypto.BIO_s_mem())
        self.out_bio = libcrypto.BIO_new(libcrypto.BIO_s_mem())

        if self.rsa_ca_priv_file:
            if hasattr(self.rsa_ca_priv_file, 'encode'):
                self.rsa_ca_priv_file = self.rsa_ca_priv_file.encode('UTF-8')
            rsa_ca = libcrypto.PEM_read_RSAPrivateKey_helper(self.rsa_ca_priv_file)
            if not rsa_ca:
                self.fatal('Could not load given controller trustpoint private key %s' % self.rsa_ca_priv_file)

            evp_ca = libcrypto.EVP_PKEY_new()
            if libcrypto.EVP_PKEY_set1_RSA(evp_ca, rsa_ca) != 1:
                raise TRexError('Could not create EVP_PKEY in SSL Context for controller key')

        else:
            if not self.rsa_cert_file:
                self.warn("The AP Certificate will be self-signed. Newer version of eWLC do not support it.")
            evp_ca = self.ssl_ctx.evp

        if self.rsa_priv_file and self.rsa_cert_file:
            self.debug('Using provided certificate')
            if hasattr(self.rsa_cert_file, 'encode'):
                self.rsa_cert_file = self.rsa_cert_file.encode('UTF-8')
            if hasattr(self.rsa_priv_file, 'encode'):
                self.rsa_priv_file = self.rsa_priv_file.encode('UTF-8')
            if libssl.SSL_use_certificate_file(self.ssl, c_buffer(self.rsa_cert_file), SSL_CONST.SSL_FILETYPE_PEM) != 1:
                self.fatal('Could not load given certificate file %s' % self.rsa_cert_file)
            if libssl.SSL_use_PrivateKey_file(self.ssl, c_buffer(self.rsa_priv_file), SSL_CONST.SSL_FILETYPE_PEM) != 1:
                self.fatal('Could not load given private key %s' % self.rsa_priv_file)
        else:
            x509_cert = None
            x509_name = None
            try:
                x509_cert = libcrypto.X509_new()
                x509_name = libcrypto.X509_NAME_new()

                '''
Cheetah:
    Data:
        Version: 3 (0x2)
        Serial Number:
            1b:56:b2:2d:00:00:00:05:0f:d0
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: O=Cisco, CN=Cisco Manufacturing CA SHA2
        Validity
            Not Before: Sep 16 19:17:57 2016 GMT
            Not After : Sep 16 19:27:57 2026 GMT
        Subject: C=US, ST=California, L=San Jose, O=Cisco Systems, CN=AP1G4-94D469F82DE8/emailAddress=support@cisco.com

eWLC:
    Data:
        Version: 3 (0x2)
        Serial Number:
            1d:6e:0a:b4:00:00:00:2f:cc:64
    Signature Algorithm: sha1WithRSAEncryption
        Issuer: O=Cisco Systems, CN=Cisco Manufacturing CA
        Validity
            Not Before: May  6 10:12:56 2015 GMT
            Not After : May  6 10:22:56 2025 GMT
        Subject: C=US, ST=California, L=San Jose, O=Cisco Systems, CN=0050569C622D/emailAddress=support@cisco.com

'''

                if libcrypto.X509_set_version(x509_cert, 2) != 1:
                    self.fatal('Could not set version of certificate')
                if libcrypto.X509_set_pubkey(x509_cert, self.ssl_ctx.evp) != 1:
                    self.fatal('Could not assign public key to certificate')
                if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'C', SSL_CONST.MBSTRING_ASC, b'US', -1, -1, 0) != 1:
                    self.fatal('Could not assign C to certificate')
                if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'ST', SSL_CONST.MBSTRING_ASC, b'California', -1, -1, 0) != 1:
                    self.fatal('Could not assign ST to certificate')
                if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'L', SSL_CONST.MBSTRING_ASC, b'San Jose', -1, -1, 0) != 1:
                    self.fatal('Could not assign L to certificate')
                #if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'O', SSL_CONST.MBSTRING_ASC, b'Cisco Systems', -1, -1, 0) != 1:
                if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'O', SSL_CONST.MBSTRING_ASC, b'Cisco Virtual Wireless LAN Controller', -1, -1, 0) != 1:
                    self.fatal('Could not assign O to certificate')
                #if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'CN', SSL_CONST.MBSTRING_ASC, ('AP1G4-%s' % hex(self.mac_bytes, delimiter = '').upper()).encode('ascii'), -1, -1, 0) != 1:
                if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'CN', SSL_CONST.MBSTRING_ASC, b'CA-vWLC', -1, -1, 0) != 1:
                    self.fatal('Could not assign CN to certificate')
                if libcrypto.X509_set_subject_name(x509_cert, x509_name) != 1:
                    self.fatal('Could not set subject name to certificate')
                if libcrypto.X509_set_issuer_name(x509_cert, x509_name) != 1:
                    self.fatal('Could not set issuer name to certificate')
                if not libcrypto.X509_time_adj_ex(libcrypto.X509_getm_notBefore(x509_cert), -999, 0, None):
                    self.fatal('Could not set "Not before" time to certificate"')
                if not libcrypto.X509_time_adj_ex(libcrypto.X509_getm_notAfter(x509_cert), 999, 0, None):
                    self.fatal('Could not set "Not after" time to certificate"')
                if not libcrypto.X509_sign(x509_cert, evp_ca, libcrypto.EVP_sha256()):
                    self.fatal('Could not sign the certificate')
                libssl.SSL_use_certificate(self.ssl, x509_cert)
            finally:
                if x509_name:
                    libcrypto.X509_NAME_free(x509_name)
                if x509_cert:
                    libcrypto.X509_free(x509_cert)

        if libssl.SSL_check_private_key(self.ssl) != 1:
            self.fatal('Error: check of RSA private key failed.')
        libssl.SSL_set_bio(self.ssl, self.in_bio, self.out_bio)
        libssl.SSL_set_connect_state(self.ssl)


    def get_config_update_capwap(self, seq):
        if 'config_update' in self._scapy_cache:
            self._scapy_cache['config_update'][20] = struct.pack('!B', seq)
        else:
            self._scapy_cache['config_update'] = CAPWAP_PKTS.config_update(self, seq)
        return self._scapy_cache['config_update']


    def get_echo_capwap(self):
        if 'echo_pkt' in self._scapy_cache:
            self._scapy_cache['echo_pkt'][20] = struct.pack('!B', self.get_capwap_seq())
        else:
            self._scapy_cache['echo_pkt'] = CAPWAP_PKTS.echo(self)
        return self._scapy_cache['echo_pkt']


    def get_echo_wrap(self, encrypted):
        if 'echo_wrap' not in self._scapy_cache:
            self._scapy_cache['echo_wrap'] = bytes(self.wrap_capwap_pkt(b'\1\0\0\0' + encrypted))[:-len(encrypted)]
        return self._scapy_cache['echo_wrap'] + encrypted


    def wrap_capwap_pkt(self, capwap_bytes, is_discovery = False, dst_port = 5246):
        if isinstance(capwap_bytes, ctypes.Array):
            capwap_bytes = capwap_bytes.raw
        assert isinstance(capwap_bytes, bytes)
        if is_discovery:
            ip = b'\x45\x00' + struct.pack('!H', 28 + len(capwap_bytes)) + b'\x00\x01\x00\x00\x40\x11\0\0' + self.ip_bytes + (b'\xff\xff\xff\xff' if not self.wlc_ip_bytes else self.wlc_ip_bytes)
            checksum = scapy.utils.checksum(ip)
            ip = ip[:10] + struct.pack('!H', checksum) + ip[12:]
            return (
                (b'\xff\xff\xff\xff\xff\xff' if not self.mac_dst_bytes else self.mac_dst_bytes) + self.mac_bytes + b'\x08\x00' +
                ip +
                struct.pack('!H', self.udp_port) + struct.pack('!H', dst_port) + struct.pack('!H', 8 + len(capwap_bytes)) + b'\0\0' +
                capwap_bytes
                )

        if 'capwap_wrap' not in self._scapy_cache:
            self._scapy_cache['capwap_wrap_ether'] = self.mac_dst_bytes + self.mac_bytes + b'\x08\x00'
            self._scapy_cache['capwap_wrap_ip1'] = b'\x45\x00' # 2 bytes of total length after this one
            self._scapy_cache['capwap_wrap_ip2'] = b'\x00\x01\x00\x00\x40\x11\0\0' + self.ip_bytes + self.wlc_ip_bytes
            self._scapy_cache['capwap_wrap_udp_src'] = struct.pack('!H', self.udp_port)
            self._scapy_cache['capwap_wrap'] = True

        ip = self._scapy_cache['capwap_wrap_ip1'] + struct.pack('!H', 28 + len(capwap_bytes)) + self._scapy_cache['capwap_wrap_ip2']
        checksum = scapy.utils.checksum(ip)
        ip = ip[:10] + struct.pack('!H', checksum) + ip[12:]
        udp = self._scapy_cache['capwap_wrap_udp_src'] + struct.pack('!H', dst_port) + struct.pack('!H', 8 + len(capwap_bytes)) + b'\0\0'
        return self._scapy_cache['capwap_wrap_ether'] + ip + udp + capwap_bytes


    def wrap_pkt_by_wlan(self, client, pkt):
        assert type(pkt) is bytes, 'wrap_pkt_by_wlan() expects bytes, got: %s' % type(pkt)
        assert len(pkt) >= 14, 'Too small buffer to wrap'
        self.dot11_seq += 1
        if self.dot11_seq > 0xfff:
            self.dot11_seq = 0

        verify = False
        if 'wlan_wrapping' not in AP._scapy_cache_static:
            verify = True
            p1 = bytes(
                CAPWAP_DATA(
                    header = CAPWAP_Header(
                        wbid = 1,
                        flags = 'WT',
                        wireless_info_802 = CAPWAP_Wireless_Specific_Information_IEEE802_11(
                            rssi = 216,
                            snr = 31,
                            data_rate = 0,
                            )
                        )
                    )/
                Dot11_swapped(
                    FCfield = 'to-DS',
                    subtype = 8,
                    type = 'Data',
                    ID = 0,
                    addr1 = self.radio_mac,
                    addr2 = client.mac,
                    addr3 = str2mac(pkt[:6]),
                    #SC = self.dot11_seq << 4
                    )/
                Dot11QoS()/
                LLC(dsap = 170, ssap = 170, ctrl = 3)/
                SNAP()[0])
            AP._scapy_cache_static['wlan_wrapping_1'] = p1[:20]
            AP._scapy_cache_static['wlan_wrapping_2'] = p1[38:-2]
            AP._scapy_cache_static['wlan_wrapping'] = True

        p = (
            AP._scapy_cache_static['wlan_wrapping_1'] +
            self.radio_mac_bytes +
            client.mac_bytes +
            pkt[:6] +
            AP._scapy_cache_static['wlan_wrapping_2']
            )
        #CAPWAP_DATA(p).dump_offsets_tree()
        # need to update following:
        # Dot11_swapped.addr1 = self.radio_mac_bytes
        # Dot11_swapped.addr2 = client.mac_bytes
        # Dot11_swapped.addr3 = pkt.dst
        # Dot11_swapped.SC = self.dot11_seq << 4
        # SNAP.code = struct.unpack('!H', pkt[12:14]

        #p1 = (
        #    p[:9] + ap.radio_mac_bytes +
        #    p[15:20] + struct.pack('!B', capwap_seq) +
        #    p[21:]
        #    )
        #
        if verify and os.getenv('VERIFY_SCAPY_CACHE'):
            print('verifying wlan_wrapping')
            assert p == p1[:-2], '\n%s\n%s\n\n%s\n%s' % (type(p), hexstr(p), type(p1), hexstr(p1))
        return self.wrap_capwap_pkt(p + pkt[12:], dst_port = 5247)

    def wrap_pkt_by_ap_msg(self, pkt):
        assert type(pkt) is bytes, 'wrap_pkt_by_ap_msg() expects bytes, got: %s' % type(pkt)
        assert len(pkt) >= 14, 'Too small buffer to wrap'

        p1 = bytes(
            CAPWAP_DATA(
                header = CAPWAP_Header(
                    wbid = 1,
                    flags = 'WT',
                    wireless_info_802 = CAPWAP_Wireless_Specific_Information_IEEE802_11(
                        rssi = 216,
                        snr = 31,
                        data_rate = 0,
                        )
                    )
                )/
            Dot11_swapped(
                FCfield = 'to-DS',
                subtype = 8,
                type = 'Data',
                ID = 0,
                addr1 = self.mac,
                addr2 = self.mac,
                addr3 = self.mac,
                )/
            Dot11QoS())

        return self.wrap_capwap_pkt(p1 + pkt, dst_port = 5247)


    def patch_stream(self, client, stream):
        assert type(stream) is STLStream, 'patch_stream() expects STLStream, got: %s' % type(stream)
        stream = copy.deepcopy(stream)
        patched_pkt = Ether(stream.pkt)
        if stream.fields['packet']['meta']:
            pkt_meta = '%s\nPatched stream: Added WLAN' % stream.fields['packet']['meta']
        else:
            pkt_meta = 'Patched stream: Added WLAN'
        port_layer = self.trex_port.get_layer_cfg()
        if stream.fields['flags'] & 1 == 0:
            pkt_meta += ', Changed source'
            patched_pkt.src = port_layer['ether']['src']
        if stream.fields['flags'] & 0x110 == 0:
            pkt_meta += ', Changed destination'
            patched_pkt.dst = port_layer['ether']['dst']

        stream.pkt = self.wrap_pkt_by_wlan(client, bytes(patched_pkt))
        stream.fields['packet'] = {'binary': base64encode(stream.pkt),
                                   'meta': pkt_meta}

        # We force the changed src & dst MAC address to 1 using the stream flags
        stream.fields['flags'] |= 0b111
        for inst in  stream.fields['vm']['instructions']:
            if 'pkt_offset' in inst:
                inst['pkt_offset'] += 78 # Size of wrapping layers minus removed Ethernet
            elif 'offset' in inst:
                inst['offset'] += 78
        return stream

    def patch_ap_stream(self, stream):
        assert type(stream) is STLStream, 'patch_stream() expects STLStream, got: %s' % type(stream)
        stream = copy.deepcopy(stream)
        patched_pkt = Ether(stream.pkt)

        stream.pkt = self.wrap_pkt_by_ap_msg(bytes(patched_pkt))
        stream.fields['packet'] = {'binary': base64encode(stream.pkt),
                                   'meta': ''}
        # We force the changed src & dst MAC address to 1 using the stream flags
        stream.fields['flags'] |= 0b011

        for inst in  stream.fields['vm']['instructions']:
            if 'pkt_offset' in inst:
                inst['pkt_offset'] += 84 # Size of wrapping layers
            elif 'offset' in inst:
                inst['offset'] += 84
        return stream


    def is_handshake_done_libssl(self):
        with self.ssl_lock:
            return bool(libssl.SSL_is_init_finished(self.ssl))


    def is_dtls_closed_libssl(self):
        with self.ssl_lock:
            return bool(libssl.SSL_get_shutdown(self.ssl))


    @property
    def is_dtls_established(self):
        return self.is_handshake_done and not self.is_dtls_closed


    def ssl_read(self):
        with self.ssl_lock:
            ret = libcrypto.BIO_read(self.out_bio, self.openssl_buf, 10000)
            if ret >= 0:
                return self.openssl_buf[:ret]
            ret = libcrypto.BIO_test_flags(self.out_bio, SSL_CONST.BIO_FLAGS_SHOULD_RETRY)
            if ret:
                return ''
            self.is_connected = False


    # without lock, careful
    def __ssl_write(self, buf):
        if isinstance(buf, ctypes.Array):
            ret = libcrypto.BIO_write(self.in_bio, buf, len(buf))
        else:
            ret = libcrypto.BIO_write(self.in_bio, c_buffer(buf), len(buf) + 1)
        if ret >= 0:
            return ret
        ret = libcrypto.BIO_test_flags(out_bio, SSL_CONST.BIO_FLAGS_SHOULD_RETRY)
        if ret:
            return ''
        self.is_connected = False


    def encrypt(self, buf):
        with self.ssl_lock:
            if isinstance(buf, Packet):
                raise TRexError('Consider converting to buffer: %s' % buf.command())
            if isinstance(buf, ctypes.Array):
                ret = libssl.SSL_write(self.ssl, buf, len(buf))
            else:
                ret = libssl.SSL_write(self.ssl, c_buffer(buf), len(buf))
            #err = SSL_CONST.ssl_err.get(libcrypto.ERR_get_error(self.ssl, ret))
            #if err != 'SSL_ERROR_NONE':
            #    self.fatal('Got SSL error: %s (ret %s)' % (err, ret))
            return self.ssl_read()


    def decrypt(self, buf):
        with self.ssl_lock:
            self.__ssl_write(buf)
            ret = libssl.SSL_read(self.ssl, self.openssl_buf, 10000)
            #err = SSL_CONST.ssl_err.get(libcrypto.ERR_get_error(self.ssl, ret))
            #if err != 'SSL_ERROR_NONE':
            #    self.fatal('Got SSL error: %s' % (err, ret))
            return self.openssl_buf[:ret]


    def get_arp_pkt(self, op, src_mac_bytes, src_ip_bytes):
        assert len(src_mac_bytes) == 6
        assert len(src_ip_bytes) == 4
        if op == 'who-has':
            arp_dst = b'\xff\xff\xff\xff\xff\xff' + self.wlc_ip_bytes
        elif op == 'is-at':
            arp_dst = self.mac_dst_bytes + self.wlc_ip_bytes
        elif op == 'garp':
            arp_dst = b'\0\0\0\0\0\0' + src_ip_bytes
        else:
            raise TRexError('Bad op of ARP: %s' % op)
        return (
            (b'\xff\xff\xff\xff\xff\xff' if op in ('who-has', 'garp') else self.mac_dst_bytes) + src_mac_bytes + b'\x08\x06' + # Ethernet
            b'\x00\x01\x08\x00\x06\x04' +
            (b'\x00\x01' if op in ('who-has', 'garp') else b'\x00\x02') +
            src_mac_bytes + src_ip_bytes + arp_dst # ARP
            )


    def get_capwap_seq(self):
        seq = self.__capwap_seq
        if self.__capwap_seq < 0xff:
            self.__capwap_seq += 1
        else:
            self.__capwap_seq = 0
        return seq

    def get_vap_entry(self, slot_id, vap_id):
        return self.SSID.get(slot_id, vap_id)

    def get_open_auth_vap(self):
        for vap in self.SSID.values():
            if vap.encrypt_policy == 1:
                return vap

    def create_vap(self, ssid, slot_id, vap_id):
        """
        Create a new VAP and insert it into the AP

        Return the newly created VAP
        """
        vap = VAP(ssid=ssid, slot_id=slot_id, vap_id=vap_id)
        self.SSID[(slot_id, vap_id)] = vap
        return vap

    def delete_vap(self, slot_id, vap_id):
        del self.SSID[(slot_id, vap_id)]


class APClient:
    def __init__(self, mac, ip, ap):
        if ':' in mac:
            self.mac_bytes = mac2str(mac)
            self.mac = mac
        else:
            self.mac_bytes = mac
            self.mac = str2mac(mac)
        if '.' in ip:
            self.ip_bytes = is_valid_ipv4_ret(ip)
            self.ip = ip
        elif len(ip) == 4:
            self.ip_bytes = ip
            self.ip = str2ip(ip)
        else:
            raise TRexError('Bad IP provided, should be x.x.x.x, got: %s' % ip)
        check_mac_addr(self.mac)
        check_ipv4_addr(self.ip)
        assert isinstance(ap, AP)
        self.ap = ap
        self.reset()

    def reset(self):
        self.got_disconnect = False
        self.is_associated = False
        self.seen_arp_reply = False

    def disconnect(self):
        self.reset()
        self.got_disconnect = True


class AP_Manager:
    def __init__(self, trex_client = None, server = None, sync_port = 4501, async_port = 4500):
        self.ssl_ctx = None
        if not (bool(server) ^ bool(trex_client)):
            raise STLError('Please specify either trex_client or server argument.')
        if trex_client:
            conn_info = trex_client.get_connection_info()
            server     = conn_info['server']
            sync_port  = conn_info['sync_port']
            async_port = conn_info['async_port']
        self.bg_client = STLClient('AP Manager', server, sync_port, async_port, verbose_level = 'none')
        self.trex_client = trex_client or self.bg_client
        self.aps = []
        self.clients = []
        self.ap_by_name = {}
        self.ap_by_mac = {}
        self.ap_by_ip = {}
        self.client_by_id = {}
        self.bg_lock = threading.RLock()
        self.service_ctx = {}
        self.base_file_path = '/tmp/trex/console/%s_%s_%s_%s.wlc_base' % (get_current_user(), server, sync_port, async_port)
        base_file_dir = os.path.dirname(self.base_file_path)
        if not os.path.exists(base_file_dir):
            os.makedirs(base_file_dir, mode = 0o777)
        self._init_base_vals()


    def init(self, trex_port_ids):
        if type(trex_port_ids) is int:
            trex_port_ids = [trex_port_ids]
        if not self.bg_client.is_connected():
            self.bg_client.connect()

        for port_id in trex_port_ids:
            if port_id in self.service_ctx:
                raise TRexError('AP manager already initialized on port %s. Close it to proceed.' % port_id)
            if port_id >= len(self.trex_client.ports):
                raise TRexError('TRex port %s does not exist!' % port_id)
            trex_port = self.trex_client.ports[port_id]
            if not trex_port.is_acquired():
                raise TRexError('Port %s is not acquired' % port_id)
            if trex_port.get_vlan_cfg():
                raise TRexError('Port %s has VLAN, plugin does not support it. Use trunk with native vlans.' % port_id)

        for port_id in trex_port_ids:
            success = False
            try:
                self.service_ctx[port_id] = {}
                if not self.ssl_ctx:
                    self.ssl_ctx = SSL_Context()
                self.trex_client.set_service_mode(port_id, True)
                if not self.trex_client.get_port_attr(port = port_id)['prom'] == 'on':
                    self.trex_client.set_port_attr(ports = port_id, promiscuous = True)
                self.service_ctx[port_id]['synced'] = True
                self.service_ctx[port_id]['bg'] = ServiceApBgMaintenance(self, port_id)
                self.service_ctx[port_id]['fg'] = ServiceBufferedCtx(self.trex_client, port_id)
                self.service_ctx[port_id]['bg'].run()
                success = True
            finally:
                if not success:
                    del self.service_ctx[port_id]


    def _init_base_vals(self):
        try:
            self.set_base_values(load = True)
        except Exception as e:
            self.err('Error setting base values (%s), will use default' % e)
            self.next_ap_mac = '94:12:12:12:12:01'
            self.next_ap_ip = '9.9.12.1'
            self.next_client_mac = '94:13:13:13:13:01'
            self.next_client_ip = '9.9.13.1'
            self.wlc_ip = '255.255.255.255'


    def _get_ap_by_id(self, ap_id):
        if isinstance(ap_id, AP):
            return ap_id
        if ap_id in self.ap_by_name:
            return self.ap_by_name[ap_id]
        elif ap_id in self.ap_by_mac:
            return self.ap_by_mac[ap_id]
        elif ap_id in self.ap_by_ip:
            return self.ap_by_ip[ap_id]
        else:
            raise TRexError('AP with id %s does not exist!' % ap_id)


    def _get_client_by_id(self, client_id):
        if isinstance(client_id, APClient):
            return client_id
        elif client_id in self.client_by_id:
            return self.client_by_id[client_id]
        else:
            raise TRexError('Client with id %s does not exist!' % client_id)


    def create_ap(self, trex_port_id, mac, ip, wlc_ip = None, verbose_level = AP.VERB_WARN,rsa_ca_priv_file = None, rsa_priv_file = None, rsa_cert_file = None):
        if trex_port_id not in self.service_ctx:
            raise TRexError('TRex port %s does not exist!' % trex_port_id)
        if ':' not in mac:
            mac = str2mac(mac)
        if mac in self.ap_by_mac:
            raise TRexError('AP with such MAC (%s) already exists!' % mac)
        if ip in self.ap_by_ip:
            raise TRexError('AP with such IP (%s) already exists!' % ip)
        ap = AP(self.ssl_ctx, self.trex_client.logger, self.trex_client.ports[trex_port_id], mac, ip, wlc_ip, verbose_level, rsa_ca_priv_file, rsa_priv_file, rsa_cert_file)
        self.ap_by_name[ap.name] = ap
        self.ap_by_mac[mac] = ap
        self.ap_by_ip[ip] = ap
        with self.bg_lock:
            self.aps.append(ap)
            self.service_ctx[trex_port_id]['synced'] = False


    def remove_ap(self, ap_id):
        ap = self._get_ap_by_id(ap_id)
        if ap.is_dtls_established:
            self.service_ctx[ap.port_id]['fg'].run([ServiceApShutdownDTLS(ap)])
        with self.bg_lock:
            for client in ap.clients:
                for key, val in dict(self.client_by_id).items():
                    if val == client:
                        del self.client_by_id[key]
                self.clients.remove(client)
            self.aps.remove(ap)
            self.service_ctx[ap.port_id]['synced'] = False
        del self.ap_by_name[ap.name]
        del self.ap_by_mac[ap.mac]
        del self.ap_by_ip[ap.ip]


    def remove_client(self, id):
        client = self._get_client_by_id(id)
        with self.bg_lock:
            self.clients.remove(client)
            client.ap.clients.remove(client)
            self.service_ctx[client.ap.port_id]['synced'] = False
            for key, val in dict(self.client_by_id).items():
                if val == client:
                    del self.client_by_id[key]


    @staticmethod
    def _get_ap_per_port(aps):
        ap_per_port = {}
        for ap in aps:
            if ap.port_id in ap_per_port:
                ap_per_port[ap.port_id].append(ap)
            else:
                ap_per_port[ap.port_id] = [ap]
        return ap_per_port


    def _compare_aps(self, good_aps, aps, err, show_success = True):
        if len(good_aps) != len(aps):
            self.trex_client.logger.post_cmd(False)
            bad_aps = set(aps) - set(good_aps)
            raise TRexError('Following AP(s) could not %s: %s' % (err, ', '.join(sorted([ap.name for ap in bad_aps], key = natural_sorted_key))))
        if show_success:
            self.trex_client.logger.post_cmd(True)

    '''
    ids is a list, each index can be either mac, ip or name
    '''
    def join_aps(self, ids = None):
        if not ids:
            aps = self.aps
        else:
            aps = [self._get_ap_by_id(id) for id in ids]
        if not aps:
            raise TRexError('No APs to join!')

        MAX_JOINS = 512
        if len(aps) > MAX_JOINS:
            raise TRexError('Can not join more than %s at once, please split the joins' % MAX_JOINS)

        # discover
        self.trex_client.logger.pre_cmd('Discovering WLC')
        for port_id, aps_of_port in self._get_ap_per_port(aps).items():
            self.service_ctx[port_id]['fg'].run([ServiceApDiscoverWLC(ap) for ap in aps_of_port])

        # check results
        good_aps = [ap for ap in aps if ap.ip_dst and ap.mac_dst]
        self._compare_aps(good_aps, aps, 'discover WLC')

        # establish DTLS
        self.trex_client.logger.pre_cmd('Establishing DTLS connection')
        for port_id, aps_of_port in self._get_ap_per_port(aps).items():
            self.service_ctx[port_id]['fg'].run([ServiceApEstablishDTLS(ap) for ap in aps_of_port])

        # check results
        good_aps = [ap for ap in aps if ap.is_dtls_established]
        self._compare_aps(good_aps, aps, 'establish DTLS session')


        # join ap
        self.trex_client.logger.pre_cmd('Join WLC and get SSID')
        for port_id, aps_of_port in self._get_ap_per_port(aps).items():
            self.service_ctx[port_id]['fg'].run([ServiceApJoinWLC(ap) for ap in aps_of_port])

        # check results
        good_aps = [ap for ap in aps if ap.SSID]
        self._compare_aps(good_aps, aps, 'join or get SSID', show_success = False)
        good_aps = [ap for ap in aps if ap.is_connected]
        self._compare_aps(good_aps, aps, 'get Keep-alive response')


    def create_client(self, mac, ip, ap_id):
        if ':' not in mac:
            mac = str2mac(mac)
        ap = self._get_ap_by_id(ap_id)
        if mac in self.client_by_id:
            raise TRexError('Client with such MAC (%s) already exists!' % mac)
        if ip in self.client_by_id:
            raise TRexError('Client with such IP (%s) already exists!' % ip)
        client = APClient(mac = mac, ip = ip, ap = ap)
        self.client_by_id[mac] = client
        self.client_by_id[ip] = client
        with self.bg_lock:
            client.ap.clients.append(client)
            self.clients.append(client)
            self.service_ctx[ap.port_id]['synced'] = False


    def join_clients(self, ids = None):
        if not ids:
            clients = self.clients
        else:
            clients = set([self._get_client_by_id(id) for id in ids])
        if not clients:
            raise TRexError('No Clients to join!')

        # Assoc clients
        batch_size = 1024
        self.trex_client.logger.pre_cmd('Associating clients')
        clients_per_ap_per_port = {}
        clients_count = 0
        for client in clients:
            clients_count += 1
            if client.ap.port_id not in clients_per_ap_per_port:
                clients_per_ap_per_port[client.ap.port_id] = {}
            if client.ap not in clients_per_ap_per_port[client.ap.port_id]:
                clients_per_ap_per_port[client.ap.port_id][client.ap] = [client]
            else:
                clients_per_ap_per_port[client.ap.port_id][client.ap].append(client)
            if clients_count >= batch_size:
                for port_id, clients_per_ap in clients_per_ap_per_port.items():
                    self.service_ctx[port_id]['fg'].run([ServiceApAddClients(ap, c) for ap, c in clients_per_ap.items()])
                clients_per_ap_per_port = {}
                clients_count = 0

        for port_id, clients_per_ap in clients_per_ap_per_port.items():
            self.service_ctx[port_id]['fg'].run([ServiceApAddClients(ap, c) for ap, c in clients_per_ap.items()])

        # check results
        no_assoc_clients = [client.ip for client in clients if not client.is_associated]
        if no_assoc_clients:
            self.trex_client.logger.post_cmd(False)
            raise TRexError('Following client(s) could not be associated: %s' % ', '.join(no_assoc_clients))
        no_resp_clients = [client.ip for client in clients if not client.seen_arp_reply]
        if no_resp_clients:
            self.trex_client.logger.post_cmd(False)
            raise TRexError('Following client(s) did not receive ARP response from WLC: %s' % ', '.join(no_resp_clients))
        self.trex_client.logger.post_cmd(True)


    def add_streams(self, client_id, streams):
        if isinstance(streams, STLStream):
            streams = [streams]
        client = self._get_client_by_id(client_id)
        streams = [client.ap.patch_stream(client, stream) for stream in streams]
        self.trex_client.add_streams(streams, [client.ap.port_id])

    def add_ap_streams(self, ap, streams):
        if isinstance(streams, STLStream):
            streams = [streams]
        streams = [ap.patch_ap_stream(stream) for stream in streams]
        self.trex_client.add_streams(streams, [ap.port_id])

    def add_profile(self, client_id, filename, **k):
        validate_type('filename', filename, basestring)
        profile = STLProfile.load(filename, **k)
        self.add_streams(client_id, profile.get_streams())


    def get_info(self):
        info_per_port = {}
        for ap in self.aps:
            ssid = ap.SSID.get(0)
            if type(ssid) is bytes:
                ssid = ssid.decode('ascii')
            if ap.port_id not in info_per_port:
                info_per_port[ap.port_id] = {
                    'bg_thread_alive': bool(self.service_ctx[ap.port_id]['bg'].is_running()),
                    'aps': {},
                    }
            info_per_port[ap.port_id]['aps'][ap.name] = {
                'mac': ap.mac,
                'ip': ap.ip,
                'dtls_established': ap.is_dtls_established,
                'is_connected': ap.is_connected,
                'ssid': ssid,
                'clients': [],
                }
            for client in ap.clients:
                info_per_port[ap.port_id]['aps'][ap.name]['clients'].append({
                    'mac': client.mac,
                    'ip': client.ip,
                    'is_associated': client.is_associated,
                    })
        return info_per_port


    def get_connected_aps(self):
        return [ap for ap in self.aps if ap.is_connected]


    def disconnect_aps(self, aps = None):
        if aps is None:
            aps = self.aps
        ap_per_port = self._get_ap_per_port(aps)
        for port_id, aps in ap_per_port.items():
            if aps:
                assert(port_id in self.service_ctx)
                self.service_ctx[port_id]['fg'].run([ServiceApShutdownDTLS(ap) for ap in aps])


    def close(self, ports = None):
        self.disable_proxy_mode(ignore_errors = True)
        if ports is None:
            ports = list(self.service_ctx.keys())
        else:
            ports = listify(ports)
        ap_per_port = self._get_ap_per_port([ap for ap in self.aps if ap.port_id in ports])

        for port_id in ports:
            if port_id not in self.service_ctx:
                raise TRexError('AP manager is not initialized on port %s!' % port_id)
            service = self.service_ctx[port_id]
            service['bg'].stop()
            aps = ap_per_port.get(port_id, [])
            if aps:
                service['fg'].run([ServiceApShutdownDTLS(ap) for ap in aps])

            for ap in aps:
                self.remove_ap(ap)

            del self.service_ctx[port_id]


    def _gen_ap_params(self):
        # mac
        while self.next_ap_mac in self.ap_by_mac:
            self.next_ap_mac = increase_mac(self.next_ap_mac)
            assert is_valid_mac(self.next_ap_mac)

        # ip
        while self.next_ap_ip in self.ap_by_ip:
            self.next_ap_ip = increase_ip(self.next_ap_ip)
            assert is_valid_ipv4(self.next_ap_ip)

        return self.next_ap_mac, self.next_ap_ip, self.wlc_ip


    def _gen_client_params(self):
        # mac
        while self.next_client_mac in self.client_by_id:
            self.next_client_mac = increase_mac(self.next_client_mac)
            assert is_valid_mac(self.next_client_mac)

        # ip
        while self.next_client_ip in self.client_by_id:
            self.next_client_ip = increase_ip(self.next_client_ip)
            assert is_valid_ipv4(self.next_client_ip)

        return self.next_client_mac, self.next_client_ip


    def log(self, msg):
        self.trex_client.logger.info(msg)

    def err(self, msg):
        self.trex_client.logger.error(msg)

    def set_base_values(self, mac = None, ip = None, client_mac = None, client_ip = None, wlc_ip = None, save = None, load = None):
        if load:
            if any([mac, ip, client_mac, client_ip, save]):
                raise TRexError('Can not use --load with other arguments.')
            if not os.path.exists(self.base_file_path):
                raise TRexError('No saved file.')
            try:
                self.trex_client.logger.pre_cmd('Loading base values')
                with open(self.base_file_path) as f:
                    base_values = yaml.safe_load(f.read())
                mac        = base_values['ap_mac']
                ip         = base_values['ap_ip']
                client_mac = base_values['client_mac']
                client_ip  = base_values['client_ip']
                if 'wlc_ip' in base_values:
                    wlc_ip = base_values['wlc_ip']
                else:
                    wlc_ip = '255.255.255.255'
            except Exception as e:
                self.trex_client.logger.post_cmd(False)
                raise TRexError('Parsing of config file %s failed, error: %s' % (self.base_file_path, e))
            self.trex_client.logger.post_cmd(True)

        # first pass, check arguments
        if mac:
            check_mac_addr(mac)
        if ip:
            check_ipv4_addr(ip)
        if client_mac:
            check_mac_addr(client_mac)
        if client_ip:
            check_ipv4_addr(client_ip)
        if wlc_ip:
            check_ipv4_addr(wlc_ip)

        # second pass, assign arguments
        if mac:
            self.next_ap_mac = mac
        if ip:
            self.next_ap_ip = ip
        if client_mac:
            self.next_client_mac = client_mac
        if client_ip:
            self.next_client_ip = client_ip
        if wlc_ip:
            self.wlc_ip = wlc_ip
        if save:
            self.trex_client.logger.pre_cmd('Saving base values')
            try:
                with open(self.base_file_path, 'w') as f:
                    f.write(yaml.dump({
                        'ap_mac':     self.next_ap_mac,
                        'ap_ip':      self.next_ap_ip,
                        'client_mac': self.next_client_mac,
                        'client_ip':  self.next_client_ip,
                        'wlc_ip':     self.wlc_ip,
                        }))
            except Exception as e:
                self.trex_client.logger.post_cmd(False)
                raise TRexError('Could not save config file %s, error: %s' % (self.base_file_path, e))
            self.trex_client.logger.post_cmd(True)


    @staticmethod
    def _set_proxy_mode(port, params):
        params.update({
            'handler': port.handler,
            'port_id': port.port_id,
            'type': 'capwap_proxy',
            })
        rc = port.transmit('set_rx_feature', params)
        if not rc:
            raise TRexError(rc.err())


    def enable_proxy_mode(self, wired_port, wireless_port, dest_mac = None, filter = False):
        assert wired_port in self.service_ctx, 'Specified wired port %s does not have any APs' % wired_port
        assert wireless_port in self.trex_client.ports, 'Specified wireless port %s is invalid' % wireless_port
        assert wireless_port not in self.service_ctx, 'Specified wireless port %s should not have any APs' % wireless_port
        clients = [c for c in self.clients if (c.ap.port_id == wired_port and c.ap.is_connected and c.is_associated)]
        assert clients, 'Specified wired port %s does not have any connected clients' % wired_port

        # wireless side
        port = self.trex_client.ports[wireless_port]
        assert port.is_acquired(), 'Port of "wireless" side is not acquired!'
        assert port.is_resolved(), 'Port destination is not resolved!'
        wireless_src_mac = mac2str(port.get_layer_cfg()['ether']['src'])
        wireless_dst_mac = mac2str(port.get_layer_cfg()['ether']['dst'])

        capwap_map = {}
        for client in clients:
            if dest_mac:
                dest_mac_bytes = mac2str(dest_mac)
            else:
                dest_mac_bytes = client.ap.mac_dst_bytes
            wlan_wrapping = client.ap.wrap_pkt_by_wlan(client, dest_mac_bytes + client.mac_bytes + b'\x08\x00')
            #Ether(wlan_wrapping).show2()
            capwap_map[client.ip] = base64encode(wlan_wrapping)

        if filter:
            wlc_ip_num = ipv4_str_to_num(self.aps[0].wlc_ip_bytes)
        else:
            wlc_ip_num = 0 # IP will not match and not be filtered

        params = {
            'enabled': True,
            'pair_port_id': wired_port,
            'is_wireless_side': True,
            'capwap_map': capwap_map,
            'wlc_ip': wlc_ip_num,
            }
        self.trex_client.logger.pre_cmd('Setting wireless side')
        try:
            self._set_proxy_mode(port, params)
        except:
            self.trex_client.logger.post_cmd(False)
            raise
        self.trex_client.logger.post_cmd(True)

        # wired side
        port = self.trex_client.ports[wired_port]
        capwap_map = {}
        for client in clients:
            assert client.ap.mac_dst
            ether_wrapping = wireless_dst_mac + wireless_src_mac + b'\x08\x00'
            #ether_wrapping.show2()
            capwap_map[client.ip] = base64encode(ether_wrapping)

        params = {
            'enabled': True,
            'pair_port_id': wireless_port,
            'is_wireless_side': False,
            'capwap_map': capwap_map,
            'wlc_ip': wlc_ip_num,
            }
        self.trex_client.logger.pre_cmd('Setting wired side')
        try:
            self._set_proxy_mode(port, params)
        except:
            self.trex_client.logger.post_cmd(False)
            raise
        self.trex_client.logger.post_cmd(True)



    def disable_proxy_mode(self, ports = None, ignore_errors = False):
        if ports is None:
            ports = list(self.trex_client.ports.keys())
        else:
            ports = listify(ports)
        for port_id in ports:
            try:
                assert port_id in self.trex_client.ports, 'Invalid port id: %s' % port_id
                params = {'enabled': False}
                self._set_proxy_mode(self.trex_client.ports[port_id], params)
            except:
                if not ignore_errors:
                    raise


    def get_proxy_stats(self, ports = None, decode_map = True):
        if ports is None:
            ports = list(self.trex_client.ports.keys())
        else:
            ports = listify(ports)
        per_port_info = {}
        for port_id in ports:
            port = self.trex_client.ports[port_id]
            port.sync()
            capwap_proxy_info = port.status['rx_info'].get('capwap_proxy')
            if capwap_proxy_info and decode_map and capwap_proxy_info['is_active']:
                capwap_map_base64 = capwap_proxy_info['capwap_map']
                if capwap_map_base64:
                    capwap_map = {}
                    del capwap_proxy_info['capwap_map']
                    for client_ip, wrapping in capwap_map_base64.items():
                        pkt = Ether(base64decode(wrapping))
                        pkt.hide_defaults()
                        capwap_map[client_ip] = pkt.command()
                    capwap_proxy_info['capwap_map'] = capwap_map
            per_port_info[port_id] = capwap_proxy_info
        return per_port_info


    def __del__(self):
        self.close()



