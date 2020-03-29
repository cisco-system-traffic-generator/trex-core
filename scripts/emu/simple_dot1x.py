from trex.emu.api import *
from trex.emu.trex_emu_conversions import Mac, Ipv4

import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for i in range(vport, ns_size + vport):
            ns = EMUNamespaceObj(vport  = i,
                                tci     = tci,
                                tpid    = tpid,
                                def_c_plugs = self.def_c_plugs
                                )

            mac = Mac('00:00:00:70:00:01')
            ipv4 = Ipv4('1.1.1.3')
            dg = Ipv4('1.1.1.2')
           
            # create a different client each time
            for j in range(clients_size):               
                u = "hhaim{}".format(j + 1)
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4[i].V(),
                                      ipv4_dg = dg.V(),
                                      plugs   = {'arp': {},
                                                 'dot1x': {'user':u,'password':u,'flags':1},
                                                'icmp':{}
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


