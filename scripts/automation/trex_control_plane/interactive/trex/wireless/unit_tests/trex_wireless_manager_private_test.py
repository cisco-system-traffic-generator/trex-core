import sys
import os
import unittest

from wireless.trex_wireless_manager import APMode
from wireless.trex_wireless_manager_private import *


class APInfoTest(unittest.TestCase):
    """Tests methods for the APInfo class."""

    def test_init_correct(self):
        """Test the __init__ method when parameters are correct."""
        # mocks of files
        rsa_ca_priv_file, rsa_priv_file, rsa_cert_file = range(3)

        ap = APInfo(port_id=1, ip="2.2.2.2", mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                    gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=rsa_ca_priv_file, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)

        self.assertEqual(ap.ip, '2.2.2.2')

    def test_init_no_mac(self):
        """Test the __init__ method when parameter 'mac' is None.
        Should raise an AttributeError.
        """
        # mocks of files
        rsa_ca_priv_file, rsa_priv_file, rsa_cert_file = range(3)

        with self.assertRaises(ValueError):
            ap = APInfo(port_id=1, ip="2.2.2.2", mac=None, radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                        gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=rsa_ca_priv_file, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)

    def test_init_no_ip(self):
        """Test the __init__ method when parameter 'ip' is None.
        Since the field is optional, it should pass.
        """
        # mocks of files
        rsa_ca_priv_file, rsa_priv_file, rsa_cert_file = range(3)

        ap = APInfo(port_id=1, ip=None, mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                    gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=rsa_ca_priv_file, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)
        self.assertEqual(ap.ip, None)

    def test_str(self):
        """Test the __str__ method."""
        # mocks of files
        rsa_ca_priv_file, rsa_priv_file, rsa_cert_file = range(3)

        ap = APInfo(port_id=1, ip="2.2.2.2", mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                    gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=rsa_ca_priv_file, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)

        self.assertEqual(str(ap), 'APbbbb.bbbb.bbbb')
        self.assertEqual(str(ap), ap.name)


class ClientInfoTest(unittest.TestCase):
    """Tests methods for the ClientInfo class."""

    def setUp(self):
        # mocks of files
        rsa_ca_priv_file, rsa_priv_file, rsa_cert_file = range(3)
        self.ap = APInfo(port_id=1, ip="2.2.2.2", mac="bb:bb:bb:bb:bb:bb", radio_mac="bb:bb:bb:bb:bb:00", udp_port=12345, wlc_ip='1.1.1.1',
                         gateway_ip='1.1.1.2', ap_mode=APMode.LOCAL, rsa_ca_priv_file=rsa_ca_priv_file, rsa_priv_file=rsa_priv_file, rsa_cert_file=rsa_cert_file)

    def test_init_correct(self):
        """Test the __init__ method when parameters are correct."""
        client = ClientInfo("cc:cc:cc:cc:cc:cc", ip="3.3.3.3", ap_info=self.ap)
        self.assertEqual(client.ip, "3.3.3.3")
        self.assertEqual(client.ip_bytes, b'\x03\x03\x03\x03')

    def test_init_no_mac(self):
        """Test the __init__ method when mandatory parameter 'mac' is None."""
        with self.assertRaises(ValueError):
            client = ClientInfo(None, ip="3.3.3.3", ap_info=self.ap)

    def test_init_no_ip(self):
        """Test the __init__ method when parameter 'ip' is None.
        Since the field is optional, it should pass.
        """
        client = ClientInfo("cc:cc:cc:cc:cc:cc", ip=None, ap_info=self.ap)
        self.assertEqual(client.ip, None)
        self.assertEqual(client.ip_bytes, None)

    def test_init_wrong_ap_type(self):
        """Test the __init__ method when mandatory parameter 'ap_info' is of wrnong type."""
        ap_wrong = object()
        with self.assertRaises(ValueError):
            client = ClientInfo("cc:cc:cc:cc:cc:cc",
                                ip="3.3.3.3", ap_info=ap_wrong)

    def test_str(self):
        """Test the __str__ method."""
        client = ClientInfo("cc:cc:cc:cc:cc:cc", ip="3.3.3.3", ap_info=self.ap)
        self.assertEqual(str(client), "Client cc:cc:cc:cc:cc:cc - 3.3.3.3")
        self.assertEqual(str(client), client.name)
