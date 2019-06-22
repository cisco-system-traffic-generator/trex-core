from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self, step, size, op):

        base_pkt = Ether()/IP()/UDP(dport=12, sport=1025)/ (24 * 'x')

        vm = STLVM()
        vm.var(name='var1', min_value=ord('a'), max_value=ord('e'), size=size, step=step, op=op, next_var='var2')
        vm.write(fv_name='var1', pkt_offset=42+2*size)
        vm.var(name='var2', min_value=ord('a'), max_value=ord('d'), size=size, step=step, op=op, next_var='var3')
        vm.write(fv_name='var2', pkt_offset=42+size)
        vm.var(name='var3', min_value=ord('a'), max_value=ord('c'), size=size, step=step, op=op)
        vm.write(fv_name='var3', pkt_offset=42)
        
        return STLStream(packet = STLPktBuilder(pkt = base_pkt, vm = vm),
                         mode = STLTXCont())


    def get_streams (self, **kwargs):
        # create 1 stream 
        step = kwargs.get('step', 1)
        size = kwargs.get('size', 2)
        op = kwargs.get('op', 'inc')
        return [ self.create_stream(step=step, size=size, op=op) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



