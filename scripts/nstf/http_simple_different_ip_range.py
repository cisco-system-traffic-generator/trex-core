import os
from trex_nstf_lib.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        ip_gen_c1 = NSTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s1 = NSTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen1 = NSTFIPGen(glob=NSTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c1,
                            dist_server=ip_gen_s1)

        ip_gen_c2 = NSTFIPGenDist(ip_range=["10.0.0.1", "10.0.0.255"], distribution="seq")
        ip_gen_s2 = NSTFIPGenDist(ip_range=["20.0.0.1", "20.255.255"], distribution="seq")
        ip_gen2 = NSTFIPGen(glob=NSTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c2,
                            dist_server=ip_gen_s2)

        profile = NSTFProfile(cap_list=[
            NSTFCapInfo(file="../cap2/http_get.pcap", ip_gen=ip_gen1),
            NSTFCapInfo(file="../cap2/http_get.pcap", ip_gen=ip_gen2, port=8080)
            ])

        return profile

    def get_profile(self):
        return self.create_profile()


def register():
    return Prof1()
