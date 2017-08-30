import os
from trex_astf_lib.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        ip_gen_c1 = ASTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s1 = ASTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen1 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c1,
                            dist_server=ip_gen_s1)

        ip_gen_c2 = ASTFIPGenDist(ip_range=["10.0.0.1", "10.0.0.255"], distribution="seq")
        ip_gen_s2 = ASTFIPGenDist(ip_range=["20.0.0.1", "20.0.255.255"], distribution="seq")
        ip_gen2 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c2,
                            dist_server=ip_gen_s2)

        profile = ASTFProfile(default_ip_gen=ip_gen1, cap_list=[
            ASTFCapInfo(file="../cap2/http_get.pcap"),
            ASTFCapInfo(file="../cap2/http_get.pcap", ip_gen=ip_gen2, port=8080)
            ])

        return profile

    def get_profile(self):
        return self.create_profile()


def register():
    return Prof1()
