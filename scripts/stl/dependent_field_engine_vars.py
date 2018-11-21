from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self):

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        vm = STLVM()
        vm.var(name='var1', min_value=ord('a'), max_value=ord('d'), size=2, step=1, op='inc', next_var='var2')
        vm.write(fv_name='var1', pkt_offset=44)
        vm.var(name='var2', min_value=ord('a'), max_value=ord('c'), size=2, step=1, op='inc', next_var='var3')
        vm.write(fv_name='var2', pkt_offset=43)
        vm.var(name='var3', min_value=ord('a'), max_value=ord('b'), size=2, step=1, op='inc')
        vm.write(fv_name='var3', pkt_offset=42)

        return STLStream(packet = STLPktBuilder(pkt = base_pkt, vm = vm),
                         mode = STLTXCont())


    def get_streams (self, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



