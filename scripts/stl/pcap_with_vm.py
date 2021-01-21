import os
from trex_stl_lib.api import *
import argparse
import ast


def validate_IP_dict(ip_dict):
    try:
        ip = ast.literal_eval(ip_dict)
        if not isinstance(ip, dict):
            raise argparse.ArgumentTypeError('Dictionary is expected.')
    except:
        raise argparse.ArgumentTypeError("Couldn't evaluate {} to dictionary.".format(ip_dict))
    if "start" not in ip or "end" not in ip:
        raise argparse.ArgumentTypeError("The dictionary must contain the following keys: (\"start\", \"end\").")
    if not (is_valid_ipv4(ip["start"]) and is_valid_ipv4(ip["end"])):
        raise argparse.ArgumentTypeError("Start and End values must be valid Ipv4 addresses.")
    return ip


# PCAP profile
class STLPcap(object):

    def __init__ (self, pcap_file):
        self.pcap_file = pcap_file

    def create_vm (self, ip_src_range, ip_dst_range):
        if not ip_src_range and not ip_dst_range:
            return None

        vm = STLVM()

        if ip_src_range:
            vm.var(name="src", min_value = ip_src_range['start'], max_value = ip_src_range['end'], size = 4, op = "inc")
            vm.write(fv_name="src",pkt_offset= "IP.src")
            
        if ip_dst_range:
            vm.var(name="dst", min_value = ip_dst_range['start'], max_value = ip_dst_range['end'], size = 4, op = "inc")
            vm.write(fv_name="dst",pkt_offset = 30)

        vm.fix_chksum()
        

        return vm


    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ipg_usec',
                            type=float,
                            default=10.0,
                            help="Inter-packet gap in microseconds.")
        parser.add_argument('--loop_count',
                            type=int,
                            default=5,
                            help="How many times to transmit the cap")
        parser.add_argument('--ip_src_range',
                            type=validate_IP_dict,
                            help="The range of the source IP."
                                 'for example {"start":"48.0.0.1","end":"48.0.0.254"}')
        parser.add_argument('--ip_dst_range',
                            type=validate_IP_dict,
                            default={'start' : '16.0.0.1', 'end': '16.0.0.254'},
                            help="The range of the destination IP.")

        args = parser.parse_args(tunables)

        vm = self.create_vm(args.ip_src_range, args.ip_dst_range)

        profile = STLProfile.load_pcap(self.pcap_file,
                                       ipg_usec = args.ipg_usec,
                                       loop_count = args.loop_count,
                                       vm = vm)

        return profile.get_streams()



# dynamic load - used for trex console or simulator
def register():
    # get file relative to profile dir
    return STLPcap(os.path.join(os.path.dirname(__file__), 'sample.pcap'))



