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
                        ipv4_dg = ipv4_dg.V())
ns_key = EMUNamespaceKey(vport = port)
emu_ns = EMUNamespaceObj(ns_key  = ns_key,
                         clients = emu_client)
profile = EMUProfile(ns = emu_ns, def_ns_plugs = {'ipv6':{}})

# connect
c.connect()

c.load_profile(profile = profile)

# print tables of namespaces and clients
print("Show all emu ns and clients:")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

# add mld
mlds = [
    [0xFF,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0],
    [0xFF,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,1],
    [0xFF,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,2],
    [0xFF,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,3],
    [0xFF,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,4],
]
print("Adding mlds...")
c.ipv6.add_mld(ns_key, ipv6_vec = mlds)

# show mld * Notice data from API always return as list of bytes *
res = c.ipv6.iter_mld(ns_key)
print("Mld before remove:" )
pprint.pprint(res)

# remove mld
print('Removing %s' % mlds[1:3])
c.ipv6.remove_mld(ns_key, ipv6_vec = mlds[1:3])

# show updated mld
res = c.ipv6.iter_mld(ns_key)
print("Mld after remove:")
pprint.pprint(res)

# ipv6 counters
print('IPv6 counters:')
res = c.ipv6.get_counters(ns_key, zero = True, verbose = False)
pprint.pprint(res)