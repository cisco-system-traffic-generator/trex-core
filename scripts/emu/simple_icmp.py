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
    def __init__(self, mac, ipv4, dg, total_clients, mac_inc=0, ipv4_inc=0, dg_inc=0):
        self.mac      = mac
        self.ipv4     = ipv4
        self.dg       = dg
        self.mac_inc  = mac_inc
        self.ipv4_inc = ipv4_inc
        self.dg_inc   = dg_inc
        self.total    = total_clients

    def __iter__(self):
        for _ in range(self.total):
            yield self.mac, self.ipv4, self.dg
            self.mac  = increase_mac(self.mac, self.mac_inc)
            self.ipv4 = increase_ip(self.ipv4, self.ipv4_inc)
            self.dg   = increase_ip(self.dg, self.dg_inc)


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'icmp': {'enable': True},
                              'arp' : {'enable': True}}

        self.def_c_plugs  = {'arp':  {'enable': True},
                             'icmp': {'enable': True},
                             }

    def create_profile(self, ns_size, clients_size, mac, ipv4, dg):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        ns_gen = NsGen(vport, tci, tpid, ns_size, p_inc=1, tci_inc=1)
        c_gen = ClientGen(mac, ipv4, dg, clients_size, mac_inc=1, ipv4_inc=1, dg_inc=0)
        for vport, tci, tpid in ns_gen:
            ns = EMUNamespaceObj(vport  = vport,
                                tci     = tci,
                                tpid    = tpid,
                                def_c_plugs = self.def_c_plugs
                                )

            # create a different client each time
            for i, (mac, ipv4, dg) in enumerate(c_gen):
                client = EMUClientObj(mac     = mac,
                                      ipv4    = ipv4,
                                      ipv4_dg = dg,
                                      plugs   = {'arp':  {'enable': True},
                                                 'icmp': {'enable': True},
                                                },
                                      )
                ns.add_clients(client)

            ns_list.append(ns)

        return EMUProfile(ns=ns_list, def_ns_plugs=self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple emu profile.')
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 15,
                    help='Number of clients to create in each namespace')

        # client args
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:03',
            help='Mac address of the first client')
        parser.add_argument('--4', type = str, default = '1.1.1.3', dest = 'ipv4',
            help='IPv4 address of the first client')
        parser.add_argument('--dg', type = str, default = '1.1.1.1',
            help='Default Gateway address of the first client')

        args = parser.parse_args(tuneables)

        assert args.ns > 0, 'namespaces must be positive!'
        assert args.clients > 0, 'clients must be positive!'

        return self.create_profile(args.ns, args.clients, args.mac, args.ipv4, args.dg)


def register():
    return Prof1()

