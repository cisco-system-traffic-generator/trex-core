from trex_stl_lib.api import *

# Adding header that does not exists yet in Scapy
# This was taken from pull request of Scapy 
# 


# RFC 7348 - Virtual eXtensible Local Area Network (VXLAN):
# A Framework for Overlaying Virtualized Layer 2 Networks over Layer 3 Networks
# http://tools.ietf.org/html/rfc7348
_VXLAN_FLAGS = ['R' for i in range(0, 24)] + ['R', 'R', 'R', 'I', 'R', 'R', 'R', 'R', 'R'] 

class VXLAN(Packet):
    name = "VXLAN"
    fields_desc = [FlagsField("flags", 0x08000000, 32, _VXLAN_FLAGS),
                   ThreeBytesField("vni", 0),
                   XByteField("reserved", 0x00)]

    def mysummary(self):
        return self.sprintf("VXLAN (vni=%VXLAN.vni%)")

bind_layers(UDP, VXLAN, dport=4789)
bind_layers(VXLAN, Ether)


class STLS1(object):

    def __init__ (self):
        pass;

    def create_stream (self):
        pkt =  Ether()/IP()/UDP(sport=1337,dport=4789)/VXLAN(vni=42)/Ether()/IP()/('x'*20)
        #pkt.show2()
        #hexdump(pkt)

        # burst of 17 packets
        return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = []),
                         mode = STLTXSingleBurst( pps = 1, total_pkts = 17) )


    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]

def register():
    return STLS1()



