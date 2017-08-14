from trex_astf_lib.api import *


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
            ASTFCapInfo(file="../cap2/Oracle.pcap", cps=150),
            ASTFCapInfo(file="../cap2/Video_Calls.pcap", cps=11.4),
            ASTFCapInfo(file="../cap2/rtp_160k.pcap", cps=3.6),
            ASTFCapInfo(file="../cap2/rtp_250k_rtp_only_1.pcap", cps=4),
            ASTFCapInfo(file="../cap2/rtp_250k_rtp_only_2.pcap", cps=4, port=1027),
            ASTFCapInfo(file="../cap2/smtp.pcap", cps=34.2),
            ASTFCapInfo(file="../cap2/Voice_calls_rtp_only.pcap", cps=66),
            ASTFCapInfo(file="../cap2/citrix.pcap", cps=105),
            ASTFCapInfo(file="../cap2/dns.pcap", cps=2400),
            ASTFCapInfo(file="../cap2/exchange.pcap", cps=630),
            ASTFCapInfo(file="../cap2/http_browsing.pcap", cps=267),
            ASTFCapInfo(file="../cap2/http_get.pcap", cps=345, port=8080),
            ASTFCapInfo(file="../cap2/http_post.pcap", cps=345, port=8081),
            ASTFCapInfo(file="../cap2/https.pcap", cps=111, port=8082),
            ASTFCapInfo(file="../cap2/mail_pop.pcap", cps=34.2)
        ])

        return profile

    def get_profile(self):
        return self.create_profile()


def register():
    return Prof1()
