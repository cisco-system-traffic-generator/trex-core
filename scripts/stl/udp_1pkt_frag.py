from trex_stl_lib.api import *
import argparse


class STLS1(object):

    def create_stream (self):

        pkt1 = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/(1000*'x')

        frags = fragment(pkt1,fragsize=20)

        return STLStream( 
            packet = 
                    STLPktBuilder(
                        pkt = frags[0]
                    ),
             mode = STLTXCont())

    def get_streams (self, direction, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



