from trex.emu.api import *
import argparse
import base64

#simulate SSDP program 

SSDP_L7 = r'''NOTIFY * HTTP/1.1\r
HOST: 239.255.255.250:1900\r
CACHE-CONTROL: max-age=120\r
lOCATION: http://192.168.1.1:7406/rootDesc.xml\r
SERVER: UPnP/Tomato 1.28.7500 MIPSR2Toastman-RT K26 USB Ext UPnP/1.0 MiniUPnPd/1.6\r
NT: upnp:rootdevice\r
USN: uuid:04645c3d-e6b4-4eea-bd35-5c320c12969f::upnp:rootdevice\r
NTS: ssdp:alive\r
OPT: "http://schemas.upnp.org/upnp/1/0/";\r
01-NLS: 1\r
BOOTID.UPNP.ORG: 1\r
CONFIGID.UPNP.ORG: 1337\r
'''

NS_PROG = {
    "buf_list": [
        "",
        "SFRUUC8xLjEgMjAwIE9LDQpTZXJ2ZXI6IE1pY3Jvc29mdC1JSVMvNi4wDQpDb250ZW50LVR5cGU6IHRleHQvaHRtbA0KQ29udGVudC1MZW5ndGg6IDMyMDAwDQoNCjxodG1sPjxwcmU+KioqKioqKioqKjwvcHJlPjwvaHRtbD4="
    ],
    "program_list": [
        {
            "commands": [
                {
                  "msec": 10,
                  "name": "keepalive"
                },
                {
                    "buf_index": 0,
                    "name": "tx_msg"
                },
                {
                    "min_pkts": 1,
                    "name": "rx_msg"
                }
            ]
        },
        {
            "commands": [
                {
                "msec": 10,
                "name": "keepalive"
                },
                {
                    "min_pkts": 1,
                    "name": "rx_msg"
                },
                {
                    "buf_index": 1,
                    "name": "tx_msg"
                }
            ]
        }
    ],

    "templates": [{
        "client_template" :{"program_index": 0,
                "port": 80,
                "cps": 1
              },
        "server_template" : {"assoc": [
                    {
                        "port": 80
                    }
                ],
                "program_index": 1
                }
    }]
} 



def getNsProgram():
    NS_PROG["buf_list"][0]= base64.b64encode(SSDP_L7.encode('ascii')).decode('ascii')
    return NS_PROG


class Prof1():
    def __init__(self):
        self.def_ns_plugs  = {'icmp': {},
                              'arp' : {'enable': True},
                              'transport': {},
                              'appsim': getNsProgram() }

        self.def_c_plugs  = {'arp':  {'enable': True},
                             'icmp': {},
                             'transport': {},
                             }


    def get_init_json(self, is_server):
        """Get Init Json for  Client

        :parameters:
            is_server: bool
                Indicate if the client is a name server.
        """

        simple_client = { "data" : { "s-1" : { "cps": 1.0, "t": "c", "tid": 0, "ipv6": False, "stream": False, "dst" :"239.255.255.250:1900"}}}

        # server , cps is not relevant
        simple_server = { "data" : { "s-1" : { "cps": 1.0, "t": "s", "tid": 0, "ipv6": False, "stream": False, "dst" :"239.255.255.250:1900"}}}

        return simple_server if is_server else simple_client

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
            ipv4 = Ipv4('1.1.5.2')
            ipv6 = Ipv6("2001:DB8:1::3")
            dns_ip = ipv6.S() if dns_ipv6 else ipv4.S()
            dg = Ipv4('1.1.5.1')

            # create a different client each time
            for j in range(clients_size):
                client = EMUClientObj(mac     = mac[j].V(),
                                      ipv4    = ipv4[j].V(),
                                      ipv6    = ipv6[j].V(),
                                      ipv4_dg = dg.V(),
                                      plugs  = {'appsim': self.get_init_json(is_server=j==0),
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

