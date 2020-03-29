#!/usr/bin/python

import emu_path
from trex.emu.api import *

import pprint

EMU_SERVER = "localhost"

c = EMUClient(server=EMU_SERVER,
                sync_port=4510,
                verbose_level= "error",
                logger=None,
                sync_timeout=None)

# connect
c.connect()

mac_1, mac_2   = Mac('00:00:00:70:00:01'), Mac('00:00:00:70:00:02')
ipv4_1, ipv4_2 = Ipv4('1.1.1.3'), Ipv4('1.1.1.4')
ipv4_dg        = Ipv4('1.1.1.1')

emu_client1 = EMUClientObj(mac = mac_1.V(), ipv4 = ipv4_1.V(), ipv4_dg = ipv4_dg.V())

# plugins only for c2, will override default
c2_plugs = {'arp': {'timer': 30},
            'igmp': {}
}

emu_client2 = EMUClientObj(mac     = mac_2.V(),
                           ipv4    = ipv4_2.V(),
                           ipv4_dg = ipv4_dg.V(),
                           plugs   = c2_plugs)

# Default plugin for all clients in the ns
def_c_plugs = {'arp': {'timer': 50},
               'icmp': {},
}

emu_clients = [emu_client1, emu_client2]
ns_key = EMUNamespaceKey(vport = 0)
emu_ns = EMUNamespaceObj(ns_key      = ns_key,
                         clients     = emu_clients,
                         def_c_plugs = def_c_plugs)

profile = EMUProfile(ns = emu_ns)

# start the emu profile
c.load_profile(profile = profile, max_rate = 2048, verbose = True)

# print tables of namespaces and clients
print("Let's see the clients we added")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

# remove all of them
print("Removing Profile..")
c.remove_all_clients_and_ns()

# print again (should be empty)
print("Let's see the profile after removal")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)
