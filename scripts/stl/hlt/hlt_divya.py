from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Divya Saxena's profile
    '''

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(name = "stream_1",
                            mode = "create",
                            port_handle = 0,
                            frame_size = 64,
                            l3_protocol = "ipv4",
                            ip_src_addr = "10.0.0.1",
                            ip_src_mode = "increment",
                            ip_src_count = 254,
                            ip_dst_addr = "8.0.0.1",
                            ip_dst_mode = "increment",
                            ip_dst_count = 254,
                            l4_protocol = "udp",
                            udp_dst_port = 12,
                            udp_src_port = 1025,
                            rate_pps = 1339285,
                            transmit_mode = "multi_burst",
                            pkts_per_burst = 1,
                            vlan_id = 10,
                            # TODO: fix
                            # https://trex-tgn.cisco.com/youtrack/issue/trex-418
                            #burst_loop_count = 1339285,
                            burst_loop_count = 100,
                            )

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



