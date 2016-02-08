import stl_path
from trex_control_plane.stl.api import *

import time

def simple_burst ():
        

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

        # create two bursts and link them
        s1 = STLStream(packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = 5000))
        
        s2 = STLStream(packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = 3000),
                       next_stream_id = s1.get_id())

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [0, 3])

        # add both streams to ports
        stream_ids = c.add_streams([s1, s2], ports = [0, 3])

        # run 5 times
        for i in xrange(1, 6):
            c.clear_stats()
            c.start(ports = [0, 3], mult = "1gbps")
            c.wait_on_traffic(ports = [0, 3])

            stats = c.get_stats()
            ipackets  = stats['total']['ipackets']

            print "Test iteration {0} - Packets Received: {1} ".format(i, ipackets)
            # (5000 + 3000) * 2 ports = 16,000
            if (ipackets != (16000)):
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

