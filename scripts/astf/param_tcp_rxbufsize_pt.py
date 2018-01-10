from trex_astf_lib.api import *

# per template rx-tx buffer size 
#
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

        c_info = ASTFGlobalInfoPerTemplate()
        c_info.tcp.rxbufsize = 256*1024
        c_info.tcp.txbufsize = 256*1024

        s_info = ASTFGlobalInfoPerTemplate()
        s_info.tcp.rxbufsize = 8*1024
        s_info.tcp.txbufsize = 8*1024


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1,
                                     c_glob_info=c_info, s_glob_info=s_info)
                                     ]
                           )


def register():
    return Prof1()
