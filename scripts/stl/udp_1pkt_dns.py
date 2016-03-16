from trex_stl_lib.api import *
from scapy.layers.dns import * # import from layers. in default only ipv4/ipv6 are imported for speedup 


class STLS1(object):

    def __init__ (self):
        pass;

    def create_stream (self):
        # DNS
        pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(sport=1025)/DNS();

        # burst of 17 packets
        return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = []),
                         mode = STLTXSingleBurst( pps = 1, total_pkts = 17) )


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]

def register():
    return STLS1()



