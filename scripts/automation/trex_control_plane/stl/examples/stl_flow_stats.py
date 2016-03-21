import stl_path
from trex_stl_lib.api import *

import time
import pprint

def rx_example (tx_port, rx_port, burst_size):

    print("\nGoing to inject {0} packets on port {1} - checking RX stats on port {2}\n".format(burst_size, tx_port, rx_port))

    # create client
    c = STLClient()
    passed = True
    
    try:
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        total_pkts = burst_size
        s1 = STLStream(name = 'rx',
                       packet = pkt,
                       flow_stats = STLFlowStats(pg_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts,
                                               #pps = total_pkts
                                               percentage = 80
                                               ))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add both streams to ports
        c.add_streams([s1], ports = [tx_port])

        print("\ninjecting {0} packets on port {1}\n".format(total_pkts, tx_port))

        for i in range(0, 10):
            print("\nStarting iteration: {0}:".format(i))
            rc = rx_iteration(c, tx_port, rx_port, total_pkts, pkt.get_pkt_len())
            if not rc:
                passed = False
                break


    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if passed:
        print("\nTest has passed :-)\n")
    else:
        print("\nTest has failed :-(\n")

# RX one iteration
def rx_iteration (c, tx_port, rx_port, total_pkts, pkt_len):
    
    c.clear_stats()

    c.start(ports = [tx_port])
    c.wait_on_traffic(ports = [tx_port])

    flow_stats = c.get_stats()['flow_stats'].get(5)
    if not flow_stats:
        print("no flow stats available")
        return False

    tx_pkts  = flow_stats['tx_pkts'].get(tx_port, 0)
    tx_bytes = flow_stats['tx_bytes'].get(tx_port, 0)
    rx_pkts  = flow_stats['rx_pkts'].get(rx_port, 0)

    if tx_pkts != total_pkts:
        print("TX pkts mismatch - got: {0}, expected: {1}".format(tx_pkts, total_pkts))
        pprint.pprint(flow_stats)
        return False
    else:
        print("TX pkts match   - {0}".format(tx_pkts))

    if tx_bytes != (total_pkts * pkt_len):
        print("TX bytes mismatch - got: {0}, expected: {1}".format(tx_bytes, (total_pkts * pkt_len)))
        pprint.pprint(flow_stats)
        return False
    else:
        print("TX bytes match  - {0}".format(tx_bytes))

    if rx_pkts != total_pkts:
        print("RX pkts mismatch - got: {0}, expected: {1}".format(rx_pkts, total_pkts))
        pprint.pprint(flow_stats)
        return False
    else:
        print("RX pkts match   - {0}".format(rx_pkts))

    return True

# run the tests
rx_example(tx_port = 1, rx_port = 2, burst_size = 500000)

