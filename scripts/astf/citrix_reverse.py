from trex.astf.api import *



class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        profile = ASTFProfile(default_ip_gen=ip_gen, cap_list=[
            ASTFCapInfo(file="../avl/delay_10_citrix_0.pcap", cps=1),
            ASTFCapInfo(file="../avl/delay_10_dns_0.pcap", cps=1)
        ])

        return profile

    def get_profile(self, **kwargs):
        return self.create_profile()


def register():
    return Prof1()
