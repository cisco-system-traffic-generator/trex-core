from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self):
        return STLStream( 
            packet = 
                    STLPktBuilder(
                        pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/
                                UDP(dport=12,sport=1025)/(10*'x')
                    ),
             mode = STLTXCont())

    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



