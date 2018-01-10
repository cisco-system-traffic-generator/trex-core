from trex_astf_lib.api import *


class Prof1():
    def __init__(self):
        pass

    def get_profile(self, **kwargs):
        DEFAULT_MSS_C = 500
        DEFAULT_MSS_S = 600
        OVERRIDE_MSS_C = 700
        OVERRIDE_MSS_S = 800

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        c_glob_info = ASTFGlobalInfo()
        s_glob_info = ASTFGlobalInfo()
        c_glob2 = ASTFGlobalInfoPerTemplate()
        s_glob2 = ASTFGlobalInfoPerTemplate()
        c_glob_info.tcp.mss = DEFAULT_MSS_C
        s_glob_info.tcp.mss = DEFAULT_MSS_S
        c_glob2.tcp.mss = OVERRIDE_MSS_C
        s_glob2.tcp.mss = OVERRIDE_MSS_S

        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info, default_s_glob_info=s_glob_info,
                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1),
                                     #  For a specific file, it is possible to override the defaults
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1, port=8080,
                                                 c_glob_info=c_glob2, s_glob_info=s_glob2),
                                     ]
                           )


def register():
    return Prof1()
