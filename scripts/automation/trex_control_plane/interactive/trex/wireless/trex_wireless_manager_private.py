import socket
from abc import ABC
from trex.common import *
from .trex_wireless_client_state import *
from trex.utils.parsing_opts import check_mac_addr, check_ipv4_addr
from scapy.utils import mac2str

"""
Attached Information of a Wireless_Manager
"""

class DeviceInfo(ABC):
    """Information on a WirelessDevice"""
    def __init__(self, identifier):
        """Create a DeviceInfo.

        Args:
            identifier: an identifier for a WirelessDevice
        """
        self.__identifier = identifier

    @property
    def identifier(self):
        return self.__identifier

class APInfo(DeviceInfo):
    """Information on an AP as seen by a Wireless_Manager."""

    def __init__(self, port_id, ip, mac, radio_mac, udp_port, wlc_ip, gateway_ip, ap_mode, rsa_ca_priv_file, rsa_priv_file, rsa_cert_file):
        """Create the basic info for an AP.

        Args:
            port_id: the port id of the trex client that the AP will be attached to
            ip: ipv4 address of the AP, in string format, or None
            mac: mac address of the AP, in string format
            radio_mac: mac address of the AP, in string format
            udp_port: udp port of the AP for traffic
            wlc_ip: the ipv4 address of the wlc, or None in case of DHCP
            ap_mode: The Mode of the AP, APMode.LOCAL for Local mode APs, APMode.REMOTE for Remote aps (also called FlexConnect)
            rsa_ca_priv_file: rsa private key of WLC CA
            rsa_priv_file: rsa private key of AP (required if no rsa_ca_priv_file)
            rsa_cert_file: rsa certificate of AP (required if no rsa_ca_priv_file)
        """
        super().__init__(identifier=mac)
        # Mandatory parameters
        if not rsa_ca_priv_file and not (rsa_priv_file and rsa_cert_file):
            raise ValueError("APInfo should be instanciated with values for either rsa_ca_priv_file either rsa_priv_file and rsa_cert_file")

        if any([p is None for p in (port_id, mac, radio_mac, udp_port)]):
            raise ValueError("APInfo should be instanciated with values for (port_id, mac, radio_mac, udp_port)")

        # MAC addresses checks
        check_mac_addr(mac)
        check_mac_addr(radio_mac)

        # IP addresses checks
        if ip:
            check_ipv4_addr(ip)
        if wlc_ip:
            check_ipv4_addr(wlc_ip)
        if gateway_ip:
            check_ipv4_addr(gateway_ip)

        self.name = 'AP%s%s.%s%s.%s%s' % (
            mac[:2], mac[3:5], mac[6:8], mac[9:11], mac[12:14], mac[15:17])
        self.port_id = port_id
        self.mac = mac
        self.ip = ip
        self.radio_mac = radio_mac
        self.udp_port = udp_port
        self.wlc_ip = wlc_ip
        self.wlc_mac = None
        self.gateway_ip = gateway_ip
        self.ap_mode = ap_mode

        self.rsa_ca_priv_file = rsa_ca_priv_file
        self.rsa_priv_file = rsa_priv_file
        self.rsa_cert_file = rsa_cert_file

        self.clients = []

    def __str__(self):
        return self.name


class ClientInfo(DeviceInfo):
    """Information on an wireless Client as seen by a Wireless_Manager."""

    def __init__(self, mac, ip, ap_info):
        """Create the basic info for an AP.

        Args:
            mac: mac address of the client in string format
            ip: ipv4 address of the client in string format, or None if DHCP
            ap_info: APInfo of the AP attached to the client
        """
        super().__init__(identifier=mac)
        # Mandatory parameters
        if any([p is None for p in (mac, ap_info)]):
            raise ValueError("ClientInfo should be instanciated with values for (mac, ap_info)")
       
        if not isinstance(ap_info, APInfo):
            raise ValueError("ClientInfo should be instanciated with a APInfo")

        self.ap_info = ap_info

        # MAC addresses check
        check_mac_addr(mac)
        
        self.mac_bytes = mac2str(mac)
        self.mac = mac

        if ip:
            # IP address check
            check_ipv4_addr(ip)
            self.ip_bytes = socket.inet_aton(ip)
            self.ip = ip
        else:
            self.ip = None
            self.ip_bytes = None

        self.name = "Client {} - {}".format(mac, ip)
        self.got_disconnect = False
        self.is_associated = False
        self.seen_arp_reply = False
        self.state = ClientState.ASSOCIATION
        self.retries = 0
        self.gateway_ip = self.ap_info.gateway_ip

    def __str__(self):
        return self.name

# Update

class WirelessDeviceStateUpdate:
    """Represents an update report,
    describing a state change for a WirelessDevice.
    """

    def __init__(self, identifier, update):
        """Create a WirelessDeviceStateUpdate.

        It comprises:
            an identifier for the WirelessDevice, for the Manager to identify it
            a dictionnary {attribute_name -> new_value} of updated attributes

            e.g. : WirelessDeviceStateUpdate("aa:bb:cc:cc:bb:aa", {
                "ip": "5.5.5.5",
                "gateway_ip": "1.1.1.1",
            })

        Attention: all the fields should exist for the DeviceInfo

        Args:
            identifier: wireless device identifier
            update: dict {attibute_name -> new_value} of updated attributes of the wireless device
        """
        self.identifier = identifier
        self.update = update

    def __str__(self):
        return "device id: {}, update: {}".format(self.identifier, self.update)