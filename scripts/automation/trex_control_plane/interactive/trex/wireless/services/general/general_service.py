import simpy

from ..trex_wireless_service import *

Interrupt = simpy.events.Interrupt

class GeneralService(WirelessService):
    """A Service that runs on a set of simulated wireless device (access points or clients)."""
    
    FILTER = ""
    
    # a simpy.resources.resource.Resource for throttling the number of concurrent same services
    concurrent_resource = None

    def __init__(self, devices, env, tx_conn, topics_to_subs, done_event=None):
        """Create a WirelessService.

        Args:
            devices (list): the WirelessDevices the service will run on, 
                either a list of APs or a list of Clients
            env: simpy environment of this service
            tx_conn: connection (e.g. pipe end) for sending packets
                needs to have a thread safe 'send' method
            topics_to_subs: dict topics -> subscription (wrapped simpy events) used to send WirelessServiceEventInfo, for requesting simpy events to be triggered when a event is received
            done_event: event that will be awaken when the service completes for the device
            max_concurrent: maximum number of concurrent services of this type to be run
        """
        if not devices:
            raise ValueError("cannot create a GeneralService with no devices")

        # A GeneralService should run only once, thus max_conccurent=1
        super().__init__(env, tx_conn, topics_to_subs, devices[0].pubsub, done_event, max_concurrent=1)

        self.devices = devices

        # construct map {device_mac -> device}
        self.devices_by_mac = {}
        for device in devices:
            if device.mac in self.devices_by_mac:
                raise ValueError("cannot launch two same services on same device")
            self.devices_by_mac[device.mac] = device

        for device in self.devices:
            if not self.name in device.services_info:
                device.services_info[self.name] = {}

        # service may have subservices
        self.subservices = []

        if not hasattr(self.__class__, "concurrent_resource") or not self.__class__.concurrent_resource:
            self.__class__._set_concurrent_resource(env, max_concurrent)

    ################### api ##########################

    def add_service_info(self, key, value):
        ...

    def get_service_info(self, key):
        ...


    class Connection:
        """Connection (e.g. pipe end) wrapper for sending packets from devices."""

        def __init__(self, connection):
            """Construct a Connection.

            Args:
                connection: a Connection (e.g. pipe end), that has a 'send' method
            """
            self.connection = connection
        
        def send(self, pkt):
            """Send a packet on to connection.
            Send the packet as is with no added layers.
            """
            self.connection.send(pkt)




