from trex_astf_lib.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(dist_client=ip_gen_c, dist_server=ip_gen_s)

        profile = ASTFProfile(default_ip_gen=ip_gen, cap_list=[
            ASTFCapInfo(file="../cap2/http_browsing.pcap", l7_percent=10),
            ASTFCapInfo(file="../cap2/http_get.pcap", l7_percent=60, port=8080),
            ASTFCapInfo(file="../cap2/http_post.pcap", l7_percent=30, port=8081),
        ])

        return profile

    def get_profile(self, **kwargs):
        return self.create_profile()


def register():
    return Prof1()
