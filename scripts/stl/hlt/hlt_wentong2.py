from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Example number 2 of using HLTAPI from Wentong
    Creates Eth/802.1Q/IPv6/UDP stream without VM (if num_of_sessions_per_spoke is 1)
    '''

    def get_streams (self, direction = 0, **kwargs):
        ipv6_tgen_rtr = '2005:0:1::1'
        num_of_sessions_per_spoke = 1 # in given example is not passed forward, taking default
        ipv6_address_step = '0:0:0:1:0:0:0:0'
        ipv6_tgen_hub = '2005:10:1::1'
        normaltrafficrate = 0.9
        vlan_id = 0 # in given example is not passed forward, taking default
        tgen_dst_mac_rtr = '0026.cb0c.6040'

        return STLHltStream(
                l3_protocol = 'ipv6',
                l4_protocol = 'udp',
                ipv6_next_header = 17,
                l3_length = 200,
                transmit_mode= 'continuous',
                ipv6_src_addr = ipv6_tgen_rtr,
                ipv6_src_mode = 'increment',
                ipv6_src_count = num_of_sessions_per_spoke,
                ipv6_dst_step  = ipv6_address_step,
                ipv6_dst_addr = ipv6_tgen_hub,
                ipv6_dst_mode = 'fixed',
                rate_percent  = normaltrafficrate,
                vlan_id = vlan_id,
                vlan_id_mode = 'increment',
                vlan_id_step  = 1,
                mac_src = '0c00.1110.3101',
                mac_dst = tgen_dst_mac_rtr,
                )

# dynamic load  used for trex console or simulator
def register():
    return STLS1()



