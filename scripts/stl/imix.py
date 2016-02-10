from trex_stl_lib.api import *

# IMIX profile - involves 3 streams of UDP packets
# 1 - 60 bytes
# 2 - 590 bytes
# 3 - 1514 bytes
class STLImix(object):

    def __init__ (self):
        # default IP range
        self.ip_range = {'src': {'start': "10.0.0.1", 'end': "10.0.0.254"},
                         'dst': {'start': "8.0.0.1",  'end': "8.0.0.254"}}

        # default IMIX properties
        self.imix_table = [ {'size': 60,   'pps': 28,  'isg':0 },
                            {'size': 590,  'pps': 20,  'isg':0.1 },
                            {'size': 1514, 'pps': 4,   'isg':0.2 } ]


    def create_stream (self, size, pps,isg, vm ):
        # create a base packet and pad it to size
        base_pkt = Ether()/IP()/UDP()
        pad = max(0, size - len(base_pkt)) * 'x'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(isg = isg,
                         packet = pkt,
                         mode = STLTXCont())


    def get_streams (self, direction = 0):

        if direction == 0:
            src = self.ip_range['src']
            dst = self.ip_range['dst']
        else:
            src = self.ip_range['dst']
            dst = self.ip_range['src']

        # construct the base packet for the profile

        vm =[
            # src
            STLVmFlowVar(name="src",min_value=src['start'],max_value=src['end'],size=4,op="inc"),
            STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src"),

            # dst
            STLVmFlowVar(name="dst",min_value=dst['start'],max_value=dst['end'],size=4,op="inc"),
            STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst"),

            # checksum
            STLVmFixIpv4(offset = "IP")

            ]

        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.imix_table]



# dynamic load - used for trex console or simulator
def register():
    return STLImix()



