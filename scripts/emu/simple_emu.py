from trex.emu.api import *
from trex.utils.common import increase_mac, increase_ip, generate_ipv6
import argparse

MAX_UINT16 = (2 ** 16) - 1
MAX_TCI    = (2 ** 12) - 1 

class NsGen:
    def __init__(self, vport, tci, tpid, total):
        self.vport = vport
        self.tci   = tci
        self.tpid  = tpid
        self.total = total
    
    def __iter__(self):
        for _ in range(self.total):
            yield self.vport, self.tci, self.tpid
            if self.vport < MAX_UINT16:
                self.vport += 1
            else:
                self.vport = 1
                self.tci = list(self.tci)
                if self.tci[0] < MAX_TCI:
                    self.tci[0] += 1
                elif self.tci[1] < MAX_TCI:
                    self.tci[0] = 0
                    self.tci[1] += 1

class ClientGen:
    def __init__(self, mac, ipv4, dg, total):
        self.mac  = mac
        self.ipv4 = ipv4
        self.dg   = dg
        self.total = total

    def __iter__(self):    
        for _ in range(self.total):
            yield self.mac, self.ipv4, self.dg, generate_ipv6(self.mac)
            self.mac, self.ipv4, self.dg = increase_mac(self.mac), increase_ip(self.ipv4), increase_ip(self.dg)

class Prof1():
    def __init__(self):
        self.def_c_plugs = {'arp': {'enable': True},
                                'icmp': {'param': 42}}
        self.def_ns_plugs = {'arp': {'enable': False}}

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 1, [1, 1], [0x8100, 0x8100]
        ns_gen = NsGen(vport, tci, tpid, ns_size)
        for vport, tci, tpid in ns_gen:
            ns = EMUNamespaceObj(vport  = vport,
                                tci     = tci,
                                tpid    = tpid,
                                def_client_plugs = self.def_c_plugs
                                )

            mac = '00:00:00:00:00:01'
            ipv4 = '1.1.1.1'
            dg = '1.1.1.2'
            ipv6 = generate_ipv6(mac)

            c_gen = ClientGen(mac, ipv4, dg, clients_size)
            # create a different client each time
            for mac, ipv4, dg, ipv6 in c_gen:
                client = EMUClientObj(mac     = mac,
                                      ipv4    = ipv4,
                                      ipv4_dg = dg,
                                      ipv6    = ipv6
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

