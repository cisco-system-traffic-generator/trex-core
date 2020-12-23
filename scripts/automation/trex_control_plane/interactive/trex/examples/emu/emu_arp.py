#!/usr/bin/python

import emu_path
from trex.emu.api import *

import pprint


c = EMUClient(server='localhost',
                sync_port=4510,
                verbose_level= "error",
                logger=None,
                sync_timeout=None)

port = 0

mac = Mac('00:00:00:70:00:01')
ipv4 = Ipv4('1.1.1.3')
ipv4_dg = Ipv4('1.1.1.1')
emu_client = EMUClientObj(mac = mac.V(),
                        ipv4 = ipv4.V(),
                        ipv4_dg = ipv4_dg.V(),
                        plugs = {'arp': {'enable': True}})
ns_key = EMUNamespaceKey(vport = port)
c_key  = EMUClientKey(ns_key, mac.V()) 
emu_ns = EMUNamespaceObj(ns_key  = ns_key,
                         clients = emu_client)
profile = EMUProfile(ns = emu_ns, def_ns_plugs = {'ipv6':{}})


#  MAKE sure the TRex server is in --software mode promiscuous and to get multicast enabled 
# trex_c.connect()
# trex_c.acquire(ports = my_ports)
# trex_c.set_port_attr(promiscuous=True)  << get all the packets
# # move the mode to service mode in case DHCP is enabled  
# trex_c.set_service_mode(ports = my_ports, enabled = False, filtered = True, mask = (NO_TCP_UDP_MASK | DHCP_MASK ))
# 


# connect and load profile
c.connect()
c.load_profile(profile = profile)

# print tables of namespaces and clients
print("Show all emu ns and clients:")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

# send gratuitous arp
print("Let's enable gratuitous arp")
c.arp.cmd_query(c_key, garp = True)

# show arp cache
res = c.arp.show_cache(ns_key)
print("Arp cache:")
for r in res:
    pprint.pprint(r)

# arp counters
print('Arp counters:')
res = c.arp.get_counters(ns_key, zero = True, verbose = False)
pprint.pprint(res)
