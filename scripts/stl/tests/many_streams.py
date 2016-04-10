from trex_stl_lib.api import *

class STLS1(object):

    def get_streams (self, direction = 0, **kwargs):
        s1 = STLStream(name = 's1',
                       packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXSingleBurst(pps = 100, total_pkts = 7),
                       next = 's2'

                       )
        s2 = STLStream(name = 's2',
                       self_start = False,
                       packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.2")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXSingleBurst(pps = 317, total_pkts = 13),
                       next = 's3'
                       )


        s3 = STLStream(name = 's3',
                       self_start = False,
                       packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.3")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXMultiBurst(pps = 57, pkts_per_burst = 9, count = 5, ibg = 12),
                       next = 's4'
                       )

        s4 = STLStream(name = 's4',
                       self_start = False,
                       packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.3")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXSingleBurst(pps = 4, total_pkts = 22),
                       next = 's5'
                       )

        s5 = STLStream(name = 's5',
                       self_start = False,
                       packet = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.3")/UDP(dport=12,sport=1025)/(10*'x')),
                       mode = STLTXSingleBurst(pps = 17, total_pkts = 27),
                       action_count = 17,
                       next = 's1'
                       )

        return [ s1, s2, s3, s4, s5 ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



