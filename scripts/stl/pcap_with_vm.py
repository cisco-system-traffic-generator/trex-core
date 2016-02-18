import os
from trex_stl_lib.api import *

# PCAP profile
class STLPcap(object):

    def __init__ (self, pcap_file):
        self.pcap_file = pcap_file

    def create_vm (self, ip_src_range, ip_dst_range):
        if not ip_src_range and not ip_dst_range:
            return None

        vm = [STLVmFixIpv4(offset = "IP")]

        if ip_src_range:
            vm += [STLVmFlowVar(name="src", min_value = ip_src_range['start'], max_value = ip_src_range['end'], size = 4, op = "inc"),
                   STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src")
                  ]

        if ip_dst_range:
            vm += [STLVmFlowVar(name="dst", min_value = ip_dst_range['start'], max_value = ip_dst_range['end'], size = 4, op = "inc"),
                   STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst")
                   ]

        return vm


    def get_streams (self,
                     ipg_usec = 10.0,
                     loop_count = 1,
                     ip_src_range = None,
                     ip_dst_range = {'start' : '10.0.0.1', 'end': '10.0.0.254'}):

        vm = self.create_vm(ip_src_range, ip_dst_range)
        profile = STLProfile.load_pcap(self.pcap_file, ipg_usec = ipg_usec, loop_count = loop_count, vm = vm)

        return profile.get_streams()



# dynamic load - used for trex console or simulator
def register():
    # get file relative to profile dir
    return STLPcap(os.path.join(os.path.dirname(__file__), 'sample.pcap'))



