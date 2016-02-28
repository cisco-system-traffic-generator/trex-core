from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IPv6/UDP stream with VM, to change the ipv6 addr (only 32 lsb)
    '''

    def create_streams (self, direction = 0):
        return STLHltStream(l3_protocol = 'ipv6', l3_length = 150, l4_protocol = 'udp',
                            ipv6_src_addr = '1111:2222:3333:4444:5555:6666:7777:8888',
                            ipv6_dst_addr = '1111:1111:1111:1111:1111:1111:1111:1111',
                            ipv6_src_mode = 'increment', ipv6_src_step = 5, ipv6_src_count = 10,
                            ipv6_dst_mode = 'decrement', ipv6_dst_step = '1111:1111:1111:1111:1111:0000:0000:0011', ipv6_dst_count = 150,
                            )

    def get_streams (self, direction = 0):
        return self.create_streams(direction)

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



