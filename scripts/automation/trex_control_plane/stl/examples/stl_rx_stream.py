import stl_path
from trex_stl_lib.api import *

import time
import pprint

def rx_example (tx_port, rx_port):

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        total_pkts = 500000
        s1 = STLStream(name = 'rx',
                       packet = pkt,
                       rx_stats = STLRxStats(user_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts, bps_L2 = 250000000))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add both streams to ports
        c.add_streams([s1], ports = [tx_port])

        print "injecting {0} packets on port {1}".format(total_pkts, tx_port)
        c.clear_stats()
        c.start(ports = [tx_port])
        c.wait_on_traffic(ports = [tx_port])

        time.sleep(1)
        stats = c.get_stats()
        pprint.pprint(stats['rx_stats'])
        print "port 3: ipackets {0}, ibytes {1}".format(stats[3]['ipackets'], stats[3]['ibytes'])
        #rx_stas  = stats['rx_stats']


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
rx_example(tx_port = 0, rx_port = 3)

