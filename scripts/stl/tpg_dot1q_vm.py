from trex_stl_lib.api import *
import argparse


MIN_VLAN, MAX_VLAN = 1, (1 << 12) - 1


class Dot1QFieldEngine(object):

    def create_streams(self, burst_size, pps, vlans):
        """
        Get Single Burst Streams with given pps and burst size.

        Args:
            burst_size (int): Burst size for STL Single Burst Stream.
            pps (int): Packets per Second in Stream.
            vlans (int): Number of Vlans starts from 1.

        Returns: list
            List of Single Burst Streams.
        """

        vm = STLScVmRaw( [ STLVmFlowVar(name="dot1q_tag",
                                        min_value=1,
                                        max_value=vlans,
                                        size=2, op="inc"),
                            STLVmWrFlowVar(fv_name="dot1q_tag", pkt_offset= "Dot1Q.vlan" )])

        base_pkt = Ether()/Dot1Q(vlan=1)/IP(src="16.0.0.1", dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed'

        pkt = STLPktBuilder(pkt=base_pkt, vm=vm)

        stream = STLStream(name="dot1q_tpg",
                           packet=pkt,
                           flow_stats=STLTaggedPktGroup(tpgid=0),
                           mode=STLTXSingleBurst(total_pkts=burst_size, pps=pps))

        return [stream]

    def get_streams (self, tunables, **kwargs):
        description = """
        Argparser for {}.
        This profile has one stream with a field engine that install Dot1Q vlans with tpgid 0.
        NOTE: This profile is meant to have sequence errors.
        With each Vlan increase through the field engine, the sequence is also increased.
        """.format(os.path.basename(__file__))
        parser = argparse.ArgumentParser(description=description,
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        parser.add_argument("--pps", type=int, default=40, help="Packets per second")
        parser.add_argument("--burst-size", type=int, default=200, help="Burst Size")
        parser.add_argument("--vlans", type=int, default=50, help="Num of Vlans to add starting from 1.")
        args = parser.parse_args(tunables)

        if args.vlans >= MAX_VLAN:
            raise Exception("Vlan can't be greater or equal {}".format(MAX_VLAN))

        # create 1 stream
        return self.create_streams(args.burst_size, args.pps, args.vlans)


# dynamic load - used for trex console or simulator
def register():
    return Dot1QFieldEngine()




