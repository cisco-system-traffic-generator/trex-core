from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):

    def create_streams (self):
        return STLHltStream(length_mode = 'imix', rate_pps = 2,
                            l3_protocol = 'ipv4', l4_protocol = 'tcp')

    def get_streams (self, direction = 0):
        return self.create_streams()

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



