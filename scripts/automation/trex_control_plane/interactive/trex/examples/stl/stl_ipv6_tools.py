#!/usr/bin/python
import stl_path
from trex.stl.api import *
import sys

'''
Simple script that demonstrates:
1) Automatic discovery of IPv6 nodes
2) Ping first node in the list
'''

c = STLClient(verbose_level = 'info')
c.connect()
c.reset()
c.set_service_mode()

results = c.scan6(ports = [0], timeout = 2, verbose = True)[0]

if not results:
    print('No devices found.')
    sys.exit(0)

# Setting default destination to MAC of first result and ping it

c.set_l2_mode(port = 0, dst_mac = results[0]['mac'])

c.ping_ip(src_port = 0, dst_ip = results[0]['ipv6'])


print('\nDone.\n')