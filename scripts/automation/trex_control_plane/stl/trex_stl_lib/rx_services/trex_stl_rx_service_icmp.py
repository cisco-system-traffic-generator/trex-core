from .trex_stl_rx_service_api import RXServiceAPI

from ..trex_stl_streams import STLStream, STLTXSingleBurst
from ..trex_stl_packet_builder_scapy import STLPktBuilder

from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, ICMP


class RXServiceICMP(RXServiceAPI):
    
    def __init__ (self, port, ping_ip, pkt_size):
        
        super(RXServiceICMP, self).__init__(port, layer_mode = RXServiceAPI.LAYER_MODE_L3)
        self.ping_ip  = ping_ip
        self.pkt_size = pkt_size

    def get_name (self):
        return "PING"
        
    def pre_execute (self):

        if not self.port.is_resolved():
            return self.port.err('ping - port has an unresolved destination, cannot determine next hop MAC address')

        self.layer_cfg = dict(self.port.get_layer_cfg())

        return self.port.ok()


    # return a list of streams for request
    def generate_request (self):

        base_pkt = Ether(dst = self.layer_cfg['ether']['dst'])/IP(src = self.layer_cfg['ipv4']['src'], dst = self.ping_ip)/ICMP(type = 8)
        pad = max(0, self.pkt_size - len(base_pkt))

        base_pkt = base_pkt / (pad * 'x')

        s1 = STLStream( packet = STLPktBuilder(pkt = base_pkt), mode = STLTXSingleBurst(total_pkts = 1) )

        self.base_pkt = base_pkt

        return [s1]

    def on_pkt_rx (self, pkt, start_ts):
        
        scapy_pkt = Ether(pkt['binary'])
        if not 'ICMP' in scapy_pkt:
            return None

        ip = scapy_pkt['IP']
        if ip.dst != self.layer_cfg['ipv4']['src']:
            return None

        icmp = scapy_pkt['ICMP']

        dt = pkt['ts'] - start_ts

        # echo reply
        if icmp.type == 0:
            # check seq
            if icmp.seq != self.base_pkt['ICMP'].seq:
                return None
            return self.port.ok('Reply from {0}: bytes={1}, time={2:.2f}ms, TTL={3}'.format(ip.src, len(pkt['binary']), dt * 1000, ip.ttl))

        # unreachable
        elif icmp.type == 3:
            # check seq
            if icmp.payload.seq != self.base_pkt['ICMP'].seq:
                return None
            return self.port.ok('Reply from {0}: Destination host unreachable'.format(icmp.src))

        else:
            # skip any other types
            #scapy_pkt.show2()
            return None



    # return the str of a timeout err
    def on_timeout_err (self, retries):
        return self.port.ok('Request timed out.')

