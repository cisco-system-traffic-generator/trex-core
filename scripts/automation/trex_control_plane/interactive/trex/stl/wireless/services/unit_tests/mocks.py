"""
Mocks used for Service testing.
"""

import queue
import logging
import threading
import os
from wireless.trex_wireless_config import *

global config
config = load_config()

class _Connection():
    """Mock of a multiprocessing.Connection.
    Stores the sent messages in a queue.
    To push packets into _connection (so that _connection.recv() returns a packet), use _connection._push()
    """
    def __init__(self):
        self.tx_packets = queue.Queue()
        self.rx_packets = queue.Queue()
    
    def send(self, obj):
        self.tx_packets.put(obj)
    
    def recv(self):
        return self.rx_packets.get()

    def fileno(self):
        return 1

    # for testing, not mocking

    def _rx_empty(self):
        return self.rx_packets.qsize() == 0
    
    def _tx_empty(self):
        return self.tx_packets.qsize() == 0

    def _push(self, obj):
        """Push an object into the input queue of _connection so a call to _connection.recv() will return the packet."""
        self.rx_packets.put(obj)


class _Rx_store:
    """Mock of a simpy.Store."""
    def __init__(self):
        self.queue = queue.Queue()

    def get(self, time_sec, num_packets=1):
        """Waits for 'time_sec' seconds or until there are 'num_packets' available and return at most 'num_packets'.
        Mocked version retruns at most 'num_packets' without waiting."""
        if not num_packets:
            num_packets = 1

        out = []
        for i in range(num_packets):
            if not self.queue.empty():
                out.append(self.queue.get())
        return out

    def put(self, obj):
        """Put 'obj' into the _Rx_store."""
        self.queue.put(obj)


class _pipe():
    """Mock class for a pipe, stores the received and sent packet in a list for later checks."""
    def __init__(self):
        self.sent = []
        self.received = []

    def send(self, pkt):
        self.sent.append(pkt)

    def recv(self):
        self.received.append("dummy packet")

    def async_wait_for_pkt(self, time_sec=0, limit=1):
        return []
        

class _pubsub:
    """Mocks the PubSub module"""
    def _publish(self, value, topics):
        pass
    def publish(self, value, topics):
        pass

class _publisher:
    """Mocks the Publisher module"""
    def SubPublisher(self, name):
        return _publisher()

    def publish(self, msg, topics=None):
        pass


class _worker:
    """Mocks the logger of a WirelessWorker."""
    def __init__(self):
        self.logger = logging.getLogger()
        self.pkt_pipe = _pipe()
        self.services = {}
        self.event_store = queue.Queue()
        self.pubsub = _pubsub()
        self.publisher = _publisher()
        self.stl_services = {}
        self.services_lock = threading.Lock()
        global config
        self.config = config


class _env:
    """Mock of Simpy environment."""
    def __init__(self):
        pass
    
    def timeout(self, sec):
        return 0

    def process(self, p):
        return p
