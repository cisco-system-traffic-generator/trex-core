from wireless.services.general.general_service import GeneralService
from wireless.trex_wireless_ap import AP
from scapy.contrib.capwap import *

class GeneralAPServiceExample(GeneralService):
    """A Service that runs on a set of simulated APs doing nothing but waiting."""

    def __init__(self, devices, env, tx_conn, topics_to_subs, done_event=None):
        super().__init__(devices, env, tx_conn, topics_to_subs, done_event)
        for device in devices:
            if not isinstance(device, AP):
                raise ValueError("GeneralAPServiceExample can only run on an AP")
        self.aps = devices
        self.subservices.append(GeneralAPSubServiceExample(devices, env, tx_conn, topics_to_subs, done_event))

    def run(self):
        while True:
            yield self.async_wait(1)


class GeneralAPSubServiceExample(GeneralService):
    """A Service that runs on a set of simulated APs doing nothing but waiting."""
    
    def run(self):
        while True:
            yield self.async_wait(1)

