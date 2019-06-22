
import base64
import copy
import threading
import os
import ctypes
import struct
import queue
from collections import deque

from scapy.contrib.capwap import CAPWAP_PKTS, CAPWAP_DATA, CAPWAP_Header, CAPWAP_Wireless_Specific_Information_IEEE802_11, CAPWAP_Radio_MAC
from trex_openssl import *

from trex.common import *
from .services.trex_stl_ap import *
from .trex_wireless_ap_state import *
from .trex_wireless_client import *
from .trex_wireless_device import WirelessDevice
from trex.utils.parsing_opts import check_mac_addr, check_ipv4_addr
from .trex_wireless_config import *
from scapy.contrib.capwap import *


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
            raise Exception('Could not create SSL Context')

        try:
            bne = libcrypto.BN_new()
            libcrypto.BN_set_word(bne, SSL_CONST.RSA_F4)
            rsa = libcrypto.RSA_new()
            if libcrypto.RSA_generate_key_ex(rsa, 1024, bne, None) != 1:
                raise Exception('Could not generate RSA key in SSL Context')

            if libssl.SSL_CTX_use_RSAPrivateKey(self.ctx, rsa) != 1:
                raise Exception('Could not set RSA key into SSL Context')

            self.evp = libcrypto.EVP_PKEY_new()
            if libcrypto.EVP_PKEY_set1_RSA(self.evp, rsa) != 1:
                raise Exception('Could not create EVP_PKEY in SSL Context')

            libssl.SSL_CTX_set_options(
                self.ctx, SSL_CONST.SSL_OP_NO_TICKET)  # optimization

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

