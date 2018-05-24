from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Creating 4 streams Eth/IP/UDP with different size and rate (smallest with highest rate)
    Each stream will get rate_pps * his ratio / sum of ratios
    '''

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(length_mode = 'imix', rate_pps = 2,
                            l3_imix1_size = 60, l3_imix1_ratio = 4,
                            l3_imix2_size = 400, l3_imix2_ratio = 3,
                            l3_imix3_size = 2000, l3_imix3_ratio = 2,
                            l3_imix4_size = 8000, l3_imix4_ratio = 1,
                            l3_protocol = 'ipv4',
                            l4_protocol = 'udp',
                            direction = direction,
                            )

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



