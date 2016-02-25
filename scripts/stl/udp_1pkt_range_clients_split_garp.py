from trex_stl_lib.api import *


# send G ARP from many clients
# clients are   "00:00:dd:dd:00:01+x", psrc="55.55.1.1+x" DG= self.dg

class STLS1(object):

    def __init__ (self):
        self.num_clients  =3000; # max is 16bit

    def create_stream (self):
        # create a base packet and pad it to size
        base_pkt =  Ether(src="00:00:dd:dd:00:01",dst="ff:ff:ff:ff:ff:ff")/ARP(psrc="55.55.1.1",hwsrc="00:00:dd:dd:00:01", hwdst="00:00:dd:dd:00:01", pdst="55.55.1.1")

        vm = CTRexScRaw( [ STLVmFlowVar(name="mac_src", min_value=1, max_value=self.num_clients, size=2, op="inc"),
                           STLVmWrFlowVar(fv_name="mac_src", pkt_offset= 10),                                        
                           STLVmWrFlowVar(fv_name="mac_src" ,pkt_offset="ARP.psrc",offset_fixup=2),                
                           STLVmWrFlowVar(fv_name="mac_src" ,pkt_offset="ARP.hwsrc",offset_fixup=4),
                           STLVmWrFlowVar(fv_name="mac_src" ,pkt_offset="ARP.pdst",offset_fixup=2),                
                           STLVmWrFlowVar(fv_name="mac_src" ,pkt_offset="ARP.hwdst",offset_fixup=4),
                          ]
                         ,split_by_field = "mac_src"  # split 
                        )

        return STLStream(packet = STLPktBuilder(pkt = base_pkt,vm = vm),
                         mode = STLTXSingleBurst( pps=10, total_pkts = self.num_clients  )) # single burst of G-ARP

    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



