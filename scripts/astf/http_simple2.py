from trex.astf.api import *


class Prof1():
    def __init__(self):
        pass

    def get_profile(self, **kwargs):
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(dist_client=ip_gen_c, dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen, cap_list=[
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=1, port=81),
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=2, port=82),
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=3, port=83),
        ])


def register():
    return Prof1()
