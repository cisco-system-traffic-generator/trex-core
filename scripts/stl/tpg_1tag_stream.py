from trex_stl_lib.api import *
import argparse


MIN_VLAN, MAX_VLAN = 1, (1 << 12) - 1
QINQ_INNER_VLAN, QINQ_OUTTER_VLAN = 1, 100


class TPGStreamPerTag(object):

    def create_streams(self, burst_size, pps, qinq, vlans):
        """
        Get Single Burst Streams with given pps and burst size.

        Args:
            burst_size (int): Burst size for STL Single Burst Stream.
            pps (int): Packets per Second in Stream.
            qinq (bool): Add QinQ stream.
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
                                    flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                    mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                        pps=pps)))
            tpgid += 1
        return streams

    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        parser.add_argument("--pps", type=int, default=1, help="Packets per second")
        parser.add_argument("--burst-size", type=int, default=5, help="Burst Size")
        parser.add_argument("--qinq", action="store_true", help="Add a QinQ tag")
        parser.add_argument("--vlans", type=int, default=10, help="Num of Vlans to add starting from 1. If this is big, errors can happen.")
        args = parser.parse_args(tunables)

        if args.vlans >= MAX_VLAN:
            raise Exception("Vlan can't be greater or equal {}".format(MAX_VLAN))

        return self.create_streams(args.burst_size, args.pps, args.qinq, args.vlans)


# dynamic load - used for trex console or simulator
def register():
    return TPGStreamPerTag()




