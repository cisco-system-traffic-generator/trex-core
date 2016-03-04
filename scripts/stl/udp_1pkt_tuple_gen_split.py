from trex_stl_lib.api import *


# split the range of IP to cores 
#
class STLS1(object):

    def __init__ (self):
        self.fsize  =64;

    def create_stream (self):
        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = CTRexScRaw( [   STLVmTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.10", 
                                             port_min=1025, port_max=65535,
                                             name="tuple"), # define tuple gen 

                             STLVmWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             STLVmFixIpv4(offset = "IP"),                                # fix checksum
                             STLVmWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                                  ]
                              ,split_by_field = "tuple"  # split to cores base on the tuple generator 
                              );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)
        stream = STLStream(packet = pkt,
                         mode = STLTXCont())
        print stream.to_code()
        return stream


    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



