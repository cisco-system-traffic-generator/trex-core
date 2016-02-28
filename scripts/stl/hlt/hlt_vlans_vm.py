from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/802.1Q/802.1Q/802.1Q/802.1Q/802.1Q/IPv4/UDP stream with complex VM on vlan_id's
    Missing values will be filled with defaults
    '''

    def create_streams (self, direction = 0):

        return STLHltStream(frame_size = 100,
                            vlan_id = '1 2 1000 4 5',                          # 5 vlans
                            vlan_id_mode = 'increment fixed decrement random', # 5th vlan will be default fixed
                            vlan_id_step = 2,                                  # 1st vlan step will be 2, others - default 1
                            vlan_id_count = [4, 1, 10],                        # 4th independent on count, 5th will be fixed
                            l3_protocol = 'ipv4',
                            l4_protocol = 'udp',
                            direction = direction,
                            )

    def get_streams (self, direction = 0):
        return self.create_streams(direction = direction)

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



