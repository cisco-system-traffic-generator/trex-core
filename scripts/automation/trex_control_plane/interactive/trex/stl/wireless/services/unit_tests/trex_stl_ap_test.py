import sys
import os
import unittest
import unittest.mock
from unittest.mock import patch
import time
import logging
from collections import namedtuple
import queue

sys.path.append(os.path.dirname(__file__) + os.pardir +
                os.sep + os.pardir + os.sep + os.pardir + "/examples/")
    
# from trex_stl_lib.api import *
from wireless.services import *
from wireless.trex_wireless_ap import *
from wireless.trex_wireless_client import *
from wireless.services.trex_stl_ap import *
from wireless.trex_wireless_config import *
from scapy.contrib.capwap import *
from trex_openssl import *
from wireless.services.unit_tests.mocks import _worker, _env, _pipe, _pubsub

def is_valid_discover_req(pkt):
    '''Check a packet is a discovery request packet and is valid.'''
    try:
        is_capwap_control = is_valid_capwap_control(pkt)
        is_discovery = pkt[42:43] == b'\0'
        return is_capwap_control and is_discovery
    except:
        return False


def is_valid_capwap_control(pkt):
    """Check if a packet is capwap control."""
    try:
        is_ipv4 = pkt[12:14] == b'\x08\x00'
        is_udp = pkt[23:24] == b'\x11'
        is_capwap_control = pkt[36:38] == b'\x14\x7e' or pkt[34:36] == b'\x14\x7e'
        return is_ipv4 and is_udp and is_capwap_control
    except:
        return False


def is_valid_arp(pkt):
    """Check if a packet is arp."""
    try:
        return pkt[12:14] == b'\x08\x06'
    except:
        return False

def control_header_type(pkt):
    """Return the control header type of a capwap packet."""
    try:
        capwap_bytes = pkt[42:]
        capwap_hlen = (struct.unpack('!B', capwap_bytes[1:2])[
            0] & 0b11111000) >> 1
        ctrl_header_type = struct.unpack(
            '!B', capwap_bytes[capwap_hlen + 3:capwap_hlen + 4])[0]
        return ctrl_header_type
    except:
        return None


def next_gen(gen):
    action = next(gen)
    val = None
    if type(action) is tuple and len(action) == 2:
        action, val = action
    return action, val


def ServiceAP_init(s, ap):
    '''Mock constructor of ServiceAP.'''
    s.name = "Discover"
    s.sent = []  # packet sent
    s.next = None  # when done, set to the next process
    s.ap = ap
    s.logger = ap.logger
    s.worker = _worker()
    s.env = _env()
    s.topics_to_subs = {}
    return None


def first_then(count, first, then):
    '''Return a function that return 'count' times 'first', then indefinitely 'then'.'''
    def f(s):
        if f.left > 0:
            f.left -= 1
            return first
        else:
            return then
    f.left = count
    return f


def false_then_true(count):
    '''Create a function that return 'count' times False, then indefinitely True.'''
    return first_then(count, False, True)

def mock_control_round_trip(count, first, then):
    def f(self, tx_pkt, expected_response_type, debug_msg=None):
        if f.left > 0:
            f.left -= 1
            yield first
            return
        else:
            yield then
            return
    f.left = count
    return f

class ApTestCase(unittest.TestCase):
    """Base class for AP FSM tests."""
    def setUp(self):
        """SetUP for any AP FSM test."""

        global config
        config = load_config()
        self._worker = _worker()  # mock worker
        self.ap = AP(worker=self._worker, ssl_ctx=SSL_Context(), port_layer_cfg=None, port_id=0,
                     mac="11:11:11:11:11:11", ip="9.9.9.9", port=5555, radio_mac="aa:bb:cc:dd:ee:00", wlc_ip="1.1.1.1", gateway_ip="2.2.2.2")
        self.ap.start_join_time = time.time()
        # Set WLC mac
        self.ap.wlc_mac_bytes = bytearray.fromhex('aabbccddeeff')


