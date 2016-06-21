from trex_stl_lib.api import *

# 1 clients MAC override the LSB of destination
class STLS1(object):

    def __init__ (self):
        self.fsize  =64; # the size of the packet 


    def create_stream (self):

        # Create base packet and pad it to size
        size = self.fsize - 4; # HW will add 4 bytes ethernet FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt1 =  Ether()/IP(src="16.0.0.2",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt2 =  Ether()/IP(src="16.0.0.3",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'


        return STLProfile( [ STLStream( isg = 10.0, # start in delay 
                                        name    ='S0',
                                        packet = STLPktBuilder(pkt = base_pkt/pad),
                                        mode = STLTXSingleBurst( pps = 10, total_pkts = 1),
                                        next = 'S1'), # point to next stream 

                             STLStream( self_start = False, # Stream is disabled. Will run because it is pointed from S0
                                        name    ='S1',
                                        packet  = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode    = STLTXSingleBurst( pps = 10, total_pkts = 2),
                                        next    = 'S2' ),

                             STLStream(  self_start = False, # Stream is disabled. Will run because it is pointed from S1
                                         name   ='S2',
                                         packet = STLPktBuilder(pkt = base_pkt2/pad),
                                         mode = STLTXSingleBurst( pps = 10, total_pkts = 3 ),
                                         action_count = 2, # loop 2 times 
                                         next    = 'S0' # back to S0 loop
                                        )
                            ]).get_streams()


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return self.create_stream() 


# dynamic load - used for trex console or simulator
def register():
    return STLS1()




