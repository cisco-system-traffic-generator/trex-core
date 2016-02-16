from trex_stl_lib.api import *

# 1 clients MAC override the LSB of destination
# overide the src mac  00:bb:12:34:56:01 - 00:bb:12:34:56:0a
class STLS1(object):


    def __init__ (self):
        self.fsize  =64; # the size of the packet 

    def create_stream (self):

        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS

        # Ether(src="00:bb:12:34:56:01") this will tell TRex to take the src-mac from packet and not from config file
        base_pkt =  Ether(src="00:bb:12:34:56:01")/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)  
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = CTRexScRaw( [ STLVmFlowVar(name="dyn_mac_src", min_value=1, max_value=10, size=1, op="inc"), # 1 byte varible, range 1-1 ( workaround)
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



