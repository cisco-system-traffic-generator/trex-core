import stl_path
from trex_stl_lib.api import *

import time

def simple_burst (port_a, port_b, pkt_size, burst_size, rate):

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt_base = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()
        pad = max(0, pkt_size - len(pkt_base)) * 'x'
        pkt = STLPktBuilder(pkt = pkt_base / pad)

        # create two bursts and link them
        s1 = STLStream(name = 'A',
                       packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = burst_size),
                       next = 'B')
        
        s2 = STLStream(name = 'B',
                       self_start = False,
                       packet = pkt,
                       mode = STLTXSingleBurst(total_pkts = burst_size))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [port_a, port_b])

        # add both streams to ports
        stream_ids = c.add_streams([s1, s2], ports = [port_a, port_b])

        # run 5 times
        for i in range(1, 6):
            c.clear_stats()
            c.start(ports = [port_a, port_b], mult = rate)
            c.wait_on_traffic(ports = [port_a, port_b])

            stats = c.get_stats()
            ipackets  = stats['total']['ipackets']

            print("Test iteration {0} - Packets Received: {1} ".format(i, ipackets))
            # two streams X 2 ports
            if (ipackets != (burst_size * 2 * 2)):
                passed = False

    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)

    if passed and not c.get_warnings():
        print("\nTest has passed :-)\n")
    else:
        print("\nTest has failed :-(\n")


# run the tests
simple_burst(0, 1, 256, 50000, "10%")

