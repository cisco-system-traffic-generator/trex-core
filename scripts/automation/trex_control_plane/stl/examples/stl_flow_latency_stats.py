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
        pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/'at_least_16_bytes_payload_needed')
        total_pkts = burst_size
        s1 = STLStream(name = 'rx',
                       packet = pkt,
                       flow_stats = STLFlowLatencyStats(pg_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts,
                                               percentage = bw))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add both streams to ports
        c.add_streams([s1], ports = [tx_port])

        print("\nInjecting {0} packets on port {1}\n".format(total_pkts, tx_port))

        rc = rx_iteration(c, tx_port, rx_port, total_pkts, pkt.get_pkt_len(), bw)
        if not rc:
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

# RX one iteration
def rx_iteration (c, tx_port, rx_port, total_pkts, pkt_len, bw):
    
    c.clear_stats()

    c.start(ports = [tx_port])
    c.wait_on_traffic(ports = [tx_port])

    stats = c.get_stats()
    flow_stats = stats['flow_stats'].get(5)
    lat_stats = stats['latency'].get(5)
    if not flow_stats:
        print("no flow stats available")
        return False
    if not lat_stats:
        print("no latency stats available")
        return False

    tx_pkts  = flow_stats['tx_pkts'].get(tx_port, 0)
    tx_bytes = flow_stats['tx_bytes'].get(tx_port, 0)
    rx_pkts  = flow_stats['rx_pkts'].get(rx_port, 0)
    drops = lat_stats['err_cntrs']['dropped']
    ooo = lat_stats['err_cntrs']['out_of_order']
    dup = lat_stats['err_cntrs']['dup']
    sth = lat_stats['err_cntrs']['seq_too_high']
    stl = lat_stats['err_cntrs']['seq_too_low']
    lat = lat_stats['latency']
    jitter = lat['jitter']
    avg = lat['average']
    tot_max = lat['total_max']
    tot_min = lat['total_min']
    last_max = lat['last_max']
    hist = lat ['histogram']
    
    if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)
            return False

    print('Error counters: dropped:{0}, ooo:{1} dup:{2} seq too high:{3} seq too low:{4}'.format(drops, ooo, dup, sth, stl))
    print('Latency info:')
    print("  Maximum latency(usec): {0}".format(tot_max))
    print("  Minimum latency(usec): {0}".format(tot_min))
    print("  Maximum latency in last sampling period (usec): {0}".format(last_max))
    print("  Average latency(usec): {0}".format(avg))
    print("  Jitter(usec): {0}".format(jitter))
    print("  Latency distribution histogram:")
    l = hist.keys()
    l.sort()
    for sample in l:
        range_start = sample
        if range_start == 0:
            range_end = 10
        else:
            range_end  = range_start + pow(10, (len(str(range_start))-1))
        val = hist[sample]
        print ("    Packets with latency between {0} and {1}:{2} ".format(range_start, range_end, val))
    
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
rx_example(tx_port = 1, rx_port = 0, burst_size = 500000, bw = 50)

