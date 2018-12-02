from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self):

        base_pkt = Ether()/IP()/UDP(dport=12, sport=1025)/ (24 * 'x')

        vm = STLVM()
        vm.var(name='IP_src', min_value=None, max_value=None, size=4, op='dec', step=1, value_list = ['16.0.0.1', '10.0.0.1', '14.0.0.1'], next_var='var1')
        vm.write(fv_name='IP_src', pkt_offset='IP.src')
        vm.var(name='var1', min_value=ord('a'), max_value=ord('c'), size=1, step=1, op='inc', next_var='var2')
        vm.write(fv_name='var1', pkt_offset=45)
        vm.repeatable_random_var(fv_name='var2', min_value=ord('a'), max_value=ord('z'), size=1, limit=4, seed=0, next_var='var3')
        vm.write(fv_name='var2', pkt_offset=44)
        vm.var(name='var3', min_value=ord('a'), max_value=ord('b'), size=1, step=1, op='inc', split_to_cores=False)
        vm.write(fv_name='var3', pkt_offset=43)

        return STLStream(packet = STLPktBuilder(pkt = base_pkt, vm = vm),
                         mode = STLTXCont())


    def get_streams (self, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



