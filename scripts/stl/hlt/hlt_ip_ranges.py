from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Eth/IP/TCP stream with VM to get different ip addresses
    '''

    def get_streams (self, direction = 0):
        return STLHltStream(split_by_cores = 'duplicate',
                            l3_protocol = 'ipv4',
                            ip_src_addr = '192.168.1.1',
                            ip_src_mode = 'increment',
                            ip_src_count = 5,
                            ip_dst_addr = '5.5.5.5',
                            ip_dst_mode = 'random',
                            consistent_random = True,
                            direction = direction,
                            rate_pps = 1)

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



