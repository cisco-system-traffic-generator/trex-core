from trex.emu.api import *
from trex.emu.trex_emu_conversions import Mac, Ipv4, Ipv6
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size, plug, mac, ipv4, dg, ipv6):
        empty_ipv4 = '0.0.0.0'
        mac_obj = Mac(mac)
        mac_as_bytes = mac_obj.V()

        PLUGS_MAP = {
        'ipv4': {'def_c': {'arp': {'enable': True}, 'icmp': {}},
                },

        'igmp': {'def_c': {'arp': {'enable': True}, 'icmp': {}, 'igmp': {'dmac': mac_as_bytes}}
                },

        'ipv6': {'def_c': {'ipv6': {}},
                'def_ns': {'ipv6': {'dmac': mac_as_bytes}},
                'ipv4': empty_ipv4, 'dg': empty_ipv4
                },

        'dhcpv4': {'def_c': {'arp': {'enable': True}, 'icmp': {}, 'dhcp': {}},
                'ipv4': empty_ipv4
                },

        'dhcpv6': {'def_c': {'ipv6': {}, 'dhcpv6': {}},
                'def_ns': {'ipv6': {'dmac': mac_as_bytes}},
                'ipv4': empty_ipv4, 'dg': empty_ipv4
                },

        'dot1x': {'def_c': {'dot1x': {}},
                'def_ns': {},},

        'all': {'def_c': {'arp':{'enable': True}, 'icmp': {}, 'igmp': {'dmac': mac_as_bytes}, 'ipv6': {}, 'dhcpv6': {}, 'dot1x': {}},
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
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

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
        parser = argparse.ArgumentParser(description='Argparser for plugs_emu profile.', formatter_class = argparse.RawTextHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 15,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--plugs', type = str.lower, choices = {'all', 'ipv4', 'ipv6', 'igmp', 'dhcpv4', 'dhcpv6', 'dot1x'},
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

