from trex_stl_lib.api import *
import argparse


class STLS1(object):

    def get_streams (self, direction, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        
        # create 1 stream 
        if direction==0:
            src_ip="16.0.0.1"
            dst_ip="48.0.0.1"
        else:
            src_ip="48.0.0.1"
            dst_ip="16.0.0.1"

        pkt   = STLPktBuilder(
                              pkt = Ether()/IP(src=src_ip,dst=dst_ip)/
                              UDP(dport=12,sport=1025)/(10*'x') )

        return [ STLStream( packet = pkt,mode = STLTXCont()) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



