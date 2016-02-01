
from common.trex_streams import *
from client_utils.packet_builder import CTRexPktBuilder


class STLImix(object):

    def __init__ (self):
        ip_range = {'src' : {}, 'dst': {}}

        ip_range['src']['start'] = "10.0.0.1"
        ip_range['src']['end']   = "10.0.0.254"
        ip_range['dst']['start'] = "8.0.0.1"
        ip_range['dst']['end']   = "8.0.0.254"

        self.ip_range = ip_range

    def get_streams (self, flip = False):

        # construct the base packet for the profile
        base_pkt = CTRexPktBuilder()

        base_pkt.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
        base_pkt.set_layer_attr("l2", "type", dpkt.ethernet.ETH_TYPE_IP)
        base_pkt.add_pkt_layer("l3_ip", dpkt.ip.IP())
        base_pkt.add_pkt_layer("l4_udp", dpkt.udp.UDP())


        if not flip:
            src = self.ip_range['src']
            dst = self.ip_range['dst']
        else:
            src = self.ip_range['dst']
            dst = self.ip_range['src']

        base_pkt.set_vm_ip_range(ip_layer_name = "l3_ip",
                                 ip_field = "src",
                                 ip_start = src['start'],
                                 ip_end = src['end'],
                                 operation = "inc",
                                 split = True)

        base_pkt.set_vm_ip_range(ip_layer_name = "l3_ip",
                                 ip_field = "dst",
                                 ip_start = dst['start'],
                                 ip_end = dst['end'],
                                 operation = "inc")



        # pad to 60 bytes
        pkt_1 = base_pkt.clone()
        payload_size = 60 - len(pkt_1.get_layer('l2'))
        pkt_1.set_pkt_payload("a" * payload_size)

        pkt_1.set_layer_attr("l3_ip", "len", len(pkt_1.get_layer('l3_ip')))


        s1 = STLStream(packet = pkt_1,
                       mode = STLTXCont())

        # stream 2
        pkt_2 = base_pkt.clone()
        payload_size = 590 - len(pkt_2.get_layer('l2'))
        pkt_2.set_pkt_payload("a" * payload_size)

        pkt_2.set_layer_attr("l3_ip", "len", len(pkt_2.get_layer('l3_ip')))

        s2 = STLStream(packet = pkt_2,
                       mode = STLTXCont())


        # stream 3
        pkt_3 = base_pkt.clone()
        payload_size = 1514 - len(pkt_3.get_layer('l2'))
        pkt_3.set_pkt_payload("a" * payload_size)

        pkt_3.set_layer_attr("l3_ip", "len", len(pkt_3.get_layer('l3_ip')))

        s3 = STLStream(packet = pkt_3,
                       mode = STLTXCont())

        return [s1, s2, s3]

# dynamic load
def register():
    return STLImix()
