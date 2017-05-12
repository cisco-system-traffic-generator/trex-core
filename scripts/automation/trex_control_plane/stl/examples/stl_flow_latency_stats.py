# Example showing how to define stream for latency measurement, and how to parse the latency information

import stl_path
from trex_stl_lib.api import *

import time
import pprint

def rx_example (tx_port, rx_port, burst_size, pps):

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
                                               pps = pps))

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [tx_port, rx_port])

        # add both streams to ports
        c.add_streams([s1], ports = [tx_port])

        print("\nInjecting {0} packets on port {1}\n".format(total_pkts, tx_port))

        rc = rx_iteration(c, tx_port, rx_port, total_pkts, pkt.get_pkt_len())
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
    
    c.clear_stats()

    c.start(ports = [tx_port])
    pgids = c.get_active_pgids()
    print ("Currently used pgids: {0}".format(pgids))

    for i in range(1,8):
        time.sleep(1)
        stats = c.get_pgid_stats(pgids['latency'])
        flow_stats = stats['flow_stats'].get(5)
        rx_pps = flow_stats['rx_pps'][rx_port]
        tx_pps = flow_stats['tx_pps'][tx_port]
        rx_bps = flow_stats['rx_bps'][rx_port]
        tx_bps = flow_stats['tx_bps'][tx_port]
        rx_bps_l1 = flow_stats['rx_bps_l1'][rx_port]
        tx_bps_l1 = flow_stats['tx_bps_l1'][tx_port]
        print("rx_pps:{0} tx_pps:{1}, rx_bps:{2}/{3} tx_bps:{4}/{5}"
              .format(rx_pps, tx_pps, rx_bps, rx_bps_l1, tx_bps, tx_bps_l1));
    c.wait_on_traffic(ports = [tx_port])
    stats = c.get_pgid_stats(pgids['latency'])
    flow_stats = stats['flow_stats'].get(5)
    global_lat_stats = stats['latency']
    lat_stats = global_lat_stats.get(5)
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
    old_flow = global_lat_stats['global']['old_flow']
    bad_hdr = global_lat_stats['global']['bad_hdr']
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
    if old_flow:
        print ('Packets arriving too late after flow stopped: {0}'.format(old_flow))
    if bad_hdr:
        print ('Latency packets with corrupted info: {0}'.format(bad_hdr))
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

    if tx_bytes != (total_pkts * (pkt_len + 4)): # +4 for ethernet CRC
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
rx_example(tx_port = 0, rx_port = 1, burst_size = 1000, pps = 100)

