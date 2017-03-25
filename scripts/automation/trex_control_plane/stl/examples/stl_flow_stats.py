# Example showing how to define stream for getting per flow statistics, and how to parse the received statistics

import stl_path
from trex_stl_lib.api import *

import time
import pprint

def rx_example (tx_port, rx_port, burst_size, bw):

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
                                               percentage = bw))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add stream to port
        c.add_streams([s1], ports = [tx_port])

        print("\ngoing to inject {0} packets on port {1}\n".format(total_pkts, tx_port))

        rc = rx_iteration(c, tx_port, rx_port, total_pkts, s1.get_pkt_len())
        if not rc:
            passed = False

    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if passed:
        print("\nTest passed :-)\n")
    else:
        print("\nTest failed :-(\n")

# RX one iteration
def rx_iteration (c, tx_port, rx_port, total_pkts, pkt_len):
    ret = True

    c.clear_stats()

    c.start(ports = [tx_port])
    c.wait_on_traffic(ports = [tx_port])

    global_flow_stats = c.get_stats()['flow_stats']
    flow_stats = global_flow_stats.get(5)
    if not flow_stats:
        print("no flow stats available")
        return False

    tx_pkts  = flow_stats['tx_pkts'].get(tx_port, 0)
    tx_bytes = flow_stats['tx_bytes'].get(tx_port, 0)
    rx_pkts  = flow_stats['rx_pkts'].get(rx_port, 0)
    rx_bytes = flow_stats['rx_bytes'].get(rx_port, 0)

    if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)
            return False

    if tx_pkts != total_pkts:
        print("TX pkts mismatch - got: {0}, expected: {1}".format(tx_pkts, total_pkts))
        pprint.pprint(flow_stats)
        ret = False
    else:
        print("TX pkts match   - {0}".format(tx_pkts))

    if tx_bytes != (total_pkts * pkt_len):
        print("TX bytes mismatch - got: {0}, expected: {1}".format(tx_bytes, (total_pkts * pkt_len)))
        pprint.pprint(flow_stats)
        ret = False
    else:
        print("TX bytes match  - {0}".format(tx_bytes))

    if rx_pkts != total_pkts:
        print("RX pkts mismatch - got: {0}, expected: {1}".format(rx_pkts, total_pkts))
        pprint.pprint(flow_stats)
        ret = False
    else:
        print("RX pkts match   - {0}".format(rx_pkts))

    # On x710, by default rx_bytes will be 0. See manual for details.
    # If you use x710, and need byte count, run the TRex server with --no-hw-flow-stat
    if rx_bytes != 0:
        if rx_bytes != total_pkts * pkt_len:
            print("RX bytes mismatch - got: {0}, expected: {1}".format(rx_bytes, (total_pkts * pkt_len)))
            pprint.pprint(flow_stats)
            ret = False
        else:
            print("RX bytes match  - {0}".format(rx_bytes))

    for field in ['rx_err', 'tx_err']:
        for port in global_flow_stats['global'][field].keys():
            if global_flow_stats['global'][field][port] != 0:
                print ("\n{0} on port {1}: {2} - You should consider increasing rx_delay_ms value in wait_on_traffic"
                       .format(field, port, global_flow_stats['global'][field][port]))

    return ret

# run the tests
rx_example(tx_port = 0, rx_port = 1, burst_size = 500, bw = 50)

