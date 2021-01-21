from trex_stl_lib.api import *
import argparse


class STLS1(object):

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream_icmpv6_nd_ns (self):
        target_addr = "2001:0:4137:9350:8000:f12a:b9c8:2815"

        # ICMPv6 NS packet
        base_pkt = Ether()/IPv6(dst=target_addr, src="2001:4860:0:2001::68")/ICMPv6ND_NS(tgt=target_addr)

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_dst", 
                                        min_value="16.0.0.0", 
                                        max_value="18.0.0.254", 
                                        size=4, op="random"),

                           STLVmWrFlowVar(fv_name="ip_dst", pkt_offset ="IPv6.dst", offset_fixup=12),
                           STLVmWrFlowVar(fv_name="ip_dst",
                                          # actual ICMPv6 NS layer name in scapy notation is
                                          # 'ICMPv6 Neighbor Discovery - Neighbor Solicitation'
                                          # which is too verbose
                                          pkt_offset ="{}.tgt".format(base_pkt[2].name),
                                          offset_fixup=12),

                           # must be last 
                           STLVmFixIcmpv6(l3_offset = "IPv6",
                                          l4_offset = base_pkt[2].name)

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,
                         mode = STLTXCont())

    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        args = parser.parse_args(tunables)
        return [ self.create_stream_icmpv6_nd_ns()]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



