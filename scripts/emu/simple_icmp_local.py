from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:02')
        self.def_ns_plugs  = {'arp': {},'icmp': {},'ipv6' : {'dmac':self.mac.V()}} 


        self.def_c_plugs  = {
                            'arp': {},
                            'icmp': {},
                            'ipv6': {},
                            'transport': {},
                            'transe': {'addr':'11.0.0.1:9001', 'size':100000, 'loops':100}
                             }

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                     tci     = tci,
                                     tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = Mac('00:00:00:70:00:02')
            ipv4 = Ipv4('11.0.0.2')
            dg = Ipv4('11.0.0.1')
            ipv6 = Ipv6("2001:DB8:1::2")

            # create a different client each time
            for j in range(clients_size):
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4[j].V(),
                                      ipv4_dg = dg.V(),
                                      ipv6    = ipv6[j].V(),
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

        assert args.ns > 0, 'namespaces must be positive!'
        assert args.clients > 0, 'clients must be positive!'

        return self.create_profile(args.ns, args.clients)


def register():
    return Prof1()

