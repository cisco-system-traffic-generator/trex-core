from trex.astf.api import *
import argparse

# per template ip.tos/ip.ttl for ipv4
#
# 

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

        c_info = ASTFGlobalInfoPerTemplate()
        c_info.ip.tos = 0x80
        c_info.ip.ttl = 100

        c_info_udp = ASTFGlobalInfoPerTemplate()
        c_info_udp.ip.tos = 0x82
        c_info_udp.ip.ttl = 100


        s_info = ASTFGlobalInfoPerTemplate()
        s_info.ip.tos = 0x83
        s_info.ip.ttl = 41

        s_info_udp = ASTFGlobalInfoPerTemplate()
        s_info_udp.ip.tos = 0x84
        s_info_udp.ip.ttl = 42


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1,
                                     c_glob_info=c_info, s_glob_info=s_info),
                                     ASTFCapInfo(file="../avl/delay_dns_0.pcap", cps=1,
                                     c_glob_info=c_info_udp, s_glob_info=s_info_udp)

                                     ]
                           )


def register():
    return Prof1()
