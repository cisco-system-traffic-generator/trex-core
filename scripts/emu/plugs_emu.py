from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'ipv6': {'dmac': [1,2,3,4,5,6], 'mtu': 2000},
                              'arp': {'enable': False},
                             }
        self.def_c_plugs   = {
                             'dhcp': {'timerd': 1, 'timero': 2},
                             'icmp': {'enable': False}
                            }

    def create_profile(self, ns_size, clients_size):
        
        ns = EMUNamespaceObj(vport  = 1,
                            plugs = {'ipv6': {'dmac': [1, 1, 1, 1, 1, 1], 'mtu': 2500}},
                            def_c_plugs = self.def_c_plugs
                            )

        client = EMUClientObj(mac     = '00:00:00:00:00:01',
                              ipv4    = '1.1.1.1',
                              ipv4_dg = '1.1.1.2',
                              ipv6    = '::1234',
                              plugs   = {'dhcp': {'timerd': 10, 'timero': 20}
                                        }
                            )
        ns.add_clients(client)

        return EMUProfile(ns = ns, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple emu profile.')
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 15,
                    help='Number of clients to create in each namespace')

        args = parser.parse_args(tuneables)

        return self.create_profile(args.ns, args.clients)


def register():
    return Prof1()
