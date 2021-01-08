#!/usr/bin/python

import emu_path
from trex.emu.api import *
from trex.emu.trex_emu_conversions import Mac, Ipv4

import pprint


c = EMUClient(server='localhost',
                sync_port=4510,
                verbose_level= "error",
                logger=None,
                sync_timeout=None)

port = 0
mac = Mac('00:00:00:70:00:01')
ipv4, ipv4_dg = Ipv4('1.1.1.3'), Ipv4('1.1.1.1')
emu_client = EMUClientObj(mac = mac.V(),
                        ipv4 = ipv4.V(),
                        ipv4_dg = ipv4_dg.V(),
                        plugs = {'igmp': {}})
ns_key = EMUNamespaceKey(vport = port)
emu_ns = EMUNamespaceObj(ns_key  = ns_key,
                         clients = emu_client)
profile = EMUProfile(ns = emu_ns, def_ns_plugs = {'ipv6':{}})

# connect and load profile
c.connect()
c.load_profile(profile = profile)

# print tables of namespaces and clients
print("Show all emu ns and clients:")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

mcs = [[224,0,0,0], [224,1,0,0], [224,2,0,0], [224,3,0,0]]

# add multicast addresses
print("Adding multicasts: %s" % mcs)
c.igmp.add_mc(ns_key, mcs)

# show multicast addresses * Notice data from API always return as list of bytes *
res = c.igmp.iter_mc(ns_key)
print("Current multicasts:")
for i, mc in enumerate(res, start = 1):
    print('Multicast #{0}: {1}'.format(i, mc))

# remove some multicast addresses
print('Removing mcs: {0}'.format(mcs[1:3]))
c.igmp.remove_mc(ns_key, mcs[1:3])

# show multicast addresses after removeal
print('After removal, current mcs:')
res = c.igmp.iter_mc(ns_key)
for i, mc in enumerate(res, start = 1):
    print('Multicast #{0}: {1}'.format(i, mc))

# igmp counters
print('IGMP counters:')
res = c.igmp.get_counters(ns_key, zero = True, verbose = False)
pprint.pprint(res)