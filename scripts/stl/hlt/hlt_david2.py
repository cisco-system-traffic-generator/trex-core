from trex_stl_lib.trex_stl_hltapi import STLHltStream


class STLS1(object):
    '''
    Example number 2 of using HLTAPI from David
    Creates 3 streams (imix) Eth/802.1Q/IP/TCP without VM
    '''

    def get_streams (self, direction = 0, **kwargs):
        return STLHltStream(
                #enable_auto_detect_instrumentation = '1', # not supported yet
                ip_dst_addr = '192.168.3.3',
                ip_dst_count = '1',
                ip_dst_mode = 'increment',
                ip_dst_step = '0.0.0.1',
                ip_src_addr = '192.168.2.3',
                ip_src_count = '1',
                ip_src_step = '0.0.0.1',
                l3_imix1_ratio = 7,
                l3_imix1_size = 68,
                l3_imix2_ratio = 4,
                l3_imix2_size = 570,
                l3_imix3_ratio = 1,
                l3_imix3_size = 1518,
                l3_protocol = 'ipv4',
                length_mode = 'imix',
                #mac_dst_mode = 'discovery', # not supported yet
                mac_src = '00.00.c0.a8.02.03',
                mac_src2 = '00.00.c0.a8.03.03',
                pkts_per_burst = '200000',
                rate_percent = '0.75',
                transmit_mode = 'continuous',
                vlan_id = '2',
                direction = direction,
                )

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



