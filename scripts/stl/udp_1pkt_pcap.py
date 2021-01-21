from trex_stl_lib.api import *
import os
import argparse

# stream from pcap file. continues pps 10 in sec 

CP = os.path.join(os.path.dirname(__file__))

class STLS1(object):

    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        return [STLStream(packet = STLPktBuilder(pkt = os.path.join(CP, "udp_64B_no_crc.pcap")),
                         mode = STLTXCont(pps=10)) ] #rate continues, could be STLTXSingleBurst,STLTXMultiBurst


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



