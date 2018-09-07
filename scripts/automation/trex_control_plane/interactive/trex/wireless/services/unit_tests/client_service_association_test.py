import sys
import os
import queue
import unittest
import unittest.mock
import time
import logging
from unittest.mock import patch
from collections import deque

from wireless.trex_wireless_ap import *
from wireless.trex_wireless_client import *
from wireless.services.client.client_service_association import *

from mocks import _Connection, _Rx_store, _worker, _env, _pubsub

from wireless.trex_wireless_config import *
global config
config = load_config()

@patch("wireless.services.client.client_service.ClientService.async_request_start", lambda x, first_start: None)
@patch("wireless.services.client.client_service.ClientService.async_request_stop", lambda x, done, success: None)
@patch("wireless.services.client.client_service.ClientService.async_wait_for_any_events", lambda x, events, wait_packet=None, timeout_sec=None: None)
class ClientServiceAssociation_test(unittest.TestCase):
    """Test methods for ClientServiceAssociation."""

    def setUp(self):
        self.worker = _worker()
        self.ap = AP(worker=self.worker, ssl_ctx=SSL_Context(), port_layer_cfg=None, port_id=0,
                     mac="aa:bb:cc:dd:ee:ff", ip="9.9.9.9", port=5555, radio_mac="aa:bb:cc:dd:ee:00", wlc_ip="1.1.1.1", gateway_ip="1.1.1.1")
        self.ap.create_vap(ssid=b"0", vap_id=1, slot_id=0)
        self.ap.wlc_mac_bytes = b'\xaa\xbb\xcc\xdd\xee\xff'
        self.client = APClient(
            self.worker, "aa:aa:aa:aa:aa:aa", '2.2.2.2', self.ap, gateway_ip="1.1.1.1")
        self.client.state = ClientState.ASSOCIATION
        self.client.dhcp = False
        self.env = _env()
        # mock of a multiprocessing.Connection with 'send' and 'recv" methods
        self.tx_conn = _Connection()
        self.rx_store = _Rx_store()

        self.resp_template = (
            Dot11_swapped(
                type='Management',
                FCfield='to-DS',
                ID=14849,
                addr1=self.client.mac,
                addr2="aa:aa:aa:aa:aa:aa",
                addr3="aa:aa:aa:aa:aa:aa",
                SC=256,
                addr4=None,
            ) /
            Dot11AssoResp(
                cap='res8+IBSS+CFP',
            ) /
            Dot11Elt(
                ID='Rates',
                info='\x82\x84\x8b\x96\x0c\x12\x18$',
            )
        )

    def test_timeout(self):
        """Test the timeout and exponential bachkoff of the Service.
        Should be slot_time^(nb_retries + 1) + uniform_rand(-1,1) with a maximum number of retries after wich the timeout does not increase.
        """

        service = ClientServiceAssociation(
            self.client, self.env, tx_conn=self.tx_conn, topics_to_subs=None, done_event=None)

        slot_time = service.SLOT_TIME
        max_retries = service.MAX_RETRIES

        # expected waiting times for the first timeouts
        expected_times = [pow(slot_time, i+1) for i in range(max_retries)]

        for i in range(max_retries):
            wait_time = service.wait_time
            expected = expected_times[i]
            self.assertTrue(wait_time >= expected -
                            1 and wait_time <= expected + 1)

            service.timeout()

        # after MAX_RETRIES have been done, the waiting time should be reset
        expected_times = [pow(slot_time, i+1) for i in range(max_retries)]

        for i in range(max_retries):
            wait_time = service.wait_time
            expected = expected_times[i]
            self.assertTrue(wait_time >= expected -
                            1 and wait_time <= expected + 1)

            service.timeout()

    @patch("wireless.services.client.client_service.ClientService.raise_event", lambda x, y: None)
    def test_with_response(self):
        """Test the service when the client receives an association response (best case)."""
        WLAN_ASSOC_RESP = b'\x00\x10'

        service = ClientServiceAssociation(
            self.client, self.env, tx_conn=self.tx_conn, topics_to_subs=queue.Queue(), done_event=None)
        service.rx_store = self.rx_store
        # service.raise_event = lambda x: None
        gen = service.run()

        # request start
        got = next(gen)
        self.assertTrue(got is None)  # mocked

        got = next(gen)

        # wait for response
        # should have sent an assoc request just before
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)

        gen.send([WLAN_ASSOC_RESP])  # mock packet

        # done
        self.assertEqual(self.client.state, ClientState.IP_LEARN)  # mocked

        # now send the arp
        # send dummy packet, setting received arp response
        self.client.seen_arp_reply = True
        gen.send([b'\x00\x10'])

        # wait for response
        got = next(gen)
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 3)

    def test_no_response(self):
        """Test the service when the client receives no association response."""
        WLAN_ASSOC_RESP = b'\x00\x10'

        service = ClientServiceAssociation(
            self.client, self.env, tx_conn=self.tx_conn, topics_to_subs=None, done_event=None)
        service.rx_store = self.rx_store
        gen = service.run()

        # request start
        got = next(gen)
        self.assertTrue(got is None)  # mocked

        # wait for response
        got = next(gen)
        # should have sent an assoc request just before
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)

        # wait for response again
        got = next(gen)
        # should have sent an assoc request just before
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 2)


if __name__ == "__main__":
    unittest.main()
