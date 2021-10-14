from trex.astf.api import *
import argparse

# Disable Nagle Algorithm
# Will push any packet with PUSH (*NOT* standard, simulates Spirent)
# and will respond with ACK immediately (standard)


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

        c_glob_info = ASTFGlobalInfo()
        c_glob_info.tcp.no_delay = 2
        c_glob_info.tcp.mss = 1400
        c_glob_info.tcp.initwnd = 1


        s_glob_info = ASTFGlobalInfo()
        s_glob_info.tcp.no_delay = 2
        s_glob_info.tcp.mss = 1400
        s_glob_info.tcp.initwnd = 1


        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,
                           default_s_glob_info=s_glob_info,

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=2.776)
                                     ]
                           )


def register():
    return Prof1()