@patch("wireless.services.trex_stl_ap.ServiceAPDiscoverWLC.__init__", ServiceAP_init)
class ServiceAPDiscoverWLCTest(ApTestCase):

    def test_immediate_reply(self):
        '''Tests Discovery Service under best case scenario.

        Goes through all stages of the Discovery State Machine, mocking the reception of the packets.
        '''
        
        self.service = ServiceAPDiscoverWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # First action should be to schedule the AP Join
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'start_ap')

        # Second to Shutdown DLTS
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")

        # Send the discovery request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_discover_req(val))

        # Then wait for the response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending response
        self.ap.rx_responses[2] = 0

        # Now the Discovery should be done and DTLS launched
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(self.ap.state, APState.DTLS)
        self.assertEqual(val.__class__.__name__, "ServiceAPEstablishDTLS")

    def test_resolve_wlc(self):
        '''Tests Discovery Service under best case scenario when WLC's mac needs to be resolved.'''

        # Set IP and no MAC addresses for the WLC
        self.ap.wlc_ip = '9.9.9.9'
        self.ap.wlc_mac_bytes = None

        self.service = ServiceAPDiscoverWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # First action should be to schedule the AP Join overall FSM
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'start_ap')

        # First action should be to reset dtls
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")

        # Then send the arp
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_arp(val))

        # Set WLC mac
        self.ap.wlc_mac_bytes = bytearray.fromhex('aabbccddeeff')

        # Wait
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Then send the discovery request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_discover_req(val))

        # Then wait for the response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending response
        self.ap.rx_responses[2] = 0

        # Now the Discovery should be done and DTLS launched
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(self.ap.state, APState.DTLS)
        self.assertEqual(val.__class__.__name__, "ServiceAPEstablishDTLS")

    def test_no_reply(self):
        '''Tests Discovery Service when no response is sent at the first stage.

        The AP shoud send capwap_MaxRetransmit times the discovery packet before rollback the service.
        '''

        self.service = ServiceAPDiscoverWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # First action should be to request a spot
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'start_ap')

        # First action should be to reset dtls
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")

        # Shoud retransmit MaxRetransmit times :
        global config
        for _ in range(config.capwap.max_retransmit):
            # First action should be to send the discovery request
            action, val = next_gen(running_gen)
            self.assertEqual(action, 'put')
            # TODO test packet ?

            # Then wait for the response
            action, val = next_gen(running_gen)
            self.assertEqual(action[0], 'process_with_timeout')

        # Timeout: restart DTLS
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")
        # First action should be to send the discovery request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')


    def test_bad_reply(self):
        '''Tests Discovery Service when response is first wrong.

        The AP should send the discovery packet first, then receive the response and output an error since the response code is not correct.
        '''
        self.service = ServiceAPDiscoverWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # First action should be to request a spot
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'start_ap')

        # First action should be to reset dtls
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")

        # First action should be to send the discovery request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_discover_req(val))

        # Then wait for the response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending bad response
        self.ap.rx_responses[2] = 35

        # Error
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'err')

        # Should restart Discovery by attempting to shut down the DTLS session
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(val.__class__.__name__, "ServiceAPShutdownDTLS")

    def test_bad_state(self):
        '''Tests Discovery Service when AP's state is not Discover.

        The AP's Discovery FSM should stop, by raising a StopIteration as it is a generator.
        '''
        self.service = ServiceAPDiscoverWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # set AP in state
        self.ap.state = APState.CLOSING

        # First action should be to request a spot
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'start_ap')

        # should raise StopIteration as it is not in a correct state
        self.assertRaises(StopIteration, next, running_gen)


