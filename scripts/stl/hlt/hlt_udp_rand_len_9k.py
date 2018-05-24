from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Create Eth/IP/UDP steam with random packet size (L3 size from 50 to 9*1024)
    '''

    def get_streams (self, direction = 0, random_seed = 0, **kwargs):
        min_size = 50
        max_size = 9*1024
        return [STLHltStream(length_mode   = 'random',
                             l3_length_min = min_size,
                             l3_length_max = max_size,
                             l3_protocol   = 'ipv4',
                             ip_src_addr   = '16.0.0.1',
                             ip_dst_addr   = '48.0.0.1',
                             l4_protocol   = 'udp',
                             udp_src_port  = 1025,
                             udp_dst_port  = 12,
                             rate_pps      = 1000,
                             ignore_macs   = True,
                             consistent_random = (random_seed != 0)
                             )
               ]

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



