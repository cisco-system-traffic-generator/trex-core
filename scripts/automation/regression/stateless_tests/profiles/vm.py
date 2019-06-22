from trex.stl.api import *

class Prof():

    def get_streams(self, direction = 0, pkt_size = 64, cache_size = None, **kwargs):
        size = pkt_size - 4; # HW will add 4 bytes ethernet FCS
        src_ip = '16.0.0.1'
        dst_ip = '48.0.0.1'

        base_pkt = Ether()/IP(src=src_ip,dst=dst_ip)/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = STLScVmRaw( [   STLVmFlowVar ( "ip_src",  min_value="10.0.0.1", max_value="10.0.0.255", size=4, step=1,op="inc"),
                             STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src" ),
                             STLVmFixIpv4(offset = "IP")
                         ],
                         cache_size = cache_size
                        );

        pkt = STLPktBuilder(pkt = base_pkt/pad, vm = vm)
        return STLStream(packet = pkt, mode = STLTXCont())


def register():
    return Prof()
