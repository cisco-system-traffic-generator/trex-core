from trex_stl_lib.api import *
import os
import argparse


# stream from pcap file. continues pps 10 in sec 

CP = os.path.join(os.path.dirname(__file__))

class STLS1(object):

    def create_stream (self):

        return STLProfile( [ STLStream( isg = 10.0, # start in delay 
                                        name    ='S0',
                                        packet = STLPktBuilder(pkt = os.path.join(CP, "udp_64B_no_crc.pcap")),
                                        mode = STLTXSingleBurst( pps = 10, total_pkts = 10),
                                        next = 'S1'), # point to next stream 

                             STLStream( self_start = False, # Stream is disabled. Will run because it is pointed from S0
                                        name    ='S1',
                                        packet  = STLPktBuilder(pkt = os.path.join(CP, "udp_594B_no_crc.pcap")),
                                        mode    = STLTXSingleBurst( pps = 10, total_pkts = 20),
                                        next    = 'S2' ),

                             STLStream(  self_start = False, # Stream is disabled. Will run because it is pointed from S1
                                         name   ='S2',
                                         packet = STLPktBuilder(pkt = os.path.join(CP, "udp_1518B_no_crc.pcap")),
                                         mode = STLTXSingleBurst( pps = 10, total_pkts = 30 )
                                        )
                            ]).get_streams()


    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # create 1 stream 
        return self.create_stream()



# dynamic load - used for trex console or simulator
def register():
    return STLS1()



