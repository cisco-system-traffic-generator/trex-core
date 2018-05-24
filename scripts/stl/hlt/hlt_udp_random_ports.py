from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IP/UDP stream with VM for random UDP ports inc/dec.
    Using "consistent_random = True" to have same random ports each test
    '''

    def get_streams (self, direction = 0, **kwargs):
        return [STLHltStream(l3_protocol = 'ipv4',
                             l4_protocol = 'udp',
                             udp_src_port_mode = 'random',
                             udp_dst_port_mode = 'random',
                             direction = direction,
                             rate_pps = 1000,
                             consistent_random = True,
                             ),
               ]

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



