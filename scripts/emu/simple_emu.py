from trex.emu.api import *
from trex.utils.common import increase_mac, increase_ip, generate_ipv6
import argparse

MAX_UINT16 = (2 ** 16) - 1
MAX_TCI    = (2 ** 12) - 1 

class NsGen:
    def __init__(self, vport, tci, tpid, total, p_inc = 0, tci_inc = 0):
        self.vport    = vport
        self.tci      = tci
        self.tpid     = tpid
        self.p_inc    = p_inc
        self.tci_inc  = tci_inc 
        self.total    = total

    def __iter__(self):
        for _ in range(self.total):
            yield self.vport, self.tci, self.tpid
            if self.vport < MAX_UINT16:
                self.vport += self.p_inc
            else:
                self.vport = 1
                self.tci = list(self.tci)
                if self.tci[0] < MAX_TCI:
                    self.tci[0] += self.tci_inc
                elif self.tci[1] < MAX_TCI:
                    self.tci[0] = 0
                    self.tci[1] += self.tci_inc

class ClientGen:
    def __init__(self, mac, ipv4, dg, total_clients, mac_inc = 0, ipv4_inc = 0, dg_inc = 0):
        self.mac      = mac
        self.ipv4     = ipv4
        self.dg       = dg
        self.mac_inc  = mac_inc
        self.ipv4_inc = ipv4_inc
        self.dg_inc   = dg_inc
        self.total    = total_clients

    def __iter__(self):    
        for _ in range(self.total):
            yield self.mac, self.ipv4, self.dg, generate_ipv6(self.mac)
            self.mac  = increase_mac(self.mac, self.mac_inc)
            self.ipv4 = increase_ip(self.ipv4, self.ipv4_inc)
            self.dg   = increase_ip(self.dg, self.dg_inc)

class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = {'arp': {'enable': True},
                             'igmp': {'enable': True},
                             'icmp': {'enable': True},
                             'ipv6': {'enable': True},
                             'dhcp': {'enable': True}
                             }

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 1, [0, 0], [0x8100, 0x8100]
        ns_gen = NsGen(vport, tci, tpid, ns_size, p_inc = 1, tci_inc = 1)
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

            c_gen = ClientGen(mac, ipv4, dg, clients_size, mac_inc = 2, ipv4_inc = 5)
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

