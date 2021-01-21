from trex.astf.api import *
import argparse


class Prof1():
    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range = ["16.0.0.1", "16.0.0.200"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range = ["48.0.0.1", "48.0.0.200"], distribution="seq")
        ip_gen = ASTFIPGen(glob = ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client = ip_gen_c,
                           dist_server = ip_gen_s)

        return ASTFProfile(default_ip_gen = ip_gen,
                            cap_list = [ASTFCapInfo(file = 'nginx.pcap',
                            cps = 1)])


def register():
    return Prof1()

