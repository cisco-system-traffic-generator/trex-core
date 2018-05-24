from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Example number 1 of using HLTAPI from Wentong
    Creates Eth/802.1Q/IP/UDP stream with  VM on src IP
    '''

    def get_streams (self, direction = 0, **kwargs):
        ipv4_address_step = '0.0.1.0'
        num_of_sessions_per_spoke = 1000
        ip_tgen_hub = '190.1.0.1'
        pkt_size = 128
        normaltrafficrate = 0.9
        vlan_id = 2
        tgen_dst_mac = '588d.090d.7310'

        return STLHltStream(
                l3_protocol = 'ipv4',
                l4_protocol = 'udp',
                transmit_mode = 'continuous',
                ip_src_addr = '200.10.0.1',
                ip_src_mode = 'increment',
                ip_src_step = ipv4_address_step,
                ip_src_count = num_of_sessions_per_spoke,
                ip_dst_addr = ip_tgen_hub,
                ip_dst_mode = 'fixed',
                frame_size = pkt_size,
                rate_percent = normaltrafficrate,
                vlan_id = vlan_id,
                vlan_id_mode = 'increment',
                vlan_id_step = 1,
                mac_src = '0c00.1110.3101',
                mac_dst = tgen_dst_mac,
                )

# dynamic load  used for trex console or simulator
def register():
    return STLS1()



