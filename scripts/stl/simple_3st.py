from trex_stl_lib.api import *

# 1 clients MAC override the LSB of destination
class STLS1(object):

    def __init__ (self):
        self.fsize  =64; # the size of the packet 


    def create_stream (self):

        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt1 =  Ether()/IP(src="16.0.0.2",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt2 =  Ether()/IP(src="16.0.0.3",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'


        return STLProfile( [ STLStream( isg = 1.0, # star in delay 
                                        packet = STLPktBuilder(pkt = base_pkt/pad),
                                        mode = STLTXCont( pps = 10),
                                        ), # point to next stream 

                             STLStream( isg = 2.0,
                                        packet  = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode    = STLTXCont( pps = 20),
                                        ),

                             STLStream(  isg = 3.0,
                                         packet = STLPktBuilder(pkt = base_pkt2/pad),
                                         mode    = STLTXCont( pps = 30)
                                         
                                        )
                            ]).get_streams()


    def get_streams (self, direction = 0):
        # create 1 stream 
        return self.create_stream() 


# dynamic load - used for trex console or simulator
def register():
    return STLS1()




