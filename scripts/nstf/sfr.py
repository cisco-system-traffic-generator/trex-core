from trex_nstf_lib.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # ip generator
        ip_gen_c = NSTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s = NSTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen = NSTFIPGen(glob=NSTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        profile = NSTFProfile(default_ip_gen=ip_gen, cap_list=[
            NSTFCapInfo(file="../cap2/oracle.pcap", cps=150),
            NSTFCapInfo(file="../cap2/Video_Calls.pcap", cps=11.4),
            NSTFCapInfo(file="../cap2/rtp_160k.pcap", cps=3.6),
            NSTFCapInfo(file="../cap2/rtp_250k_rtp_only_1.pcap", cps=4),
            NSTFCapInfo(file="../cap2/rtp_250k_rtp_only_2.pcap", cps=4, port=1027),
            NSTFCapInfo(file="../cap2/smtp.pcap", cps=34.2),
            NSTFCapInfo(file="../cap2/Voice_calls_rtp_only.pcap", cps=66),
            NSTFCapInfo(file="../cap2/citrix.pcap", cps=105),
            NSTFCapInfo(file="../cap2/dns.pcap", cps=2400),
            NSTFCapInfo(file="../cap2/exchange.pcap", cps=630),
            NSTFCapInfo(file="../cap2/http_browsing.pcap", cps=267),
            NSTFCapInfo(file="../cap2/http_get.pcap", cps=345, port=8080),
            NSTFCapInfo(file="../cap2/http_post.pcap", cps=345, port=8081),
            NSTFCapInfo(file="../cap2/https.pcap", cps=111, port=8082),
            NSTFCapInfo(file="../cap2/mail_pop.pcap", cps=34.2)
        ])

        return profile

    def get_profile(self):
        return self.create_profile()


def register():
    return Prof1()
