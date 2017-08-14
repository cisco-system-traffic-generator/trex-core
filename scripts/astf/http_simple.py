from trex_astf_lib.api import ASTFProfile, ASTFCapInfo


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):
        return ASTFProfile(cap_list=[ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap")])


def register():
    return Prof1()
