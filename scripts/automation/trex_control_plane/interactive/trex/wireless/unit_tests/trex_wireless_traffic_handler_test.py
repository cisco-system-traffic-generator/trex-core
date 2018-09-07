import logging
import multiprocessing as mp
import queue
import sys
import threading
import time
import unittest
from unittest.mock import PropertyMock, patch

from wireless.pubsub.pubsub import PubSub
from wireless.trex_wireless_rpc_message import *
from wireless.trex_wireless_traffic_handler import *
from wireless.trex_wireless_traffic_handler_rpc import *


class _port:
    """Mock of the trex_stl_lib.trex_stl_port's Port class."""

    def __init__(self):
        pass

    def get_layer_cfg(self):
        return "port_layer_cfg"


class _zmq_socket:
    """ Mock of the ZMQ socket for packet transfers """

    def __init__(self, *args):
        self.out_queue = queue.Queue()  # queue that stores the packets sent

        # used for generating packets
        self.fetch_capture_packets_counter = 0

        self.handle = 0xffaa

    def send(self, pkt):
        self.out_queue.put(pkt)

    def recv(self):
        pass

    def recv_packets_example(self):
        """Returns 3 different packets for switching test"""
        pkt1 = b'\x03\x03\x03\x03\x03\x01'
        pkt2 = b'\x03\x03\x03\x03\x03\x02'
        pkt3 = b'\x03\x03\x03\x03\x03\x03'
        self.fetch_capture_packets_counter += 1
        if self.fetch_capture_packets_counter == 1:
            return pkt1
        elif self.fetch_capture_packets_counter == 2:
            return pkt2
        elif self.fetch_capture_packets_counter == 3:
            return pkt3
        else:
            return None

    def recv_packets_broadcast(self):
        """Returns 1 broadcast packet."""
        return b'\xff\xff\xff\xff\xff\xff'


def _cyclic_gen(l):
    """Infinite generator of elements from a list.
    e.g. _cyclic_gen([1,2]) will yield [1,2] * inf
    """
    length = len(l)
    i = 0
    while True:
        yield l[i % length]
        i += 1


class TrafficHandlerTest(unittest.TestCase):
    """Tests methods for the TrafficHandler class."""

    def setUp(self):
        self.num_workers = 3

        # connection for commands from manager to traffic handler
        self.manager_cmd_pipe, self.cmd_pipe = mp.Pipe()

        # connection for packets from/to workers
        worker_pipes = [mp.Pipe() for _ in range(self.num_workers)]
        # give one end to workers and the other end to the traffic handler
        self.worker_connections_workers, self.worker_connections_th = zip(
            *worker_pipes)

        self.th = TrafficHandler(
            self.cmd_pipe,
            server_ip='localhost',
            port_id=0,
            worker_connections=list(self.worker_connections_th),
            log_queue=queue.Queue(),
            log_level=logging.NOTSET,
            log_filter=None,
            pubsub=PubSub(),
        )

        self.th.zmq_socket = _zmq_socket()
        self.th.zmq_context = None

class TrafficHandlerRoutingTest(TrafficHandlerTest):
    """Tests routing methods for the TrafficHandler class."""

    def test_route_macs(self):
        """Test the 'route_macs' method.
        Check that macs and connection id are correctly stored in routing table.
        """
        macs = ["cc:cc:cc:cc:cc:%02x" % i for i in range(255)]
        conn_id_gen = _cyclic_gen(range(self.num_workers))
        mac_to_connection_id_map = {key: value for (
            key, value) in zip(macs, conn_id_gen)}

        self.th.route_macs(mac_to_connection_id_map)
        conn_id_gen = _cyclic_gen(range(self.num_workers))
        for mac in macs:
            mac_key = bytes.fromhex(mac.replace(
                ":", ""))  # mac string to bytes
            self.assertEqual(
                self.th.pkt_connection_id_by_mac[mac_key], next(conn_id_gen))

    def test_route_macs_bad_conn_id(self):
        """Test the 'route_macs' method.
        Try to store a route mac -> conn id where the connection id is not known to the traffic handler.
        It should raise a ValueError.
        """
        mac = "cc:cc:cc:cc:cc:aa"
        bad_conn_ids = [-1, self.num_workers]

        for bad_conn_id in bad_conn_ids:
            mac_to_connection_id_map = {
                mac: bad_conn_id,
            }
            with self.assertRaises(ValueError, msg="connection id %d does not exist in traffic handler, ValueError should be raised" % bad_conn_id):
                self.th.route_macs(mac_to_connection_id_map)

