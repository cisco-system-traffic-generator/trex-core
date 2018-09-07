from .client_service import *
from ..trex_wireless_service_event import WirelessServiceEvent
from ...trex_wireless_client_state import ClientState
from scapy.contrib.capwap import *
from ..trex_wireless_service_event import WirelessServiceEvent
from trex.common.services.trex_service_int import PktRX
from ..trex_stl_ap import APJoinConnectedEvent, APVAPReceivedEvent

import time
import random

# Events
class ClientAssociationAssociatedEvent(WirelessServiceEvent):
    """Raised when client gets associated."""
    def __init__(self, env, device):
        service = ClientServiceAssociation.__name__
        value = "associated"
        super().__init__(env, device, service, value)

class ClientAssociationDeassociatedEvent(WirelessServiceEvent):
    """Raised when client gets deassociated."""
    def __init__(self, env, device):
        service = ClientServiceAssociation.__name__
        value = "deassociated"
        super().__init__(env, device, service, value)


class ClientServiceAssociation(ClientService):
    """Client Service that implement Association (open authentication).
    Runs forever for ressociation purposes.
    """

    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'), stop=False):
        """Instanciate an Assocation service on a client, wich goal is to association the client to a WLC.
        It also handles deassociation and deauthentication and retry the Association Process.

        Args:
            device (APClient): client that owns the service
            env: simpy environment
            tx_conn: Connection (e.g. pipe end with 'send' method) used to send packets.
                ServiceClient should send packet via this connection as if a real client would send them.
                e.g. Dot11 / Dot11QoS / LLC / SNAP / IP / UDP / DHCP here.
                One can use a wrapper on the connection to provide capwap encapsulation.
            done_event: event that will be awaken when the service completes for the client
            stop: if set, will return once the association is done, if not, will wait for e.g. deauthentication packets
        """
        client = device
        super().__init__(device=client, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)

        # configuration
        configuration = client.config.client.association

        self.SLOT_TIME = configuration.slot_time
        self.MAX_RETRIES = configuration.max_retries

        self.__garp = None
        self.__arp = None
        self.__assoc_packet = None
        self.__disassoc_packet = None

        self.client.state = ClientState.ASSOCIATION
        self.retries = 0 # number of retries from last hard timeout
        self.total_retries = 0 # total number of retries

        

        if not ClientServiceAssociation.concurrent_resource:
            ClientServiceAssociation._set_concurrent_resource(env, max_concurrent)

    def __build_packets(self):
        vap = self.ap.get_open_auth_vap()
        if vap is None:
            return False
        if not self.__assoc_packet:
            self.__assoc_packet = CAPWAP_PKTS.client_assoc(self.ap, vap=vap, client_mac=self.client.mac_bytes).raw
        if not self.__disassoc_packet:
            self.__disassoc_packet = CAPWAP_PKTS.client_disassoc(self.ap, vap=vap, client_mac=self.client.mac_bytes).raw
        
        if not self.client.dhcp and not self.__garp:
            self.__garp = self.client.ap.get_arp_pkt('garp', self.client.mac_bytes, self.client.ip_bytes, self.client.gateway_ip_bytes)
        if not self.client.dhcp and not self.__arp:
            self.__arp = self.client.ap.get_arp_pkt('who-has', self.client.mac_bytes, self.client.ip_bytes, self.client.gateway_ip_bytes)

        return True

    @staticmethod
    def _set_concurrent_resource(env, max_concurrent):
        """"Set the shared resource describing the maximum number of ClientServiceAssociation that can be run concurrently.

        Args:
            env: simpy environment attached to all ClientServiceAssociation
            max_concurrent: number of maximum concurrent ClientServiceAssociation
        """
        if hasattr(ClientServiceAssociation, "concurrent_resource") and ClientServiceAssociation.concurrent_resource:
            raise RuntimeError("concurrent_resource for class ClientServiceAssociation is already set")
        else:
            ClientServiceAssociation.concurrent_resource = simpy.resources.resource.Resource(env, capacity=max_concurrent)

    @property
    def wait_time(self):
        """Return the next delay to wait for packets."""
        # exponential backoff with randomized +/- 1
        return pow(self.SLOT_TIME, self.retries + 1) + random.uniform(-1, 1)

    def timeout(self):
        """Increase the delay to wait, for next retransmission, or rollback to INIT state."""
        if self.retries < self.MAX_RETRIES:
            self.retries += 1
            self.total_retries += 1
        if self.retries >= self.MAX_RETRIES:
            self.retries = 0
            self.total_retries += 1
            self.client.state = ClientState.ASSOCIATION # rollback
            self.timed_out = True

    def reset(self):
        """Perform a complete rollback of the FSM."""
        self.retries = 0
        self.client.state = ClientState.ASSOCIATION
        self.timed_out = False
        self.__garp = None
        self.__arp = None
        self.__assoc_packet = None
        self.__disassoc_packet = None
        if self.client.dhcp:
            self.client.ip = None
            self.client.ip_bytes = None

    def __parse_packet(self, dot11_bytes):
        """Parse a Dot11 packet for use in Association Process, and return the next state of the client corresponding to the received packet.
        
        Args:
            dot11_bytes: Dot11 packet and sublayers
        """
        WLAN_ASSOC_RESP = b'\x00\x10'
        WLAN_DEAUTH = b'\x00\xc0'
        WLAN_DEASSOC = b'\x00\xa0'

        if dot11_bytes[:2] == WLAN_DEAUTH:  # dot11 deauthentication
            self.client.logger.info("received Deauthentication")
            return ClientState.ASSOCIATION
        elif dot11_bytes[:2] == WLAN_DEASSOC:
            self.client.logger.info("received deassociation")
            return ClientState.ASSOCIATION
        elif dot11_bytes[:2] == WLAN_ASSOC_RESP:
            if self.client.state == ClientState.ASSOCIATION:
                return ClientState.IP_LEARN
            else:
                # discard
                return self.client.state
        # discard
        return self.client.state

    def stop(self, hard=False):
        # run after run returns
        # FIXME
        if not hard:
            if not self.__build_packets:
                self.client.logger.info("Cannot send disassociation as VAP is already deleted")
            else:
                self.client.logger.info("Association sending disassociation")
                self.send_pkt(self.client.ap.wrap_capwap_pkt(self.__disassoc_packet, dst_port=5247))
        self.reset()
        pass
        
    def run(self):
        WLAN_ASSOC_RESP = b'\x00\x10'
        WLAN_DEAUTH = b'\x00\xc0'
        WLAN_DEASSOC = b'\x00\xa0'
    
        succeeded = False # True if the service succeeded at least once
        client = self.client
        ap = client.ap
        client.state = ClientState.ASSOCIATION

        client.logger.info("ClientServiceAssociation set to start")
        yield self.async_request_start(first_start=True)
        client.logger.info("ClientServiceAssociation started")

        # run loop
        while True:
            if client.state > ClientState.IP_LEARN:
                # care for deauth / deassoc packets or AP disconnect

                # wait until at least one packet is received
                client.logger.debug("ClientServiceAssociation waiting for packet or AP disconnection")
                wait_for_events = [APJoinConnectedEvent(self.env, self.client.ap.mac)]
                events = yield self.async_wait_for_any_events(wait_for_events, wait_packet=True)
                client.logger.debug("ClientServiceAssociation awaken")
                pkts = []
                for event, value in events.items():
                    if isinstance(event, PktRX):
                        # got packet
                        pkts = value
                    else:
                        # got disconnected
                        pkts = []
                        # restart
                        self.reset()
                        self.raise_event(ClientAssociationDeassociatedEvent(self.env, client.mac))
                        yield self.async_request_start(first_start=False)
                        break

                for pkt in pkts:
                    # update the state of the client according to the packets received
                    client.state = self.__parse_packet(pkt)
                    if client.state <= ClientState.IP_LEARN:
                        # restart
                        self.reset()
                        self.raise_event(ClientAssociationDeassociatedEvent(self.env, client.mac))
                        yield self.async_request_start(first_start=False)

            elif client.state == ClientState.ASSOCIATION:
                if not self.__build_packets():
                    # VAP is missing, probably WLAN is down, wait for it
                    # FIXME: Use event instead of timer, the event is not genreated as we need 
                    # to refactor how VAP is handled. Right now it's from the background thread so
                    # we cannot generate events from there
                    # wait_for_events = [APVAPReceivedEvent(self.env, self.client.ap.mac)]
                    # yield self.async_wait_for_any_events(wait_for_events, wait_packet=True)
                    yield self.async_wait(1)
                    continue

                # send association request
                client.logger.info("Association sending assocation request")
                self.send_pkt(self.client.ap.wrap_capwap_pkt(self.__assoc_packet, dst_port=5247))

                client.logger.info("Association waiting assocation response")
                pkts = yield self.async_recv_pkt(time_sec=self.wait_time)

                if not pkts:
                    self.timeout()
                    continue
                
                for pkt in pkts:
                    if pkt[:2] == WLAN_ASSOC_RESP:
                        client.logger.debug("Association received assocation response")
                        client.logger.info("Associated")
                        client.state = ClientState.IP_LEARN
                        self.add_service_info("total_retries", self.total_retries)
                        break
                continue
            elif client.state == ClientState.IP_LEARN:
                if client.dhcp:
                    client.state = ClientState.RUN
                    yield self.async_request_stop(done=False, success=True)
                    continue
                else:
                    if not self.__build_packets():
                        # Reset the client
                        self.reset()
                        self.raise_event(ClientAssociationDeassociatedEvent(self.env, client.mac))
                        yield self.async_request_start(first_start=False)
                        continue

                    # sending packets ARP who-has and gratuitous ARP
                    client.logger.debug("Association sending ARP packets")
                    self.send_pkt(self.client.ap.wrap_client_ether_pkt(self.client, self.__garp))
                    self.send_pkt(self.client.ap.wrap_client_ether_pkt(self.client, self.__arp))

                    pkts = yield self.async_recv_pkt(time_sec=self.wait_time)

                    if not pkts:
                        self.timeout()
                        continue
                        
                    for pkt in pkts:
                        if pkt[:2] in (WLAN_DEAUTH, WLAN_DEASSOC):
                            # deassociation
                            client.logger.info("Deassociated")
                            self.reset()
                            break

                    if client.state != ClientState.IP_LEARN:
                        # deassociation
                        continue

                    if client.seen_arp_reply:
                        client.state = ClientState.RUN
                        client.logger.info("Received ARP")
                        client.logger.info("ClientServiceAssociation finished successfully")
                        # send event as Client
                        self.raise_event(ClientAssociationAssociatedEvent(self.env, client.mac))
                        # send event as AP
                        self.raise_event(ClientAssociationAssociatedEvent(self.env, client.ap.mac))
                        yield self.async_request_stop(done=False, success=(not succeeded))
                        succeeded = True
                        continue

                    client.logger.info("ClientServiceAssociation timeout")
                    self.timeout()
                    continue
                    
    # Overriding Connection of ServiceClient because capwap encapsulation is slightly different for the association process
    class Connection:
        # Dummy connection
        def __init__(self, connection, client):
            self.connection = connection
            self.client = client
        def send(self, pkt):
            self.connection.send(pkt)
