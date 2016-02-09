import stl_path
from trex_stl_lib.api import *

import time
import json

# simple packet creation
def create_pkt (size, direction):

    ip_range = {'src': {'start': "10.0.0.1", 'end': "10.0.0.254"},
                'dst': {'start': "8.0.0.1",  'end': "8.0.0.254"}}

    if (direction == 0):
        src = ip_range['src']
        dst = ip_range['dst']
    else:
        src = ip_range['dst']
        dst = ip_range['src']

    vm = [
        # src
        STLVmFlowVar(name="src",min_value=src['start'],max_value=src['end'],size=4,op="inc"),
        STLVmWriteFlowVar(fv_name="src",pkt_offset= "IP.src"),

        # dst
        STLVmFlowVar(name="dst",min_value=dst['start'],max_value=dst['end'],size=4,op="inc"),
        STLVmWriteFlowVar(fv_name="dst",pkt_offset= "IP.dst"),

        # checksum
        STLVmFixIpv4(offset = "IP")
        ]


    base = Ether()/IP()/UDP()
    pad = max(0, len(base)) * 'x'

    return STLPktBuilder(pkt = base/pad,
                         vm  = vm)


def simple_burst ():
   
  
    # create client
    c = STLClient()
    passed = True

    try:
        # turn this on for some information
        #c.set_verbose("high")

        # create two streams
        s1 = STLStream(packet = create_pkt(200, 0),
                       mode = STLTXCont(pps = 100))

        # second stream with a phase of 1ms (inter stream gap)
        s2 = STLStream(packet = create_pkt(200, 1),
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

        print json.dumps(stats[0], indent = 4, separators=(',', ': '), sort_keys = True)
        print json.dumps(stats[1], indent = 4, separators=(',', ': '), sort_keys = True)

        lost_a = stats[0]["opackets"] - stats[1]["ipackets"]
        lost_b = stats[1]["opackets"] - stats[0]["ipackets"]

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

