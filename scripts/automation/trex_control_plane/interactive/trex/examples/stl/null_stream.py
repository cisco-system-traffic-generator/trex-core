from trex_stl_lib.api import *

class STLS1(object):

    def __init__ (self):
        self.fsize = 64; # the size of the packet 

    def create_stream (self):
        size = self.fsize - 4  # no FCS
        base_pkt1 = Ether()/IP(src = "16.0.0.1", dst = "48.0.0.1")/UDP(dport = 12, sport = 1025)
        base_pkt2 = Ether()/IP(src = "16.0.0.2", dst = "48.0.0.1")/UDP(dport = 12, sport = 1025)
        pad = max(0, size - len(base_pkt1)) * 'x'

        return STLProfile([ STLStream(	name = 'S1',
                                        packet = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode = STLTXSingleBurst(pps = 10, total_pkts = 5),
                                        next = 'S2'),
                            STLStream(	self_start = False,
                                        isg = 1000000.0, # 1 sec
                                        name = 'S2',
                                        packet = STLPktBuilder(pkt = base_pkt2/pad),
                                        mode = STLTXSingleBurst(pps = 10, total_pkts = 1),
                                        dummy_stream = True,
                                        next = 'S1')
                            ]).get_streams()

    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream
        return self.create_stream() 


# dynamic load - used for trex console or simulator
def register():
    return STLS1()

