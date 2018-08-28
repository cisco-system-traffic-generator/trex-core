from wireless.services.trex_wireless_device_service import WirelessDeviceService
from wireless.trex_wireless_device import WirelessDevice
from wireless.services.trex_wireless_service_event import WirelessServiceEvent
from wireless.services.unit_tests.mocks import _pipe, _Connection
from wireless.pubsub.pubsub import PubSub
from wireless.trex_wireless_worker import WirelessWorker
from wireless.trex_wireless_config import *
import os

global config
config = load_config()

import time
import queue
import logging
import unittest

class DummyDevice(WirelessDevice):
    def __init__(self, worker):
        super().__init__(worker, "Dummy", "aa:aa:aa:aa:aa:aa", "1.1.1.1")
        self.mac = "aa:aa:aa:aa:aa:aa"
    @property
    def attached_devices_macs(self):
        return [self.mac]

    def is_closed(self):
        return False
    def is_running(self):
        return True

class DummyServiceEvent(WirelessServiceEvent):
    """A Dummy Event."""
    def __init__(self, env, device, value):
        super().__init__(env, device, "TestService", "dummy_value")

class DoerService(WirelessDeviceService):

    def __init__(self, device, env, tx_conn, topics_to_subs):
        """Instanciate a Dummy WirelessService that reports Events for testing.
        
        Args:
            device: WirelessDevice that owns the service
            env: simpy environment
            tx_conn: Connection (e.g. pipe end with 'send' method) used to send packets.
        """

        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs)

    def run(self):
        """Run the DHCP Service for the device."""
        ev  = DummyServiceEvent(self.env, "aa:aa:aa:aa:aa:aa", "dummy_value")
        yield self.async_wait(0.1) # give processing back to other service
        # raise event
        self.raise_event(ev)
        return


class WaiterService(WirelessDeviceService):

    def __init__(self, device, env, tx_conn, topics_to_subs, stop_queue):
        """Instanciate a Dummy WirelessService that waits for Events to happen.
        
        Args:
            device: WirelessDevice that owns the service
            env: simpy environment
            tx_conn: Connection (e.g. pipe end with 'send' method) used to send packets.
        """

        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs)
        self.done = False
        self.stop_queue = stop_queue
        self.received_event = False

    def run(self):
        """Run the Service for the device."""
        ev  = DummyServiceEvent(self.env, "aa:aa:aa:aa:aa:aa", "dummy_value")
        # waiting for event
        yield self.async_wait_for_event(ev)
        self.received_event = True
        # stop 
        self.stop_queue.put(None)
        return
        


class WirelessServiceEventTest(unittest.TestCase):

    def setUp(self):
        cmd_pipe = _Connection()
        pkt_pipe = _Connection()
        log_queue = queue.Queue()
        global config
        self.config = config
        self.stop_queue = queue.Queue()
        self.pubsub = PubSub()
        self.pubsub.start()
        self.worker = WirelessWorker(0, cmd_pipe, pkt_pipe, 0, config, None, log_queue, logging.WARN, None, self.pubsub)
        self.worker.init()
        self.device = DummyDevice(self.worker)

    def tearDown(self):
        # stop PubSub
        self.pubsub.stop()

    def test_equality_events(self):
        ev1 = DummyServiceEvent(self.worker.env, "DUMMY", "val")
        ev2 = DummyServiceEvent(self.worker.env, "DUMMY", "val")
        self.assertTrue(ev1 == ev2)

    def test_event(self):
        """Test the raising and waiting of events."""
        waiter = WaiterService(self.device, self.worker.env, _Connection(), self.worker.topics_to_subs, self.stop_queue)
        doer = DoerService(self.device, self.worker.env, _Connection(), self.worker.topics_to_subs)
        services = [
            waiter,
            doer
        ]
        self.worker._WirelessWorker__sim(self.stop_queue, services)
        self.assertTrue(waiter.received_event)
