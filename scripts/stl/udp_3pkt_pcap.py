from trex_stl_lib.api import *

# stream from pcap file. continues pps 10 in sec 

class STLS1(object):

    def create_stream (self):

        return STLProfile( [ STLStream( isg = 10.0, # star in delay 
                                        name    ='S0',
                                        packet = STLPktBuilder(pkt ="stl/yaml/udp_64B_no_crc.pcap"),
                                        mode = STLTXSingleBurst( pps = 10, total_pkts = 10),
                                        next = 'S1'), # point to next stream 

                             STLStream( self_start = False, # stream is  disabled enable trow S0
                                        name    ='S1',
                                        packet  = STLPktBuilder(pkt ="stl/yaml/udp_594B_no_crc.pcap"),
                                        mode    = STLTXSingleBurst( pps = 10, total_pkts = 20),
                                        next    = 'S2' ),

                             STLStream(  self_start = False, # stream is  disabled enable trow S0
                                         name   ='S2',
                                         packet = STLPktBuilder(pkt ="stl/yaml/udp_1518B_no_crc.pcap"),
                                         mode = STLTXSingleBurst( pps = 10, total_pkts = 30 )
                                        )
                            ]).get_streams()


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return self.create_stream() 



# dynamic load - used for trex console or simulator
def register():
    return STLS1()



