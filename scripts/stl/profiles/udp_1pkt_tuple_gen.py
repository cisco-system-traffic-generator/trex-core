
from trex_control_plane.stl.api import *

class STLS1(object):

    def __init__ (self):
        self.fsize  =64;

    def create_stream (self):
        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'

        vm = CTRexScRaw( [   CTRexVmDescTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.2", 
                                                   port_min=1025, port_max=65535,
                                                    name="tuple"), # define tuple gen 

                             CTRexVmDescWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             CTRexVmDescFixIpv4(offset = "IP"),                                # fix checksum
                             CTRexVmDescWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                                  ]
                              );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(packet = pkt,
                         mode = STLTXCont())



    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



