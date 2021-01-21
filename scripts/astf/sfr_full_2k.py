from trex.astf.api import *
import argparse



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
            ASTFCapInfo(file="../avl/delay_10_http_get_0.pcap", cps=404.52,port=8080),
            ASTFCapInfo(file="../avl/delay_10_http_post_0.pcap", cps=404.52,port=8081),
            ASTFCapInfo(file="../avl/delay_10_https_0.pcap", cps=130.87),
            ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap", cps=709.89),


            ASTFCapInfo(file="../avl/delay_10_exchange_0.pcap", cps=253.81),

            ASTFCapInfo(file="../avl/delay_10_mail_pop_0.pcap", cps=4.759),
            ASTFCapInfo(file="../avl/delay_10_mail_pop_1.pcap", cps=4.759,port=111),
            ASTFCapInfo(file="../avl/delay_10_mail_pop_2.pcap", cps=4.759,port=112),

            ASTFCapInfo(file="../avl/delay_10_oracle_0.pcap", cps=79.31),

            ASTFCapInfo(file="../avl/delay_10_rtp_160k_0.pcap", cps=2.776),
            ASTFCapInfo(file="../avl/delay_10_rtp_160k_1.pcap", cps=2.776),
            ASTFCapInfo(file="../avl/delay_10_rtp_250k_0_0.pcap", cps=1.982),
            ASTFCapInfo(file="../avl/delay_10_rtp_250k_1_0.pcap", cps=1.982),

            ASTFCapInfo(file="../avl/delay_10_smtp_0.pcap", cps=7.33),
            ASTFCapInfo(file="../avl/delay_10_smtp_1.pcap", cps=7.33,port=26),
            ASTFCapInfo(file="../avl/delay_10_smtp_2.pcap", cps=7.33,port=27),

            ASTFCapInfo(file="../avl/delay_10_video_call_0.pcap", cps=24),

            ASTFCapInfo(file="../avl/delay_10_video_call_rtp_0.pcap", cps=29.347),

            ASTFCapInfo(file="../avl/delay_10_citrix_0.pcap", cps=43.62),

            ASTFCapInfo(file="../avl/delay_10_dns_0.pcap", cps=417),

            ASTFCapInfo(file="../avl/delay_10_sip_0.pcap", cps = 29.34),
            ASTFCapInfo(file="../avl/delay_10_rtsp_0.pcap", cps = 4.8),

        ])

        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        return self.create_profile()


def register():
    return Prof1()
