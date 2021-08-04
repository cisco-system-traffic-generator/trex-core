from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:01')
        self.def_ns_plugs  = {'igmp' : {'dmac':self.mac.V()}} 
        self.def_c_plugs  = {'arp': {'timer': 50},
                             'igmp': {},
                            }

    def create_profile(self, ns_size, clients_size, mac, ipv4, dg, ipv6):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, 1, 0x8100

        # iterate namespaces, increment vport each time 
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                    tci     = tci,
                                    tpid    = tpid
                                    )
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs
                                )

            mac  = Mac(mac)
            ipv4 = Ipv4(ipv4)
            dg   = Ipv4(dg)
            ipv6 = Ipv6(ipv6)
            
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
        parser = argparse.ArgumentParser(description='Argparser for simple emu profile.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 15,
                    help='Number of clients to create in each namespace')

        # client args
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:01',
            help='Mac address of the first client')
        parser.add_argument('--4', type = str, default = '1.1.2.3', dest = 'ipv4',
            help='IPv4 address of the first client')
        parser.add_argument('--dg', type = str, default = '1.1.2.1',
            help='Default Gateway address of the first client')
        parser.add_argument('--6', type = str, default = '2001:DB8:1::2', dest = 'ipv6',
            help='IPv6 address of the first client')

        args = parser.parse_args(tuneables)
        
        assert 0 < args.ns < 65535, 'Namespcaes size must be positive! in range of ports: 0 < ns < 65535'
        assert 0 < args.clients, 'Clients size must be positive!'
        
        return self.create_profile(args.ns, args.clients, args.mac, args.ipv4, args.dg, args.ipv6)

def register():
    return Prof1()

