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
        ip_gen_c = ASTFIPGenDist(ip_range=["172.28.0.0", "172.28.100.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["127.30.0.0", "127.30.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        c_glob_info = ASTFGlobalInfo()
        c_glob_info.ipv6.src_msb ="2a00:4986:04ff:65a0:0000:0000::"
        c_glob_info.ipv6.dst_msb ="2a00:4986:04ff:65a0:0000:0000::"
        c_glob_info.ipv6.enable  =1



        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,
                           cap_list=[
                                     ASTFCapInfo(file="../cap2/http_post.pcap", cps=100)
                                     ]
                           )


def register():
    return Prof1()
