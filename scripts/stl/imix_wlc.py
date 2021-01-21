from trex_stl_lib.api import *
import argparse


class STLImix(object):

    imix_table = [
        {'size': 60,   'pps': 7,  'isg':0 },
        {'size': 590,  'pps': 4,  'isg':0.1 },
        {'size': 1414, 'pps': 1,  'isg':0.2 } # originally 1514, gladius has limitation on 1500 mtu
        ]

    def create_stream (self, size, pps, isg, vm):
        base_pkt = Ether()/IP()/UDP(sport = 2234, dport = 4323, chksum = 0)
        pad = max(0, size - len(base_pkt)) * 'x'
        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(isg = isg,
                         packet = pkt,
                         mode = STLTXCont(pps = pps))


    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for imix_wlc',
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--src',
                            type=str,
                            required=True,
                            help="defines the first source IP.")
        parser.add_argument('--dst',
                            type=str,
                            required=True,
                            help="defines the first destination IP.")
        parser.add_argument('--src_count',
                            type=int,
                            default=1,
                            help="defines the number of source IP. [src, src+src_count-1].")
        parser.add_argument('--dst_count',
                            type=int,
                            default=1,
                            help="defines the number of destination IP. [dst, dst+dst_count-1].")
        parser.add_argument('--port_count',
                            type=int,
                            default=1,
                            help="defines the the number of source port. [2234, 2234+port_count-1].")

        args = parser.parse_args(tunables)

        src = args.src
        dst = args.dst
        src_count = args.src_count
        dst_count = args.dst_count
        port_count = args.port_count


        vm =[
            STLVmFlowVar(name = 'src_ip', min_value = src, max_value = ipv4_str_to_num(is_valid_ipv4_ret(src)) + src_count - 1, size = 4, op = 'inc'),
            STLVmFlowVar(name = 'dst_ip', min_value = dst, max_value = ipv4_str_to_num(is_valid_ipv4_ret(dst)) + dst_count - 1, size = 4, op = 'inc'),
            STLVmFlowVar(name = 'src_port', min_value = 2234, max_value = 2234 + port_count - 1, size = 2, op = 'inc'),

            STLVmWrFlowVar(fv_name='src_ip', pkt_offset= 'IP.src'),
            STLVmWrFlowVar(fv_name='dst_ip', pkt_offset= 'IP.dst'),
            STLVmWrFlowVar(fv_name='src_port', pkt_offset= 'UDP.sport'),
            STLVmFixIpv4(offset = 'IP')

            ]

        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.imix_table]



# dynamic load - used for trex console or simulator
def register():
    return STLImix()



