import os
from trex_stl_lib.api import *
import argparse


# PCAP profile
class STLPcap(object):

    def __init__ (self, pcap_file):
        self.pcap_file = pcap_file

    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ipg_usec',
                            type=float,
                            default=10.0,
                            help="Inter-packet gap in microseconds.")
        parser.add_argument('--loop_count',
                            type=int,
                            default=1,
                            help="How many times to transmit the cap")
        args = parser.parse_args(tunables)
        profile = STLProfile.load_pcap(self.pcap_file, ipg_usec = args.ipg_usec, loop_count = args.loop_count)

        return profile.get_streams()



# dynamic load - used for trex console or simulator
def register():
    # get file relative to profile dir
    return STLPcap(os.path.join(os.path.dirname(__file__), 'sample.pcap'))



