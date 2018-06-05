from trex.astf.api import *

# IPV6 tunable example 
#
# ipv6.src_msb
# ipv6.dst_msb 
# ipv6.enable
# 

class Prof1():
    def __init__(self):
        pass

    def get_profile(self, **kwargs):

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        c_glob_info = ASTFGlobalInfo()
        c_glob_info.tcp.do_rfc1323 = 0

        s_glob_info = ASTFGlobalInfo()
        s_glob_info.tcp.do_rfc1323 = 0


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,
                           default_s_glob_info=s_glob_info,

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1)
                                     ]
                           )


def register():
    return Prof1()
