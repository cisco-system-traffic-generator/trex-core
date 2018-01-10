from trex_astf_lib.api import *

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
        # Enable IPV6 for client side and set the default SRC/DST IPv6 MSB 
        # LSB will be taken from ip generator 
        c_glob_info.ipv6.src_msb ="ff02::"
        c_glob_info.ipv6.dst_msb ="ff03::"
        c_glob_info.ipv6.enable  =1


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,
                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1)
                                     ]
                           )


def register():
    return Prof1()
