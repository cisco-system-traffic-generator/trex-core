from trex_stl_lib.api import *

# stream from pcap file. continues pps 10 in sec 
# path_relative_to_profile = True

class STLS1(object):

    def get_streams (self, direction = 0, **kwargs):
        return [STLStream(packet = STLPktBuilder(pkt ="udp_64B_no_crc.pcap",
                                                 path_relative_to_profile = True), # path relative to profile and not to loader path
                         mode = STLTXCont(pps=10)) ] #rate continues, could be STLTXSingleBurst,STLTXMultiBurst


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



