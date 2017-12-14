from trex_stl_lib.api import *

# no split
class STLS1(object):
    """ attack 48.0.0.1 at port 80
    """

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream (self):

        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")

        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                        min_value="16.0.0.0", 
                                        max_value="18.0.0.254", 
                                        size=4, op="inc"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),

                           STLVmFixIpv4(offset = "IP"), # fix checksum
                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         mode = STLTXSingleBurst(total_pkts = 20))



    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



