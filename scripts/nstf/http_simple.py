from trex_nstf_lib.api import NSTFProfile, NSTFCapInfo


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):
        return NSTFProfile(cap_list=[NSTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap")])


def register():
    return Prof1()
