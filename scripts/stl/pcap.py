import os
from trex_stl_lib.api import *

# PCAP profile
class STLPcap(object):

    def __init__ (self, pcap_file):
        self.pcap_file = pcap_file

    def get_streams (self, direction = 0, **kwargs):

        profile = STLProfile.load_pcap(self.pcap_file, ipg_usec = kwargs.get('ipg_usec', 10.0), loop_count = kwargs.get('loop_count', 1))

        return profile.get_streams()



# dynamic load - used for trex console or simulator
def register():
    # get file relative to profile dir
    return STLPcap(os.path.join(os.path.dirname(__file__), 'sample.pcap'))



