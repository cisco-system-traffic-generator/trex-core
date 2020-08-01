from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = None
        self.def_c_plugs  = None


    def get_template_261_fields(self):
        return [
                {
                    "name": "clientIPv4Address",
                    "type": 45004,
                    "length": 4,
                    "enterprise_number": 9,
                    "data": [16, 0, 0, 1]
                },
                {
                    "name": "serverIPv4Address",
                    "type": 45005,
                    "length": 4,
                    "enterprise_number": 9,
                    "data": [24, 0, 0, 1]
                },
                {
                    "name": "protocolIdentifier",
                    "type": 4,
                    "length": 1,
                    "data": [17]
                },
                {
                    "name": "clientTransportPort",
                    "type": 45008,
                    "length": 2,
                    "enterprise_number": 9,
                    "data": [128, 232]
                },
                {
                    "name": "serverTransportProtocol",
                    "type": 45009,
                    "length": 2,
                    "enterprise_number": 9,
                    "data": [0, 53]
                },
                {
                    "name": "applicationId",
                    "type": 95,
                    "length": 4,
                    "data": [3, 0, 0, 53]
                },
                {
                    "name": "nbar2HttpHost",
                    "type": 45003,
                    "length": 7,
                    "enterprise_number": 9,
                    "data": [115, 115, 115, 46, 101, 100, 117]
                },
                {
                    "name": "nbar2HttpHostBlackMagic1",
                    "type": 45003,
                    "length": 7,
                    "enterprise_number": 9,
                    "data": [3, 0, 0, 53, 52, 4, 0]
                },
                {
                    "name": "nbar2HttpHostBlackMagic2",
                    "type": 45003,
                    "length": 7,
                    "enterprise_number": 9,
                    "data": [3, 0, 0, 53, 52, 5, 133]
                },
                {
                    "name": "flowStartSysUpTime",
                    "type": 22,
                    "length": 4,
                    "data": [0, 0, 0, 1]
                },
                {
                    "name": "flowEndSysUpTime",
                    "type": 21,
                    "length": 4,
                    "data": [0, 0, 0, 10]
                },
                {
                    "name": "flowStartMilliseconds",
                    "type": 152,
                    "length": 8,
                    "data": [0, 0, 0, 0, 0, 0, 0, 0]
                },
                {
                    "name": "responderPackets",
                    "type": 299,
                    "length": 8,
                    "data": [0, 0, 0, 0, 0, 0, 0, 1]
                },
                {
                    "name": "initiatorPackets",
                    "type": 298,
                    "length": 8,
                    "data": [0, 0, 0, 0, 0, 0, 0, 1]
                },
                {
                    "name": "serverBytesL3",
                    "type": 41105,
                    "length": 8,
                    "enterprise_number": 9,
                    "data": [0, 0, 0, 0, 0, 0, 0, 127]
                },
                {
                    "name": "clientBytesL3",
                    "type": 41106,
                    "length": 8,
                    "enterprise_number": 9,
                    "data": [0, 0, 0, 0, 0, 0, 0, 127]
                }
        ]

    def get_init_json_examples(self, mac, ipv4, ipv6, example_number):
        example1 = {
            "netflow_version": 10,
            "dst_mac": mac.V(),
            "dst_ipv4": ipv4.V(),
            "dst_ipv6": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "dst_port": 6007,
            "src_port": 30334,
            "domain_id": 1000+example_number,
            "generators": [
                {
                    "name": "dns",
                    "auto_start": True,
                    "rate_pps": 2,
                    "data_records_num": 7,
                    "template_id": 260+example_number,
                    "fields": self.get_template_261_fields(),
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
                        },
                        {
                            "engine_name": "protocolIdentifier",
                            "engine_type": "histogram_uint",
                            "params":
                            {
                                "size": 1,
                                "offset": 0,
                                "entries": [
                                    {
                                        "v": 17,
                                        "prob": 5
                                    },
                                    {
                                        "v": 1,
                                        "prob": 1,
                                    },
                                    {
                                        "v": 6,
                                        "prob": 3
                                    }
                                ]
                            }
                        },
                        {
                            "engine_name": "applicationId",
                            "engine_type": "histogram_uint_list",
                            "params":
                            {
                                "size": 1,
                                "offset": 3,
                                "entries": [
                                    {
                                        "list": [0, 2, 4, 6, 8],
                                        "prob": 1
                                    },
                                    {
                                        "list": [1, 3, 5, 7, 9],
                                        "prob": 1
                                    }
                                ]
                            }
                        },
                        {
                            "engine_name": "initiatorPackets",
                            "engine_type": "histogram_uint64_range",
                            "params":
                            {
                                "size": 8,
                                "offset": 0,
                                "entries": [
                                    {
                                        "min": 0,
                                        "max": 4294967295,
                                        "prob": 1
                                    },
                                    {
                                        "min": 4294967296,
                                        "max": 8589934591,
                                        "prob": 1
                                    }
                                ]
                            }
                        }
                    ]
                }
            ]
        }

        example2 = {
            "netflow_version": 9,
            "dst_ipv4": ipv4.V(),
            "dst_ipv6": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "src_port": 30334,
            "domain_id": 2000+example_number,
            "generators": [
                {
                    "name": "dnsv9",
                    "auto_start": True,
                    "rate_pps": 0.5,
                    "data_records_num": 1,
                    "template_id": 260+example_number,
                    "fields": [
                        {
                            "name": "protocolIdentifier",
                            "type": 4,
                            "length": 1,
                            "data": [17]
                        },
                        {
                            "name": "applicationId",
                            "type": 95,
                            "length": 4,
                            "data": [3, 0, 0, 53]
                        },
                        {
                            "name": "flowStartSysUpTime",
                            "type": 22,
                            "length": 4,
                            "data": [0, 0, 0, 1]
                        },
                        {
                            "name": "flowEndSysUpTime",
                            "type": 21,
                            "length": 4,
                            "data": [0, 0, 0, 10]
                        },
                        {
                            "name": "flowStartMilliseconds",
                            "type": 152,
                            "length": 8,
                            "data": [0, 0, 0, 0, 0, 0, 0, 0]
                        },
                        {
                            "name": "responderPackets",
                            "type": 299,
                            "length": 8,
                            "data": [0, 0, 0, 0, 0, 0, 0, 1]
                        },
                        {
                            "name": "initiatorPackets",
                            "type": 298,
                            "length": 8,
                            "data": [0, 0, 0, 0, 0, 0, 0, 1]
                        }
                    ],
                    "engines": [
                        {
                            "engine_name": "protocolIdentifier",
                            "engine_type": "histogram_uint",
                            "params":
                            {
                                "size": 1,
                                "offset": 0,
                                "entries": [
                                    {
                                        "v": 17,
                                        "prob": 5
                                    },
                                    {
                                        "v": 1,
                                        "prob": 1,
                                    },
                                    {
                                        "v": 6,
                                        "prob": 3
                                    }
                                ]
                            },
                        },
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

        example3 = {
            "netflow_version": 9,
            "dst_ipv4": [0, 0, 0, 0],
            "dst_mac": mac.V(),
            "dst_ipv6": ipv6.V(),
            "src_port": 30334,
            "domain_id": 3000+example_number,
            "generators": [
                {
                    "name": "protocolID",
                    "auto_start": True,
                    "rate_pps": 1,
                    "data_records_num": 0,
                    "template_id": 260+example_number,
                    "fields": [
                        {
                            "name": "protocolIdentifier",
                            "type": 4,
                            "length": 1,
                            "data": [17]
                        }
                    ]
                },
                {
                    "name": "ipVersion",
                    "auto_start": True,
                    "rate_pps": 2,
                    "data_records_num": 0,
                    "template_id": 266,
                    "fields": [
                        {
                            "name": "ipVersion",
                            "type": 60,
                            "length": 1,
                            "data": [4]
                        }
                    ]
                }
            ]
        }

        example4 = {
            "netflow_version": 10,
            "dst_ipv4": ipv4.V(),
            "dst_mac": mac.V(),
            "dst_ipv6": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "domain_id": 4000+example_number,
            "generators": [
                {
                    "name": "dns_with_app_id",
                    "auto_start": True,
                    "rate_pps": 1,
                    "data_records_num": 0,
                    "template_id": 260+example_number,
                    "is_options_template": True,
                    "scope_count": 7,
                    "fields": self.get_template_261_fields(),
                    "engines": [
                        {
                            "engine_name": "applicationId",
                            "engine_type": "uint",
                            "params": {
                                "size": 2,
                                "offset": 2,
                                "op": "rand",
                                "min": 20,
                                "max": 50000
                            }
                        }
                    ]
                },
                {
                    "name": "bes",
                    "auto_start": True,
                    "rate_pps": 0.2,
                    "data_records_num": 7,
                    "template_id": 256,
                    "fields": [
                        {
                            "name": "sumServerRespTime",
                            "type": 42074,
                            "length": 4,
                            "enterprise_number": 9,
                            "data": [0, 0, 0, 10]
                        },
                        {
                            "name": "serverTransportProtocol",
                            "type": 45009,
                            "length": 2,
                            "enterprise_number": 9,
                            "data": [0, 53]
                        }
                    ],
                    "engines": [
                        {
                            "engine_name": "serverTransportProtocol",
                            "engine_type": "histogram_uint_list",
                            "params": {
                                "size": 1,
                                "offset": 1,
                                "entries": [
                                    {
                                        "list": [53],
                                        "prob": 5
                                    },
                                    {
                                        "list": [67, 68],
                                        "prob": 4
                                    },
                                    {
                                        "list": [20, 21],
                                        "prob": 5
                                    }
                                ]
                            }
                        }
                    ]
                }
            ]
        }

        examples = [example1, example2, example3, example4]
        return examples[example_number%len(examples)]

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
                c = EMUClientObj(   mac     = mac[j].V(),
                                    ipv4    = ipv4[j].V(),
                                    ipv4_dg = dg.V(),
                                    ipv4_force_dg = True,
                                    ipv4_force_mac = Mac('00:00:00:00:05:01'),
                                    ipv6    = ipv6[j].V(),
                                    plugs  = {'ipfix': self.get_init_json_examples(dst_mac, dst_ipv4, dst_ipv6, j)}
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
            help='Default Gateway address of the clients')
        parser.add_argument('--6', type = str, default = '2001:DB8:1::2', dest = 'ipv6',
            help='IPv6 address of the first client')

        args = parser.parse_args(tuneables)

        assert 0 < args.ns < 65535, 'Namespaces size must be positive! in range of ports: 0 < ns < 65535'
        assert 0 < args.clients, 'Clients size must be positive!'

        return self.create_profile(args.ns, args.clients, args.mac, args.ipv4, args.dg, args.ipv6, args.dst_4)

def register():
    return Prof1()

