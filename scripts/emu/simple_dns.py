from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'icmp': {},
                              'arp' : {'enable': True}}

        self.def_c_plugs  = {'arp':  {'enable': True},
                             'icmp': {},
                             'transport': {}
                             }


    def get_init_json(self, is_name_server, dns_ip):
        """Get Init Json for EMU Dns Client

        :parameters:
            is_name_server: bool
                Indicate if the client is a name server.

            dns_ip: str
                IP address of Dns Name Server. Can be Ipv4 or Ipv6.
        """

        simple_client = {
            "name_server": False,
            "dns_server_ip": dns_ip
        }

        name_server = {
            "name_server": True,
            "database": {
                "trex-tgn.cisco.com": [
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "173.36.109.208"
                    }
                ],
                "cisco.com": [
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "72.163.4.185"
                    },
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "72.163.4.161"
                    },
                    {
                        "type": "AAAA",
                        "class": "IN",
                        "answer": "2001:420:1101:1::185"
                    },
                    {
                        "type": "TXT",
                        "class": "IN",
                        "answer": "desc=Best place to work!, location=Israel"
                    }
                ],
                "8.8.8.8.in-addr.arpa": [
                    {
                        "type": "PTR",
                        "class": "IN",
                        "answer": "google.com"
                    }
                ]
            }
        }

        return name_server if is_name_server else simple_client

    def create_profile(self, ns_size, clients_size, dns_ipv6):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = Mac('00:00:00:70:00:03')
            ipv4 = Ipv4('1.1.1.3')
            ipv6 = Ipv6("2001:DB8:1::3")
            dns_ip = ipv6.S() if dns_ipv6 else ipv4.S()
            dg = Ipv4('1.1.1.1')

            # create a different client each time
            for j in range(clients_size):
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4[j].V(),
                                      ipv6    = ipv6[j].V(),
                                      ipv4_dg = dg.V(),
                                      plugs  = {'dns': self.get_init_json(is_name_server=j==0, dns_ip=dns_ip),
                                                'ipv6': {}
                                                }
                )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple Emu Dns profile.')
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 10,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--ipv6', action='store_true', help='Use Ipv6 instead of Ipv4')

        args = parser.parse_args(tuneables)

        assert args.ns > 0, 'namespaces must be positive!'
        assert args.clients > 0, 'clients must be positive!'

        return self.create_profile(args.ns, args.clients, args.ipv6)


def register():
    return Prof1()

