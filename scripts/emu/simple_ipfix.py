from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'arp': {}, 'icmp': {}}
        self.def_c_plugs  = {'arp': {'timer': 50},
                             'icmp': {},
                            }

    def create_profile(self, ns_size, clients_size, mac, ipv4, dg, ipv6, dst_ipv4):
        ns_list = []
        mac      = Mac(mac)
        ipv4     = Ipv4(ipv4)
        ipv6     = Ipv6(ipv6)
        dg       = Ipv4(dg)
        dst_ipv4 = Ipv4(dst_ipv4)
        dst_ipv6 = Ipv6('::1234')
        dst_mac  = Mac('00:25:84:7c:d7:40')
        
        for i in range(ns_size):
            # create different namespace each time
            ns_key = EMUNamespaceKey(vport = i, tci = 0,tpid = 0)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            for j in range(clients_size):
                c = EMUClientObj(mac       = mac.V(),
                                    ipv4    = ipv4[j].V(),
                                    ipv4_dg = dg.V(),
                                    ipv6    = ipv6[j].V(),
                                    plugs  = {
                                        'avcnet': {
                                            "netflow_version": 10,
                                            "dst_mac": dst_mac.V(),
                                            "dst_ipv4": dst_ipv4.V(),
                                            # "dst_ipv6": dst_ipv6.V(),
                                            "dst_port": 6007,
                                            "src_port": 30334,
                                            "generators": {
                                                "gen1": {
                                                    "type": "dns",
                                                    "auto_start": True,
                                                    "rate_pps": 5,
                                                    "data_records": 7,
                                                    "gen_type_data": {
                                                        # DNS example
                                                        "client_ip": [15, 0, 0, 1],
                                                        "range_of_clients": 100,
                                                        "dns_servers": 150,
                                                        "nbar_hosts": 125
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    )
                    ns.add_clients(c)
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
        parser.add_argument('--4', type = str, default = '1.1.1.3', dest = 'ipv4',
            help='IPv4 address of the first client')
        parser.add_argument('--dst-4', type = str, default = '10.56.97.19', dest = 'dst_4',
            help='Ipv4 address of collector')
        parser.add_argument('--dg', type = str, default = '1.1.1.1',
            help='Default Gateway address of the first client')
        parser.add_argument('--6', type = str, default = '2001:DB8:1::2', dest = 'ipv6',
            help='IPv6 address of the first client')

        args = parser.parse_args(tuneables)
        
        assert 0 < args.ns < 65535, 'Namespcaes size must be positive! in range of ports: 0 < ns < 65535'
        assert 0 < args.clients, 'Clients size must be positive!'
        
        return self.create_profile(args.ns, args.clients, args.mac, args.ipv4, args.dg, args.ipv6, args.dst_4)

def register():
    return Prof1()

