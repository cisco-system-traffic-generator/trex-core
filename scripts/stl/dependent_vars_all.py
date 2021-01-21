from trex_stl_lib.api import *
import argparse


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


    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--step',
                            type = int,
                            default = 1,
                            help="define the steps between each value in the range.")
        parser.add_argument('--size',
                            type = int,
                            default = 2,
                            help="The variable's bytes amount.")
        parser.add_argument('--op',
                            type = str,
                            default = "inc",
                            choices={'inc', 'dec', 'random'},
                            help='''define the we choose the next value in the range.
                                    inc - the value will be chosen by ascending order.
                                    dec - the value will be chosen by descending order.
                                    random - the value will be chosen randomly''')

        args = parser.parse_args(tunables)

        # create 1 stream 
        step = args.step
        size = args.size
        op = args.op
        return [ self.create_stream(step=step, size=size, op=op) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



