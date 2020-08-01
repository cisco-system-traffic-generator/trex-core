from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None

    def get_init_json(self):
        return {
                "netflow_version": 10,
                "dst_mac": [0, 0, 2, 0, 0, 0],
                "dst_ipv4": [48, 0, 0, 0],
                "dst_ipv6": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                "dst_port": 4739,
                "src_port": 30334,
                "domain_id": 7777,
                "generators": [
                    {
                        "name": "protocolIdentifierGen",
                        "auto_start": True,
                        "rate_pps": 1,
                        "data_records_num": 2,
                        "template_id": 261,
                        "is_options_template": True,
                        "scope_count": 1,
                        "fields": [
                            {
                                "name": "clientIPv4Address",
                                "type": 45004,
                                "length": 4,
                                "enterprise_number": 9,
                                "data": [16, 0, 0, 1]
                            },
                            {
                                "name": "protocolIdentifier",
                                "type": 4,
                                "length": 1,
                                "data": [17]
                            }
                        ],
                        "engines": [
                            {
                                "engine_name": "clientIPv4Address",
                                "engine_type": "uint",
                                "params":
                                {
                                    "size": 1,
                                    "offset": 3,
                                    "min": 1,
                                    "max": 255,
                                    "op": "inc",
                                    "step": 1,
                                }
                            }
                        ]
                    },
                    {
                        "name": "ipVersionGen",
                        "auto_start": True,
                        "rate_pps": 0.5,
                        "data_records_num": 5,
                        "template_id": 266,
                        "fields": [
                            {
                                "name": "ipVersion",
                                "type": 60,
                                "length": 1,
                                "data": [4]
                            }
                        ],
                        "engines": [
                            {
                                "engine_name": "ipVersion",
                                "engine_type": "histogram_uint",
                                "params": {
                                    "size": 1,
                                    "offset": 0,
                                    "entries": [
                                        {
                                            "v": 4,
                                            "prob": 3,
                                        },
                                        {
                                            "v": 6,
                                            "prob": 1
                                        }
                                    ]
                                }
                            }
                        ]
                    }
                ]
            }

    def create_profile(self, ns_size, clients_size, mac, ipv4, dg, dst_ipv4):
        ns_list = []
        mac      = Mac(mac)
        ipv4     = Ipv4(ipv4)
        dg       = Ipv4(dg)
        dst_ipv4 = Ipv4(dst_ipv4)
        dst_mac  = Mac('00:25:84:7c:d7:40')

        for i in range(ns_size):
            # create different namespace each time
            ns_key = EMUNamespaceKey(vport = i, tci = 0,tpid = 0)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            for j in range(clients_size):
                c = EMUClientObj(   mac     = mac[j].V(),
                                    ipv4    = ipv4[j].V(),
                                    ipv4_dg = dg.V(),
                                    plugs  = {'ipfix': self.get_init_json()}
                                )
                ns.add_clients(c)
            ns_list.append(ns)
        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple emu profile.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 1,
                    help='Number of clients to create in each namespace')

        # client args
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:01',
            help='Mac address of the first client')
        parser.add_argument('--4', type = str, default = '1.1.1.3', dest = 'ipv4',
            help='IPv4 address of the first client')
        parser.add_argument('--dst-4', type = str, default = '10.56.97.19', dest = 'dst_4',
            help='Ipv4 address of collector')
        parser.add_argument('--dg', type = str, default = '1.1.1.1',
            help='Default Gateway address of the clients')

        args = parser.parse_args(tuneables)
        
        assert 0 < args.ns < 65535, 'Namespaces size must be positive! in range of ports: 0 < ns < 65535'
        assert 0 < args.clients, 'Clients size must be positive!'
        
        return self.create_profile(args.ns, args.clients, args.mac, args.ipv4, args.dg, args.dst_4)

def register():
    return Prof1()

