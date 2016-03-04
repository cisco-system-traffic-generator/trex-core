from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/802.1Q/802.1Q/802.1Q/802.1Q/IPv6/TCP stream without VM
    Missing values will be filled with defaults
    '''

    def get_streams (self, direction = 0):
        return STLHltStream(frame_size = 100,
                            vlan_id = [1, 2, 3, 4], # can be either array or string separated by spaces
                            vlan_protocol_tag_id = '8100 0x8100', # hex with optional prefix '0x'
                            vlan_user_priority = '4 3 2', # forth will be default
                            l3_protocol = 'ipv6',
                            l4_protocol = 'tcp',
                            direction = direction)

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



