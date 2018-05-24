from trex.stl.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Example number 3 of using HLTAPI from David
    Creates Eth/802.1Q/802.1Q/IP/TCP stream
    '''

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(
                l3_protocol = 'ipv4',
                ip_src_addr = '100.1.1.1',
                ip_src_mode = 'fixed',
                ip_src_count = 1,
                ip_src_step = '0.0.0.1',
                ip_dst_addr = '200.1.1.1',
                ip_dst_mode = 'fixed',
                ip_dst_step = '0.1.0.0',
                ip_dst_count = 1,
                l3_length = 474,
                ip_dscp = 10,
                rate_bps = 256000000,
                mac_src_mode = 'fixed',
                mac_src_step = 1,
                mac_src_count = 1,
                #mac_dst_mode = discovery  # not supported yet,
                mac_dst_step = 1,
                mac_dst_count = 1,
                #mac_src = [ip_to_mac 200.1.1.1] # not supported yet,
                #mac_src2 = [ip_to_mac = 100.1.1.1] # not supported yet,
                #mac_dst = [ip_to_mac 100.1.1.1] # not supported yet,
                #mac_dst2 = [ip_to_mac = 200.1.1.1] # not supported yet,
                vlan_id_mode = 'fixed fixed',
                vlan_id = '200 100',
                vlan_id_count = '1 1',
                vlan_id_step = '1 1',
                vlan_user_priority = '3 0',
                vlan_cfi = '1 1',
                direction = direction,
                )

# dynamic load  used for trex console or simulator
def register():
    return STLS1()



