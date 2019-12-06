import stl_path
from trex_stl_lib.api import *

class STLS1(object):

    def __init__ (self):
        pass


    def create_stream (self):

        base_pkt =  Ether()/IP(src="55.55.1.1",dst="58.0.0.1")/UDP(dport=12,sport=1025)

        return STLProfile ([STLStream(name = 'S0',
                                      packet = STLPktBuilder(pkt = base_pkt),
                                      mode = STLTXCont(),
                                      core_id = 0),
                            STLStream(name = 'S1',
                                      packet = STLPktBuilder(pkt = base_pkt),
                                      mode = STLTXCont(),
                                      core_id = 1)
                            ]).get_streams()

    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return self.create_stream()


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



