from trex_stl_lib.api import *


# step is not 1. 
class STLS1(object):

    def __init__ (self):
        self.fsize  =64; # the size of the packet 

    def create_stream (self):

        # Create base packet and pad it to size
        size = self.fsize - 4; # HW will add 4 bytes ethernet FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = STLScVmRaw( [ STLVmFlowVar(name="mac_src", min_value=1, max_value=30, size=2, op="dec",step=1), 
                           STLVmWrMaskFlowVar(fv_name="mac_src", pkt_offset= 10,pkt_cast_size=2, mask=0xff00,shift=8) # take the var shift it 8 (x256) write only to LSB
                          ]
                       )

        return STLStream(packet = STLPktBuilder(pkt = base_pkt/pad,vm = vm),
                         mode = STLTXCont( pps=10 ))

    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



