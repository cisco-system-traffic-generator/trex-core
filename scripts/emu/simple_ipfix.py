from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:03')
        self.def_ns_plugs = {'arp': {'enable': True},
                            'ipv6' : {'dmac':self.mac.V()}}
        self.def_c_plugs = {'arp': {'enable': True}}

    def get_init_json(self, host_port):
        return {
                "netflow_version": 10,
                "dst": host_port.encode(),
                "domain_id": 7777,
                "generators": [
                    {
                        "name": "mix1",
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
                            },
                            {
                                "name": "nbar2HttpHost",
                                "type": 0xafcb,
                                "length": 60,
                                "enterprise_number": 9, 
                                "data":[0] * 60
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
                            },
                            {
                                "engine_name": "nbar2HttpHost",
                                "engine_type": "histogram_url",
                                "params":
                                {
                                    "size": 60,
                                    "offset": 0,
                                    "entries": [
                                        {
                                            "schemes": ["https"],
                                            "hosts": ["google.com"],
                                            "random_queries": True,
                                            "prob": 5
                                        },
                                        {
                                            "schemes": ["ftp"],
                                            "hosts": ["downloads.cisco.com", "software.cisco.com"],
                                            "paths": ["ios-xe", "nx-os"],
                                            "queries": ["version=16", "version=7"],
                                            "prob": 2
                                        },
                                        {
                                            "schemes": ["http", "https"],
                                            "hosts": ["trex-tgn.cisco.com"],
                                            "paths": ["doc", "releases", "sdk"],
                                            "prob": 3
                                        }
                                    ]
                                }
                            }
                        ]
                    },
                    {
                        "name": "mix2",
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
                            },
                            {
                                "name": "flowStartSysUpTime",
                                "type": 0x0016,
                                "length": 4,
                                "data": [0, 0, 0, 0]
                            },
                            {
                                "name": "flowEndSysUpTime",
                                "type": 0x0015,
                                "length": 4,
                                "data": [0, 0, 0, 0]
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
                            },
                            {
                                "engine_name": "flowStartSysUpTime",
                                "engine_type": "time_start",
                                "params":
                                    {
                                        "size": 4,
                                        "offset": 0,
                                        "time_end_engine_name": "flowEndSysUpTime",
                                        "ipg_min": 10000,
                                        "ipg_max": 20000,
                                        "time_offset": 200000
                                    }
                            },
                            {
                                "engine_name": "flowEndSysUpTime",
                                "engine_type": "time_end",
                                "params":
                                    {
                                        "size": 4,
                                        "offset": 0,
                                        "time_start_engine_name": "flowStartSysUpTime",
                                        "duration_min": 5000,
                                        "duration_max": 10000,
                                    }
                            }
                        ]
                    }
                ]
            }

    def create_profile(self, is_ipv6, mac, ipv4, ipv6, dg_4, dst_ipv4, dst_ipv6, dst_port):
        mac = Mac(mac)
        ipv4 = Ipv4(ipv4)
        dg_4 = Ipv4(dg_4)
        ipv6 = Ipv6(ipv6)
        host_port = HostPort(dst_ipv6, dst_port) if is_ipv6 else HostPort(dst_ipv4, dst_port)

        ns_key = EMUNamespaceKey(vport = 0, tci = 0,tpid = 0)
        ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

        c = EMUClientObj(mac = mac.V(),
                         ipv4 = ipv4.V(),
                         ipv6 = ipv6.V(),
                         ipv4_dg = dg_4.V(),
                         plugs  = {'ipfix': self.get_init_json(host_port),
                                   'arp': {'enable': True},
                                   'ipv6': {}})
        ns.add_clients(c)
        return EMUProfile(ns = [ns], def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple_ipfix.py', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        # client args
        parser.add_argument('--is_ipv6', action = 'store_true', dest = 'is_ipv6',
            help="IPv6 traffic?")
        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:03',
            help='Mac address of the client.')
        parser.add_argument('--4', type = str, default = '1.1.1.3', dest = 'ipv4',
            help='IPv4 address of the client.')
        parser.add_argument('--6', type = str, default = '2001:DB8:1::2', dest = 'ipv6',
            help='IPv6 address of the client.')
        parser.add_argument('--dg-4', type = str, default = '1.1.1.1',
            help='IPv4 Address of the Default Gateway.', dest = 'dg_4')
        parser.add_argument('--dst-4', type = str, default = '10.56.97.19', dest = 'dst_4',
            help='IPv4 address of collector')
        parser.add_argument('--dst-6', type = str, default = '2001:DB8:3::1', dest = 'dst_6',
            help='IPv6 address of collector')
        parser.add_argument('--dst-port', type = str, default = '4739', dest = 'dst_port',
            help='Destination Port.')

        args = parser.parse_args(tuneables)
        return self.create_profile(args.is_ipv6, args.mac, args.ipv4, args.ipv6, args.dg_4, args.dst_4, args.dst_6, args.dst_port)

def register():
    return Prof1()
