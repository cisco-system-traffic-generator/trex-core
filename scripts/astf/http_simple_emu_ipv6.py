from trex.astf.api import *
import argparse


class Prof1():
    def __init__(self):
        pass

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["1.1.1.3", "1.1.1.5"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.0.0"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        c_glob_info = ASTFGlobalInfo()
        # Enable IPV6 for client side and set the default SRC/DST IPv6 MSB 
        # LSB will be taken from ip generator 
        c_glob_info.ipv6.src_msb = "2001:DB8:0:2222::"
        c_glob_info.ipv6.dst_msb = "2001:DB8:2:2222::"
        c_glob_info.ipv6.enable  = 1

        return ASTFProfile(default_ip_gen=ip_gen,
                            default_c_glob_info=c_glob_info,
                            cap_list=[ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",
                            cps=2.776)])


def register():
    return Prof1()

