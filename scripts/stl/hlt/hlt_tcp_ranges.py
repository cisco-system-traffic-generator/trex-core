from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):

    def create_streams (self, direction = 0):
        return [STLHltStream(tcp_src_port_mode = 'decrement',
                             tcp_src_port_count = 10,
                             tcp_src_port = 1234,
                             tcp_dst_port_mode = 'increment',
                             tcp_dst_port_count = 10,
                             tcp_dst_port = 1234,
                             name = 'test_tcp_ranges',
                             direction = direction,
                             ),
               ]

    def get_streams (self, direction = 0):
        return self.create_streams(direction)

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



