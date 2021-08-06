from trex.emu.api import *
import argparse


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'icmp': {},
                              'arp' : {'enable': True},
                              'dns': {},
                              'ipv6' : {'dmac':Mac('00:00:00:70:00:01').V()}
                            }

        self.def_c_plugs  = {'arp':  {'enable': True},
                             'icmp': {},
                             'transport': {}
                            }

    def get_init_json_client(self, is_name_server, dns_ip):
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
                "domain1.com": [
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "1.1.1.1"
                    },
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "1.1.1.2"
                    }
                ],
                "domain2.com": [
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "2.2.2.2"
                    },
                    {
                        "type": "AAAA",
                        "class": "IN",
                        "answer": "2001:420:1101:1::2"
                    },
                ],
                "domain3.com": [
                    {
                        "type": "A",
                        "class": "CH",
                        "answer": "3.3.3.3"
                    }
                ],
                "domain4.com": [
                    {
                        "type": "A",
                        "class": "IN",
                        "answer": "4.4.4.4"
                    },
                    {
                        "type": "TXT",
                        "class": "IN",
                        "answer": "desc=domain4, gen=trex"
                    }
                ]
            }
        }

        # add simple entries for domains [5-255]
        for i in range (5, 256):
            name_server["database"]["domain{}.com".format(i)] = [
                {
                    "type": "A",
                    "class": "IN",
                    "answer": "{}.{}.{}.{}".format(i, i, i, i)
                }
            ]

        return name_server if is_name_server else simple_client

    def get_init_json_ns(self, clients_size, vport, query_amount):
        """Get Init Json for Dns Namespace

        :parameters:
            clients_size: int
                Number of clients

            vport: int
                Port

            query_amount: int
                Amount of queries

        Returns:
            [type]: [description]
        """
        max_client = 3 + clients_size - 1 # MAC Address starts from 3.

        return {
            "auto_play": True,
            "auto_play_params": {
                "rate": 0.5,
                "query_amount": query_amount,
                "min_client": "00:00:00:7{}:00:04".format(vport), # the first client is the name server
                "max_client": "00:00:00:7{}:00:{:02x}".format(vport, max_client),
                "client_step": 1,
                "hostname_template": "domain%v.com",
                "min_hostname": 1,
                "max_hostname": clients_size -1,
                "hostname_step": 1,
                "type": "A",
                "class": "IN",
                "program": {
                    "00:00:00:7{}:00:05".format(vport): {
                        "hostnames": ["domain2.com"],
                        "type": "AAAA",
                        "class": "IN",
                    },
                    "00:00:00:7{}:00:06".format(vport): {
                        "hostnames": ["domain3.com"],
                        "type": "A",
                        "class": "Any",
                    },
                    "00:00:00:7{}:00:07".format(vport): {
                        "hostnames": ["domain4.com"],
                        "type": "TXT",
                    }
                }
            }
        }

    def create_profile(self, ns_size, clients_size, dns_ipv6, query_amount):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for i in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = i,
                                     tci     = tci,
                                     tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs, plugs = {'dns': self.get_init_json_ns(clients_size, i, query_amount)})

            mac = Mac('00:00:00:7{}:00:03'.format(i))
            ipv4 = Ipv4('1.1.{}.3'.format(i+1))
            ipv6 = Ipv6("2001:DB8:{}::3".format(i+1))
            dns_ip = ipv6.S() if dns_ipv6 else ipv4.S()
            dg = Ipv4('1.1.{}.1'.format(i+1))

            # create a different client each time
            for j in range(clients_size):
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4[j].V(),
                                      ipv6    = ipv6[j].V(),
                                      ipv4_dg = dg.V(),
                                      plugs  = {'dns': self.get_init_json_client(is_name_server=j==0, dns_ip=dns_ip),
                                                'ipv6': {}
                                                }
                )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def get_profile(self, tuneables):
        # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for Auto Play Emu Dns profile.')
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 5,
                    help='Number of clients to create in each namespace')
        parser.add_argument('--ipv6', action='store_true', help='Use Ipv6 instead of Ipv4')
        parser.add_argument('--amount', type=int, default = 0, help='Amount of queries to sent, 0 means infinite.')

        args = parser.parse_args(tuneables)

        assert args.ns > 0, 'namespaces must be positive!'
        assert args.clients > 0, 'clients must be positive!'
        assert args.clients < 254, 'Namespaces are /24 in this example, hence num clients < 254.'

        return self.create_profile(args.ns, args.clients, args.ipv6, args.amount)


def register():
    return Prof1()

