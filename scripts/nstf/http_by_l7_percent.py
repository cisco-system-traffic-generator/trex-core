from trex_nstf_lib.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        profile = NSTFProfile(cap_list=[
            NSTFCapInfo(file="../cap2/http_browsing.pcap", l7_percent=10),
            NSTFCapInfo(file="../cap2/http_get.pcap", l7_percent=60, port=8080),
            NSTFCapInfo(file="../cap2/http_post.pcap", l7_percent=30, port=8081),
        ])

        return profile

    def get_profile(self):
        return self.create_profile()


def register():
    return Prof1()
