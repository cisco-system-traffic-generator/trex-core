from trex_stl_lib.api import *

# stream from pcap file. continues pps 10 in sec 

class STLS1(object):

    def get_streams (self, direction = 0):
        return [STLStream(packet = STLPktBuilder(pkt ="stl/yaml/udp_64B_no_crc.pcap"), # path relative to pwd 
                          mode = STLTXCont(pps=1000),
                          flow_stats = STLFlowStats(pg_id = 7)),

                STLStream(packet = STLPktBuilder(pkt ="stl/yaml/udp_594B_no_crc.pcap"), # path relative to pwd 
                          mode = STLTXCont(pps=5000),
                          flow_stats = STLFlowStats(pg_id = 12))
               ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



