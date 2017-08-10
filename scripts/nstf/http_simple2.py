from trex_nstf_lib.api import NSTFProfile, NSTFCapInfo


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):
        return NSTFProfile(cap_list=[
          NSTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=1,port=81),
          NSTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=2,port=82),
          NSTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=3,port=83),
        ])


def register():
    return Prof1()
