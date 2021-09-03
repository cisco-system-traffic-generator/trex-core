from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def get_dhcpsrv_config(self):
        """Get DHCP Server configuration.

        Returns:
            dict: Dictionary that represents the Init Json.
        """
        return {
            "default_lease": 120,
            "pools": [
                {
                    "min": "1.1.2.3",
                    "max": "1.1.2.255",
                    "prefix": 24,
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

    def create_profile(self, clients_size, clientMac, serverIp, serverDgIp):

        mac = Mac(clientMac)
        srv_ns = EMUNamespaceObj(ns_key = EMUNamespaceKey(vport=0), def_c_plugs = self.def_c_plugs)
        srv = EMUClientObj(mac     = mac[0].V(),
                           ipv4    = Ipv4(serverIp).V(),
                           ipv4_dg = Ipv4(serverDgIp).V(),
                           plugs   = {'arp': {},
                                      'icmp': {},
                                      'dhcpsrv': self.get_dhcpsrv_config()}
                                      )
        srv_ns.add_clients(srv)

        c_ns = EMUNamespaceObj(ns_key = EMUNamespaceKey(vport=1), def_c_plugs = self.def_c_plugs)
        for i in range(clients_size):
            client = EMUClientObj(mac     = mac[i+1].V(),       # The first MAC is given to the server.
                                  ipv4    = Ipv4('0.0.0.0').V(),
                                  ipv4_dg = Ipv4('0.0.0.0').V(),
                                  plugs   = {'arp': {},
                                             'icmp': {},
                                             'dhcp': {},
                                          },
                                  )
            c_ns.add_clients(client)

        return EMUProfile(ns = [srv_ns, c_ns], def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for DHCPv4 Relay Server. \n Port 0 - Server. Port 1 - Clients', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--clients', type = int, default = 3,
                    help='Number of clients to create.')
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:01',
                    help='Starting MAC address')
        parser.add_argument('--srv_ip', type = str, default = "1.1.1.3",
                    help='Server IPv4')
        parser.add_argument('--srv_dg_ip', type = str, default = "1.1.1.1",
                    help='Server Default Gateway')

        args = parser.parse_args(tuneables)
        return self.create_profile(args.clients, args.mac, args.srv_ip, args.srv_dg_ip)


def register():
    return Prof1()
