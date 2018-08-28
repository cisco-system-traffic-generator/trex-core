import threading
from enum import Enum

from scapy.contrib.capwap import *
from trex_openssl import *

from .trex_wireless_ap import *
from .trex_wireless_device import WirelessDevice
from .services.trex_stl_ap import *
from trex.utils.parsing_opts import check_mac_addr, check_ipv4_addr
from trex.stl.trex_stl_packet_builder_scapy import is_valid_ipv4_ret

class APClient(WirelessDevice):
    """A Client attached to an AP, as seen by a WirelessWorker."""

    wlc_assoc_timeout = 30

    def __init__(self, worker, mac, ip, ap, gateway_ip=None, client_info=None):
        """Create a Wireless Client attached to an AP.

        Args:
            worker: attached worker
            mac: mac address for the client, in str format
            ip: ip address for the client, in str format, or None in case of DHCP
            ap: attached AP
            gateway_ip: ip of the gateway, optional if e.g. DHCP, in byte format
            client_info (ClientInfo): original ClientInfo
        """
        name = "Client_{}".format(mac)
        super().__init__(worker, name, mac, gateway_ip, client_info)

        self.mac_bytes = mac2str(mac)
        self.mac = mac
        if not ip:
            self.ip = None
            self.ip_bytes = None
            self.dhcp = True  # flag is set to True if DHCP is needed to get an IP address
        else:
            self.dhcp = False
            check_ipv4_addr(ip)
            self.ip_bytes = is_valid_ipv4_ret(ip)
            self.ip = ip
        check_mac_addr(self.mac)
        # assert isinstance(ap, AP)
        self.ap = ap
        self.reset()
        self.retries = 0
        self.state = ClientState.AUTHENTICATION
        self.gateway_ip = None

        # event awaken when the client is associated
        self.joined_event = threading.Event()

    @property
    def attached_devices_macs(self):
        return [self.mac, self.ap.mac]

    @property
    def is_closed(self):
        return self.state >= ClientState.CLOSE

    def is_running(self):
        return self.state == ClientState.RUN

    def reset(self):
        self.got_disconnect = False
        self.is_associated = False
        self.is_connected = False
        self.connect_time = None  # time at which the client receives the association repsonse
        self.seen_arp_reply = False
        self.state = ClientState.ASSOCIATION
        if self.dhcp:
            self.ip = None
            self.ip_bytes = None

    def disconnect(self):
        self.logger.debug('disconnected')
        self.reset()
        self.got_disconnect = True
        try:
            client.got_disconnected_event.succeed()
        except (RuntimeError, AttributeError):
            # already triggered or not waiting for this packet
            pass

    def __str__(self):
        return "Client {} - {}".format(self.ip, self.mac)
