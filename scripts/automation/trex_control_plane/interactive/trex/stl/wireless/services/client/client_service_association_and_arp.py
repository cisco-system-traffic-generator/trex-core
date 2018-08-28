import threading

from .client_service import *
from ...trex_wireless_client_state import ClientState
from scapy.contrib.capwap import *


from .client_service_association import *
from .client_service_arp import *


class ClientServiceAssociationAndARP(ClientService):
    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf')):
        client = device
        super().__init__(device=client, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)

        if not ClientServiceAssociationAndARP.concurrent_resource:
            ClientServiceAssociationAndARP._set_concurrent_resource(env, float("inf")) # no max_concurrent because it will be set in subservices

        self.__asso_service = ClientServiceAssociation(device=client, env=self.env, tx_conn=ClientServiceAssociation.Connection(tx_conn, client), topics_to_subs=topics_to_subs, max_concurrent=float('inf'))
        self.__arp_service = ClientServiceARP(device=client, env=self.env, tx_conn=ClientServiceARP.Connection(tx_conn, client), topics_to_subs=topics_to_subs, max_concurrent=float('inf'))

        self.subservices.append(self.__asso_service)
        self.subservices.append(self.__arp_service)

    def run(self):
        first_start = True
        while True:
            yield self.async_request_start(first_start=first_start, request_packets=False)
            if first_start:
                yield self.async_launch_service(self.__asso_service)
            if self.__asso_service.succeeded:
                yield self.async_launch_service(self.__arp_service)
                if self.__arp_service.succeeded:
                    break
            
            if not self.succeeded:
                yield self.async_request_stop(done=True, success=False)
            first_start = False

        yield self.async_request_stop(done=True, success=True)

    # Overriding Connection of ClientService to be transparent for subservices
    class Connection:
        """Wrapper of a Connection (e.g. pipe end)."""
        def __init__(self, connection, client):
            """Construct a ClientConnection.

            Args:
                connection: a Connection (e.g. pipe end), that has a 'send' method.
                client: a AP_C_Client attached to an AP.
            """
            self.connection = connection
            self.client = client
            self.send = self.connection.send
