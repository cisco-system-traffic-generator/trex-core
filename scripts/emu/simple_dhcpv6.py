from trex.emu.api import *
from trex.utils.common import increase_mac, increase_ip, generate_ipv6
from trex.emu.trex_emu_conversions import Mac, Ipv4, Ipv6

import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'ipv6': {'dmac':[0,0,0,0x70,0,1]}, # mac of one of the clients, this will be removed in the future
                            }
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = Mac('00:00:00:70:00:01')
            ipv4 = Ipv4('0.0.0.0')
            dg = Ipv4('0.0.0.0')
            ipv6 = Ipv6("2001:DB8:1::2") 

            # create a different client each time
            for j in range(clients_size):
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4.V(),
                                      ipv4_dg = dg.V(),
                                      ipv6    = ipv6[j].V(),
                                      plugs   = {'ipv6': {},
                                                 'dhcpv6': {}
                                                },
                                      )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

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

