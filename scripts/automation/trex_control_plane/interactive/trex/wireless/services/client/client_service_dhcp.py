from .client_service import *

from wireless.services.trex_stl_dhcp_parser import Dot11DHCPParser
from wireless.services.wireless_service_dhcp import WirelessServiceDHCP
from wireless.services.client.client_service_association import ClientAssociationAssociatedEvent

parser = Dot11DHCPParser()

class ClientServiceDHCP(WirelessServiceDHCP, ClientService):

    # ethertype=ipv4 and protocol=udp and src_port=dhcp(server)

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf')):
        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent, dhcp_parser=parser)
    
    def is_ready(self, device):
        """Return True if the device is ready to start DHCP (e.g. for client, client state should be associated)"""
        return device.is_running

    def raise_event_dhcp_done(self):
        event_client = ClientAssociationAssociatedEvent(self.env, self.device.mac)
        event_ap = ClientAssociationAssociatedEvent(self.env, self.device.ap.mac)
        self.raise_event(event_client)
        self.raise_event(event_ap)