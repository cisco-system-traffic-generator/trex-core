from trex_astf_lib.api import *

# Only TCP SFR data is here 
# SFR UDP templates are not here yet (e.g. DNS)
# 


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
            #ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=709.89),
            #ASTFCapInfo(file="../avl/delay_10_http_get_0.pcap", cps=404.52,port=8080),
            #ASTFCapInfo(file="../avl/delay_10_http_post_0.pcap", cps=404.52,port=8081),

            #ASTFCapInfo(file="../avl/delay_10_https_0.pcap", cps=1.0),
            #ASTFCapInfo(file="../avl/delay_10_exchange_0.pcap", cps=1),
            #ASTFCapInfo(file="../avl/delay_10_mail_pop_0.pcap", cps=4.759),
            #ASTFCapInfo(file="../avl/delay_10_oracle_0.pcap", cps=79.3178),
            #ASTFCapInfo(file="../avl/delay_10_smtp_0.pcap", cps=7.3369),
            ASTFCapInfo(file="../avl/delay_10_citrix_0.pcap", cps=43.6248)  
        ])

        return profile

    def get_profile(self, **kwargs):
        return self.create_profile()


def register():
    return Prof1()
