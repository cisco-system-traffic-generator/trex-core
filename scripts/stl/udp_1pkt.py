from trex_stl_lib.api import *

class STLS1(object):

    def __init__ (self):
        self.mode  =0;
        self.fsize  =64;

    def create_pkt_base (self):
        t=[
        Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025),
        Ether()/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025),
        Ether()/Dot1Q(vlan=12)/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025),
        Ether()/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/TCP(dport=12,sport=1025),
        Ether()/Dot1Q(vlan=12)/IPv6(src="::5")/TCP(dport=12,sport=1025),
        Ether()/IP()/UDP()/IPv6(src="::5")/TCP(dport=12,sport=1025)
        ];
        return t[self.mode]

    def create_stream (self):
        # Create base packet and pad it to size
        size = self.fsize - 4; # HW will add 4 bytes ethernet FCS

        base_pkt = self.create_pkt_base ()

        pad = max(0, size - len(base_pkt)) * 'x'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = [])

        return STLStream(packet = pkt,
                         mode = STLTXCont())



    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



