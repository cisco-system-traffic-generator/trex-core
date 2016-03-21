import stl_path
from trex_stl_lib.api import *

import time

def simple_burst ():

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

        # create two bursts and link them
        s1 = STLStream(name = 'A',
                       packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = 5000),
                       next = 'B')
        
        s2 = STLStream(name = 'B',
                       self_start = False,
                       packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = 3000))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [0, 3])

        # add both streams to ports
        stream_ids = c.add_streams([s1, s2], ports = [0, 3])

        # run 5 times
        for i in range(1, 6):
            c.clear_stats()
            c.start(ports = [0, 3], mult = "1gbps")
            c.wait_on_traffic(ports = [0, 3])

            stats = c.get_stats()
            ipackets  = stats['total']['ipackets']

            print("Test iteration {0} - Packets Received: {1} ".format(i, ipackets))
            # (5000 + 3000) * 2 ports = 16,000
            if (ipackets != (16000)):
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
simple_burst()

