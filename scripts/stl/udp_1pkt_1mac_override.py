from trex_stl_lib.api import *

# 1 clients MAC override the LSB of destination
# overide the destination mac  00:bb::12:34:56:01 -00:bb::12:34:56:0a
class STLS1(object):


    def __init__ (self):
        self.fsize  =64; # the size of the packet 

    def create_stream (self):

        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = CTRexScRaw( [ STLVmFlowVar(name="dyn_mac_src", min_value=1, max_value=10, size=1, op="inc"), # 1 byte varible, range 1-1 ( workaround)

                           STLVmFlowVar(name="static_mac_src_lsb", min_value=0x12345600, max_value=0x12345600, size=4, op="inc"), # workaround to override the mac 4 LSB byte
                           STLVmFlowVar(name="static_mac_src_msb", min_value=0x00bb, max_value=0x00bb, size=2, op="inc"),         # workaround to override the mac 2 MSB byte

                           STLVmWrFlowVar(fv_name="static_mac_src_msb", pkt_offset= 6),                           
                           STLVmWrFlowVar(fv_name="static_mac_src_lsb", pkt_offset= 8),                           

                           STLVmWrFlowVar(fv_name="dyn_mac_src", pkt_offset= 11)                           
                          ]
                       )

        return STLStream(packet = STLPktBuilder(pkt = base_pkt/pad,vm = vm),
                         mode = STLTXCont( pps=10 ))

    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



