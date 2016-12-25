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

        self.layer_cfg = dict(self.port.get_layer_cfg())
        return self.port.ok()

    # return a list of streams for request
    def generate_request (self):

        base_pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(psrc = self.layer_cfg['ipv4']['src'],
                                                      pdst = self.layer_cfg['ipv4']['dst'],
                                                      hwsrc = self.layer_cfg['ether']['src'])
        
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
        if (arp.op != 2) or (arp.psrc != self.layer_cfg['ipv4']['dst']):
            return None

        # return the data gathered from the ARP response
        return self.port.ok({'psrc' : arp.psrc, 'hwsrc': arp.hwsrc})


    def on_timeout_err (self, retries):
        return self.port.err('failed to receive ARP response ({0} retries)'.format(retries))



