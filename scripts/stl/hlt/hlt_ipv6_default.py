from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IPv6/UDP stream without VM, default values
    '''

    def get_streams (self, direction = 0):
        return [STLHltStream(l3_protocol = 'ipv6',
                             l3_length = 150,
                             l4_protocol = 'udp',
                             direction = direction,
                             ),
               ]

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



