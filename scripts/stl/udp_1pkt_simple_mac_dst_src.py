from trex_stl_lib.api import *

# stream will be sent with src MAC addrees dst="60:60:60:60:60:60" and not from default of trex_cfg.yaml port src mac  
class STLS1(object):

    def create_stream (self):
        return STLStream( packet = STLPktBuilder(pkt = Ether(src="61:61:61:61:61:61",dst="60:60:60:60:60:60")/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/(10*'x')),
                          mode = STLTXCont(),
                          #mac_dst_override_mode=STLStreamDstMAC_PKT # another way to explictly take it
                          )

    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



