from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:01')
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def get_dhcpsrv_config(self, serverIp, serverDgIp):
        """Get DHCP Server configuration.

        Args:
            serverIp (str): The DHCPv4 Server IP. It should be excluded from the pool.
            serverDgIp (str): The Default Gateway for the server. It should be excluded from the pool.

        Returns:
            dict: Init Json for the server.
        """

        return {
            "default_lease": 120,
            "pools": [
                {
                    "min": "11.0.0.0",
                    "max": "11.0.0.255",
                    "prefix": 24,
                    "exclude": [serverIp, serverDgIp]
                },
            ],
            "options": {
                "offer": [
                    {
                        "type": 6,                                                  # DNS Server
                        "data": [8, 8, 8, 8]                                        # 8.8.8.8
                    },
                    {
                        "type": 15,                                                 # Domain Name
                        "data": [99, 105, 115, 99, 111, 46, 99, 111, 109]           # Cisco.com
                    }
                ],
                "ack": [
                    {
                        "type": 6,
                        "data": [8, 8, 8, 8]
                    },
                    {
                        "type": 15,
                        "data": [99, 105, 115, 99, 111, 46, 99, 111, 109]
                    }
                ]
            }
        }

    def create_profile(self, serverIp, serverDgIp):

        srv_ns = EMUNamespaceObj(ns_key = EMUNamespaceKey(vport=0), def_c_plugs = self.def_c_plugs)
        srv = EMUClientObj(mac     = Mac('00:00:00:70:00:01').V(),
                           ipv4    = Ipv4(serverIp).V(),
                           ipv4_dg = Ipv4(serverDgIp).V(),
                           plugs   = {'arp': {},
                                      'icmp': {},
                                      'dhcpsrv': self.get_dhcpsrv_config(serverIp, serverDgIp)})
        srv_ns.add_clients(srv)

        return EMUProfile(ns = [srv_ns], def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for DHCPv4 Server with Proxy.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--srv_ip', type = str, default = "11.0.0.2",
                    help='Server IPv4')
        parser.add_argument('--srv_dg_ip', type = str, default = "11.0.0.1",
                    help='Server Default Gateway')

        args = parser.parse_args(tuneables)
        return self.create_profile(args.srv_ip, args.srv_dg_ip)


def register():
    return Prof1()
