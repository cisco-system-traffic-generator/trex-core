# Example for creating your program from pcap file

from trex.astf.api import *

class Prof1():
    def get_profile(self, **kwargs):
        file = kwargs.get('file', "../avl/delay_dns_0.pcap")
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen,
                            cap_list=[ASTFCapInfo(file=file,
                            cps=1)])



def register():
    return Prof1()
