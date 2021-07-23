from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'icmp': {},
                              'arp' : {'enable': True},
                              'mdns': {},
                              'ipv6' : {'dmac':Mac('00:00:00:70:00:01').V()}}

        self.def_c_plugs  = {'arp':  {'enable': True},
                             'icmp': {},
                             }

    def get_init_json_client(self, domain_name, client, ttl):
        """Get a simple init json"""
        return {
            "hosts": ["emu-client-{}._tcp.local".format(client), "trex-client-{}._tcp.local".format(client)],
            "txt": [
                {
                    "field": "desc",
                    "value": "client-{}".format(client)
                },
                {
                    "field": "proc",
                    "value": "trex-emu"
                },
                {
                    "field": "OS",
                    "value": "Ubuntu"
                }
            ],
            "domain_name": domain_name,
            "ttl": ttl
        }

    def get_init_json_ns(self, clients_size, vport):

        # assumes vports can be {0, 1}
        min_client = vport + 2 
        max_client = 2 * clients_size + vport + 1 # We add 1 so that the generator will always get odd/even MACs.
        min_hostname = 1 - vport
        max_hostname = 2 * (clients_size - 1) + min_hostname + 1 # We add 1 so that the generator will always get odd/even MACs.

        return {
            "auto_play": True,
            "auto_play_params": {
                "rate": 2.0,
                "query_amount": 100,
                "min_client": "00:00:00:70:00:0{}".format(min_client),
                "max_client": "00:00:00:70:00:{:02x}".format(max_client),
                "client_step": 2,
                "hostname_template": "trex-client-%v._tcp.local",
                "min_hostname": min_hostname,
                "max_hostname": max_hostname,
                "hostname_step": 2,
                "type": "A",
                "class": "IN",
                "ipv6": False,
                "program": {
                    "00:00:00:70:00:0{}".format(vport + 2): {
                        "hostnames": ["emu-client-{}._tcp.local".format(min_hostname), "trex-client-{}._tcp.local".format(min_hostname)],
                        "type": "TXT",
                        "class": "IN",
                        "ipv6": True
                    },
                }
            }
        }

    def create_profile(self, clients_size, ttl, domain_name1, domain_name2):

        ns_list = [
            EMUNamespaceObj(ns_key = EMUNamespaceKey(vport  = 0), plugs = {'mdns': self.get_init_json_ns(clients_size, 0)}, def_c_plugs = self.def_c_plugs),
            EMUNamespaceObj(ns_key = EMUNamespaceKey(vport  = 1), plugs = {'mdns': self.get_init_json_ns(clients_size, 1)}, def_c_plugs = self.def_c_plugs)
        ]


        mac = Mac('00:00:00:70:00:02')
        ipv4 = Ipv4('1.1.1.2')
        ipv6 = Ipv6("2001:DB8:1::2")
        dgNs0 = Ipv4('1.1.1.1')
        dgNs1 = Ipv4('1.1.1.128')

        for i in range(0, clients_size*2, 2):
            clientNs0 = EMUClientObj(mac = mac[i].V(),
                                     ipv4 = ipv4[i].V(),
                                     ipv4_dg = dgNs1.V(),
                                     ipv6 = ipv6[i].V(),
                                     plugs = {'mdns': self.get_init_json_client(domain_name1, i, ttl),
                                              'ipv6': {}})
            ns_list[0].add_clients(clientNs0)

            clientNs1 = EMUClientObj(mac = mac[i+1].V(),
                                     ipv4 = ipv4[i+1].V(),
                                     ipv4_dg = dgNs0.V(),
                                     ipv6 = ipv6[i+1].V(),
                                     plugs = {'mdns': self.get_init_json_client(domain_name2, i+1, ttl),
                                              'ipv6': {}})
            ns_list[1].add_clients(clientNs1)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        description = """Argparser for simple mDNS profile.
Because of mDNS this profile requires an appropriate setup. We are using the following loopback configuration
with two ports. Each port represents a namespace.

    port_info:
        - ip: 1.1.1.1
          default_gw: 1.1.1.128
        - ip: 1.1.1.128
          default_gw: 1.1.1.1

Hence the number max number of clients per namespace can be 128/2 - 1 = 63.
        """
        parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawTextHelpFormatter)
        parser.add_argument('--clients', type = int, default = 63,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--ttl', type=int, default=240, help="Time to live for mDNS responses.")
        parser.add_argument('--domain_name1', type=str, default="", help="Domain Name for mDNS namespace #1")
        parser.add_argument('--domain_name2', type=str, default="", help="Domain Name for mDNS namespace #2")

        args = parser.parse_args(tuneables)

        assert args.clients > 0, 'Num of clients must be positive!'
        assert args.clients <= 63, 'Num of clients must be smaller than 64!'

        return self.create_profile(args.clients, args.ttl, args.domain_name1, args.domain_name2)


def register():
    return Prof1()

