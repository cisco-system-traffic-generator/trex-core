import threading

from .client_service import *
from ...trex_wireless_client_state import ClientState
from scapy.contrib.capwap import *


from .client_service_association import *
from .client_service_dhcp import *
from ..trex_stl_ap import APJoinDisconnectedEvent

class ClientServiceAssociationAndDHCP(ClientService):

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf')):
        client = device
        super().__init__(device=client, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)

        # Client Association
        self.__asso_service = ClientServiceAssociation(device=client, env=self.env, tx_conn=ClientServiceAssociation.Connection(tx_conn, client), topics_to_subs=topics_to_subs, max_concurrent=float('inf'))
        # Client DHCP
        self.__dhcp_service = ClientServiceDHCP(device=client, env=self.env, tx_conn=ClientServiceDHCP.Connection(tx_conn, client), topics_to_subs=topics_to_subs, max_concurrent=float('inf'))

        self.subservices.append(self.__asso_service)
        self.subservices.append(self.__dhcp_service)

        self.add_service_info("done", False)

    def run(self):
        first_start = True

        yield self.async_request_start(first_start=first_start)

        yield self.async_launch_service(self.__asso_service)
        while True:
            yield self.async_launch_service(self.__dhcp_service)
            if not self.__dhcp_service.succeeded:
                continue
            self.add_service_info("done", True)
            yield self.async_request_stop(done=True, success=True)

            # wait for reconnection in case of a deconnection
            yield self.async_wait_for_event(ClientAssociationAssociatedEvent(self.env, self.device.mac))


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
