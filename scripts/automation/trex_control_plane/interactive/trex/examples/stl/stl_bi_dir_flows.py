from trex.stl.api import *

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
        STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src"),

        # dst
        STLVmFlowVar(name="dst",min_value=dst['start'],max_value=dst['end'],size=4,op="inc"),
        STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst"),

        # checksum
        STLVmFixIpv4(offset = "IP")
        ]


    base = Ether()/IP()/UDP()
    pad = max(0, size-len(base)) * 'x'

    return STLPktBuilder(pkt = base/pad,
                         vm  = vm)


def simple_burst (port_a, port_b, pkt_size, rate):
   
  
    # create client
    c = STLClient()
    passed = True

    try:
        # turn this on for some information
        #c.set_verbose("high")

        # create two streams
        s1 = STLStream(packet = create_pkt(pkt_size, 0),
                       mode = STLTXCont(pps = 100))

        # second stream with a phase of 1ms (inter stream gap)
        s2 = STLStream(packet = create_pkt(pkt_size, 1),
                       isg = 1000,
                       mode = STLTXCont(pps = 100))


        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [port_a, port_b])

        # add both streams to ports
        c.add_streams(s1, ports = [port_a])
        c.add_streams(s2, ports = [port_b])

        # clear the stats before injecting
        c.clear_stats()

        # here we multiply the traffic lineaer to whatever given in rate
        print("Running {:} on ports {:}, {:} for 10 seconds...".format(rate, port_a, port_b))
        c.start(ports = [port_a, port_b], mult = rate, duration = 10)

        # block until done
        c.wait_on_traffic(ports = [port_a, port_b])

        # read the stats after the test
        stats = c.get_stats()

        print(json.dumps(stats[port_a], indent = 4, separators=(',', ': '), sort_keys = True))
        print(json.dumps(stats[port_b], indent = 4, separators=(',', ': '), sort_keys = True))

        lost_a = stats[port_a]["opackets"] - stats[port_b]["ipackets"]
        lost_b = stats[port_b]["opackets"] - stats[port_a]["ipackets"]

        print("\npackets lost from {0} --> {1}:   {2} pkts".format(port_a, port_b, lost_a))
        print("packets lost from {0} --> {1}:   {2} pkts".format(port_b, port_a, lost_b))

        if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)

        if (lost_a == 0) and (lost_b == 0) and not c.get_warnings():
            passed = True
        else:
            passed = False

    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if passed:
        print("\nTest has passed :-)\n")
    else:
        print("\nTest has failed :-(\n")

# run the tests
simple_burst(0, 3, 64, "50%")

