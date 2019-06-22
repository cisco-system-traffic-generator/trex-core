from wireless.services.ap.ap_service import APService

from wireless.services.unit_tests.mocks import _Connection, _Rx_store, _worker, _env, _pipe, _pubsub
from trex_stl_ap_test import ApTestCase, ServiceAP_init

from wireless.trex_wireless_config import *
import os

global config
config = load_config()

import unittest
from unittest.mock import patch
import struct

@patch("wireless.trex_wireless_ap.AP.encrypt", lambda s, x: x)
@patch("wireless.trex_wireless_ap.AP.decrypt", lambda s, x: x)
class APServiceControlRoundTripTest(ApTestCase):
    """Tests for the control_round_trip method of a APService."""

    class APServiceEx(APService):
        """Test AP Service."""
        def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'), stop=False):
            super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)
            self.ap = device

            self.rx_store = _Rx_store()

    def setUp(self):
        super().setUp()
        self.tx_conn = _Connection()
        self.rx_store = _Rx_store()
        self.env = _env()

    def test_timeout(self):
        """Test the timeout possibility when calling control_round_trip, not receiving any packets."""
        service = APServiceControlRoundTripTest.APServiceEx(self.ap, self.env, tx_conn=self.tx_conn, topics_to_subs=[], done_event=None)
        pkt = b'101010'
        expected_response_type = 42
        gen = service.control_round_trip(capw_ctrl_pkt=pkt, expected_response_type=expected_response_type)
        success = None
        for item in gen:
            success = item
        self.assertFalse(success)

    @patch("scapy.contrib.capwap.CAPWAP_PKTS.parse_message_elements", lambda a, b, c, d: 0)
    def test_response(self):
        service = APServiceControlRoundTripTest.APServiceEx(self.ap, self.env, tx_conn=self.tx_conn, topics_to_subs=[], done_event=None)
        pkt = bytes(45) + b'\x2a' + bytes(10) # 0x2a = 42
                
        expected_response_type = 42
        gen = service.control_round_trip(capw_ctrl_pkt=pkt, expected_response_type=expected_response_type)
        next(gen)
        with self.assertRaises(StopIteration) as context:
            
            ret = gen.send([pkt])

        self.assertTrue(context.exception.value)

    @patch("scapy.contrib.capwap.CAPWAP_PKTS.parse_message_elements", lambda a, b, c, d: 0)
    def test_bad_response_type(self):
        service = APServiceControlRoundTripTest.APServiceEx(self.ap, self.env, tx_conn=self.tx_conn, topics_to_subs=[], done_event=None)
        pkt = bytes(45) + b'\x2a' + bytes(10) # 0x2a = 42
                
        expected_response_type = 43
        gen = service.control_round_trip(capw_ctrl_pkt=pkt, expected_response_type=expected_response_type)
        next(gen)
        with self.assertRaises(StopIteration) as context:
            ret = gen.send([pkt])

        self.assertFalse(context.exception.value)


if __name__ == "__main__":
    unittest.main()