def _fake_select_updown(sockets, len, timeout):
    time.sleep(0.01)
    nb = 0
    for s in sockets:
        if s.fd:
            s.revents = 1
            nb = nb + 1
        else:
            s.revents = 0

    return nb

def _fake_select_downup(sockets, len, timeout):
    time.sleep(0.01)
    for s in sockets:
        s.revents = 0
    sockets[-1].revents = 1
    return 1


@patch('wireless.trex_wireless_traffic_handler.zmq.zmq_poll', _fake_select_updown)
class TrafficHandlerUpDownTest(TrafficHandlerTest):
    """Tests methods for the TrafficHandler up_down thread."""

    @patch('wireless.trex_wireless_traffic_handler.zmq.zmq_poll', _fake_select_updown)
    def setUp(self):
        super().setUp()
        self.th.init()
        self.th.zmq_socket = _zmq_socket()
        self.th.zmq_context = None
        self.up_down = threading.Thread(
            target=self.th._TrafficHandler__traffic, daemon=True)
        self.up_down.start()

    def tearDown(self):
        super().tearDown()
        # if thread is alive, send stop command
        if self.up_down.is_alive():
            self.th.is_stopped.set()
            self.up_down.join()

    def test_up_down_push_packets(self):
        """Test the up_down thread by sending packets via the worker pipes and expecting the traffic thread to push_packets on the zmq socket."""
        pkts = []
        for conn in self.worker_connections_workers:
            pkt = str(id(conn)).encode()
            pkts.append(pkt)
            conn.send(pkt)

        # wait for thread to push the packets:
        received_pkts = []
        for _ in range(len(pkts)):
            received_pkts.append(self.th.zmq_socket.out_queue.get())
        self.assertTrue(set(received_pkts) == set(pkts))


@patch('wireless.trex_wireless_traffic_handler.zmq.zmq_poll', _fake_select_downup)
class TrafficHandlerDownUpTest(TrafficHandlerTest):
    """Test methods for the TrafficHandler down_up thread."""

    @patch('wireless.trex_wireless_traffic_handler.zmq.zmq_poll', _fake_select_downup)
    def setUp(self):
        super().setUp()
        self.th.init()
        self.th.zmq_socket = _zmq_socket()
        self.th.zmq_context = None
        self.down_up = threading.Thread(
            target=self.th._TrafficHandler__traffic, daemon=True)

        # first inform the routing table
        mac_to_connection_id_map = {
            '03:03:03:03:03:01': 0,
            '03:03:03:03:03:02': 1,
            '03:03:03:03:03:03': 2,
        }
        self.th.route_macs(mac_to_connection_id_map)

    def tearDown(self):
        super().tearDown()
        # if thread is alive, send stop command
        if self.down_up.is_alive():
            self.th.is_stopped.set()
            self.down_up.join()

    def test_down_up_broadcast(self):
        """Test the correct packet switching function of the down_up thread when a broadcast packet is sent."""
        # start pulling the packets
        self.th.zmq_socket.recv = self.th.zmq_socket.recv_packets_broadcast
        self.down_up.start()

        conn1, conn2, conn3 = self.worker_connections_workers

        pkt1 = conn1.recv()
        pkt2 = conn2.recv()
        pkt3 = conn3.recv()
        self.assertEqual(pkt1[:6], b'\xff\xff\xff\xff\xff\xff')
        self.assertEqual(pkt2[:6], b'\xff\xff\xff\xff\xff\xff')
        self.assertEqual(pkt3[:6], b'\xff\xff\xff\xff\xff\xff')

    def test_down_up_switching(self):
        """Test the correct packet switching function of the down_up thread in usual condition."""

        # start pulling the packets
        self.th.zmq_socket.recv = self.th.zmq_socket.recv_packets_example
        self.down_up.start()

        # verify that the packets have arrived at the correct connections

        # all connections should receive the same number of packets
        conn1, conn2, conn3 = self.worker_connections_workers
        pkt1 = conn1.recv()
        pkt2 = conn2.recv()
        pkt3 = conn3.recv()
        self.assertEqual(pkt1[:6], b'\x03\x03\x03\x03\x03\x01')
        self.assertEqual(pkt2[:6], b'\x03\x03\x03\x03\x03\x02')
        self.assertEqual(pkt3[:6], b'\x03\x03\x03\x03\x03\x03')


