import sys
import os
import unittest
import unittest.mock
from unittest.mock import patch
import time
import logging

# from trex_stl_lib.api import *
from wireless.trex_wireless_manager import WirelessManager


class WLCManagerTest(unittest.TestCase):
    """Tests methods for the WirelessManager class."""

    def setUp(self):
        # instanciate dummy manager
        self.manager = WirelessManager()

    # Test disabled as filter is not used for now.
    # def test_load_client_service_dhcp(self):
    #     """Test the load_client_service method of WirelessManager.

    #     It should load the existing service correctly (no Exceptions) and save its filter in self.client_filters.
    #     """
    #     self.assertTrue(True)
    #     try:
    #         self.manager.load_client_service(
    #             service_class="wireless.services.client.client_service_dhcp.ClientServiceDHCP")
    #     except Exception as e:
    #         self.fail(
    #             "Loading an existing Client Service on WirelessManager should not raise any Exception, got : {}".format(e))

    #     from wireless.services.client.client_service_dhcp import ClientServiceDHCP
    #     self.assertIn(ClientServiceDHCP.FILTER, self.manager.client_filters)

    # def test_load_ap_service_dhcp(self):
    #     """Test the load_client_service method of WirelessManager.

    #     It should load the existing service correctly (no Exceptions) and save its filter in self.client_filters.
    #     """
    #     self.assertTrue(True)
    #     try:
    #         self.manager.load_client_service(
    #             service_class="wireless.services.ap.ap_service_dhc.APServiceDHCP")
    #     except Exception as e:
    #         self.fail(
    #             "Loading an existing AP Service on WirelessManager should not raise any Exception, got : {}".format(e))

    def test_set_base_values(self):
        """Test the 'set_base_values' method of WirelessManager."""
        class TestCase:
            def __init__(self, exc=False, ap_mac=None, ap_ip=None, ap_udp=None, ap_radio=None, client_mac=None, client_ip=None):
                """Construct a TestCase for 'set_base_values' method, 'exc' specifies if the test should raise an exception."""
                self.kwargs = {
                    "ap_mac": ap_mac,
                    "ap_ip": ap_ip,
                    "ap_udp": ap_udp,
                    "ap_radio": ap_radio,
                    "client_mac": client_mac,
                    "client_ip": client_ip
                }
                self.exc = exc

        tt = {
            "no values": TestCase(),
            "wrong ap_mac (aaaaaaaaaaaa)": TestCase(exc=True, ap_mac="aaaaaaaaaaaa"),
            "good ap_mac (ff:ff:ff:ff:ff:ff)": TestCase(ap_mac="ff:ff:ff:ff:ff:ff"),
            "bad ap_ip (300.0.0.0)": TestCase(exc=True, ap_ip="300.0.0.0"),
            "bad ap_ip (-1)": TestCase(exc=True, ap_ip=-1),
        }

        for name, tc in tt.items():            
            if tc.exc:
                with self.assertRaises(Exception):
                    self.manager.set_base_values(**tc.kwargs)
            else:
                self.manager.set_base_values(**tc.kwargs)
                # check values
                for key in tc.kwargs.keys():
                    if tc.kwargs[key] is not None:
                        # example : test if set ap_mac == self.manager.next_ap_mac
                        self.assertEqual(tc.kwargs[key], getattr(self.manager, 'next_' + key), "{}: {} was not set".format(name, key))

    def test_create_aps_params(self):
        """Test the 'create_aps_params' method of WirelessManager."""
        class TestCase:
            def __init__(self, num_aps, expected=(), exc=False, ap_mac=None, ap_ip=None, ap_udp=None, ap_radio=None, client_mac=None, client_ip=None):
                """Construct a TestCase for 'create_aps_params' method, 'exc' specifies if the test should raise an exception."""
                self.kwargs = {
                    "ap_mac": ap_mac,
                    "ap_ip": ap_ip,
                    "ap_udp": ap_udp,
                    "ap_radio": ap_radio,
                    "client_mac": client_mac,
                    "client_ip": client_ip
                }
                self.num_aps = num_aps
                self.expected = expected
                self.exc = exc
        tt = {
            "num_aps: -1": TestCase(-1, exc=True,
                ap_mac="aa:aa:aa:aa:aa:aa", ap_ip="1.1.1.1", ap_udp=1001, ap_radio="aa:aa:aa:aa:aa:00"),
            "num_aps: 0": TestCase(0, expected=([], [], [], []),
                ap_mac="aa:aa:aa:aa:aa:aa", ap_ip="1.1.1.1", ap_udp=1001, ap_radio="aa:aa:aa:aa:aa:00"),
            "num_aps: 1": TestCase(1, expected=(["aa:aa:aa:aa:aa:aa"], ["1.1.1.1"], [1001], ["aa:aa:aa:aa:aa:00"]),
                ap_mac="aa:aa:aa:aa:aa:aa", ap_ip="1.1.1.1", ap_udp=1001, ap_radio="aa:aa:aa:aa:aa:00"),
            "num_aps: 2": TestCase(2, expected=(["aa:aa:aa:aa:aa:a1", "aa:aa:aa:aa:aa:a2"], ["1.1.1.1", "1.1.1.2"], [1001,1002], ["aa:aa:aa:aa:aa:00", "aa:aa:aa:aa:ab:00"]),
                ap_mac="aa:aa:aa:aa:aa:a1", ap_ip="1.1.1.1", ap_udp=1001, ap_radio="aa:aa:aa:aa:aa:00"),
        }

        for name, tc in tt.items():      
            # set base values
            self.manager.set_base_values(**tc.kwargs)
            if tc.exc:
                with self.assertRaises(Exception):
                    ap_params = self.manager.create_aps_params(tc.num_aps)
            else:
                ap_params = self.manager.create_aps_params(tc.num_aps)
                # check if ap_params is expected
                self.assertEqual(ap_params, tc.expected, "wrong ap parameters, expected: {}, got: {}".format(tc.expected, ap_params))


    # def test_add_streams(self):
    #     """Test the 'add_streams' method of WirelessManager, basic functionality."""
        