@patch("wireless.services.trex_stl_ap.ServiceAPEstablishDTLS.__init__", ServiceAP_init)
@patch("trex_openssl.libssl.SSL_clear", lambda x: None)
@patch("trex_openssl.libssl.SSL_set_info_callback", lambda x, y: None)
@patch("trex_openssl.libssl.DTLSv1_get_timeout", lambda x: ServiceAPEstablishDTLSTest.timeout)  # Timeout
@patch("trex_openssl.libssl.SSL_do_handshake", lambda x: 0)
class ServiceAPEstablishDTLSTest(ApTestCase):
    Timeout = namedtuple('timeout', ['tv_sec', 'tv_usec'])
    timeout = Timeout(0.1, 10)

    def setUp(self):
        super().setUp()
        self.ap.ip_dst = '9.1.1.1'
        self.ap.ip_dst_bytes = is_valid_ipv4_ret(self.ap.ip_dst)
        self.ap.mac_dst_bytes = bytearray.fromhex('aabbccddeeff')
        self.ap.state = APState.DTLS

    def test_bad_state(self):
        '''Tests EstablishWLC Service when AP's state is not DTLS.

        The FSM should not run (StopIteration).
        '''
        self.service = ServiceAPEstablishDTLS(self.ap)

        # set AP in state
        self.ap.state = APState.CLOSING

        running_gen = self.service.run(None)

        # should raise StopIteration
        self.assertRaises(StopIteration, next, running_gen)

    # Mock the DTLS session to seem always up
    @patch("wireless.trex_wireless_ap.AP.is_handshake_done_libssl", lambda s: True)
    def test_best_case(self):
        '''Tests EstablishWLC Service in best case scenario.

        In this case the DTLS session is mocked to be alive and up.
        The FSM should therefore not do anything, and pass the state of the ap to JOIN.
        '''

        self.service = ServiceAPEstablishDTLS(self.ap)

        running_gen = self.service.run(None)

        # Should proceed with JOIN
        out = next(running_gen)
        self.assertEqual(self.ap.state, APState.JOIN)
        self.assertIsNotNone(out)

    # Mock the DTLS session to seem always up
    @patch("wireless.trex_wireless_ap.AP.is_handshake_done_libssl", lambda s: False)
    # Mock the timer to appear timeout after the second check (wich avoids the retransmission)
    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(1))
    def test_no_handshake(self):
        '''Tests EstablishWLC Service when the handshake cannot be made.

        The FSM should wait for the packet and then time out and rollback.
        '''
        self.service = ServiceAPEstablishDTLS(self.ap)

        running_gen = self.service.run(_pipe())

        # Should yield wait for packet
        out = next(running_gen)
        self.assertEqual(out, _pipe().async_wait_for_pkt())
        self.assertEqual(self.ap.state, APState.DTLS)

        # Should rollback
        running_gen.send([])  # send no responses
        self.assertEqual(self.ap.state, APState.DISCOVER)

# named tuple for mocking conifg
Capwap_specific = namedtuple('specific', ['ssid_timeout'])
Capwap_config = namedtuple('capwap', ['specific','retransmit_interval','max_retransmit','echo_interval'])
Dtls_config = namedtuple('dtls', ['shutdown_max_retransmit','timeout'])
Openssl_config = namedtuple('openssl', ['buffer_size'])
Config = namedtuple('config', ['dtls', 'capwap', 'openssl'])

