from wireless.trex_wireless_ap_state import APState
from wireless.services.ap.ap_service import *
from wireless.services.trex_stl_ap import APJoinConnectedEvent
from wireless.services.trex_wireless_service_event import WirelessServiceEvent

from scapy.contrib.capwap import *
from scapy.all import Ether


class PeriodicAPReports(APService):
    """AP Service serving a packet periodically."""

    def init(self):
        # this payload will be sent as (Ethernet / IP / UDP / CAPWAP DATA / PAYLOAD)
        self.capwap_data_payload = b''  # empty payload
        self.time_interval = 30  # 30 seconds between packets

    def run(self):
        self.init()

        yield self.async_request_start(first_start=True, request_packets=True)
        self.ap.logger.info("PeriodicAPReports started")

        # run loop
        while True:
            if self.ap.state < APState.RUN:
                # the AP is not yet joined on controller
                self.ap.logger.info(
                    "Service PeriodicAPReports waiting for AP to Join")
                yield self.async_wait_for_event(APJoinConnectedEvent(self.env, self.ap.mac))
            else:
                self.ap.logger.info("Service PeriodicAPReports sending packet")
                self.send_pkt(self.capwap_data_payload)
                yield self.async_wait(self.time_interval)
                continue

    # Overriding Connection to automatically encapsulate into capwap data
    class Connection:
        def __init__(self, connection, ap):
            self.connection = connection
            self.ap = ap

        def send(self, pkt):
            encapsulated = self.ap.wrap_capwap_data(pkt)
            print("Sending: {}".format(Ether(encapsulated).summary()))
            self.connection.send(encapsulated)
