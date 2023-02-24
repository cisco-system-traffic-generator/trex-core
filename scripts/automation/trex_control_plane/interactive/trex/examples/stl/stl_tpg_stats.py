# Example showing how to define stream for Tagged Packet Grouping and how to parse the data.

import stl_path
from trex.stl.api import *

import argparse
from pprint import pformat


MIN_VLAN, MAX_VLAN = 1, (1 << 12) - 1
QINQ_INNER_VLAN, QINQ_OUTTER_VLAN = 1, 100
LINE_LENGTH = 35


def get_tpg_tags(qinq):
    """
    Get the Tagged Packet Groups Tag Mapping.
    This Mapping is used to Map QinQ, Dot1Q to Tag Id.

    Args:
        qinq (bool):  Add a QinQ tag.

    Returns: dict
        Dictionary of mapping QinQ, Dot1Q to tags.
    """
    tpg_conf = []
    # Add one QinQ example, Tag 0.
    if qinq:
        tpg_conf.append(
            {
                "type": "QinQ",
                "value": {
                    "vlans": [QINQ_INNER_VLAN, QINQ_OUTTER_VLAN]
                }
            }
        )
    # Add All Vlans, Vlan i = Tag i
    for i in range(MIN_VLAN, MAX_VLAN):
        tpg_conf.append(
            {
                "type": "Dot1Q",
                "value": {
                    "vlan": i,
                }
            }
        )
    return tpg_conf


