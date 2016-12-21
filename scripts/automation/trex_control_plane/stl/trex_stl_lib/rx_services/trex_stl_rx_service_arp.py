from .trex_stl_rx_service_api import RXServiceAPI

from ..trex_stl_streams import STLStream, STLTXSingleBurst
from ..trex_stl_packet_builder_scapy import STLPktBuilder

from scapy.layers.l2 import Ether, ARP


class RXServiceARP(RXServiceAPI):
    
    def __init__ (self, port_id):
        super(RXServiceARP, self).__init__(port_id, layer_mode = RXServiceAPI.LAYER_MODE_L3)

    def get_name (self):
        return "ARP"
        
    def pre_execute (self):

        self.dst = self.port.get_dst_addr()
        self.src = self.port.get_src_addr()

        return self.port.ok()

    # return a list of streams for request
    def generate_request (self):

        base_pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(psrc = self.src['ipv4'], pdst = self.dst['ipv4'], hwsrc = self.src['mac'])
        s1 = STLStream( packet = STLPktBuilder(pkt = base_pkt), mode = STLTXSingleBurst(total_pkts = 1) )

        return [s1]


    def on_pkt_rx (self, pkt, start_ts):
        # convert to scapy
        scapy_pkt = Ether(pkt['binary'])
        
        # if not ARP wait for the next one
        if not 'ARP' in scapy_pkt:
            return None

        arp = scapy_pkt['ARP']

        # check this is the right ARP (ARP reply with the address)
        if (arp.op != 2) or (arp.psrc != self.dst['ipv4']):
            return None


        return self.port.ok({'psrc' : arp.psrc, 'hwsrc': arp.hwsrc})
        
        #return self.port.ok('Port {0} - Recieved ARP reply from: {1}, hw: {2}'.format(self.port.port_id, arp.psrc, arp.hwsrc))


    def on_timeout_err (self, retries):
        return self.port.err('failed to receive ARP response ({0} retries)'.format(retries))



