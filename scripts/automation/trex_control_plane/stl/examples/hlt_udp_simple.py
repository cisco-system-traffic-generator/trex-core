#!/usr/bin/python

""" 
Sample HLTAPI application (for loopback)
Connect to TRex 
Send UDP packet in specific length 
Each direction has its own IP range
"""

import sys
import argparse
import stl_path
from trex_stl_lib.api import *
from trex_stl_lib.trex_stl_hltapi import *


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage=""" 
    Connect to TRex and send bidirectional continuous traffic

    examples:

     hlt_udp_simple.py --server <hostname/ip>

     hlt_udp_simple.py -s 300 -d 30 -rate_pps 5000000 --src <MAC> --dst <MAC>

     then run the simulator on the output 
       ./stl-sim -f example.yaml -o a.pcap  ==> a.pcap include the packet

    """,
    description="Example for TRex HLTAPI",
    epilog=" based on hhaim's stl_run_udp_simple example");

    parser.add_argument("--server", 
                        dest="server",
                        help='Remote trex address',
                        default="127.0.0.1",
                        type = str)

    parser.add_argument("-s", "--frame-size", 
                        dest="frame_size",
                        help='L2 frame size in bytes without FCS',
                        default=60,
                        type = int,)

    parser.add_argument('-d','--duration', 
                        dest='duration',
                        help='duration in second ',
                        default=10,
                        type = int,)

    parser.add_argument('--rate-pps', 
                        dest='rate_pps',
                        help='speed in pps',
                        default="100")

    parser.add_argument('--src', 
                        dest='src_mac',
                        help='src MAC',
                        default='00:50:56:b9:de:75')

    parser.add_argument('--dst', 
                        dest='dst_mac',
                        help='dst MAC',
                        default='00:50:56:b9:34:f3')

    args = parser.parse_args();

    hltapi = CTRexHltApi()
    print 'Connecting to TRex'
    res = hltapi.connect(device = args.server, port_list = [0, 1], reset = True, break_locks = True)
    check_res(res)
    ports = res['port_handle']
    if len(ports) < 2:
        error('Should have at least 2 ports for this test')
    print 'Connected, acquired ports: %s' % ports

    print 'Creating traffic'

    res = hltapi.traffic_config(mode = 'create', bidirectional = True,
                                port_handle = ports[0], port_handle2 = ports[1],
                                frame_size = args.frame_size,
                                mac_src = args.src_mac, mac_dst = args.dst_mac,
                                mac_src2 = args.dst_mac, mac_dst2 = args.src_mac,
                                l3_protocol = 'ipv4',
                                ip_src_addr = '10.0.0.1', ip_src_mode = 'increment', ip_src_count = 254,
                                ip_dst_addr = '8.0.0.1', ip_dst_mode = 'increment', ip_dst_count = 254,
                                l4_protocol = 'udp',
                                udp_dst_port = 12, udp_src_port = 1025,
                                rate_pps = args.rate_pps,
                                )
    check_res(res)

    print 'Starting traffic'
    res = hltapi.traffic_control(action = 'run', port_handle = ports[:2])
    check_res(res)
    wait_with_progress(args.duration)

    print 'Stopping traffic'
    res = hltapi.traffic_control(action = 'stop', port_handle = ports[:2])
    check_res(res)

    res = hltapi.traffic_stats(mode = 'aggregate', port_handle = ports[:2])
    check_res(res)
    print_brief_stats(res)
    
    res = hltapi.cleanup_session(port_handle = 'all')
    check_res(res)

    print 'Done'
