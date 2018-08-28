import unittest
from wireless.services.trex_wireless_service import WirelessService
from wireless.utils.test_utils import *
from inspect import signature


class WirelessServiceAPI_test(unittest.TestCase):
    """API check for WirelessServices.
    These tests will fail if the API has changed and is not common anymore.
    """

    def test_api_signature(self):
        """Test that all methods exists and have correct signature in WirelessService."""

        sigs = {
            "__init__": "(self, name, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'))",
            "async_request_start": "(self, first_start=False)",
            "async_request_stop": "(self, done, success=False)",
            "run": "(self)",
            "add_service_info": "(self, key, value)",
            "get_service_info": "(self, key)",
            "async_wait": "(self, time_sec)",
            "raise_event": "(self, event)",
            "async_wait_for_event": "(self, event, timeout_sec=None)",
            "async_recv_pkt": "(self, time_sec=None)",
            "async_recv_pkts": "(self, num_packets=None, time_sec=None)",
            "send_pkt": "(self, pkt)",
            "async_launch_service": "(self, service)",
            "async_wait_for_service": "(self)",
        }

        success, error = check_class_methods_signatures(WirelessService, sigs)
        if not success:
            self.fail(error)

