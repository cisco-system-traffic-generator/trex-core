# Example for splitting the ip range to cores in continues way using  per_core_distribution="seq" fields. 
# without that with 2 dual ports and 2 cores per dual the IPs range will be
# 40.125.1.1,40.125.1.3 dual-0
# 40.125.1.2,40.125.1.4 dual-1
# with this  per_core_distribution="seq"
# 40.125.1.1,40.125.1.2 dual-0
# 40.125.1.3,40.125.1.4 dual-1
# this way each core will get range of ips 
# see http_simple_split.py for the profile without this field 

from trex.astf.api import *



class Prof1():
    def __init__(self):
        pass

    def get_profile(self, **kwargs):
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["40.125.1.1", "40.125.1.4"], distribution="seq",per_core_distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["11.140.1.1", "11.140.1.4"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="0.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen,
                            cap_list=[ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",
                            cps=2.776)])


def register():
    return Prof1()

