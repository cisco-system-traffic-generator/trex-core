from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self):

        base_pkt = Ether()/IP()/UDP(dport=12, sport=1025)/ (24 * 'x')

        vm = STLVM()
        vm.var(name='var1', min_value=ord('a'), max_value=ord('d'), size=1, step=1, op='inc', next_var='var2')
        vm.write(fv_name='var1', pkt_offset=44)
        vm.var(name='var2', min_value=ord('a'), max_value=ord('c'), size=1, step=1, op='inc', next_var='var3', split_to_cores=False)
        vm.write(fv_name='var2', pkt_offset=43)
        vm.var(name='var3', min_value=ord('a'), max_value=ord('b'), size=1, step=1, op='inc', split_to_cores=False)
        vm.write(fv_name='var3', pkt_offset=42)

        return STLStream(packet = STLPktBuilder(pkt = base_pkt, vm = vm),
                         mode = STLTXCont())


    def get_streams (self, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



