from trex_stl_lib.api import *

class STLS1(object):

    def get_streams (self, direction = 0, **kwargs):
        s1 = STLStream(packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXSingleBurst(total_pkts = 27))

        return [s1]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



