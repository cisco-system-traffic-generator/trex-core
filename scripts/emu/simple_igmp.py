from trex.emu.api import *
import argparse
import get_args 


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:01')
        self.def_ns_plugs  = {'igmp' : {'dmac':self.mac.V()}} 
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for j in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = j,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = self.mac 
            ipv4 = Ipv4('1.1.5.2')
            dg = Ipv4('1.1.5.1')

            # create a different client each time
            for i in range(clients_size):       
                client = EMUClientObj(mac     = mac[i].V(),
                                      ipv4    = ipv4[i].V(),
                                      ipv4_dg = dg.V(),
                                      plugs   = {'arp': {},
                                                 'icmp': {},
                                                 'igmp': {}, 
                                                },
                                      )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        args = get_args.get_args(tuneables)
        return self.create_profile(args.ns, args.clients)


def register():
    return Prof1()
