from wireless.services.ap.ap_service import *
from scapy.all import Ether


class APServicePrinter(APService):
    """AP Service printing each received packet's layers."""

    def run(self):
        yield self.async_request_start(first_start=True, request_packets=True)
        self.ap.logger.info("APServicePrinter started")
        # run loop
        while True:
            pkts = yield self.async_recv_pkt(time_sec=None)
            if pkts:
                pkt = pkts[0]
                print(Ether(pkt).summary())  # print packet layers
