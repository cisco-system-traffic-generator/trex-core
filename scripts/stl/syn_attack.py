from trex_stl_lib.api import *

class STLS1(object):
    """ attack 48.0.0.1 at port 80
    """

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream (self):

        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")


        # vm
        vm = CTRexScRaw( [ CTRexVmDescFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                            CTRexVmDescFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           CTRexVmDescWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),

                           CTRexVmDescFixIpv4(offset = "IP"), # fix checksum

                           CTRexVmDescWrFlowVar(fv_name="src_port", 
                                                pkt_offset= "TCP.sport") # fix udp len  

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         mode = STLTXCont())



    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



