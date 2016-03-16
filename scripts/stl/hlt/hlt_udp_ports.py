from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IP/UDP stream with VM for different UDP ports inc/dec
    The ports overlap the max and min at very first packets
    '''

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(l3_protocol = 'ipv4',
                            l4_protocol = 'udp',
                            udp_src_port_mode = 'decrement',
                            udp_src_port_count = 45,
                            udp_src_port_step = 20,
                            udp_src_port = 123,
                            udp_dst_port_mode = 'increment',
                            udp_dst_port_count = 100,
                            udp_dst_port_step = 300,
                            udp_dst_port = 65000,
                            direction = direction,
                            rate_pps = 1000,
                            )

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



