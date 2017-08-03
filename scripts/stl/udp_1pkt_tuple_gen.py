from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self, packet_len):
        # Create base packet and pad it to size

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, packet_len - len(base_pkt)) * 'x'

        vm = STLVM()

        # create a tuple var
        vm.tuple_var(name = "tuple", ip_min = "16.0.0.1", ip_max = "16.0.0.2", port_min = 1025, port_max = 2048, limit_flows = 10000)

        # write fields
        vm.write(fv_name = "tuple.ip", pkt_offset = "IP.src")
        vm.fix_chksum()
        
        vm.write(fv_name = "tuple.port", pkt_offset = "UDP.sport")

        pkt = STLPktBuilder(pkt = base_pkt/pad, vm = vm)

        return STLStream(packet = pkt, mode = STLTXCont())



    def get_streams (self, direction = 0, packet_len = 64, **kwargs):
        # create 1 stream 
        return [ self.create_stream(packet_len - 4) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



