# include the path of trex_stl_api.py
import sys
sys.path.insert(0, "../")

from trex_stl_api import *

import dpkt
import time
import json


def simple_burst ():
    
    # build A side packet
    pkt_a = STLPktBuilder()

    pkt_a.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
    pkt_a.add_pkt_layer("l3_ip", dpkt.ip.IP())
    pkt_a.add_pkt_layer("l4_udp", dpkt.udp.UDP())
    pkt_a.set_pkt_payload("somepayload")
    pkt_a.set_layer_attr("l3_ip", "len", len(pkt_a.get_layer('l3_ip')))

    # build B side packet
    pkt_b = pkt_a.clone()

    # set IP range for pkt and split it by multiple cores
    pkt_a.set_vm_ip_range(ip_layer_name = "l3_ip",
                          ip_field = "src",
                          ip_start="10.0.0.1", ip_end="10.0.0.254",
                          operation = "inc",
                          split = True)

    pkt_a.set_vm_ip_range(ip_layer_name = "l3_ip",
                          ip_field = "dst",
                          ip_start="8.0.0.1", ip_end="8.0.0.254",
                          operation = "inc")


    # build B side packet
    pkt_b.set_vm_ip_range(ip_layer_name = "l3_ip",
                          ip_field = "src",
                          ip_start="8.0.0.1", ip_end="8.0.0.254",
                          operation = "inc",
                          split = True)

    pkt_b.set_vm_ip_range(ip_layer_name = "l3_ip",
                          ip_field = "dst",
                          ip_start="10.0.0.1", ip_end="10.0.0.254",
                          operation = "inc")

   
    # create client
    c = STLClient()
    passed = True

    try:
        # turn this on for some information
        #c.set_verbose("high")

        # create two streams
        s1 = STLStream(packet = pkt_a,
                       mode = STLTXCont(pps = 100))

        # second stream with a phase of 1ms (inter stream gap)
        s2 = STLStream(packet = pkt_b,
                       isg = 1000,
                       mode = STLTXCont(pps = 100))


        # connect to server
        c.connect()

        # prepare our ports (my machine has 0 <--> 1 with static route)
        c.reset(ports = [0, 1])

        # add both streams to ports
        c.add_streams(s1, ports = [0])
        c.add_streams(s2, ports = [1])

        # clear the stats before injecting
        c.clear_stats()

        # choose rate and start traffic for 10 seconds on 5 mpps
        print "Running 5 Mpps on ports 0, 1 for 10 seconds..."
        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        c.wait_on_traffic(ports = [0, 1])

        # read the stats after the test
        stats = c.get_stats()

        print json.dumps(stats["port 0"], indent = 4, separators=(',', ': '), sort_keys = True)
        print json.dumps(stats["port 1"], indent = 4, separators=(',', ': '), sort_keys = True)

        lost_a = stats["port 0"]["opackets"] - stats["port 1"]["ipackets"]
        lost_b = stats["port 1"]["opackets"] - stats["port 0"]["ipackets"]

        print "\npackets lost from 0 --> 1:   {0} pkts".format(lost_a)
        print "packets lost from 1 --> 0:   {0} pkts".format(lost_b)

        if (lost_a == 0) and (lost_b == 0):
            passed = True
        else:
            passed = False

    except STLError as e:
        passed = False
        print e

    finally:
        c.disconnect()

    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"


# run the tests
simple_burst()

