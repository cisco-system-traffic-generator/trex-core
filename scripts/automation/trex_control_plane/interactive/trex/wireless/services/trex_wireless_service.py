import simpy
import threading
import time
import abc


from .trex_wireless_service_event import *
from ..utils.utils import SynchronizedStore
from ..pubsub.broker import register_sub, deregister_sub ,subscribers
from ..pubsub.message import PubSubMessage
from ..pubsub.pubsub import Publisher

Interrupt = simpy.events.Interrupt

class WirelessService:
    """A Service that runs on a simulated wireless device (access point or client)."""

    FILTER = ""

    # a simpy.resources.resource.Resource for throttling the number of concurrent same services
    concurrent_resource = None

    def __init__(self, env, tx_conn, topics_to_subs, pubsub, done_event=None, max_concurrent=float('inf')):
        """Create a WirelessService.

        Args:
            env: simpy environment of this service
            tx_conn: connection (e.g. pipe end) for sending packets
                needs to have a thread safe 'send' method
            topics_to_subs: dict topics -> subscription (wrapped simpy events) used to send WirelessServiceEventInfo, for requesting simpy events to be triggered when a event is received
            done_event: event that will be awaken when the service completes for the device
            max_concurrent: maximum number of concurrent services of this type to be run
        """
        self.name = self.__class__.__name__
        self.env = env
        self.tx_conn = tx_conn
        self.rx_store = SynchronizedStore(env)

        self.done_event = done_event
        self.max_concurrent = max_concurrent
        # active is True if the service requires packet to be sent to it
        self.active = False

        # services may never return (e.g. background service)
        # however subservices may want to wait on successful run of the service
        # e.g. when a client gets associated, the association service may continue to check for deassociation packet
        # but triggers the supprocess that the association has been done
        self.service_stopped_event = simpy.events.Event(self.env)
        self.succeeded = False

        # service may have subservices
        self.subservices = []

        self.topics_to_subs = topics_to_subs

        # pubsub
        self.pubsub = pubsub
        self.publisher = Publisher(self.pubsub, prefix_topics=[self.name])
        self.publish = self.publisher.publish

        if not hasattr(self.__class__, "concurrent_resource") or not self.__class__.concurrent_resource:
            self.__class__._set_concurrent_resource(env, max_concurrent)

    ################### api ##########################

    def add_service_info(self, key, value):
        raise NotImplementedError

    def get_service_info(self, key):
        raise NotImplementedError

    def async_request_start(self, first_start=False, request_packets=True):
        """Request to start the service.

        If the number of running services is greater than self.max_concurrent, wait until this is not the case anymore.

        Args:
            first_start (bool): True if this is the first time this service is started, False otherwise
            request_packets (bool): True if the service requires reception of packets, Defaults to True
        """
        self.publish("request start")
        self.active = request_packets

        if self.done_event:
            self.done_event.clear()

        self.succeeded = False

        self.start_request = simpy.resources.resource.Request(
            type(self).concurrent_resource)

        return self.start_request

    def async_request_stop(self, done, success=False, delete=False):
        """Request to stop the service.

        The Service should be running when called.

        Args:
            done (bool): True if the service is done, that is, it does not need to receive packets until a call to 'async_request_start'
            success (bool): True if the service has succeeded (will not be launched again), False otherwise
            delete (bool): True if the service is to be deleted after this call, in this case the call should be returned (return async_request_stop(...))
        """
        self.publish("request stop")

        if done:
            self.active = False

        if self.done_event:
            self.done_event.set()

        release = simpy.resources.resource.Release(
            type(self).concurrent_resource, self.start_request)
        # del self.start_request
        self.succeeded = success

        self.service_stopped_event.succeed()
        # restart
        self.service_stopped_event = simpy.events.Event(self.env)

        return release

    def _run_until_interrupt(self):
        try:
            yield from self.run()
        except Interrupt:
            self.stop()

    def run(self):
        """Run the service, main function.

        To be implemented by subclasses.
        """
        raise NotImplementedError

    def stop(self, hard=False):
        """Cleanup method to execute after running the service.

        By default does nothing.
        """
        return

    def async_wait(self, time_sec):
        """Async wait for some time.

        Args:
            time_sec (int): number of seconds to wait
        """
        return self.env.timeout(time_sec)

    def async_wait_for_event(self, event, timeout_sec=None):
        """Async wait for an event to happen (WirelessServiceEvent).
        The event must happen in the same "context" as the device :
        a client cannot wait on an event of an AP that is not attached to this client.
        However a client can wait on events from its attached AP, and an AP can wait on its clients events.

        Args:
            event (WirelessServiceEvent): event to wait for
            timeout_sec (int): number of seconds to wait before timeout, default: no timeout

        Return:
            a simpy event to wait for
        """
        # create a subscription
        sub = WirelessServiceEventSubscription(event)
        # register subscription
        register_sub(self.topics_to_subs, sub)
        # and thats it
        if timeout_sec:
            # create an event that will be triggered when either the timer times out or the given event is triggered
            return simpy.events.AnyOf(event.env, [event.simpy_event, simpy.events.Timeout(event.env, timeout_sec)])
        return event.simpy_event


    def async_wait_for_any_events(self, events, wait_packet=False, timeout_sec=None):
        """Async wait for an event to happen (WirelessServiceEvent).
        The event must happen in the same "context" as the device :
        a client cannot wait on an event of an AP that is not attached to this client.
        However a client can wait on events from its attached AP, and an AP can wait on its clients events.

        Args:
            events (list): list of WirelessServiceEvent to wait for
            wait_packet (bool): flag set if function should return on new received packet
            timeout_sec (int): number of seconds to wait before timeout, default: no timeout
        Return:
            a simpy event to wait for
        """
        if not events:
            raise ValueError("should not wait on no events")
        env = events[0].env

        sim_events = []
        for event in events:
            sim_events.append(self.async_wait_for_event(event, timeout_sec=None))
        if timeout_sec:
            sim_events.append(simpy.events.Timeout(env, timeout_sec))
        if wait_packet:
            sim_events.append(self.rx_store.get(timeout_sec, 1))
            return simpy.events.AnyOf(self.env, sim_events)
        return simpy.events.AnyOf(self.env, sim_events)

    def async_recv_pkt(self, time_sec=None):
        """Async wait to receive one packet, wakes when either a packet is available or when timeout is done.
        Returns a list of one packet or None if timed out.

        Args:
            time_sec (int): timeout in seconds, defaults to None (no timeout).

        Return:
            a list of one packet or None
        """
        return self.rx_store.get(time_sec, 1)

    def async_recv_pkts(self, num_packets=None, time_sec=None):
        """Async wait to receive multiple packets, wakes when either 'num_packets' packets have been received or when timeout is done.

        Args:
            time_sec (int): timeout in seconds, defaults to None (no timeout).

        Return:
            a list of received packets or None
        """


        return self.rx_store.get(time_sec, num_packets)

    def raise_event(self, event):
        """Raise a WirelessServiceEvent.

        Args:
            event (WirelessServiceEvent): the event to raise
        """
        # publish on PubSub,
        # even though devices can only wait on attached devices' events,
        # remote processes e.g. WirelessManager should be able to listen for events on a WirelessDevice
        self.pubsub.publish(event.value, event.topics)

        # create a pubsubmessage
        pubsub_message = PubSubMessage(event.value, event.topics)

        # if others are waiting on this event, wake them and deregister
        subscriptions = subscribers(self.topics_to_subs, pubsub_message)
        for sub in subscriptions:
            sub.trigger()
            deregister_sub(self.topics_to_subs, sub)

    def send_pkt(self, pkt):
        """Send a packet.

        Args:
            pkt (bytes): the packet bytes to send
        """
        self.tx_conn.send(pkt)

    def async_launch_service(self, service):
        """Launch a 'subservice' from this service.
        Will return once the subservice 'stopped' (not returned).
        """
        if not service.active:
            # add service to the simulation
            self.env.process(service.run())
        if service.active and service.succeeded:
            # service already finished
            event = simpy.events.Event(self.env)
            event.succeed()
            return event
        # return the event that will be triggered when 'service' will 'stop'
        return service.async_wait_for_service()

    def async_wait_for_service(self):
        """Wait for this service to 'stop'. To be used by other services."""
        return self.service_stopped_event

    class Connection(abc.ABC):
        """Connection (e.g. multipocessing.Connection) wrapper for sending packets from a device.

        Should handle the capwap encapsulation in the case of a Client.
        """

        def send(self, pkt):
            """Send a packet via this connection.

            To be implemented by subclasses.
            """
            raise NotImplementedError

    ################### internal functions ##########################

    def _on_rx_pkt(self, pkt):
        """Put the packet to be used by the service.

        Args:
            pkt (bytes): packet bytes received for the device
        """
        self.rx_store.put(pkt)

    @classmethod
    def _set_concurrent_resource(cls, env, max_concurrent):
        """Set the shared resource describing the maximum number of 'cls' that can be run concurrently.

        Args:
            env: simpy environment attached to all 'cls'
            max_concurrent: number of maximum concurrent 'cls'
        """
        if hasattr(cls, "concurrent_resource") and cls.concurrent_resource:
            raise RuntimeError(
                "concurrent_resource for class cls is already set")
        else:
            cls.concurrent_resource = simpy.resources.resource.Resource(
                env, capacity=max_concurrent)


class SyncronizedConnection:
    """A thread safe (for writing) wrapper on Connection (pipe end).

    Allows one consumer and multiple procducer on the same end pipe.
    """

    def __init__(self, connection):
        self.connection = connection
        self.lock = threading.Lock()

    def send(self, obj):
        with self.lock:
            self.connection.send(obj)

    def recv(self):
        return self.connection.recv()
