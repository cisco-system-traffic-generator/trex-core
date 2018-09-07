from .client_service import *
from scapy.contrib.capwap import *

import time
import random

class ClientServiceARP(ClientService):

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'), stop=False):
        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)
        client = device
        self.__arp = client.ap.get_arp_pkt('who-has', client.mac_bytes, client.ip_bytes, client.gateway_ip_bytes)
        self.__garp = client.ap.get_arp_pkt('garp', client.mac_bytes, client.ip_bytes, client.gateway_ip_bytes)

        # configuration
        configuration = client.config.client.association

        self.SLOT_TIME = configuration.slot_time
        self.MAX_RETRIES = configuration.max_retries
        self.retries = 0

        if not ClientServiceARP.concurrent_resource:
            ClientServiceARP._set_concurrent_resource(env, max_concurrent)

    @staticmethod
    def _set_concurrent_resource(env, max_concurrent):
        if hasattr(ClientServiceARP, "concurrent_resource") and ClientServiceARP.concurrent_resource:
            raise RuntimeError("concurrent_resource for class ClientServiceARP is already set")
        else:
            ClientServiceARP.concurrent_resource = simpy.resources.resource.Resource(env, capacity=max_concurrent)

    @property
    def wait_time(self):
        # exponential backoff with randomized +/- 1
        return pow(self.SLOT_TIME, self.retries + 1) + random.uniform(-1, 1)

    def run(self):

        self.retries = 0
    
        client = self.client

        client.logger.info("ClientServiceARP set to start")
        yield self.async_request_start(first_start=True)
        client.logger.info("ClientServiceARP started")

        retries = 0
        while True:
            client.logger.info("ClientServiceARP sending ARP who-has and garp")
            self.send_pkt(self.__arp)
            self.send_pkt(self.__garp)

            yield self.async_recv_pkt(self.wait_time)

        
            if client.seen_arp_reply:
                client.seen_arp_reply = False
                client.logger.info("ClientServiceARP received arp reply")
                client.logger.info("ClientServiceARP finished successfully")
                yield self.async_request_stop(done=True, success=True)
                return

            if retries > 3:
                client.logger.info("ClientServiceARP stopped")
                yield self.async_request_stop(done=True, success=False)
                return
            retries += 1


    # Overriding Connection of ClientService because capwap encapsulation is slightly different for the association process
    class Connection:
        """Connection (e.g. pipe end) wrapper for sending packets from a client attached to an AP.

        Handles the capwap data encapsulation when packets are sent for the association process.
        """
        def __init__(self, connection, client):
            """Construct a ClientConnection.

            Args:
            connection -- a Connection (e.g. pipe end), that has a 'send' method.
            client -- a AP_C_Client attached to an AP.
            """
            self.connection = connection
            self.client = client
        
        def send(self, pkt):
            encapsulated = self.client.ap.wrap_client_ether_pkt(self.client, pkt)
            self.connection.send(encapsulated)

