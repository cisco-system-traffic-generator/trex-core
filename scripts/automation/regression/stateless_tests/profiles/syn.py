from trex.stl.api import *

class Prof():

    def get_streams(self, direction = 0, pkt_size = 64, **kwargs):
        size = pkt_size - 4; # HW will add 4 bytes ethernet FCS

        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")
        pad = max(0, size - len(base_pkt)) * 'x'

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                            STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),

                           STLVmFixIpv4(offset = "IP"), # fix checksum

                           STLVmWrFlowVar(fv_name="src_port", 
                                                pkt_offset= "TCP.sport") # fix udp len  

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())


def register():
    return Prof()
