from trex.astf.api import *

# no_delay_counter tunable example, notice: no_delay_counter will require high `initwnd` in order to increase tcp timer.
# if tcp timer is low it will finish before all packets and send ack earlier. 

class Prof1():
    def __init__(self):
        pass

    def get_profile(self, **kwargs):

        initwnd          = kwargs.get('initwnd', 10)
        no_delay_counter = kwargs.get('no_delay_counter', 4000)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        c_glob_info = ASTFGlobalInfo()
        c_glob_info.tcp.initwnd = initwnd
        c_glob_info.tcp.no_delay_counter = no_delay_counter

        s_glob_info = ASTFGlobalInfo()
        s_glob_info.tcp.initwnd = initwnd

        return ASTFProfile(default_ip_gen=ip_gen,
                           #  Defaults affects all files
                           default_c_glob_info=c_glob_info,
                           default_s_glob_info=s_glob_info,

                           cap_list=[
                                     ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", limit = 1)
                                     ]
                           )


def register():
    return Prof1()