class TrafficHandlerManagementTest(TrafficHandlerTest):
    """Test methods for the TrafficHandler management thread."""

    def setUp(self):
        super().setUp()
        self.th.init()
        self.th.zmq_socket = _zmq_socket()
        self.th.zmq_context = None
        self.management = threading.Thread(
            target=self.th._TrafficHandler__management, daemon=True)
        self.management.start()

    def tearDown(self):
        super().tearDown()
        # if thread is alive, send stop command
        if self.management.is_alive():
            self.manager_cmd_pipe.send(TrafficHandlerCall_stop())
            # wait for response
            self.manager_cmd_pipe.recv()
        self.management.join()

    def test_management_thread_route_macs(self):
        """Test the management thread by sending a remote call for 'route_macs' and expecting the result."""
        macs = ["cc:cc:cc:cc:cc:%02x" % i for i in range(255)]
        conn_id_gen = _cyclic_gen(range(self.num_workers))
        mac_to_connection_id_map = {key: value for (
            key, value) in zip(macs, conn_id_gen)}

        self.manager_cmd_pipe.send(
            TrafficHandlerCall_route_macs(mac_to_connection_id_map))
        resp = self.manager_cmd_pipe.recv()
        self.assertEqual(resp.code, RPCResponse.SUCCESS)

        conn_id_gen = _cyclic_gen(range(self.num_workers))
        for mac in macs:
            mac_key = bytes.fromhex(mac.replace(
                ":", ""))  # mac string to bytes
            self.assertEqual(
                self.th.pkt_connection_id_by_mac[mac_key], next(conn_id_gen))

    def test_management_thread_multiple_calls(self):
        """Test the management thread by send two remote calls before expecting responses."""
        self.manager_cmd_pipe.send(TrafficHandlerCall_route_macs({}))
        self.manager_cmd_pipe.send(TrafficHandlerCall_route_macs({}))

        resp = self.manager_cmd_pipe.recv()
        self.assertEqual(resp.code, RPCResponse.SUCCESS)

        resp = self.manager_cmd_pipe.recv()
        self.assertEqual(resp.code, RPCResponse.SUCCESS)

    # def test_management_bogus_call(self):
    #     """Test the management thread by sending a bogus remote call.
    #     The management should send back a RPCExceptionReport.
    #     """
    #     with patch('wireless.trex_wireless_traffic_handler_rpc.TrafficHandlerCall_get_port_layer_cfg.NAME', new_callable=PropertyMock) as mock_NAME:
    #         mock_NAME.return_value = None
    #         call = TrafficHandlerCall_get_port_layer_cfg()
    #         self.manager_cmd_pipe.send(call)
    #         resp = self.manager_cmd_pipe.recv()
    #         self.assertTrue(isinstance(resp, RPCExceptionReport))

    # def test_management_bad_call(self):
    #     """Test the management thread by sending a non remote_call.
    #     The management should send back a RPCExceptionReport.
    #     """
    #     not_a_call = object()
    #     self.manager_cmd_pipe.send(not_a_call)
    #     resp = self.manager_cmd_pipe.recv()
    #     self.assertTrue(isinstance(resp, RPCExceptionReport))