class AP(WirelessDevice):
    """An AP as seen by a WirelessWorker."""

    _scapy_cache_static = {}

    def __setattr__(self, name, value):
        if name in ("lock", "ssl_lock"):
            object.__setattr__(self, name, value)
            return
        super().__setattr__(name, value)

    def __init__(self, worker, ssl_ctx, port_layer_cfg, port_id, mac, ip, port, radio_mac, wlc_ip, gateway_ip, ap_mode='Local', rsa_ca_priv_file=None, rsa_priv_file=None, rsa_cert_file=None, ap_info=None):
        """Create an AP.

        Args:
            worker: attached worker of the AP
            ssl_ctx: ssl context, see SSL_Context
            port_layer_cfg: configuration of the Trex Port
            port_id: port id of the Trex Server that the AP will be attached to
            mac: mac address of the AP in string format
            ip: ipv4 address of the AP in string format, or None in case of DHCP
            port: udp port of the AP, used to generate traffic
            radio_mac: mac address of the radio of the AP, in string format
            wlc_ip: ip of the WLC, in string format
            rsa_ca_priv_file: rsa private key of WLC CA
            rsa_priv_file: rsa private key of AP (required if no rsa_ca_priv_file)
            rsa_cert_file: rsa certificate of AP (required if no rsa_ca_priv_file)
            ap_info (APInfo): original APInfo
        """
        name = 'AP%s%s.%s%s.%s%s' % (
            mac[:2], mac[3:5], mac[6:8], mac[9:11], mac[12:14], mac[15:17])
        super().__init__(worker, name, mac, gateway_ip, ap_info)
        # global config
        from .trex_wireless_config import config

        # final attributes
        self.name = name
        self.name_bytes = name.encode('ascii')
        self.ssl_ctx = ssl_ctx
        self.event_store = worker.event_store  # to be used to set events
        self.port_id = port_id
        self.port_layer_cfg = port_layer_cfg
        self.rsa_priv_file = rsa_priv_file
        self.rsa_cert_file = rsa_cert_file
        self.rsa_ca_priv_file = rsa_ca_priv_file
        self.serial_number = 'FCZ1853QQQ'
        self.country = 'CH '
        self.ap_mode = ap_mode
        self.reset_vars()

        # ip, mac
        check_mac_addr(mac)
        check_mac_addr(radio_mac)
        self.mac = mac
        if ip:
            check_ipv4_addr(ip)
            self.ip = ip
            self.ip_bytes = socket.inet_aton(self.ip)
            self.dhcp = False  # flag set in case of DHCP
        else:
            self.ip = None
            self.dhcp = True

        self.udp_port = port
        self.radio_mac = radio_mac
        self.udp_port_str = int2str(port, 2)
        self.mac_bytes = mac2str(mac)
        self.radio_mac_bytes = mac2str(radio_mac)

        # wlc
        self.wlc_ip = None
        self.wlc_ip_bytes = None
        self.wlc_mac = None
        self.wlc_mac_bytes = None
        if wlc_ip and wlc_ip != '255.255.255.255':
            check_ipv4_addr(wlc_ip)
            self.wlc_ip = wlc_ip
            self.wlc_ip_bytes = socket.inet_aton(wlc_ip)

        # ssl
        self.ssl = None
        self.ssl_lock = self.lock  # threading.RLock()
        self.in_bio = None
        self.out_bio = None
        self._create_ssl(config.openssl.buffer_size)

        self.last_echo_req_ts = 0
        self.retries = 0

        self.clients = []

        # services
        self.active_service = None  # for internal services e.g. Join or Discover

    def reset_vars(self):
        self.state = APState.INIT
        self.rx_buffer = deque(maxlen=100)
        self.client_responses = {}
        self.rx_responses = {}

        self.capwap_assemble = {}
        self.wlc_name = ''
        self.wlc_sw_ver = []

        self.echo_resp_timer = None
        self.echo_resp_retry = 0
        self.echo_resp_timeout = 0
        self.last_recv_ts = None

        self.expect_keep_alive_response = False

        self.SSID = {}

        self.session_id = None

        self.dot11_seq = 0
        self.__capwap_seq = 0
        self._scapy_cache = {}
        self.got_keep_alive = False
        self.got_disconnect = False

    def _wake_up(self):
        """Wake up AP for specific services (Join, DTLS...)."""
        try:
            self.event_store.put(self.active_service.waiting_on)
        except AttributeError:
            # no thread waiting
            pass

    @property
    def attached_devices_macs(self):
        return [self.mac] + [c.mac for c in self.clients]

    @property
    def is_closed(self):
        return self.state >= APState.CLOSING

    @property
    def is_running(self):
        return self.state == APState.RUN

    @property
    def is_connected(self):
        """Return True if the AP is joined to the WLC."""
        return self.state == APState.RUN

    def fatal(self, msg):
        """Log and Raise exception for fatal events."""
        self.logger.warn("Fatal: %s: %s" % (self.name, msg))
        raise Exception('%s: %s' % (self.name, msg))

    def __del__(self):
        if getattr(self, 'ssl', None) and libssl:
            with self.ssl_lock:
                libssl.SSL_free(self.ssl)

    def _create_ssl(self, buffer_size):
        with self.ssl_lock:
            if self.ssl:
                # Freeing if exists
                libssl.SSL_free(self.ssl)
                self.ssl = None

            self.ssl = libssl.SSL_new(self.ssl_ctx.ctx)
            self.openssl_buf = c_buffer(buffer_size)
            self.openssl_buf_size = buffer_size
            self.in_bio = libcrypto.BIO_new(libcrypto.BIO_s_mem())
            self.out_bio = libcrypto.BIO_new(libcrypto.BIO_s_mem())

            if self.rsa_ca_priv_file:
                rsa_ca = libcrypto.PEM_read_RSAPrivateKey_helper(
                    self.rsa_ca_priv_file)
                if not rsa_ca:
                    self.fatal(
                        'Could not load given controller trustpoint private key %s' % self.rsa_ca_priv_file)

                evp_ca = libcrypto.EVP_PKEY_new()
                if libcrypto.EVP_PKEY_set1_RSA(evp_ca, rsa_ca) != 1:
                    raise Exception(
                        'Could not create EVP_PKEY in SSL Context for controller key')

            elif not (self.rsa_priv_file and self.rsa_cert_file):
                self.logger.info(
                    "The AP Certificate will be self-signed. Newer version of eWLC do not support it.")
                evp_ca = self.ssl_ctx.evp

            if self.rsa_priv_file and self.rsa_cert_file:
                self.logger.debug('Using provided certificate')
                if libssl.SSL_use_certificate_file(self.ssl, c_buffer(self.rsa_cert_file), SSL_CONST.SSL_FILETYPE_PEM) != 1:
                    self.fatal('Could not load given certificate file %s' %
                               self.rsa_cert_file)
                if libssl.SSL_use_PrivateKey_file(self.ssl, c_buffer(self.rsa_priv_file), SSL_CONST.SSL_FILETYPE_PEM) != 1:
                    self.fatal('Could not load given private key %s' %
                               self.rsa_priv_file)
            else:
                x509_cert = None
                x509_name = None
                x509_subj = None
                try:
                    x509_cert = libcrypto.X509_new()
                    x509_name = libcrypto.X509_NAME_new()
                    x509_subj = libcrypto.X509_NAME_new()

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
                    # This is to force UTF8 string for CN, but PrintableString for the rest
                    libssl.ASN1_STRING_set_default_mask_asc(b"utf8only")
                    if libcrypto.X509_set_version(x509_cert, 2) != 1:
                        self.fatal('Could not set version of certificate')
                    if libcrypto.X509_set_pubkey(x509_cert, self.ssl_ctx.evp) != 1:
                        self.fatal(
                            'Could not assign public key to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'CN', SSL_CONST.MBSTRING_ASC, b'CA-vWLC/emailAddress=support@vwlc.com', -1, -1, 0) != 1:
                        self.fatal('Could not assign CN to certificate')
                    libssl.ASN1_STRING_set_default_mask_asc(b"default")

                    if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'O', SSL_CONST.MBSTRING_ASC, b'Cisco Virtual Wireless LAN Controller', -1, -1, 0) != 1:
                        self.fatal('Could not assign O to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'L', SSL_CONST.MBSTRING_ASC, b'San Jose', -1, -1, 0) != 1:
                        self.fatal('Could not assign L to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'ST', SSL_CONST.MBSTRING_ASC, b'California', -1, -1, 0) != 1:
                        self.fatal('Could not assign ST to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_name, b'C', SSL_CONST.MBSTRING_ASC, b'US', -1, -1, 0) != 1:
                        self.fatal('Could not assign C to certificate')

                    libssl.ASN1_STRING_set_default_mask_asc(b"utf8only")
                    if sys.version_info >= (3, 0):
                        cn = b'AP'+bytes(self.mac, 'utf-8')
                    else:
                        cn = b'AP'+bytes(self.mac)
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_subj, b'CN', SSL_CONST.MBSTRING_ASC, b'AP'+cn, -1, -1, 0) != 1:
                        self.fatal('Could not assign CN to certificate')
                    libssl.ASN1_STRING_set_default_mask_asc(b"default")
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_subj, b'O', SSL_CONST.MBSTRING_ASC, b'TRex', -1, -1, 0) != 1:
                        self.fatal('Could not assign O to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_subj, b'L', SSL_CONST.MBSTRING_ASC, b'San Jose', -1, -1, 0) != 1:
                        self.fatal('Could not assign L to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_subj, b'ST', SSL_CONST.MBSTRING_ASC, b'California', -1, -1, 0) != 1:
                        self.fatal('Could not assign ST to certificate')
                    if libcrypto.X509_NAME_add_entry_by_txt(x509_subj, b'C', SSL_CONST.MBSTRING_ASC, b'US', -1, -1, 0) != 1:
                        self.fatal('Could not assign C to certificate')

                    if libcrypto.X509_set_subject_name(x509_cert, x509_subj) != 1:
                        self.fatal('Could not set subject name to certificate')
                    if libcrypto.X509_set_issuer_name(x509_cert, x509_name) != 1:
                        self.fatal('Could not set issuer name to certificate')
                    if not libcrypto.X509_time_adj_ex(libcrypto.X509_getm_notBefore(x509_cert), -999, 0, None):
                        self.fatal(
                            'Could not set "Not before" time to certificate"')
                    if not libcrypto.X509_time_adj_ex(libcrypto.X509_getm_notAfter(x509_cert), 999, 0, None):
                        self.fatal(
                            'Could not set "Not after" time to certificate"')
                    if not libcrypto.X509_sign(x509_cert, evp_ca, libcrypto.EVP_sha256()):
                        self.fatal('Could not sign the certificate')
                    libssl.SSL_use_certificate(self.ssl, x509_cert)

                    # libcrypto.PEM_write_bio_X509_to_file(x509_cert, b"/Users/mamonney/out.pem")
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
        """Return a configuration update response (capwap packet, payload of udp) with given sequence number.

        Args:
            seq: the sequence number of the capwap control header
        """
        if 'config_update' in self._scapy_cache:
            self._scapy_cache['config_update'][20] = struct.pack('!B', seq)
        else:
            self._scapy_cache['config_update'] = CAPWAP_PKTS.config_update(
                self, seq)
        return self._scapy_cache['config_update']

    def get_echo_capwap(self):
        """"Return a Echo Request capwap control packet layer (payload of capwap)."""
        if 'echo_pkt' in self._scapy_cache:
            self._scapy_cache['echo_pkt'][20] = struct.pack(
                '!B', self.get_capwap_seq())
        else:
            self._scapy_cache['echo_pkt'] = CAPWAP_PKTS.echo(self)
        return self._scapy_cache['echo_pkt']

    def get_echo_wrap(self, encrypted):
        """"Return an ecrypted Echo Request capwap control packet.

        Args:
            encrypted: encrypted capwap echo
        """
        if 'echo_wrap' not in self._scapy_cache:
            self._scapy_cache['echo_wrap'] = bytes(
                self.wrap_capwap_pkt(b'\1\0\0\0' + encrypted))[:-len(encrypted)]
        return self._scapy_cache['echo_wrap'] + encrypted

    def wrap_capwap_pkt(self, capwap_bytes, is_discovery=False, dst_port=5246):
        """Return the packet encapsulated in CAPWAP.

        Args:
            capwap_bytes: packet to encapsulate, will become the capwap payload
            is_discovery: if the packet to encapsulate is a discovery packet
            dst_port: port of destination, should be capwap data or capwap control port
        """
        if isinstance(capwap_bytes, ctypes.Array):
            capwap_bytes = capwap_bytes.raw
        assert isinstance(capwap_bytes, bytes)
        if is_discovery:
            ip = b'\x45\x00' + struct.pack('!H', 28 + len(capwap_bytes)) + b'\x00\x01\x00\x00\x40\x11\0\0' + \
                self.ip_bytes + \
                (b'\xff\xff\xff\xff' if not self.wlc_ip_bytes else self.wlc_ip_bytes)
            checksum = scapy.utils.checksum(ip)
            ip = ip[:10] + struct.pack('!H', checksum) + ip[12:]
            return (
                (b'\xff\xff\xff\xff\xff\xff' if not self.wlc_mac_bytes else self.wlc_mac_bytes) + self.mac_bytes + b'\x08\x00' +
                ip +
                struct.pack('!H', self.udp_port) + struct.pack('!H', dst_port) + struct.pack('!H', 8 + len(capwap_bytes)) + b'\0\0' +
                capwap_bytes
            )
        if 'capwap_wrap' not in self._scapy_cache:
            self._scapy_cache['capwap_wrap_ether'] = self.wlc_mac_bytes + \
                self.mac_bytes + b'\x08\x00'
            # 2 bytes of total length after this one
            self._scapy_cache['capwap_wrap_ip1'] = b'\x45\x00'
            self._scapy_cache['capwap_wrap_ip2'] = b'\x00\x01\x00\x00\x40\x11\0\0' + \
                self.ip_bytes + self.wlc_ip_bytes
            self._scapy_cache['capwap_wrap_udp_src'] = struct.pack(
                '!H', self.udp_port)
            self._scapy_cache['capwap_wrap'] = True

        ip = self._scapy_cache['capwap_wrap_ip1'] + struct.pack(
            '!H', 28 + len(capwap_bytes)) + self._scapy_cache['capwap_wrap_ip2']
        checksum = scapy.utils.checksum(ip)
        ip = ip[:10] + struct.pack('!H', checksum) + ip[12:]
        udp = self._scapy_cache['capwap_wrap_udp_src'] + struct.pack(
            '!H', dst_port) + struct.pack('!H', 8 + len(capwap_bytes)) + b'\0\0'
        return self._scapy_cache['capwap_wrap_ether'] + ip + udp + capwap_bytes

    def wrap_capwap_data(self, pkt):
        """Encapsulate 'pkt' into a capwap data packet and return the packet."""
        self.dot11_seq += 1
        if self.dot11_seq > 0xfff:
            self.dot11_seq = 0
        if 'capwap_data_wrapping' not in AP._scapy_cache_static:
            p1 = bytes(
                CAPWAP_DATA(
                    header=CAPWAP_Header(
                        wbid=1,
                        flags='WT',
                        wireless_info_802=CAPWAP_Wireless_Specific_Information_IEEE802_11(
                            rssi=216,
                            snr=31,
                            data_rate=0,
                        )
                    )
                )
            )
            AP._scapy_cache_static['capwap_data_wrapping'] = p1

        p = AP._scapy_cache_static['capwap_data_wrapping']
        return self.wrap_capwap_pkt(p + pkt, dst_port=5247)

    def wrap_client_pkt(self, pkt):
        """Process the client packet, encapsulates it into CAPWAP packet and return it.

        Args:
            pkt: Dot11 packet from a wireless client
        """

        assert type(
            pkt) is bytes, 'wrap_client_pkt() expects bytes, got: %s' % type(pkt)
        assert len(pkt) >= 14, 'Too small buffer to wrap'

        self.dot11_seq += 1
        if self.dot11_seq > 0xfff:
            self.dot11_seq = 0

        if 'wlan_client_wrapping' not in AP._scapy_cache_static:
            p1 = bytes(
                CAPWAP_DATA(
                    header=CAPWAP_Header(
                        wbid=1,
                        flags='WT',
                        wireless_info_802=CAPWAP_Wireless_Specific_Information_IEEE802_11(
                            rssi=216,
                            snr=31,
                            data_rate=0,
                        )
                    )
                ) /
                Dot11_swapped(
                    FCfield='to-DS',
                    subtype=8,
                    type='Data',
                    ID=0,
                    addr1=self.radio_mac,
                    addr2=self.radio_mac,
                    addr3=self.radio_mac,
                    #SC = self.dot11_seq << 4
                ) /
                Dot11QoS() /
                LLC(dsap=170, ssap=170, ctrl=3) /
                SNAP())

            # capwap data header
            AP._scapy_cache_static['wlan_client_wrapping_1'] = p1[:16]
            # left of Dot11, Dot11QoS, LLC, SNAP exept Protocol ID (2bytes)
            AP._scapy_cache_static['wlan_client_wrapping_2'] = p1[38:-2]
            AP._scapy_cache_static['wlan_client_wrapping'] = True

        p = (
            AP._scapy_cache_static['wlan_client_wrapping_1'] +
            bytes(reversed(pkt[:2])) +  # reversed FC 2 bytes
            pkt[2:22] +  # Same Duration/ID, addr 1, addr 2 and addr 3
            AP._scapy_cache_static['wlan_client_wrapping_2'] +
            pkt[32:34]  # snap protocol ID
        )

        return self.wrap_capwap_pkt(p + pkt[34:], dst_port=5247)

    def wrap_client_ether_pkt(self, client, pkt):
        """Process the client packet, transforming ether header to Dot11, encapsulates it into CAPWAP packet and return it.

        Args:
            pkt: Ether packet from a client
        """
        assert type(
            pkt) is bytes, 'wrap_client_ether_pkt() expects bytes, got: %s' % type(pkt)
        assert len(pkt) >= 14, 'Too small buffer to wrap'
        self.dot11_seq += 1
        if self.dot11_seq > 0xfff:
            self.dot11_seq = 0

        if 'wlan_wrapping' not in AP._scapy_cache_static:
            p1 = bytes(
                CAPWAP_DATA(
                    header=CAPWAP_Header(
                        wbid=1,
                        flags='WT',
                        wireless_info_802=CAPWAP_Wireless_Specific_Information_IEEE802_11(
                            rssi=216,
                            snr=31,
                            data_rate=0,
                        )
                    )
                ) /
                Dot11_swapped(
                    FCfield='to-DS',
                    subtype=8,
                    type='Data',
                    ID=0,
                    addr1=self.radio_mac,
                    addr2=client.mac,
                    addr3=str2mac(pkt[:6]),
                    #SC = self.dot11_seq << 4
                ) /
                Dot11QoS() /
                LLC(dsap=170, ssap=170, ctrl=3) /
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

        return self.wrap_capwap_pkt(p + pkt[12:], dst_port=5247)

    def __wrap_pkt_by_ap_msg(self, pkt):
        assert type(
            pkt) is bytes, 'wrap_pkt_by_ap_msg() expects bytes, got: %s' % type(pkt)
        assert len(pkt) >= 14, 'Too small buffer to wrap'

        p1 = bytes(
            CAPWAP_DATA(
                header=CAPWAP_Header(
                    wbid=1,
                    flags='WT',
                    wireless_info_802=CAPWAP_Wireless_Specific_Information_IEEE802_11(
                        rssi=216,
                        snr=31,
                        data_rate=0,
                    )
                )
            ) /
            Dot11_swapped(
                FCfield='to-DS',
                subtype=8,
                type='Data',
                ID=0,
                addr1=self.mac,
                addr2=self.mac,
                addr3=self.mac,
            ) /
            Dot11QoS())

        return self.wrap_capwap_pkt(p1 + pkt, dst_port=5247)

    def patch_stream(self, client, stream):
        assert type(
            stream) is STLStream, 'patch_stream() expects STLStream, got: %s' % type(stream)
        stream = copy.deepcopy(stream)
        patched_pkt = Ether(stream.pkt)
        if stream.fields['packet']['meta']:
            pkt_meta = '%s\nPatched stream: Added WLAN' % stream.fields['packet']['meta']
        else:
            pkt_meta = 'Patched stream: Added WLAN'
        # port_layer = self.trex_port.get_layer_cfg()
        port_layer = self.port_layer_cfg
        if stream.fields['flags'] & 1 == 0:
            pkt_meta += ', Changed source'
            patched_pkt.src = port_layer['ether']['src']
        if stream.fields['flags'] & 0x110 == 0:
            pkt_meta += ', Changed destination'
            patched_pkt.dst = port_layer['ether']['dst']

        # Fix the IP layer so that the packets come from the client
        if IP in patched_pkt:
            patched_pkt[IP].src = client.ip
            pkt_meta += ', Changed source IP'

        stream.pkt = self.wrap_client_ether_pkt(client, bytes(patched_pkt))
        stream.fields['packet'] = {'binary': base64encode(stream.pkt),
                                   'meta': pkt_meta}

        # We force the changed src & dst MAC address to 1 using the stream flags
        stream.fields['flags'] |= 0b111
        for inst in stream.fields['vm']['instructions']:
            if 'pkt_offset' in inst:
                # Size of wrapping layers minus removed Ethernet
                inst['pkt_offset'] += 78
            elif 'offset' in inst:
                inst['offset'] += 78
        return stream

    def patch_ap_stream(self, stream):
        assert type(
            stream) is STLStream, 'patch_stream() expects STLStream, got: %s' % type(stream)
        stream = copy.deepcopy(stream)
        patched_pkt = Ether(stream.pkt)

        stream.pkt = self.__wrap_pkt_by_ap_msg(bytes(patched_pkt))
        stream.fields['packet'] = {'binary': base64encode(stream.pkt),
                                   'meta': ''}
        # We force the changed src & dst MAC address to 1 using the stream flags
        stream.fields['flags'] |= 0b011

        for inst in stream.fields['vm']['instructions']:
            if 'pkt_offset' in inst:
                inst['pkt_offset'] += 84  # Size of wrapping layers
            elif 'offset' in inst:
                inst['offset'] += 84
        return stream

    # override
    def setIPAddress(self, ip_int):
        super().setIPAddress(ip_int)

    def is_handshake_done_libssl(self):
        with self.ssl_lock:
            return bool(libssl.SSL_is_init_finished(self.ssl))

    def is_dtls_closed_libssl(self):
        with self.ssl_lock:
            return bool(libssl.SSL_get_shutdown(self.ssl))

    @property
    def is_dtls_established(self):
        """Return True if and only if the AP's DTLS session is established."""
        return self.is_handshake_done_libssl() and not self.is_dtls_closed_libssl()

    def ssl_read(self):
        with self.ssl_lock:
            ret = libcrypto.BIO_read(
                self.out_bio, self.openssl_buf, self.openssl_buf_size)
            if ret >= 0:
                return self.openssl_buf[:ret]
            ret = libcrypto.BIO_test_flags(
                self.out_bio, SSL_CONST.BIO_FLAGS_SHOULD_RETRY)
            if ret:
                return ''

    # without lock, careful
    def __ssl_write(self, buf):
        if isinstance(buf, ctypes.Array):
            ret = libcrypto.BIO_write(self.in_bio, buf, len(buf))
        else:
            ret = libcrypto.BIO_write(self.in_bio, c_buffer(buf), len(buf) + 1)
        if ret >= 0:
            return ret
        ret = libcrypto.BIO_test_flags(
            out_bio, SSL_CONST.BIO_FLAGS_SHOULD_RETRY)
        if ret:
            assert False
            return ''

    def encrypt(self, buf):
        with self.ssl_lock:
            if isinstance(buf, Packet):
                raise Exception(
                    'Consider converting to buffer: %s' % buf.command())
            if isinstance(buf, ctypes.Array):
                ret = libssl.SSL_write(self.ssl, buf, len(buf))
            else:
                ret = libssl.SSL_write(self.ssl, c_buffer(buf), len(buf))
            err = SSL_CONST.ssl_err.get(libcrypto.ERR_get_error(self.ssl, ret))
            if err and err != 'SSL_ERROR_NONE':
                self.fatal('Got SSL error: %s (ret %s)' % (err, ret))
            return self.ssl_read()

    def decrypt(self, buf):
        with self.ssl_lock:
            self.__ssl_write(buf)
            ret = libssl.SSL_read(
                self.ssl, self.openssl_buf, self.openssl_buf_size)
            err = SSL_CONST.ssl_err.get(
                libcrypto.ERR_get_error(self.ssl, ret))
            if err and err != 'SSL_ERROR_NONE':
                print(err)
                self.fatal('Got SSL error: %s' % (err, ret))
            return self.openssl_buf[:ret]

    def get_arp_pkt(self, op, src_mac_bytes, src_ip_bytes, dst_ip_bytes):
        """Construct an ARP who-has packet for the mac 'dst'_ip_bytes'.

        Args:
            op: op code in string format : 'who-has' or 'is-at' or 'garp'
            src_mc_bytes: source mac address in bytes
            src_ip_bytes: source ipv4 address in bytes
            dst_ip_bytes: destination ipv4 address in bytes
        """
        assert len(src_mac_bytes) == 6
        assert len(src_ip_bytes) == 4
        if op == 'who-has':
            arp_dst = b'\xff\xff\xff\xff\xff\xff' + dst_ip_bytes
        elif op == 'is-at':
            arp_dst = self.wlc_mac_bytes + dst_ip_bytes
        elif op == 'garp':
            arp_dst = b'\0\0\0\0\0\0' + src_ip_bytes
        else:
            raise Exception('Bad op of ARP: %s' % op)
        return (
            # Ethernet
            (b'\xff\xff\xff\xff\xff\xff' if op in ('who-has', 'garp') else dst_ip_bytes) + src_mac_bytes + b'\x08\x06' +
            b'\x00\x01\x08\x00\x06\x04' +
            (b'\x00\x01' if op in ('who-has', 'garp') else b'\x00\x02') +
            src_mac_bytes + src_ip_bytes + arp_dst  # ARP
        )

    def get_capwap_seq(self):
        """Return new capwap sequence number for crafting packets."""
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