def get_streams(burst_size, pps, qinq, vlans):
    """
    Get Single Burst Streams with given pps and burst size.

    Args:
        burst_size (int): Burst size for STL Single Burst Stream.
        pps (int): Packets per Second in Stream.
        qinq (bool): Should add a QinQ stream?
        vlans (int): Number of Vlans starts from 1. Each Vlan is a separate stream -> Separate TPGID.

    Returns: list
        List of Single Burst Streams.
    """
    streams = []
    total_pkts = burst_size
    tpgid = 0
    if qinq:
        pkt = STLPktBuilder(pkt=Ether()/Dot1Q(vlan=QINQ_INNER_VLAN)/Dot1Q(vlan=QINQ_OUTTER_VLAN)
                                    /IP(src="16.0.0.1", dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed')
        streams.append(STLStream(name="QinQ",
                                 packet=pkt,
                                 flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                 mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                       pps=pps)))
        tpgid += 1
    for i in range(MIN_VLAN, vlans + 1):
        pkt = STLPktBuilder(pkt=Ether()/Dot1Q(vlan=i)/IP(src="16.0.0.1",
                                                         dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed')
        streams.append(STLStream(name="vlan_{}".format(i),
                                 packet=pkt,
                                 isg=30 * i, # introduce some isg to avoid bursts in certain setups
                                 flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                 mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                       pps=pps)))
        tpgid += 1
    return streams


def verify_stream_stats(rx_stats, tx_stats, rx_port, tx_port, pkt_len, burst_size, tpgid, verbose, ignore_seq_err):
    """
    Verify Tagged Packet Group Stats per stream.

    Args:
        stats (dict): Dictionary of stats.
        pkt_len (int): Length of the packet in the stream.
        rx_port (int): Port on which we receive the stats.
        tpgid (int): Tagged Packet Group Identifier
        verbose (bool): Be verbose.
        ignore_seq_err (bool): Ignore Sequence errors (can happen if too many vlans). 
                In this case verify only pkts and bytes are verified.

    Returns: bool
        True iff the stream stats are valid.

    """

    ##### Verify Tx Stats #######

    tx_port_stats = tx_stats.get(str(tx_port), None)
    if tx_port_stats is None:
        if verbose:
            print("Port {} not found in Tx stats".format(tx_port))
        return False
    tx_tpgid_stats = tx_port_stats.get(str(tpgid), None)
    if tpgid is None:
        if verbose:
            print("tpgid {} not found in port {} Tx stats".format(tpgid, tx_port))
        return False
    tx_exp = {
        "pkts": burst_size,
        "bytes": burst_size * pkt_len
    }

    if tx_exp != tx_tpgid_stats:
        if verbose:
            print("tpgid {} Tx stats differ from expected: ".format(tpgid))
            print("Want: {}".format(pformat(tx_exp)))
            print("Have: {}".format(pformat(tx_tpgid_stats)))
        return False


    ##### Verify Rx Stats #######

    # We have build the streams in such a way that Stream with tpgid = i should
    # have traffic on tag number i only.
    tag = tpgid
    port_stats = rx_stats.get(str(rx_port), None)
    if port_stats is None:
        if verbose:
            print("Port {} not found in Rx stats".format(rx_port))
        return False
    tpgid_stats = port_stats.get(str(tpgid), None)
    if tpgid is None:
        if verbose:
            print("tpgid {} not found in port {} Rx stats".format(tpgid, rx_port))
        return False
    tag_stats = tpgid_stats.get(str(tag), None)
    if tag_stats is None:
        if verbose:
            print("tag {} not found in tpgid {} Rx stats".format(tag, tpgid))
            return False
    exp = {
        "pkts": burst_size,
        "bytes": burst_size * pkt_len,
        "ooo": 0,
        "dup": 0,
        "seq_err": 0,
        "seq_err_too_big": 0,
        "seq_err_too_small": 0
    }
    if ignore_seq_err:
        relevant_keys = {"pkts", "bytes"}
        tag_stats = {k: v for k, v in tag_stats.items() if k in relevant_keys}
        exp = {k: v for k, v in exp.items() if k in relevant_keys}
    if exp != tag_stats:
        if verbose:
            print("Tag {} Rx stats differ from expected: ".format(tag))
            print("Want: {}".format(pformat(exp)))
            print("Have: {}".format(pformat(tag_stats)))
        return False
    return True


def rx_example(tx_port, rx_port, burst_size, pps, qinq, vlans, verbose, ignore_seq_err):
    """
    Example of using Tagged Packet Groups.

    Send simple single burst streams from tx_port to rx_port for each vlan in range [1-vlans]
    If qinq is true, also provide a qinq stream.
    In the end verify the TPG stats.

    Args:
        tx_port (int): Port in which we transmit traffic.
        rx_port (int): Port in which we receive traffic. This port also collects TPG stats.
        burst_size (int): Burst size for STL Single Burst Stream.
        pps (int): Packets per Second in Stream.
        qinq (bool): Should add a QinQ stream?
        vlans (int): Number of Vlans starts from 1. Each Vlan is a separate stream -> Separate TPGID.
        verbose (bool): Should be verbose when running the test.
        ignore_seq_err (bool): Should ignore sequence errors when verifying test result?
    """

    print("\nGoing to inject {0} packets on port {1} - checking TX/RX stats on ports {2}/{3} respectively.\n".format(
        burst_size, tx_port, tx_port, rx_port))

    # create client
    c = STLClient()
    passed = True
    try:
        streams = get_streams(burst_size, pps, qinq, vlans)

        num_streams = len(streams)

        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports=[tx_port, rx_port])

        # get tpg tags configuration
        tags = get_tpg_tags(qinq)

        num_tags = len(tags)

        # enable tpg
        c.enable_tpg(num_tpgids=num_streams, tags=tags, rx_ports=[rx_port])

        # add the streams
        c.add_streams(streams, ports=[tx_port])

        print("\nInjecting {0} packets on port {1}\n".format(burst_size, tx_port))

        # start the traffic
        c.start(ports=[tx_port], force=True)

        # wait for the traffic to finish
        c.wait_on_traffic(ports=[tx_port])

        # Traffic might have ended but takes some time to parse the packets and update the stats.
        time.sleep(0.5)

        # verify the counters
        for i in range(num_streams):
            # stream number = tpgid
            if verbose:
                print("*"*LINE_LENGTH)
                print("Verifying stats for tpgid {}".format(i))

            tx_stats = c.get_tpg_tx_stats(port=tx_port, tpgid=i)
            rx_stats = c.get_tpg_stats(port=rx_port, tpgid=i, min_tag=0, max_tag=num_tags, max_sections=num_tags)[0]

            tpgid_passed = verify_stream_stats(rx_stats, tx_stats, rx_port, tx_port, streams[i].get_pkt_len(), burst_size, i, verbose, ignore_seq_err)
            if not tpgid_passed:
                passed = False
            if verbose:
                msg = "PASSED" if tpgid_passed else "FAILED"
                print(msg)

    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disable_tpg()
        c.disconnect()

    if passed:
        print("\nTest passed :-)\n")
    else:
        print("\nTest failed :-(\n")


def get_args(argv=None):
    """
    Get Arguments for the script.

    Args:
        argv (list, optional): List of strings to parse. The default is taken from sys.argv. Defaults to None.

    Returns:
        dict: Dictionary with arguments as keys and the user chosen options as values.
    """
    parser = argparse.ArgumentParser(description="Tagged Packet Group Test Argument Parser")
    parser.add_argument("--pps", type=int, default=1, help="Packets per second")
    parser.add_argument("--burst-size", type=int, default=5, help="Burst Size")
    parser.add_argument("--qinq", action="store_true", help="Add a QinQ tag")
    parser.add_argument("--vlans", type=int, default=10, help="Num of Vlans to add starting from 1. If this is big, errors can happen.")
    parser.add_argument("--verbose", action="store_true", help="Be verbose")
    parser.add_argument("--ignore-seq-err", action="store_true", help="Ignore Sequence errors.")
    args = parser.parse_args(argv)
    if args.vlans >= MAX_VLAN:
        raise Exception("Vlan can't be greater or equal {}".format(MAX_VLAN))
    return vars(args)


def main():
    """
    Main Function

    1. Parse argument
    2. Run the test.
    """
    args = get_args()
    rx_example(tx_port=0, rx_port=1, burst_size=args["burst_size"], pps=args["pps"],
               qinq=args["qinq"], vlans=args["vlans"], verbose=args["verbose"],
               ignore_seq_err=args["ignore_seq_err"])


if __name__ == "__main__":
    main()

