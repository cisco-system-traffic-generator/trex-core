from wireless.trex_wireless_ap_state import APState
from wireless.services.ap.ap_service import *
from wireless.services.trex_stl_ap import APJoinConnectedEvent
from wireless.services.trex_wireless_service_event import WirelessServiceEvent
from trex.stl.trex_stl_packet_builder_scapy import increase_mac

from scapy.contrib.capwap import *
from scapy.contrib.anqp import *
from scapy.all import *
from trex.utils.common import PassiveTimer
import struct

class ANQPQueryNonAssociated(APService):
    """Plugin to send GAS queries on behalf of clients, finish when all requests
       have received an answer.

       The GAS payload is then stored in the service info 'gas_payloads'

       Note: fragmentation (Comeback request) is not supported.
       The clients MAC address correspond to the first 2 bytes of AP radio MAC
       + a suffix of '01:01:01:01'. This can be override by setting the 'next_mac'
       service info variable.

       Service info that be customized:
       - nb_clients: Total number of clients that will send ANQP requests
       - next_mac: First MAC address to use for the first client. It will be
                   incremented for subsequent clients
       - anqp_element_ids: Array of integer representing the GAS/ANQP Element IDs
                           to put in the GAS Initial Request

       Example of use:

            service = "wireless.services.ap.ap_service_anqp.ANQPQueryNonAssociated"
            print('Starting ANQP')
            manager.set_aps_service_info_for_service('ANQPQueryNonAssociated', 'nb_clients', 5)
            manager.set_aps_service_info_for_service('ANQPQueryNonAssociated', 'anqp_element_ids', [258, 277])
            manager.add_aps_service(ap_ids=ap_macs, service_class=service, wait_for_completion=True, max_concurrent=float('inf'))
            print('ANQP Done, received GAS payloads:')
            print(manager.get_aps_service_info_for_service('ANQPQueryNonAssociated', 'gas_payloads'))
    """

    PUBLIC_ACTION_FRAME=0x0d

    # Seconds after which to timeout the plugin work
    GAS_TIMEOUT=5

    def get_next_client_mac(self):
        m = self.next_mac
        self.next_mac = increase_mac(self.next_mac)
        return m

    def init(self):
        self.add_service_info("done", False)
        self.client_macs = []

        self.received = {}

    def run(self):
        self.init()

        yield self.async_request_start(first_start=True, request_packets=True)
        self.ap.logger.info("ANQPQueryNonAssociated started")

        try:
            self.nb_clients = self.get_service_info("nb_clients")
        except:
            self.nb_clients = 1

        try:
            self.next_mac = self.get_service_info("next_mac")
        except:
            self.next_mac = str2mac(self.ap.radio_mac_bytes[0:2] + b'\x01\x01\x01\x01')

        try:
            self.anqp_element_ids = self.get_service_info("anqp_element_ids")
        except:
            self.anqp_element_ids = [258]

        # Generate the MACs
        for _ in range(self.nb_clients):
            self.client_macs.append(self.get_next_client_mac())

        # Create the base packet
        self.capwap_data_payload = (Dot11_swapped(
                    type = 'Management',
                    subtype = self.PUBLIC_ACTION_FRAME,
                    FCfield = 'to-DS',
                    ID = 100,
                    addr1 = self.ap.radio_mac, # Radio MAC,
                    addr2 = self.next_mac, # Client MAC
                    addr3 = self.ap.radio_mac, # Radio MAC
                    SC = 256,
                    addr4 = None,
                    )/Dot11uGASAction(action='GAS Initial Request', dialogToken=1)
                    /Dot11uGASInitialRequest()
                    /ANQPElementHeader(element_id=256) # ANQP Query List
                    /ANQPQueryList(element_ids=self.anqp_element_ids))

        # run loop
        while True:
            if self.ap.state < APState.RUN:
                # the AP is not yet joined on controller
                self.ap.logger.info(
                    "Service ANQPQueryNonAssociated waiting for AP to Join")
                yield self.async_wait_for_event(APJoinConnectedEvent(self.env, self.ap.mac))
            else:
                self.ap.logger.info("Service ANQPQueryNonAssociated sending ANQP requests")
                for c in self.client_macs:
                    self.capwap_data_payload[Dot11_swapped].addr2 = c
                    self.send_pkt(bytes(self.capwap_data_payload))
                self.ap.logger.info("Service ANQPQueryNonAssociated waiting for answer")
                timer = PassiveTimer(self.GAS_TIMEOUT)
                received = False
                while not received and not timer.has_expired():
                    pkts = yield self.async_recv_pkt(time_sec=1)
                    if pkts:
                        pkt = Ether(pkts[0])
                        if Dot11_swapped in pkt:
                            dot11 = pkt[Dot11_swapped]
                            if dot11.addr1 in self.client_macs and dot11.subtype == self.PUBLIC_ACTION_FRAME:
                                if Dot11uGASInitialResponse in pkt:
                                    gas = pkt[Dot11uGASInitialResponse]
                                    self.received[dot11.addr1] = bytes(gas.payload)
                                    if len(self.received.keys()) == len(self.client_macs):
                                        received = True
                                        self.add_service_info("gas_payloads", self.received)
                                        self.ap.logger.info("ANQPQueryNonAssociated received back all the ANQP answers")
                                        break
                self.add_service_info("done", True)
                return self.async_request_stop(done=True, success=received, delete=True)


    # Overriding Connection to automatically encapsulate into capwap data
    class Connection:
        def __init__(self, connection, ap):
            self.connection = connection
            self.ap = ap

        def send(self, pkt):
            encapsulated = self.ap.wrap_capwap_data(pkt)
            self.connection.send(encapsulated)
