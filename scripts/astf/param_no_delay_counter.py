from trex.astf.api import *
import argparse

# no_delay_counter tunable example, notice: no_delay_counter will require high `initwnd` in order to increase tcp timer.
# if tcp timer is low it will finish before all packets and send ack earlier. 

class Prof1():
    def __init__(self):
        pass

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--initwnd',
                            type=int,
                            default=10,
                            help='initial wnd size')
        parser.add_argument('--no_delay_counter',
                            type=int,
                            default=4000,
                            help="no_delay_counter will require high initwnd in order to increase tcp timer")
        args = parser.parse_args(tunables)

        initwnd          = args.initwnd
        no_delay_counter = args.no_delay_counter

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
