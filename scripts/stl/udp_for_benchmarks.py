from trex_stl_lib.api import *
import argparse


# Tunable example 
#
#trex>profile -f stl/udp_for_benchmarks.py
#
#Profile Information:
#
#
#General Information:
#Filename:         stl/udp_for_benchmarks.py
#Stream count:          1      
#
#Specific Information:
#Type:             Python Module
#Tunables:         ['stream_count = 1', 'direction = 0', 'packet_len = 64']
#
#trex>start -f stl/udp_for_benchmarks.py -t  packet_len=128 --port 0
#

class STLS1(object):
    '''
    Generalization of udp_1pkt_simple, can specify number of streams and packet length
    '''
    def create_stream (self, packet_len, stream_count):
        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt_len = len(base_pkt)
        base_pkt /= 'x' * max(0, packet_len - base_pkt_len)
        packets = []
        for i in range(stream_count):
            packets.append(STLStream(
                packet = STLPktBuilder(pkt = base_pkt),
                mode = STLTXCont()
                ))
        return packets

    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--packet_len',
                            type=int,
                            default=64,
                            help="The packets length in the stream")
        parser.add_argument('--stream_count',
                            type=int,
                            default=1,
                            help="The number of streams")
        args = parser.parse_args(tunables)
        # create 1 stream 
        return self.create_stream(args.packet_len - 4, args.stream_count)


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



