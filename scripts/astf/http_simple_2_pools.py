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
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        ip_gen_c1 = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s1 = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen1 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c1,
                           dist_server=ip_gen_s1)

        ip_gen_c2 = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s2 = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen2 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c2,
                           dist_server=ip_gen_s2)

        return ASTFProfile(default_ip_gen=ip_gen,
                            cap_list=[
                            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=2.776,ip_gen=ip_gen1,port=100),
                            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=2.776,ip_gen=ip_gen2)

                            ])


def register():
    return Prof1()

