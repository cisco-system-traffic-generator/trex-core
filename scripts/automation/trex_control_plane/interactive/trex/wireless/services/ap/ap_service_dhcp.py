from wireless.services.ap.ap_service import *

from wireless.services.trex_stl_dhcp_parser import EthernetDHCPParser
from wireless.services.wireless_service_dhcp import WirelessServiceDHCP
from wireless.services.trex_wireless_service_event import WirelessServiceEvent

parser = EthernetDHCPParser()

class APDHCPDoneEvent(WirelessServiceEvent):
    """Raised when AP gets an IP from DHCP."""
    def __init__(self, env, device):
        service = APDHCPDoneEvent.__name__
        value = "obtained IP address from DHCP"
        super().__init__(env, device, service, value)

class APServiceDHCP(WirelessServiceDHCP, APService):
    """AP Service for DHCP.
    
    Implements a simplified version of RFC2131.
    That is, only account for these states : INIT, SELECTING, REQUESTING and BOUND.
    No timers are launched after the reception of DHCPACK from server, 
    that means that if the APs stays online for too long they may lose their DHCP reservation IP.
    No checks are made to verify that the offered IP is not already in use.

    Implementation Details:
    - AP collects first DHCPOffer(s) and discard others.
    - Retransmition strategy : backoff exponential with max threshold, each time being added to a random value between -1 and +1
    
    If the DHCP process succeeds, the fields 'ip', 'ip_bytes' and 'gateway_ip' will be set on the AP.
    """

    FILTER = '(udp dst port 68)'  # dhcp client filter

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf')):
        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent, dhcp_parser=parser)
    
    def is_ready(self, device):
        """Return True if the device is ready to start DHCP (e.g. for client, client state should be associated)"""
        return not device.is_closed

    def raise_event_dhcp_done(self):
        event = APDHCPDoneEvent(self.env, self.device)
        self.raise_event(event)
