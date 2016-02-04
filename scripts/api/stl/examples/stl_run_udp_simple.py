#!/usr/bin/python
import sys, getopt
import argparse;


sys.path.insert(0, "../")

from trex_stl_api import *

H_VER = "trex-x v0.1 "

class t_global(object):
     args=None;


import dpkt
import time
import json
import string

def generate_payload(length):
    word = ''
    alphabet_size = len(string.letters)
    for i in range(length):
        word += string.letters[(i % alphabet_size)]
    return word

# simple packet creation
def create_pkt (frame_size = 9000, direction=0):

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

    pkt_base  = Ether(src="00:00:00:00:00:01",dst="00:00:00:00:00:02")/IP()/UDP(dport=12,sport=1025)
    pyld_size = frame_size - len(pkt_base);
    pkt_pyld   = generate_payload(pyld_size) 

    return STLPktBuilder(pkt = pkt_base/pkt_pyld,
                         vm  = vm)


def simple_burst (duration = 10, frame_size = 9000, speed = '1gbps'):
   
    if (frame_size < 60):
        frame_size = 60

    pkt_dir_0 = create_pkt (frame_size, 0) 

    pkt_dir_1 = create_pkt (frame_size, 1) 

    # create client
    c = STLClient(server = t_global.args.ip)

    passed = True

    try:
        # turn this on for some information
        #c.set_verbose("high")

        # create two streams
        s1 = STLStream(packet = pkt_dir_0,
                       mode = STLTXCont(pps = 100))

        # second stream with a phase of 1ms (inter stream gap)
        s2 = STLStream(packet = pkt_dir_1,
                       isg = 1000,
                       mode = STLTXCont(pps = 100))

        if t_global.args.debug:
            STLStream.dump_to_yaml ("example.yaml", [s1,s2]) # export to YAML so you can run it on simulator ./stl-sim -f example.yaml -o o.pcap 

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
        print "Running {0} on ports 0, 1 for 10 seconds, UDP {1}...".format(speed,frame_size+4)
        c.start(ports = [0, 1], mult = speed, duration = duration)

        # block until done
        c.wait_on_traffic(ports = [0, 1])

        # read the stats after the test
        stats = c.get_stats()

        #print stats
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
        print "\nPASSED\n"
    else:
        print "\nFAILED\n"

def process_options ():
    parser = argparse.ArgumentParser(usage=""" 
    connect to TRex and send burst of packets 
    """,
    description="example for TRex api",
    epilog=" written by hhaim");

    parser.add_argument("-s", "--frame-size", 
                        dest="frame_size",
                        help='L2 frame size in bytes without FCS',
                        default=60,
                        type = int,
                        )

    parser.add_argument("--ip", 
                        dest="ip",
                        help='remote trex ip default local',
                        default="127.0.0.1",
                        type = str
                        )


    parser.add_argument('-d','--duration', 
                        dest='duration',
                        help='duration in second ',
                        default=10,
                        type = int,
                        )


    parser.add_argument('-m','--multiplier', 
                        dest='mul',
                        help='speed in gbps/pps for example 1gbps, 1mbps, 1mpps ',
                        default="1mbps"
                        )

    parser.add_argument('--debug', 
                        action='store_true',
                        help='see debug into ')

    parser.add_argument('--version', action='version',
                        version=H_VER )

    t_global.args = parser.parse_args();
    print t_global.args



def main():
    process_options ()
    simple_burst(duration = t_global.args.duration,  
                 frame_size = t_global.args.frame_size,
                 speed = t_global.args.mul
                 )

if __name__ == "__main__":
    main()

