from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Two Eth/IP/UDP streams with VM to get different size of packet by frame_size
    '''

    def get_streams (self, direction = 0):
        return [STLHltStream(length_mode = 'increment',
                             frame_size_min = 100,
                             frame_size_max = 3000,
                             l3_protocol = 'ipv4',
                             l4_protocol = 'udp',
                             rate_bps = 1000000,
                             direction = direction,
                             ),
                STLHltStream(length_mode = 'decrement',
                             frame_size_min = 100,
                             frame_size_max = 3000,
                             l3_protocol = 'ipv4',
                             l4_protocol = 'udp',
                             rate_bps = 100000,
                             direction = direction,
                             )
               ]

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



