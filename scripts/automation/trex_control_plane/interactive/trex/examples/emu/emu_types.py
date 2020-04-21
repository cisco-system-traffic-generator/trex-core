#!/usr/bin/python
import emu_path

from trex.emu.trex_emu_conversions import *

mac_v = Mac([1, 2, 3, 4, 5, 0])
mac_s = Mac(Mac('ff:ff:ff:ff:ff:00'))    # can accept same object as well
ipv4_s = Ipv4('224.0.0.255', mc = True)  # mc for multicast validation
ipv4_v = Ipv4([10, 1, 0 ,1])
ipv6_s = Ipv6('FF00:AAAA::1234', mc = True)
ipv6_v = Ipv6([255, 255, 170, 170, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18, 52])

list_types = [mac_s, mac_v, ipv4_s, ipv4_v , ipv6_s, ipv6_v]
incs = [1, 10, -1, -10]

for obj in list_types:
    print("This is my numeric val: %s" % obj.num_val)
    print('str: %s' % obj.S())    # convert to string
    print('bytes: %s' % obj.V())  # convert to list of bytes
    for i in incs:
        print("inc by %s: %s" %(i, obj[i]))
    print('-' * 42)
    