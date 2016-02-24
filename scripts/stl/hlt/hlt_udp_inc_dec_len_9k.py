from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Create 2 Eth/IP/UDP steams with different packet size:
    First stream will start from 64 bytes (default) and will increase until max_size (9,216)
    Seconds stream will decrease the packet size in reverse way
    '''

    def create_streams (self):
        max_size = 9*1024
        return [STLHltStream(length_mode = 'increment',
                             frame_size_max = max_size,
                             ip_src_addr = '16.0.0.1',
                             ip_dst_addr = '48.0.0.1',
                             l4_protocol = 'udp',
                             udp_src_port = 1025,
                             udp_dst_port = 12,
                             rate_pps = 1,
                             ),
                STLHltStream(length_mode = 'decrement',
                             frame_size_max = max_size,
                             ip_src_addr = '16.0.0.1',
                             ip_dst_addr = '48.0.0.1',
                             l4_protocol = 'udp',
                             udp_src_port = 1025,
                             udp_dst_port = 12,
                             rate_pps = 1,
                             )
               ]

    def get_streams (self, direction = 0):
        return self.create_streams()

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



