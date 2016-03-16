from trex_stl_lib.api import *


class STLS1(object):

    def create_stream (self):
        # Teredo Ipv6 over Ipv4 
        pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=3797,sport=3544)/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")/UDP(dport=12,sport=1025)/ICMPv6Unknown()

        vm = STLScVmRaw( [ 
                            # tuple gen for inner Ipv6 
                            STLVmTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.2", 
                                            port_min=1025, port_max=65535,
                                            name="tuple"), # define tuple gen 

                             STLVmWrFlowVar (fv_name="tuple.ip", pkt_offset= "IPv6.src",offset_fixup=12 ), # write ip to packet IPv6.src to LSB
                             STLVmWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP:1.sport" )  #write udp.port (after ipv6)
                          ]
                       )

        # burst of 100 packets
        return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = vm),
                         mode = STLTXSingleBurst( pps = 1, total_pkts = 17) )


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]

def register():
    return STLS1()




