from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(length_mode = 'imix', rate_pps = 2,
                            l3_protocol = 'ipv4', l4_protocol = 'udp')

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



