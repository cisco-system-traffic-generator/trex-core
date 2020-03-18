from trex.emu.api import *
from trex.utils.common import increase_mac, increase_ip, increase_ipv6
from trex.emu.trex_emu_conversions import conv_to_bytes
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
    def __init__(self, mac, ipv4, dg, ipv6, total_clients, mac_inc = 0, ipv4_inc = 0, dg_inc = 0, ipv6_inc = 0):
        self.mac      = mac
        self.ipv4     = ipv4
        self.dg       = dg
        self.ipv6     = ipv6
        self.mac_inc  = mac_inc
        self.ipv4_inc = ipv4_inc
        self.dg_inc   = dg_inc
        self.ipv6_inc = ipv6_inc
        self.total    = total_clients

    def __iter__(self):
        for _ in range(self.total):
            yield self.mac, self.ipv4, self.dg, self.ipv6
            self.mac  = increase_mac(self.mac, self.mac_inc)
            self.ipv4 = increase_ip(self.ipv4, self.ipv4_inc)
            self.dg   = increase_ip(self.dg, self.dg_inc)
            self.ipv6 = increase_ipv6(self.ipv6, self.ipv6_inc)

class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size, plug, mac, ipv4, dg, ipv6):
        empty_ipv4 = '0.0.0.0'
        mac_as_bytes = conv_to_bytes(mac, 'mac')

        PLUGS_MAP = {
        'ipv4': {'def_c': {'arp': {'enable': True}, 'icmp': {}},
                },

        'igmp': {'def_c': {'arp': {'enable': True}, 'icmp': {}, 'igmp': {'dmac': mac_as_bytes}}
                },

        'ipv6': {'def_c': {'ipv6': {'enable': True}},
                'def_ns': {'ipv6': {'dmac': mac_as_bytes}},
                'ipv4': empty_ipv4, 'dg': empty_ipv4
                },

        'dhcpv4': {'def_c': {'arp': {'enable': True}, 'icmp': {}, 'dhcp': {}},
                'ipv4': empty_ipv4
                },

        'dhcpv6': {'def_c': {'ipv6': {'enable': True}, 'dhcpv6': {}},
                'def_ns': {'ipv6': {'dmac': mac_as_bytes}},
                'ipv4': empty_ipv4, 'dg': empty_ipv4
                },

        'all': {'def_c': {'arp':{'enable': True}, 'icmp': {}, 'igmp': {'dmac': mac_as_bytes}, 'ipv6': {'enable': True}, 'dhcpv6': {}},
                'def_ns': {'ipv6': {'dmac': mac_as_bytes}},
                'ipv4': empty_ipv4, 'dg': empty_ipv4}
        }

        if plug is not None:
            plug_info         = PLUGS_MAP[plug]
            self.def_ns_plugs = plug_info.get('def_ns')
            self.def_c_plugs  = plug_info.get('def_c')
            ipv4, dg          = plug_info.get('ipv4', ipv4), plug_info.get('dg', dg)
            
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0, 0]
        ns_gen = NsGen(vport, tci, tpid, ns_size, p_inc = 1, tci_inc = 1)
        for vport, tci, tpid in ns_gen:
            ns = EMUNamespaceObj(vport  = vport,
                                tci     = tci,
                                tpid    = tpid,
                                def_c_plugs = self.def_c_plugs
                                )


            c_gen = ClientGen(mac, ipv4, dg, ipv6, clients_size, 
                                mac_inc = 1, ipv4_inc = 0 if ipv4 == empty_ipv4 else 1, ipv6_inc = 1)
            
            # create a different client each time
            for i, (mac, ipv4, dg, ipv6) in enumerate(c_gen):
                client = EMUClientObj(mac     = mac,
                                      ipv4    = ipv4,
                                      ipv4_dg = dg,
                                      ipv6    = ipv6,
                                      )
                ns.add_clients(client)


            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for plugs_emu profile.', formatter_class = argparse.RawTextHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 15,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--plugs', type = str.lower, choices = {'all', 'ipv4', 'ipv6', 'igmp', 'dhcpv4', 'dhcpv6'},
                    help='''Set the wanted plugins in the profile:
                    ipv4: arp, icmp
                    igmp: arp, icmp, igmp
                    ipv6: ipv6
                    dhcpv4: arp, icmp, dhcp
                    dhcpv6: ipv6, dhcpv6
                    ''')
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

        assert args.ns > 0, 'namespcaes must be positive!'
        assert args.clients > 0, 'clients must be positive!'

        return self.create_profile(args.ns, args.clients, args.plugs, args.mac, args.ipv4, args.dg, args.ipv6)


def register():
    return Prof1()

