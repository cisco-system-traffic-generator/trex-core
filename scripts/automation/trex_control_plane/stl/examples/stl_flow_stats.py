import stl_path
from trex_stl_lib.api import *

import time
import pprint

def rx_example (tx_port, rx_port, burst_size):

    print "\nGoing to inject {0} packets on port {1} - checking RX stats on port {2}\n".format(burst_size, tx_port, rx_port)

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

        total_pkts = burst_size
        s1 = STLStream(name = 'rx',
                       packet = pkt,
                       flow_stats = STLRxStats(pg_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts, bps_L2 = 250000000))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add both streams to ports
        c.add_streams([s1], ports = [tx_port])

        print "injecting {0} packets on port {1}\n".format(total_pkts, tx_port)
        c.clear_stats()
        c.start(ports = [tx_port])
        c.wait_on_traffic(ports = [tx_port])

        # no error check - just an example... should be 5
        flow_stats = c.get_stats()['flow_stats'][5]

        tx_pkts  = flow_stats['tx_pkts'][tx_port]
        tx_bytes = flow_stats['tx_bytes'][tx_port]
        rx_pkts  = flow_stats['rx_pkts'][rx_port]

        if tx_pkts != total_pkts:
            print "TX pkts mismatch - got: {0}, expected: {1}".format(tx_pkts, total_pkts)
            passed = False
            return
        else:
            print "TX pkts match   - {0}".format(tx_pkts)

        if tx_bytes != (total_pkts * pkt.get_pkt_len()):
            print "TX bytes mismatch - got: {0}, expected: {1}".format(tx_bytes, (total_pkts * len(pkt)))
            passed = False
            return
        else:
            print "TX bytes match  - {0}".format(tx_bytes)

        if rx_pkts != total_pkts:
            print "RX pkts mismatch - got: {0}, expected: {1}".format(rx_pkts, total_pkts)
            passed = False
            return
        else:
            print "RX pkts match   - {0}".format(rx_pkts)


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
rx_example(tx_port = 0, rx_port = 3, burst_size = 500000)

