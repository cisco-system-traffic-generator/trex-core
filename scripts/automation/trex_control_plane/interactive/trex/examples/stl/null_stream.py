from trex_stl_lib.api import *

class STLS1(object):

    def __init__(self):
        self.fsize = 64  # the size of the packet

    def create_stream(self):
        size = self.fsize - 4  # no FCS
        base_pkt = Ether()/IP(src = "16.0.0.1", dst = "48.0.0.1")/UDP(dport = 12, sport = 1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        return STLProfile([ STLStream( name = 'S1',
                                       packet = STLPktBuilder(pkt = base_pkt/pad),
                                       mode = STLTXSingleBurst(pps = 10, total_pkts = 5),
                                       next = 'NULL'),
                            STLStream( self_start = False,
                                       isg = 1000000.0, 
                                       name = 'NULL',
                                       mode = STLTXSingleBurst(),
                                       dummy_stream = True,
                                       next = 'S1')
                            ]).get_streams()
        
    def get_streams(self, direction = 0, **kwargs):
        # create 1 stream
        return self.create_stream()
        
def register():
    return STLS1()

