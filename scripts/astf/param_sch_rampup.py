from trex.astf.api import *

# scheduler.rampup_sec =5 means that it will get to maximum rate after 5 sec 
# CPS will increase linearly (every 1 sec) 


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # only client side should have it, no need for server side 
        c_glob_info = ASTFGlobalInfo()
        c_glob_info.scheduler.rampup_sec = 5


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=2.776)
                                     ]
                           )


def register():
    return Prof1()