@patch("wireless.services.trex_stl_ap.ServiceAPJoinWLC.__init__", ServiceAP_init)
@patch("wireless.trex_wireless_ap.AP.encrypt", lambda s, x: x)
@patch("wireless.trex_wireless_ap.AP.decrypt", lambda s, x: x)
# all seq equals 0
@patch("wireless.trex_wireless_ap.AP.get_capwap_seq", lambda x: 0)
# Mock the DTLS session to always appear up
@patch("wireless.trex_wireless_ap.AP.is_dtls_established", lambda x: True)
@patch("random.getrandbits", lambda x: 0)  # no random
class ServiceAPJoinWLCTest(ApTestCase):

    def setUp(self):
        super().setUp()
        self.ap.ip_dst = '9.1.1.1'
        self.ap.ip_dst_bytes = is_valid_ipv4_ret(self.ap.ip_dst)
        self.ap.mac_dst_bytes = bytearray.fromhex('aabbccddeeff')
        self.ap.state = APState.JOIN

    def test_control_round_trip_best_case(self):
        '''Tests JoinWLC's sub service control_round_trip when response is sent immediately.'''

        self.service = ServiceAPJoinWLC(self.ap)
        pkt = b'101010'
        expected_response_type = 42
        running_gen = self.service.control_round_trip(
            tx_pkt=pkt, expected_response_type=expected_response_type)

        # Should send the wrapped packet
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Wait for packet
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Send response
        self.ap.rx_responses[expected_response_type] = 0

        action, val = next_gen(running_gen)
        self.assertEqual(action, "good_resp")

    def test_control_round_trip_bad_response(self):
        '''Tests JoinWLC's sub service control_round_trip when response is not correct.'''

        self.service = ServiceAPJoinWLC(self.ap)
        pkt = b'101010'
        expected_response_type = 42
        running_gen = self.service.control_round_trip(
            tx_pkt=pkt, expected_response_type=expected_response_type)

        # Should send the wrapped packet
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Wait for packet
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')


        # Send incorrect response
        self.ap.rx_responses[expected_response_type] = -42

        action, val = next_gen(running_gen)
        self.assertEqual(action, "err")

        # Generator should be done
        self.assertRaises(StopIteration, next, running_gen)

    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(4))
    def test_control_round_trip_broken_dtls(self):
        '''Tests JoinWLC's sub service control_round_trip when dtls breaks.'''

        self.service = ServiceAPJoinWLC(self.ap)
        self.ap.capwap_MaxRetransmit = 4

        pkt = b'101010'
        expected_response_type = 42
        running_gen = self.service.control_round_trip(
            tx_pkt=pkt, expected_response_type=expected_response_type)

        # Should send the wrapped packet
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Break DTLS
        self.ap.is_dtls_established = False
        self.assertFalse(self.ap.is_dtls_established)

        # Make sleep
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should break
        action, val = next_gen(running_gen)
        self.assertEqual(action, "dtls_broke")

    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(4))
    # set retransmissions to 2
    @patch("wireless.trex_wireless_config.config",
        Config(
            openssl=Openssl_config(buffer_size=1000),
            dtls=Dtls_config(shutdown_max_retransmit=1, timeout=1),
            capwap=Capwap_config(
                specific=Capwap_specific(ssid_timeout=1),
                retransmit_interval=1,
                max_retransmit=2, echo_interval=1
                )
            )
    )
    def test_control_round_trip_retransmition(self):
        '''Tests JoinWLC's sub service control_round_trip when response timeout.'''

        self.service = ServiceAPJoinWLC(self.ap)

        pkt = b'101010'
        expected_response_type = 42
        running_gen = self.service.control_round_trip(
            tx_pkt=pkt, expected_response_type=expected_response_type)

        # Should send the wrapped packet
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Make sleep
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should retransmit
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Make sleep
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')


        # Should timeout
        action, val = next_gen(running_gen)
        self.assertEqual(action, "time")

    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(4))
    def test_control_round_trip_timeout(self):
        '''Tests JoinWLC's sub service control_round_trip when response takes time to arrive.'''

        self.service = ServiceAPJoinWLC(self.ap)
        self.ap.capwap_MaxRetransmit = 2

        pkt = b'101010'
        expected_response_type = 42
        running_gen = self.service.control_round_trip(
            tx_pkt=pkt, expected_response_type=expected_response_type)

        # Should send the wrapped packet
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(pkt))
        self.assertEqual(val, expected)

        # Make sleep
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Send response
        self.ap.rx_responses[expected_response_type] = 0

        action, val = next_gen(running_gen)
        self.assertEqual(action, "good_resp")

    def test_bad_state(self):
        '''Tests JoinWLC Service when AP's state is not JOIN.'''
        self.service = ServiceAPJoinWLC(self.ap)

        # set AP in state
        self.ap.state = APState.CLOSING

        running_gen = self.service.run_with_buffer()

        # should raise StopIteration
        self.assertRaises(StopIteration, next, running_gen)

    def test_best_case(self):
        '''Tests JoinWLC Service in best case scenario.'''
        self.service = ServiceAPJoinWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # Should send Join Request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.join(self.ap)))
        self.assertEqual(expected, val)

        # Wait response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending good response
        self.ap.rx_responses[4] = 0  # Join Response

        # Should send Configuration Status Request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.conf_status_req(self.ap)))
        self.assertEqual(expected, val)

        # Wait response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending good response
        self.ap.rx_responses[6] = 0  # Configuration Status Request Response

        # Should send Change State Request for radio 0
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.change_state(self.ap, radio_id=0)))
        self.assertEqual(expected, val)

        # Wait response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending good response
        self.ap.rx_responses[12] = 0  # Change State Response

        # Should send Change State Request for radio 1
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.change_state(self.ap, radio_id=1)))
        self.assertEqual(expected, val)

        # Wait response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Sending good response
        self.ap.rx_responses[12] = 0  # Change State Response

        # Fake Config Updates
        self.ap.create_vap(ssid=b"TRex Test SSID", vap_id=1, slot_id=0)
        self.ap.last_recv_ts = time.time() - 5

        
        # Wait for SSID
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should send Keep Alive
        action, val = next_gen(running_gen)
        self.assertEqual(action, "put")
        expected = self.ap.wrap_capwap_pkt(
            CAPWAP_PKTS.keep_alive(self.ap), dst_port=5247)
        self.assertEqual(expected, val)

        # Wait response
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Send response
        self.ap.got_keep_alive = True

        # Should have finished Join
        action, val = next_gen(running_gen)
        self.assertEqual(action, "done_ap")

        action, val = next_gen(running_gen)
        self.assertEqual(action, "service")
        self.assertEqual(self.ap.state, APState.RUN)

    # avoid looping, set number of retries to 1
    @patch("wireless.trex_wireless_config.config",
        Config(
            openssl=Openssl_config(buffer_size=1000),
            dtls=Dtls_config(shutdown_max_retransmit=1, timeout=1),
            capwap=Capwap_config(
                specific=Capwap_specific(ssid_timeout=1),
                retransmit_interval=1,
                max_retransmit=1, echo_interval=1
                )
            )
    )
    def test_timeout(self):
        '''Tests JoinWLC Service in case of timeout.'''

        self.service = ServiceAPJoinWLC(self.ap)

        # set wlc mac
        self.ap.wlc_mac_bytes = b'\x22\x22\x22\x22\x22\x22'

        running_gen = self.service.run_with_buffer()

        # Should send Join Request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.join(self.ap)))
        self.assertEqual(expected, val)

        # Wait
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should rollback
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'service')
        self.assertEqual(self.ap.state, APState.DISCOVER)

    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(3))
    # set number of retries to 2
    @patch("wireless.trex_wireless_config.config",
        Config(
            openssl=Openssl_config(buffer_size=1000),
            dtls=Dtls_config(shutdown_max_retransmit=1, timeout=1),
            capwap=Capwap_config(
                specific=Capwap_specific(ssid_timeout=1),
                retransmit_interval=1,
                max_retransmit=2, echo_interval=1
                )
            )
    )
    def test_retransmition(self):
        '''Tests JoinWLC Service in case of need of retransmission.'''
        self.service = ServiceAPJoinWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # Should send Join Request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.join(self.ap)))
        self.assertEqual(expected, val)

        # Should wait for packet

        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should repeat once
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.join(self.ap)))
        self.assertEqual(expected, val)

        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # No response : should rollback (timer is expired)
        action, val = next_gen(running_gen)
        self.assertEqual(self.ap.state, APState.DISCOVER)

    def test_broken_dtls(self):
        '''Tests JoinWLC Service in case of broken dtls.'''
        self.service = ServiceAPJoinWLC(self.ap)

        # avoid looping
        self.ap.capwap_MaxRetransmit = 1
        self.ap.capwap_RetransmitInterval = 0

        running_gen = self.service.run_with_buffer()

        # Should send Join Request
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        self.assertTrue(is_valid_capwap_control(val))
        expected = self.ap.wrap_capwap_pkt(
            b'\1\0\0\0' + self.ap.encrypt(CAPWAP_PKTS.join(self.ap)))
        self.assertEqual(expected, val)


        # Wait for packet
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')


        # Break DTLS
        self.ap.is_dtls_established = False
        self.assertFalse(self.ap.is_dtls_established)

        # Should rollback
        action, val = next_gen(running_gen)
        self.assertEqual(action, "service")
        self.assertEqual(self.ap.state, APState.DISCOVER)

    # avoids the first steps (until config updates not included)
    @patch("wireless.services.trex_stl_ap.ServiceAPJoinWLC.control_round_trip", mock_control_round_trip(10, "good_resp", "time"))
    @patch("trex.utils.common.PassiveTimer.has_expired", false_then_true(3))
    # set number of retries to 1
    @patch("wireless.trex_wireless_config.config",
    Config(
        openssl=Openssl_config(buffer_size=1000),
        dtls=Dtls_config(shutdown_max_retransmit=1, timeout=1),
        capwap=Capwap_config(
            specific=Capwap_specific(ssid_timeout=1),
            retransmit_interval=1,
            max_retransmit=1, echo_interval=1
            )
        )
    )
    def test_keep_alive_no_keepalive(self):
        '''Tests JoinWLC Service in case of no keep alive received.'''

        self.service = ServiceAPJoinWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # Fake Config Updates
        self.ap.create_vap(ssid=b"TRex Test SSID", vap_id=1, slot_id=0)
        self.ap.last_recv_ts = time.time() - 5

        self.ap.got_keep_alive = False


        
        # Wait for SSID
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should send keep alive
        action, val = next_gen(running_gen)
        self.assertEqual(action, 'put')
        expected = self.ap.wrap_capwap_pkt(
            CAPWAP_PKTS.keep_alive(self.ap), dst_port=5247)
        self.assertEqual(expected, val)

        # Should wait
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should detect no SSID and rollback
        action, val = next_gen(running_gen)
        self.assertEqual(action, "service")
        self.assertEqual(self.ap.state, APState.DISCOVER)

    # avoids the first steps (until config updates not included)
    @patch("wireless.services.trex_stl_ap.ServiceAPJoinWLC.control_round_trip", mock_control_round_trip(10, "good_resp", "time"))
    def test_no_ssid(self):
        '''Tests JoinWLC Service in case of no received SSID.'''
        self.service = ServiceAPJoinWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        self.ap.capwap_MaxRetransmit = -1

        self.ap.SSID = {}

        # Wait for config update
        action, val = next_gen(running_gen)
        self.assertEqual(action[0], 'process_with_timeout')

        # Should detect no SSID and rollback
        action, val = next_gen(running_gen)
        self.assertEqual(action, "service")
        self.assertEqual(self.ap.state, APState.DISCOVER)


