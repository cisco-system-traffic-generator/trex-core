from trex_stl_lib.api import *
import argparse


# in case of azure packet with 60 bytes size are not optimized so we change the imix a bit to start from 68 
# IMIX profile - involves 3 streams of UDP packets
# 1 - 60 bytes
# 2 - 590 bytes
# 3 - 1514 bytes
class STLImix(object):

    def __init__ (self):
        # default IP range
        self.ip_range = {'src': {'start': "16.0.0.1", 'end': "16.0.0.254"},
                         'dst': {'start': "48.0.0.1",  'end': "48.0.0.254"}}

        # default IMIX properties
        self.imix_table = [ {'size': 68,   'pps': 28,  'isg':0 },
                            {'size': 590,  'pps': 16,  'isg':0.1 },
                            {'size': 1514, 'pps': 4,   'isg':0.2 } ]


    def create_stream (self, size, pps, isg, vm ):
        # Create base packet and pad it to size
        base_pkt = Ether()/IP()/UDP(chksum=0)
        pad = max(0, size - len(base_pkt)) * 'x'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(isg = isg,
                         packet = pkt,
                         mode = STLTXCont(pps = pps))


    def get_streams (self, direction, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        args = parser.parse_args(tunables)
        if direction == 0:
            src = self.ip_range['src']
            dst = self.ip_range['dst']
        else:
            src = self.ip_range['dst']
            dst = self.ip_range['src']

        # construct the base packet for the profile
        vm = STLVM()
        
        # define two vars (src and dst)
        vm.var(name="src",min_value=src['start'],max_value=src['end'],size=4,op="inc")
        vm.var(name="dst",min_value=dst['start'],max_value=dst['end'],size=4,op="inc")
        
        # write them
        vm.write(fv_name="src",pkt_offset= "IP.src")
        vm.write(fv_name="dst",pkt_offset= "IP.dst")
        
        # fix checksum
        vm.fix_chksum()
        
        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.imix_table]



# dynamic load - used for trex console or simulator
def register():
    return STLImix()



