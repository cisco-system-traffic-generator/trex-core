from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IP/UDP stream with VM, to change the MAC addr (only 32 lsb)
    '''

    def get_streams (self, direction = 0):
        return STLHltStream(l3_protocol = 'ipv4', l4_protocol = 'udp',
                            mac_src = '10:00:00:00:00:01', mac_dst = '10:00:00:00:01:00',
                            mac_src2 = '11:11:00:00:00:01', mac_dst2 = '11:11:00:00:01:00',
                            mac_src_step = 2, mac_src_mode = 'decrement', mac_src_count = 19,
                            mac_dst_step = 2, mac_dst_mode = 'increment', mac_dst_count = 19,
                            mac_src2_step = 2, mac_src2_mode = 'decrement', mac_src2_count = 19,
                            mac_dst2_step = 2, mac_dst2_mode = 'increment', mac_dst2_count = 19,
                            direction = direction,
                            save_to_yaml = '/tmp/foo.yaml',
                            )

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



