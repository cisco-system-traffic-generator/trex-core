from trex_stl_lib.api import *
import argparse


class STLS1(object):

    def create_stream (self, packet_len):
        # Create base packet and pad it to size

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, packet_len - len(base_pkt)) * 'x'

        vm = STLVM()

        # create a tuple var
        vm.tuple_var(name = "tuple", ip_min = "16.0.0.1", ip_max = "16.0.0.2", port_min = 1025, port_max = 2048, limit_flows = 10000)

        # write fields
        vm.write(fv_name = "tuple.ip", pkt_offset = "IP.src")
        vm.fix_chksum()
        
        vm.write(fv_name = "tuple.port", pkt_offset = "UDP.sport")

        pkt = STLPktBuilder(pkt = base_pkt/pad, vm = vm)

        return STLStream(packet = pkt, mode = STLTXCont())



    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--packet_len',
                            type=int,
                            default=64,
                            help="The packets length in the stream")
        args = parser.parse_args(tunables)
        # create 1 stream 
        return [ self.create_stream(args.packet_len - 4) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



