#!/usr/bin/python
import emu_path

from itertools import chain, combinations
from trex.emu.api import *

import pprint

c = EMUClient(server          = 'localhost',
                sync_port     = 4510,
                verbose_level = "error",
                logger        = None,
                sync_timeout  = None)

c.connect()

mac_1, mac_2   = '00:00:00:70:00:01', '00:00:00:70:00:02'
ipv4_1, ipv4_2 = '1.1.1.3', '1.1.1.4'
ipv4_dg        = '1.1.1.1'

emu_client1 = EMUClientObj(mac = mac_1, ipv4 = ipv4_1, ipv4_dg = ipv4_dg)

# plugins only for c2, will override default
c2_plugs = {'arp': {'timer': 30},
            'igmp': {}
}

emu_client2 = EMUClientObj(mac     = mac_2,
                           ipv4    = ipv4_2,
                           ipv4_dg = ipv4_dg,
                           plugs   = c2_plugs)

# Default plugin for all clients in the ns
def_c_plugs = {'arp': {'timer': 50},
               'icmp': {},
}

emu_clients = [emu_client1, emu_client2]
emu_ns = EMUNamespaceObj(vport       = 0,
                         clients     = emu_clients,
                         def_c_plugs = def_c_plugs)

profile = EMUProfile(ns = emu_ns)

# start the emu profile
c.load_profile(profile = profile, max_rate = 2048)

# print tables of namespaces and clients
list_of_ns = c.get_all_ns_and_clients()

# Notice, you can use: "c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)" to print info in a table.
for ns in list_of_ns:
    pprint.pprint(ns)

'''
Should see:
client1 with: arp(timer of 30), icmp.
client2 with: arp(timer of 50), icmp, igmp.
'''