# Test Generator for timeout at different steps
def _create_timeout_test(step):
    # returns step times "good_resp" then "time"
    @patch("wireless.services.trex_stl_ap.ServiceAPJoinWLC.control_round_trip", mock_control_round_trip(step, "good_resp", "time"))
    @patch("wireless.services.trex_stl_ap.ServiceAPJoinWLC.__init__", ServiceAP_init)
    def test(self):
        self.service = ServiceAPJoinWLC(self.ap)

        running_gen = self.service.run_with_buffer()

        # Since the underlying control_round_trip never triggers a yield (always returning "good_resp") before a "time", the first yield correspond to a timeout (process)

        try:
            action, val = next_gen(running_gen)
            self.assertEqual(action, 'service')
            self.assertEqual(self.ap.state, APState.DISCOVER)
            self.assertIsNotNone(val)
        except StopIteration:
            self.fail(
                "Generator should not have finished yet, should have yieled process (timed out)")

    return test


def timeout_tests():
    # Create and set up the timeout tests
    for step in range(3):
        _test = _create_timeout_test(step=step)
        _test.__name__ = 'test-%d' % step
        setattr(ServiceAPJoinWLCTest, _test.__name__, _test)

# def create_


timeout_tests()


if __name__ == '__main__':
    unittest.main()
