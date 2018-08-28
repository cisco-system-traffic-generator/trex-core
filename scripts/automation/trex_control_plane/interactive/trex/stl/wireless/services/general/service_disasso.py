from .general_service import GeneralService
from ...trex_wireless_client import APClient
from ...trex_wireless_ap import VAP
from scapy.contrib.capwap import *



class DisassoService(GeneralService):
    """A Service that runs on a set of simulated Clients sending deauth packets as fast as possible."""

    def __init__(self, devices, env, tx_conn, topics_to_subs, done_event=None):
        super().__init__(devices, env, tx_conn, topics_to_subs, done_event)
        for device in devices:
            if not isinstance(device, APClient):
                raise ValueError("DisassoService can only run on a client")
        self.clients = devices
        self.packets = []
        for client in self.clients:
            # vap = VAP(ssid=b"TRex Test SSID", vap_id=1, slot_id=0)
            # pkt = client_disassoc(client.ap, vap, client.mac)
            # TODO encapsulation
            pkt = b'Jacquouille La Fripouille'
            self.packets.append(pkt)
            

    def run(self):
        while True:
            for pkt in self.packets:
                self.send_pkt(pkt)
        
