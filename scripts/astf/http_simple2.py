from trex_astf_lib.api import ASTFProfile, ASTFCapInfo


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):
        return ASTFProfile(cap_list=[
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=1,port=81),
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=2,port=82),
          ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=3,port=83),
        ])


def register():
    return Prof1()
