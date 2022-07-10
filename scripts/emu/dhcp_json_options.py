from trex.emu.api import *
import argparse
import json


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:01')
        self.def_ns_plugs  = {'igmp' : {'dmac':self.mac.V()}} 
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size, clientMac, options):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for j in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = j,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = Mac(clientMac)
            ipv4 = Ipv4('0.0.0.0')
            dg = Ipv4('0.0.0.0')
            # create a different client each time
            for i in range(clients_size):
                client = EMUClientObj(mac     = mac[i].V(),
                                      ipv4    = ipv4.V(),
                                      ipv4_dg = dg.V(),
                                      plugs   = {'arp': {},
                                                 'icmp': {},
                                                 'igmp': {}, 
                                                 'dhcp': {'options': options},
                                              },
                                      )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
      # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for Dhcp Json Option', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 1,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:01',
                    help='Mac address of the first client')
        parser.add_argument('--options', type=str, help="A JSON encoded options dictionary.", default=json.dumps({}))

        args = parser.parse_args(tuneables)
        return self.create_profile(args.ns, args.clients,args.mac, json.loads(args.options))


def register():
    return Prof1()
