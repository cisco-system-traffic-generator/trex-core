import sys
import os

# Should be removed 
# TBD fix this 
CURRENT_PATH = os.path.dirname(os.path.realpath(__file__))
API_PATH     = os.path.join(CURRENT_PATH, "../../api/stl")
sys.path.insert(0, API_PATH)

from scapy.all import *
from scapy.contrib.mpls import * # import from contrib folder of scapy 
from trex_stl_api import *


class STLS1(object):

    def __init__ (self):
        pass;

    def create_stream (self):
        # 2 MPLS label the internal with  s=1 (last one)
        pkt =  Ether()/MPLS(label=17,cos=1,s=0,ttl=255)/MPLS(label=12,cos=1,s=1,ttl=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('x'*20)

        # burst of 17 packets
        return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = []),
                         mode = STLTXSingleBurst( pps = 1, total_pkts = 17) )


    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]

def register():
    return STLS1()



