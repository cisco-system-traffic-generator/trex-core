from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):

    def create_streams (self):
        return [STLHltStream(length_mode = 'increment',
                             frame_size_max = 9*1024,
                             ip_src_addr = '16.0.0.1',
                             ip_dst_addr = '48.0.0.1',
                             l4_protocol = 'udp',
                             udp_src_port = 1025,
                             udp_dst_port = 12,
                             ),
                STLHltStream(length_mode = 'decrement',
                             frame_size_max = 9*1024,
                             ip_src_addr = '16.0.0.1',
                             ip_dst_addr = '48.0.0.1',
                             l4_protocol = 'udp',
                             udp_src_port = 1025,
                             udp_dst_port = 12,
                             )
               ]

    def get_streams (self, direction = 0):
        return self.create_streams()

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



