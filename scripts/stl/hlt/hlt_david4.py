from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Example number 4 of using HLTAPI from David
    Creates Eth/802.1Q/802.1Q/IP/TCP stream with complex VM:
        The first vlan_id will be incremented, second const.
        MAC src, IP src, IP dst will have <mac_src_count> number of incremental values
        MAC dst will have <mac_dst_count> number of incremental values
    '''

    def create_streams (self, direction = 0):
        mac_dst_count = 10
        mac_src_count = 10
        pkts_per_burst = 10
        intf_traffic_dst_ip = '48.0.0.1'
        intf_traffic_src_ip = '16.0.0.1'

        return STLHltStream(
                #enable_auto_detect_instrumentation = 1, # not supported yet
                ip_dst_addr = intf_traffic_dst_ip,
                ip_dst_count = mac_src_count,
                ip_dst_mode = 'increment',
                ip_dst_step = '0.0.1.0',
                ip_src_addr = intf_traffic_src_ip,
                ip_src_count = mac_src_count,
                ip_src_mode = 'increment',
                ip_src_step = '0.0.1.0',
                l3_protocol = 'ipv4',
                mac_dst_count = mac_dst_count,
                #mac_dst_mode = 'discovery', # not supported yet
                mac_dst_mode = 'increment',
                mac_dst_step = 1,
                mac_src_count = mac_src_count,
                mac_src_mode = 'increment',
                mac_src_step = 1,
                pkts_per_burst = pkts_per_burst,
                transmit_mode = 'continuous',
                vlan_id = '50 50',
                vlan_id_count = '2 2',
                vlan_id_mode = 'increment fixed',
                vlan_id_step = '1 1',
                #vlan_protocol_tag_id = '{8100 8100}',
                direction = direction,
                )

    def get_streams (self, direction = 0):
        return self.create_streams(direction)

# dynamic load  used for trex console or simulator
def register():
    return STLS1()



