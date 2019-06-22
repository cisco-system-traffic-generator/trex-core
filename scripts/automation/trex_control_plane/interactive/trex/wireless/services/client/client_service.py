import simpy
import threading
import time

from ..trex_wireless_device_service import WirelessDeviceService

class ClientService(WirelessDeviceService):
    """A ServiceClient is a WirelessService for a Client."""
    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'), **kw):
        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent, **kw)
        self.client = device
        self.ap = device.ap

    class Connection:
        """Connection (e.g. pipe end) wrapper for sending packets from a client attached to an AP.

        Handles the capwap data encapsulation when packets are sent.
        """
        def __init__(self, connection, client):
            """Construct a ClientConnection.

            Args:
                connection: a Connection (e.g. pipe end), that has a 'send' method.
                client: a APClient attached to an AP.
            """
            self.connection = connection
            self.client = client
        
        def send(self, pkt):
            encapsulated = self.client.ap.wrap_client_pkt(pkt)
            self.connection.send(encapsulated)

