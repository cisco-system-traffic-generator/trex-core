import simpy
import threading
import time
import abc


from .trex_wireless_service_event import *
from .trex_wireless_service import *
from trex.common.services.trex_service_int import SynchronizedStore, PktRX
from ..pubsub.broker import register_sub, deregister_sub ,subscribers
from ..pubsub.message import PubSubMessage

Interrupt = simpy.events.Interrupt

class WirelessDeviceService(WirelessService):
    """A Service that runs on a simulated wireless device (access point or client)."""
    
    FILTER = ""
    
    # a simpy.resources.resource.Resource for throttling the number of concurrent same services
    concurrent_resource = None

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf')):
        """Create a WirelessService.

        Args:
            device: the WirelessDevice the service will run on, either an AP or a Client
            env: simpy environment of this service
            tx_conn: connection (e.g. pipe end) for sending packets
                needs to have a thread safe 'send' method
            topics_to_subs: dict topics -> subscription (wrapped simpy events) used to send WirelessServiceEventInfo, for requesting simpy events to be triggered when a event is received
            done_event: event that will be awaken when the service completes for the device
            max_concurrent: maximum number of concurrent services of this type to be run
        """

        super().__init__(env, tx_conn, topics_to_subs, device.pubsub, done_event, max_concurrent)
        self.device = device

        if not self.name in device.services_info:
            self.device.services_info[self.name] = {}

        # service may have subservices
        self.subservices = []

        # pubsub override publisher
        self.pubsub = self.device.pubsub
        self.publisher = self.device.publisher.SubPublisher(self.name)
        self.publish = self.publisher.publish

        if not hasattr(self.__class__, "concurrent_resource") or not self.__class__.concurrent_resource:
            self.__class__._set_concurrent_resource(env, max_concurrent)

    ################### api ##########################

    def add_service_info(self, key, value):
        """Store information about this run of service into the attached device for statistics or other uses.
        If the same information type is already present, overwrite the information.

        Args:
            key: identifier of the type of information
            value: the information itself
        """
        self.device.services_info[self.name][key] = value

    def get_service_info(self, key):
        """Retrieve the service information from the device.

        Args:
            key: identifier of the type of information
        """
        if key in self.device.services_info[self.name]:
            return self.device.services_info[self.name][key]
        else:
            raise ValueError('Information {} not present in service informations for service {}'.format(key, self.name))

    def async_request_start(self, first_start=False, request_packets=True):
        """Request to start the service.

        If the number of running services is greater than self.max_concurrent, wait until this is not the case anymore.

        Args:
            first_start (bool): True if this is the first time this service is started, False otherwise
            request_packets (bool): True if the service requires reception of packets, Defaults to True
        """
        self.add_service_info('done', False)
        if first_start:
            self.add_service_info('start_time', time.time())
        return super().async_request_start(first_start, request_packets)

    def async_request_stop(self, done, success=False, delete=False):
        """Request to stop the service.

        The Service should be running when called.

        Args:
            done (bool): True if the service is done, that is, it does not need to receive packets until a call to 'async_request_start'
            success (bool): True if the service has succeeded (will not be launched again), False otherwise
            delete (bool): True if the service is to be deleted after this call, in this case the call should be returned (return async_request_stop(...))
        """
        if success:
            self.add_service_info('done', True)
            self.add_service_info('stop_time', time.time())
            duration = self.get_service_info(
                'stop_time') - self.get_service_info('start_time')
            self.add_service_info('duration', duration)

        if delete:
            self.device.deregister_service(self)

        return super().async_request_stop(done, success, delete)
        
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
        # overriden for checking event locality:
        if event.device not in self.device.attached_devices_macs:
            print(event.device)
            print(self.device.attached_devices_macs)
            raise ValueError("a WirelessService cannot wait on an WirelessServiceEvent on a distant WirelessDevice")
        return super().async_wait_for_event(event, timeout_sec)